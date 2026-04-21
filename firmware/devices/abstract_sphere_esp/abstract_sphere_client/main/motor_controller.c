#include "motor_controller.h"

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "motor_controller";

#define MOTOR_LEDC_MODE                 LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_TIMER                LEDC_TIMER_0
#define MOTOR_LEDC_CHANNEL              LEDC_CHANNEL_0
#define MOTOR_LEDC_DUTY_RES             LEDC_TIMER_10_BIT
#define MOTOR_LEDC_FREQ_HZ              200
#define MOTOR_LEDC_MAX_RAW_DUTY         1023

/* Conservative safety envelope for an unknown vibration motor. */
#define MOTOR_SAFE_DEFAULT_LEVEL        0.20f
#define MOTOR_SAFE_MAX_LEVEL            0.30f
#define MOTOR_MAX_CONTINUOUS_RUN_MS     2000
#define MOTOR_SAFETY_POLL_MS            100

static float s_level = MOTOR_SAFE_DEFAULT_LEVEL;
static bool s_enabled;
static TickType_t s_started_tick;

static float clamp_level(float level)
{
    if (level < 0.0f) {
        return 0.0f;
    }
    if (level > 1.0f) {
        return 1.0f;
    }
    return level;
}

static uint32_t level_to_duty(float level)
{
    const float safe_level = clamp_level(level) * MOTOR_SAFE_MAX_LEVEL;
    return (uint32_t)((float)MOTOR_LEDC_MAX_RAW_DUTY * safe_level);
}

static esp_err_t apply_output(void)
{
    const uint32_t duty = s_enabled ? level_to_duty(s_level) : 0;

    ESP_RETURN_ON_ERROR(ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL, duty), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_CHANNEL), TAG, "update duty failed");

    ESP_LOGI(TAG,
             "Vibration %s, requested level=%d%%, capped duty=%lu/%d",
             s_enabled ? "enabled" : "disabled",
             (int)(s_level * 100.0f),
             (unsigned long)duty,
             MOTOR_LEDC_MAX_RAW_DUTY);
    return ESP_OK;
}

static void motor_safety_task(void *arg)
{
    (void)arg;

    while (true) {
        if (s_enabled) {
            const TickType_t now = xTaskGetTickCount();
            if ((now - s_started_tick) >= pdMS_TO_TICKS(MOTOR_MAX_CONTINUOUS_RUN_MS)) {
                ESP_LOGW(TAG,
                         "Stopping vibration after %d ms safety limit; send set_vibration true again to restart",
                         MOTOR_MAX_CONTINUOUS_RUN_MS);
                ESP_ERROR_CHECK_WITHOUT_ABORT(motor_controller_stop());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(MOTOR_SAFETY_POLL_MS));
    }
}

esp_err_t motor_controller_init(void)
{
    ESP_LOGI(TAG,
             "Initialize DRV8833 vibration PWM on GPIO%d, default level %d%%, absolute max duty %d%%",
             BOARD_VIBE_MOTOR_PWM_PIN,
             (int)(MOTOR_SAFE_DEFAULT_LEVEL * 100.0f),
             (int)(MOTOR_SAFE_MAX_LEVEL * 100.0f));

    ledc_timer_config_t timer_config = {
        .speed_mode = MOTOR_LEDC_MODE,
        .duty_resolution = MOTOR_LEDC_DUTY_RES,
        .timer_num = MOTOR_LEDC_TIMER,
        .freq_hz = MOTOR_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "LEDC timer config failed");

    ledc_channel_config_t channel_config = {
        .gpio_num = BOARD_VIBE_MOTOR_PWM_PIN,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel = MOTOR_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = MOTOR_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "LEDC channel config failed");

    BaseType_t task_ok = xTaskCreate(motor_safety_task, "motor_safety", 2048, NULL, 4, NULL);
    ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create motor safety task");

    return motor_controller_stop();
}

esp_err_t motor_controller_set_enabled(bool enabled)
{
    if (enabled && !s_enabled) {
        s_started_tick = xTaskGetTickCount();
    }
    s_enabled = enabled;
    return apply_output();
}

esp_err_t motor_controller_set_level(float level)
{
    s_level = clamp_level(level);
    ESP_LOGI(TAG, "Vibration level set to %d%% before safety cap", (int)(s_level * 100.0f));
    return apply_output();
}

esp_err_t motor_controller_stop(void)
{
    s_enabled = false;
    return apply_output();
}