#include "lighting_controller.h"

#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "lighting_controller";

static lighting_state_t s_state = LIGHTING_STATE_NEUTRAL;

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

static const char *state_to_future_pattern(lighting_state_t state)
{
    switch (state) {
    case LIGHTING_STATE_NEUTRAL:
        return "soft white steady glow";
    case LIGHTING_STATE_LOW_NEGATIVE:
        return "dim blue slow pulse";
    case LIGHTING_STATE_LOW_POSITIVE:
        return "dim green breathing";
    case LIGHTING_STATE_HIGH_NEGATIVE:
        return "red fast pulse";
    case LIGHTING_STATE_HIGH_POSITIVE:
        return "bright amber sparkle";
    default:
        return "off";
    }
}

esp_err_t lighting_controller_init(void)
{
    ESP_LOGI(TAG, "Lighting placeholder initialized; LED ring pin=%d means not connected yet", BOARD_LED_RING_PIN);
    return lighting_controller_set_state(LIGHTING_STATE_NEUTRAL);
}

esp_err_t lighting_controller_set_state(lighting_state_t state)
{
    ESP_RETURN_ON_FALSE(state < LIGHTING_STATE_COUNT, ESP_ERR_INVALID_ARG, TAG, "invalid lighting state");

    s_state = state;
    ESP_LOGI(TAG,
             "Lighting state -> %s; would command future LED ring pattern: %s",
             lighting_controller_state_name(s_state),
             state_to_future_pattern(s_state));
    return ESP_OK;
}