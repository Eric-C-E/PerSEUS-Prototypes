#include "motion_reset.h"

#include <stdbool.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "motion_reset";

typedef struct {
    motion_reset_callback_t callback;
    void *user_ctx;
    volatile bool shake_pending;
    volatile bool touch_pending;
} motion_reset_ctx_t;

static motion_reset_ctx_t s_motion_reset;

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

        /*
         * Future MPU6050 integration point:
         *   - read accelerometer samples here or from an MPU6050 driver task
         *   - run binary shake/touch detection
         *   - call motion_reset_raise_shake_flag() when shake == true
         */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t motion_reset_init(motion_reset_callback_t callback, void *user_ctx)
{
    ESP_LOGI(TAG, "Initialize motion reset skeleton");
    s_motion_reset.callback = callback;
    s_motion_reset.user_ctx = user_ctx;
    s_motion_reset.shake_pending = false;
    s_motion_reset.touch_pending = false;

    BaseType_t task_ok = xTaskCreate(motion_reset_task, "motion_reset", 3072, NULL, 4, NULL);
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
