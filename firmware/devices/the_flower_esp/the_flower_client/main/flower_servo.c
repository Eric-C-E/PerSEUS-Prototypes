#include "flower_servo.h"

#include "board_config.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "flower_servo";

typedef enum {
    SERVO_AXIS_TILT = 0,
    SERVO_AXIS_ROTATION,
    SERVO_AXIS_COUNT,
} servo_axis_t;

typedef struct {
    gpio_num_t gpio;
    ledc_channel_t channel;
    float min_deg;
    float center_deg;
    float max_deg;
    bool reversed;
} servo_axis_config_t;

typedef struct {
    SemaphoreHandle_t lock;
    bool raw_run;
    float raw_speed;
    float raw_amplitude;
    float raw_phase;
    flower_state_t state;
    float target_tilt_deg;
    float target_rotation_deg;
} flower_servo_ctx_t;

static const servo_axis_config_t s_axis_config[SERVO_AXIS_COUNT] = {
    [SERVO_AXIS_TILT] = {
        .gpio = BOARD_SERVO_TILT_GPIO,
        .channel = LEDC_CHANNEL_0,
        .min_deg = BOARD_SERVO_TILT_MIN_DEG,
        .center_deg = BOARD_SERVO_TILT_CENTER_DEG,
        .max_deg = BOARD_SERVO_TILT_MAX_DEG,
        .reversed = BOARD_SERVO_TILT_REVERSED != 0,
    },
    [SERVO_AXIS_ROTATION] = {
        .gpio = BOARD_SERVO_ROTATION_GPIO,
        .channel = LEDC_CHANNEL_1,
        .min_deg = BOARD_SERVO_ROTATION_MIN_DEG,
        .center_deg = BOARD_SERVO_ROTATION_CENTER_DEG,
        .max_deg = BOARD_SERVO_ROTATION_MAX_DEG,
        .reversed = BOARD_SERVO_ROTATION_REVERSED != 0,
    },
};

static flower_servo_ctx_t s_servo;

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float min_float(float a, float b)
{
    return (a < b) ? a : b;
}

static uint32_t angle_to_duty(const servo_axis_config_t *axis, float angle_deg)
{
    angle_deg = clamp_float(angle_deg, axis->min_deg, axis->max_deg);
    if (axis->reversed) {
        angle_deg = axis->max_deg - (angle_deg - axis->min_deg);
    }

    const float span_deg = axis->max_deg - axis->min_deg;
    const float normalized = (span_deg > 0.0f) ? ((angle_deg - axis->min_deg) / span_deg) : 0.5f;
    const float pulse_us = BOARD_SERVO_MIN_PULSE_US +
                           normalized * (BOARD_SERVO_MAX_PULSE_US - BOARD_SERVO_MIN_PULSE_US);
    const uint32_t max_duty = (1U << BOARD_SERVO_PWM_RES_BITS) - 1U;

    return (uint32_t)((pulse_us * max_duty) / BOARD_SERVO_PERIOD_US);
}

static esp_err_t write_axis_angle(servo_axis_t axis_id, float angle_deg)
{
    const servo_axis_config_t *axis = &s_axis_config[axis_id];
    const uint32_t duty = angle_to_duty(axis, angle_deg);

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, axis->channel, duty), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, axis->channel), TAG, "update duty failed");
    return ESP_OK;
}

static void state_to_pose(flower_state_t state, float *tilt_deg, float *rotation_deg)
{
    switch (state) {
    case FLOWER_STATE_HIGH_POSITIVE:
        *tilt_deg = 60.0f;
        *rotation_deg = 120.0f;
        break;
    case FLOWER_STATE_LOW_POSITIVE:
        *tilt_deg = 105.0f;
        *rotation_deg = 112.0f;
        break;
    case FLOWER_STATE_HIGH_NEGATIVE:
        *tilt_deg = 75.0f;
        *rotation_deg = 60.0f;
        break;
    case FLOWER_STATE_LOW_NEGATIVE:
        *tilt_deg = 130.0f;
        *rotation_deg = 68.0f;
        break;
    case FLOWER_STATE_NEUTRAL:
    default:
        *tilt_deg = BOARD_SERVO_TILT_CENTER_DEG;
        *rotation_deg = BOARD_SERVO_ROTATION_CENTER_DEG;
        break;
    }
}

static esp_err_t apply_state_pose(flower_state_t state)
{
    float tilt_deg = BOARD_SERVO_TILT_CENTER_DEG;
    float rotation_deg = BOARD_SERVO_ROTATION_CENTER_DEG;
    state_to_pose(state, &tilt_deg, &rotation_deg);

    ESP_RETURN_ON_ERROR(write_axis_angle(SERVO_AXIS_TILT, tilt_deg), TAG, "tilt write failed");
    ESP_RETURN_ON_ERROR(write_axis_angle(SERVO_AXIS_ROTATION, rotation_deg), TAG, "rotation write failed");
    return ESP_OK;
}

static float normalized_to_tilt_deg(float tilt)
{
    tilt = clamp_float(tilt, 0.0f, 1.0f);
    return BOARD_SERVO_TILT_MIN_DEG +
           tilt * (BOARD_SERVO_TILT_MAX_DEG - BOARD_SERVO_TILT_MIN_DEG);
}

static void apply_raw_rotation(float phase, float amplitude, float center_deg)
{
    const float triangle = (phase < 0.5f) ? ((phase * 4.0f) - 1.0f) : (3.0f - (phase * 4.0f));
    center_deg = clamp_float(center_deg, BOARD_SERVO_ROTATION_MIN_DEG, BOARD_SERVO_ROTATION_MAX_DEG);
    const float rotation_room = min_float(center_deg - BOARD_SERVO_ROTATION_MIN_DEG,
                                          BOARD_SERVO_ROTATION_MAX_DEG - center_deg);

    const float rotation_deg = center_deg + triangle * rotation_room * amplitude;

    ESP_ERROR_CHECK_WITHOUT_ABORT(write_axis_angle(SERVO_AXIS_ROTATION, rotation_deg));
}

static void flower_servo_task(void *arg)
{
    (void)arg;

    while (true) {
        bool raw_run = false;
        float raw_speed = 0.0f;
        float raw_amplitude = 0.0f;
        float phase = 0.0f;
        float rotation_center_deg = BOARD_SERVO_ROTATION_CENTER_DEG;

        xSemaphoreTake(s_servo.lock, portMAX_DELAY);
        raw_run = s_servo.raw_run;
        if (raw_run) {
            raw_speed = s_servo.raw_speed;
            raw_amplitude = s_servo.raw_amplitude;
            rotation_center_deg = s_servo.target_rotation_deg;
            const float hz = BOARD_SERVO_RAW_MIN_HZ +
                             raw_speed * (BOARD_SERVO_RAW_MAX_HZ - BOARD_SERVO_RAW_MIN_HZ);
            s_servo.raw_phase += hz * ((float)BOARD_SERVO_UPDATE_PERIOD_MS / 1000.0f);
            while (s_servo.raw_phase >= 1.0f) {
                s_servo.raw_phase -= 1.0f;
            }
            phase = s_servo.raw_phase;
        }
        xSemaphoreGive(s_servo.lock);

        if (raw_run) {
            apply_raw_rotation(phase, raw_amplitude, rotation_center_deg);
        }

        vTaskDelay(pdMS_TO_TICKS(BOARD_SERVO_UPDATE_PERIOD_MS));
    }
}

esp_err_t flower_servo_init(void)
{
    ESP_LOGI(TAG, "Initialize Flower MG995 servo controller");

    s_servo.lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_servo.lock, ESP_ERR_NO_MEM, TAG, "servo lock allocation failed");
    s_servo.raw_run = false;
    s_servo.raw_speed = 0.0f;
    s_servo.raw_amplitude = 0.0f;
    s_servo.raw_phase = 0.0f;
    s_servo.state = FLOWER_STATE_NEUTRAL;
    s_servo.target_tilt_deg = BOARD_SERVO_TILT_CENTER_DEG;
    s_servo.target_rotation_deg = BOARD_SERVO_ROTATION_CENTER_DEG;

    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BOARD_SERVO_PWM_RES_BITS,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BOARD_SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "LEDC timer config failed");

    for (int i = 0; i < SERVO_AXIS_COUNT; i++) {
        const ledc_channel_config_t channel_config = {
            .gpio_num = s_axis_config[i].gpio,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = s_axis_config[i].channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "LEDC channel config failed");
    }

    ESP_RETURN_ON_ERROR(apply_state_pose(FLOWER_STATE_NEUTRAL), TAG, "neutral pose failed");

    BaseType_t task_ok = xTaskCreate(flower_servo_task, "flower_servo", 4096, NULL, 5, NULL);
    ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create servo task");

    return ESP_OK;
}

esp_err_t flower_servo_set_state(flower_state_t state)
{
    ESP_RETURN_ON_FALSE(state >= FLOWER_STATE_NEUTRAL && state <= FLOWER_STATE_LOW_POSITIVE,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "invalid flower state");

    bool raw_run = false;
    float tilt_deg = BOARD_SERVO_TILT_CENTER_DEG;
    float rotation_deg = BOARD_SERVO_ROTATION_CENTER_DEG;
    state_to_pose(state, &tilt_deg, &rotation_deg);

    xSemaphoreTake(s_servo.lock, portMAX_DELAY);
    s_servo.state = state;
    s_servo.target_tilt_deg = tilt_deg;
    s_servo.target_rotation_deg = rotation_deg;
    raw_run = s_servo.raw_run;
    xSemaphoreGive(s_servo.lock);

    ESP_RETURN_ON_ERROR(write_axis_angle(SERVO_AXIS_TILT, tilt_deg), TAG, "apply state tilt failed");
    if (!raw_run) {
        ESP_RETURN_ON_ERROR(write_axis_angle(SERVO_AXIS_ROTATION, rotation_deg), TAG, "apply state rotation failed");
    }

    return ESP_OK;
}

esp_err_t flower_servo_set_raw(bool run, float speed, float amplitude)
{
    speed = clamp_float(speed, 0.0f, 1.0f);
    amplitude = clamp_float(amplitude, 0.0f, 1.0f);

    float rotation_deg = BOARD_SERVO_ROTATION_CENTER_DEG;

    xSemaphoreTake(s_servo.lock, portMAX_DELAY);
    s_servo.raw_run = run;
    s_servo.raw_speed = speed;
    s_servo.raw_amplitude = amplitude;
    if (!run) {
        s_servo.raw_phase = 0.0f;
        rotation_deg = s_servo.target_rotation_deg;
    }
    xSemaphoreGive(s_servo.lock);

    if (!run) {
        ESP_RETURN_ON_ERROR(write_axis_angle(SERVO_AXIS_ROTATION, rotation_deg), TAG, "return to target rotation failed");
    }

    ESP_LOGI(TAG, "Raw rotation %s speed=%.2f amplitude=%.2f", run ? "on" : "off", speed, amplitude);
    return ESP_OK;
}

esp_err_t flower_servo_set_tilt(float tilt)
{
    const float clamped_tilt = clamp_float(tilt, 0.0f, 1.0f);
    const float tilt_deg = normalized_to_tilt_deg(clamped_tilt);

    xSemaphoreTake(s_servo.lock, portMAX_DELAY);
    s_servo.target_tilt_deg = tilt_deg;
    xSemaphoreGive(s_servo.lock);

    ESP_LOGI(TAG, "Tilt command %.2f -> %.1f deg", clamped_tilt, tilt_deg);
    return write_axis_angle(SERVO_AXIS_TILT, tilt_deg);
}

esp_err_t flower_servo_stop(void)
{
    xSemaphoreTake(s_servo.lock, portMAX_DELAY);
    s_servo.raw_run = false;
    s_servo.raw_speed = 0.0f;
    s_servo.raw_amplitude = 0.0f;
    s_servo.raw_phase = 0.0f;
    s_servo.state = FLOWER_STATE_NEUTRAL;
    s_servo.target_tilt_deg = BOARD_SERVO_TILT_CENTER_DEG;
    s_servo.target_rotation_deg = BOARD_SERVO_ROTATION_CENTER_DEG;
    xSemaphoreGive(s_servo.lock);

    ESP_LOGI(TAG, "Servo stop -> neutral");
    return apply_state_pose(FLOWER_STATE_NEUTRAL);
}
