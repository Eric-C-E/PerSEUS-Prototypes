#include "motion_reset.h"

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "motion_reset";

#define MPU6050_I2C_ADDR                 0x68
#define MPU6050_REG_SMPLRT_DIV           0x19
#define MPU6050_REG_CONFIG               0x1A
#define MPU6050_REG_ACCEL_CONFIG         0x1C
#define MPU6050_REG_ACCEL_XOUT_H         0x3B
#define MPU6050_REG_PWR_MGMT_1           0x6B
#define MPU6050_REG_WHO_AM_I             0x75
#define MPU6050_WHO_AM_I_VALUE           0x68

#define MPU6050_I2C_TIMEOUT_MS           100
#define MPU6050_SAMPLE_PERIOD_MS         50
#define MPU6050_RETRY_PERIOD_MS          2000
#define MPU6050_SHAKE_DELTA_THRESHOLD    8000
#define MPU6050_SHAKE_COOLDOWN_MS        1500

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} accel_sample_t;

typedef struct {
    motion_reset_callback_t callback;
    void *user_ctx;
    volatile bool shake_pending;
    volatile bool touch_pending;
    bool mpu_ready;
    bool have_last_sample;
    TickType_t last_mpu_retry_tick;
    TickType_t last_shake_tick;
    accel_sample_t last_sample;
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t mpu_dev;
} motion_reset_ctx_t;

static motion_reset_ctx_t s_motion_reset;

static esp_err_t mpu6050_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_motion_reset.mpu_dev, &reg_addr, 1, data, len, MPU6050_I2C_TIMEOUT_MS);
}

static esp_err_t mpu6050_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(s_motion_reset.mpu_dev, write_buf, sizeof(write_buf), MPU6050_I2C_TIMEOUT_MS);
}

static esp_err_t mpu6050_read_accel(accel_sample_t *sample)
{
    uint8_t data[6] = {0};
    ESP_RETURN_ON_ERROR(mpu6050_read(MPU6050_REG_ACCEL_XOUT_H, data, sizeof(data)), TAG, "read accel failed");

    sample->x = (int16_t)((data[0] << 8) | data[1]);
    sample->y = (int16_t)((data[2] << 8) | data[3]);
    sample->z = (int16_t)((data[4] << 8) | data[5]);
    return ESP_OK;
}

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static esp_err_t mpu6050_init(void)
{
    if (!s_motion_reset.i2c_bus) {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = BOARD_MPU6050_I2C_PORT,
            .sda_io_num = BOARD_MPU6050_PIN_SDA,
            .scl_io_num = BOARD_MPU6050_PIN_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_motion_reset.i2c_bus), TAG, "I2C bus init failed");
    }

    if (!s_motion_reset.mpu_dev) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = MPU6050_I2C_ADDR,
            .scl_speed_hz = BOARD_MPU6050_I2C_CLOCK_HZ,
        };
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_motion_reset.i2c_bus, &dev_config, &s_motion_reset.mpu_dev),
                            TAG, "MPU6050 add device failed");
    }

    uint8_t who_am_i = 0;
    ESP_RETURN_ON_ERROR(mpu6050_read(MPU6050_REG_WHO_AM_I, &who_am_i, 1), TAG, "MPU6050 WHO_AM_I read failed");
    ESP_RETURN_ON_FALSE(who_am_i == MPU6050_WHO_AM_I_VALUE, ESP_ERR_NOT_FOUND, TAG,
                        "unexpected MPU6050 WHO_AM_I 0x%02x", who_am_i);

    ESP_RETURN_ON_ERROR(mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, 0x00), TAG, "MPU6050 wake failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(mpu6050_write_byte(MPU6050_REG_CONFIG, 0x03), TAG, "MPU6050 DLPF config failed");
    ESP_RETURN_ON_ERROR(mpu6050_write_byte(MPU6050_REG_SMPLRT_DIV, 0x09), TAG, "MPU6050 sample rate config failed");
    ESP_RETURN_ON_ERROR(mpu6050_write_byte(MPU6050_REG_ACCEL_CONFIG, 0x08), TAG, "MPU6050 accel range config failed");

    s_motion_reset.have_last_sample = false;
    ESP_LOGI(TAG, "MPU6050 initialized on SDA GPIO%d, SCL GPIO%d", BOARD_MPU6050_PIN_SDA, BOARD_MPU6050_PIN_SCL);
    return ESP_OK;
}

static void mpu6050_poll(void)
{
    const TickType_t now = xTaskGetTickCount();

    if (!s_motion_reset.mpu_ready) {
        if (s_motion_reset.last_mpu_retry_tick != 0 &&
            (now - s_motion_reset.last_mpu_retry_tick) < pdMS_TO_TICKS(MPU6050_RETRY_PERIOD_MS)) {
            return;
        }

        s_motion_reset.last_mpu_retry_tick = now;
        esp_err_t ret = mpu6050_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "MPU6050 not ready: %s", esp_err_to_name(ret));
            return;
        }
        s_motion_reset.mpu_ready = true;
    }

    accel_sample_t sample = {0};
    esp_err_t ret = mpu6050_read_accel(&sample);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MPU6050 sample failed: %s", esp_err_to_name(ret));
        s_motion_reset.mpu_ready = false;
        s_motion_reset.have_last_sample = false;
        return;
    }

    if (s_motion_reset.have_last_sample) {
        int32_t delta = abs_i32((int32_t)sample.x - s_motion_reset.last_sample.x) +
                        abs_i32((int32_t)sample.y - s_motion_reset.last_sample.y) +
                        abs_i32((int32_t)sample.z - s_motion_reset.last_sample.z);
        bool cooldown_done = (now - s_motion_reset.last_shake_tick) >= pdMS_TO_TICKS(MPU6050_SHAKE_COOLDOWN_MS);

        if (delta >= MPU6050_SHAKE_DELTA_THRESHOLD && cooldown_done) {
            ESP_LOGI(TAG, "MPU6050 shake detected, accel delta=%ld", (long)delta);
            s_motion_reset.last_shake_tick = now;
            motion_reset_raise_shake_flag();
        }
    }

    s_motion_reset.last_sample = sample;
    s_motion_reset.have_last_sample = true;
}

static void motion_reset_task(void *arg)
{
    (void)arg;

    while (true) {
        if (s_motion_reset.shake_pending) {
            s_motion_reset.shake_pending = false;
            ESP_LOGI(TAG, "Shake flag detected");
            if (s_motion_reset.callback) {
                s_motion_reset.callback(MOTION_RESET_SOURCE_SHAKE, s_motion_reset.user_ctx);
            }
        }

        if (s_motion_reset.touch_pending) {
            s_motion_reset.touch_pending = false;
            ESP_LOGI(TAG, "Touch flag detected");
            if (s_motion_reset.callback) {
                s_motion_reset.callback(MOTION_RESET_SOURCE_TOUCH, s_motion_reset.user_ctx);
            }
        }

        mpu6050_poll();
        vTaskDelay(pdMS_TO_TICKS(MPU6050_SAMPLE_PERIOD_MS));
    }
}

esp_err_t motion_reset_init(motion_reset_callback_t callback, void *user_ctx)
{
    ESP_LOGI(TAG, "Initialize motion reset skeleton");
    s_motion_reset.callback = callback;
    s_motion_reset.user_ctx = user_ctx;
    s_motion_reset.shake_pending = false;
    s_motion_reset.touch_pending = false;
    s_motion_reset.mpu_ready = false;
    s_motion_reset.have_last_sample = false;
    s_motion_reset.last_mpu_retry_tick = 0;
    s_motion_reset.last_shake_tick = 0;
    s_motion_reset.i2c_bus = NULL;
    s_motion_reset.mpu_dev = NULL;

    BaseType_t task_ok = xTaskCreate(motion_reset_task, "motion_reset", 4096, NULL, 4, NULL);
    ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create motion reset task");

    return ESP_OK;
}

void motion_reset_raise_shake_flag(void)
{
    s_motion_reset.shake_pending = true;
}

void motion_reset_raise_touch_flag(void)
{
    s_motion_reset.touch_pending = true;
}
