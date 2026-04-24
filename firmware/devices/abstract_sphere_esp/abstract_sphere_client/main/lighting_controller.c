#include "lighting_controller.h"

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

static const char *TAG = "lighting_controller";

#define LED_RMT_RESOLUTION_HZ           (10 * 1000 * 1000)
#define LED_TRANSITION_MS               3000
#define LED_FRAME_MS                    33
#define LED_TASK_STACK_BYTES            3072
#define LED_TASK_PRIORITY               4

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_rgb_t;

static led_strip_handle_t s_strip;
static SemaphoreHandle_t s_lock;
static lighting_state_t s_state = LIGHTING_STATE_NEUTRAL;
static led_rgb_t s_current_color;
static led_rgb_t s_target_color;
static bool s_target_changed;

const char *lighting_controller_state_name(lighting_state_t state)
{
    switch (state) {
    case LIGHTING_STATE_NEUTRAL:
        return "neutral";
    case LIGHTING_STATE_LOW_NEGATIVE:
        return "low_negative";
    case LIGHTING_STATE_LOW_POSITIVE:
        return "low_positive";
    case LIGHTING_STATE_HIGH_NEGATIVE:
        return "high_negative";
    case LIGHTING_STATE_HIGH_POSITIVE:
        return "high_positive";
    default:
        return "unknown";
    }
}

static led_rgb_t state_to_color(lighting_state_t state)
{
    switch (state) {
    case LIGHTING_STATE_NEUTRAL:
        return (led_rgb_t){ .red = 86, .green = 72, .blue = 50 };
    case LIGHTING_STATE_LOW_NEGATIVE:
        return (led_rgb_t){ .red = 52, .green = 42, .blue = 130 };
    case LIGHTING_STATE_LOW_POSITIVE:
        return (led_rgb_t){ .red = 42, .green = 128, .blue = 150 };
    case LIGHTING_STATE_HIGH_NEGATIVE:
        return (led_rgb_t){ .red = 150, .green = 42, .blue = 0 };
    case LIGHTING_STATE_HIGH_POSITIVE:
        return (led_rgb_t){ .red = 0, .green = 125, .blue = 42 };
    default:
        return (led_rgb_t){ .red = 0, .green = 0, .blue = 0 };
    }
}

static uint8_t interpolate_channel(uint8_t start, uint8_t end, float amount)
{
    return (uint8_t)((float)start + ((float)end - (float)start) * amount);
}

static float smootherstep(float amount)
{
    if (amount <= 0.0f) {
        return 0.0f;
    }
    if (amount >= 1.0f) {
        return 1.0f;
    }
    return amount * amount * amount * (amount * (amount * 6.0f - 15.0f) + 10.0f);
}

static led_rgb_t blend_color(led_rgb_t start, led_rgb_t end, float amount)
{
    const float eased = smootherstep(amount);

    return (led_rgb_t) {
        .red = interpolate_channel(start.red, end.red, eased),
        .green = interpolate_channel(start.green, end.green, eased),
        .blue = interpolate_channel(start.blue, end.blue, eased),
    };
}

static esp_err_t show_color(led_rgb_t color)
{
    const uint8_t scaled_red = (uint8_t)(((uint32_t)color.red * BOARD_LED_RING_BRIGHTNESS_PERCENT) / 100);
    const uint8_t scaled_green = (uint8_t)(((uint32_t)color.green * BOARD_LED_RING_BRIGHTNESS_PERCENT) / 100);
    const uint8_t scaled_blue = (uint8_t)(((uint32_t)color.blue * BOARD_LED_RING_BRIGHTNESS_PERCENT) / 100);

    for (uint32_t i = 0; i < BOARD_LED_RING_COUNT; ++i) {
        ESP_RETURN_ON_ERROR(led_strip_set_pixel(s_strip, i, scaled_red, scaled_green, scaled_blue),
                            TAG,
                            "set pixel failed");
    }
    ESP_RETURN_ON_ERROR(led_strip_refresh(s_strip), TAG, "refresh failed");
    return ESP_OK;
}

static void lighting_task(void *arg)
{
    (void)arg;

    led_rgb_t start_color = s_current_color;
    led_rgb_t end_color = s_target_color;
    TickType_t transition_start = xTaskGetTickCount();

    while (true) {
        if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
            if (s_target_changed) {
                start_color = s_current_color;
                end_color = s_target_color;
                transition_start = xTaskGetTickCount();
                s_target_changed = false;
            }
            xSemaphoreGive(s_lock);
        }

        const TickType_t now = xTaskGetTickCount();
        const uint32_t elapsed_ms = pdTICKS_TO_MS(now - transition_start);
        const float amount = (float)elapsed_ms / (float)LED_TRANSITION_MS;
        const led_rgb_t next_color = blend_color(start_color, end_color, amount);

        ESP_ERROR_CHECK_WITHOUT_ABORT(show_color(next_color));

        if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
            s_current_color = next_color;
            xSemaphoreGive(s_lock);
        }

        vTaskDelay(pdMS_TO_TICKS(LED_FRAME_MS));
    }
}

esp_err_t lighting_controller_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = BOARD_LED_RING_PIN,
        .max_leds = BOARD_LED_RING_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        },
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip),
                        TAG,
                        "LED strip init failed");

    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock != NULL, ESP_ERR_NO_MEM, TAG, "LED state mutex allocation failed");

    s_current_color = state_to_color(LIGHTING_STATE_NEUTRAL);
    s_target_color = s_current_color;
    ESP_RETURN_ON_ERROR(show_color(s_current_color), TAG, "initial LED refresh failed");

    BaseType_t task_created = xTaskCreate(lighting_task,
                                          "lighting_task",
                                          LED_TASK_STACK_BYTES,
                                          NULL,
                                          LED_TASK_PRIORITY,
                                          NULL);
    ESP_RETURN_ON_FALSE(task_created == pdPASS, ESP_ERR_NO_MEM, TAG, "LED task creation failed");

    ESP_LOGI(TAG,
             "WS2812B LED ring initialized on GPIO%d with %d LEDs at %d%% brightness",
             BOARD_LED_RING_PIN,
             BOARD_LED_RING_COUNT,
             BOARD_LED_RING_BRIGHTNESS_PERCENT);
    return ESP_OK;
}

esp_err_t lighting_controller_set_state(lighting_state_t state)
{
    ESP_RETURN_ON_FALSE(state < LIGHTING_STATE_COUNT, ESP_ERR_INVALID_ARG, TAG, "invalid lighting state");

    ESP_RETURN_ON_FALSE(s_lock != NULL, ESP_ERR_INVALID_STATE, TAG, "lighting controller not initialized");

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    s_state = state;
    const led_rgb_t target_color = state_to_color(state);
    s_target_color = target_color;
    s_target_changed = true;
    const lighting_state_t logged_state = s_state;
    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG,
             "Lighting state -> %s; transitioning ring to RGB(%u,%u,%u)",
             lighting_controller_state_name(logged_state),
             target_color.red,
             target_color.green,
             target_color.blue);
    return ESP_OK;
}
