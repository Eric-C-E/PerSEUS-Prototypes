#include "face_fsm.h"

#include "esp_check.h"
#include "esp_log.h"
#include "face_renderer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "face_fsm";

typedef enum {
    FACE_FSM_MODE_IDLE = 0,
    FACE_FSM_MODE_BLINK,
    FACE_FSM_MODE_TO_NEUTRAL,
    FACE_FSM_MODE_NEUTRAL_HOLD,
    FACE_FSM_MODE_FROM_NEUTRAL,
} face_fsm_mode_t;

typedef struct {
    face_state_t state;
    face_state_t target_state;
    face_fsm_mode_t mode;
    TimerHandle_t blink_timer;
#if FACE_FSM_ENABLE_DEMO_CYCLE
    TimerHandle_t demo_state_timer;
    size_t demo_state_index;
#endif
} face_fsm_ctx_t;

static face_fsm_ctx_t s_fsm;

#if FACE_FSM_ENABLE_DEMO_CYCLE
static const face_state_t s_demo_state_cycle[] = {
    FACE_STATE_LOW_NEG,
    FACE_STATE_LOW_POS,
    FACE_STATE_HIGH_NEG,
    FACE_STATE_HIGH_POS,
    FACE_STATE_NEUTRAL,
};
#endif

static void enter_idle_loop_from_lvgl(void)
{
    s_fsm.mode = FACE_FSM_MODE_IDLE;
    ESP_ERROR_CHECK(face_renderer_play_animation_from_lvgl(face_assets_idle_for_state(s_fsm.state)));
}

static void play_from_neutral_from_lvgl(void)
{
    if (s_fsm.target_state == FACE_STATE_NEUTRAL) {
        s_fsm.state = FACE_STATE_NEUTRAL;
        enter_idle_loop_from_lvgl();
        return;
    }

    s_fsm.mode = FACE_FSM_MODE_FROM_NEUTRAL;
    ESP_LOGI(TAG, "Transition neutral -> %s", face_assets_state_name(s_fsm.target_state));
    ESP_ERROR_CHECK(face_renderer_play_animation_from_lvgl(face_assets_neutral_to_state(s_fsm.target_state)));
}

static void animation_done_cb(const face_animation_t *animation, void *user_ctx)
{
    (void)animation;
    (void)user_ctx;

    switch (s_fsm.mode) {
    case FACE_FSM_MODE_BLINK:
        ESP_LOGI(TAG, "Blink complete; returning to %s idle", face_assets_state_name(s_fsm.state));
        enter_idle_loop_from_lvgl();
        break;
    case FACE_FSM_MODE_TO_NEUTRAL:
        s_fsm.state = FACE_STATE_NEUTRAL;
        s_fsm.mode = FACE_FSM_MODE_NEUTRAL_HOLD;
        ESP_LOGI(TAG, "Reached neutral; holding neutral frame");
        ESP_ERROR_CHECK(face_renderer_play_animation_from_lvgl(face_assets_neutral_hold()));
        break;
    case FACE_FSM_MODE_NEUTRAL_HOLD:
        play_from_neutral_from_lvgl();
        break;
    case FACE_FSM_MODE_FROM_NEUTRAL:
        s_fsm.state = s_fsm.target_state;
        ESP_LOGI(TAG, "Entered %s", face_assets_state_name(s_fsm.state));
        enter_idle_loop_from_lvgl();
        break;
    case FACE_FSM_MODE_IDLE:
    default:
        break;
    }
}

static void blink_timer_cb(TimerHandle_t timer)
{
    (void)timer;

    if (s_fsm.mode != FACE_FSM_MODE_IDLE) {
        return;
    }

    s_fsm.mode = FACE_FSM_MODE_BLINK;
    ESP_LOGI(TAG, "Blink in %s", face_assets_state_name(s_fsm.state));
    ESP_ERROR_CHECK(face_renderer_play_animation(face_assets_blink_for_state(s_fsm.state)));
}

#if FACE_FSM_ENABLE_DEMO_CYCLE
static void demo_state_timer_cb(TimerHandle_t timer)
{
    (void)timer;

    const face_state_t target = s_demo_state_cycle[s_fsm.demo_state_index];
    s_fsm.demo_state_index = (s_fsm.demo_state_index + 1) %
                             (sizeof(s_demo_state_cycle) / sizeof(s_demo_state_cycle[0]));

    ESP_ERROR_CHECK(face_fsm_request_state(target));
}
#endif

esp_err_t face_fsm_request_state(face_state_t target_state)
{
    ESP_RETURN_ON_FALSE(target_state < FACE_STATE_COUNT, ESP_ERR_INVALID_ARG, TAG, "invalid target state");

    ESP_LOGI(TAG, "Request state: %s -> %s",
             face_assets_state_name(s_fsm.state),
             face_assets_state_name(target_state));

    s_fsm.target_state = target_state;

    if (s_fsm.state == target_state && s_fsm.mode == FACE_FSM_MODE_IDLE) {
        return ESP_OK;
    }

    if (s_fsm.state == FACE_STATE_NEUTRAL) {
        s_fsm.mode = FACE_FSM_MODE_NEUTRAL_HOLD;
        ESP_RETURN_ON_ERROR(face_renderer_play_animation(face_assets_neutral_hold()),
                            TAG,
                            "failed to start neutral hold");
        return ESP_OK;
    }

    s_fsm.mode = FACE_FSM_MODE_TO_NEUTRAL;
    ESP_LOGI(TAG, "Transition %s -> neutral", face_assets_state_name(s_fsm.state));
    ESP_RETURN_ON_ERROR(face_renderer_play_animation(face_assets_state_to_neutral(s_fsm.state)),
                        TAG,
                        "failed to start transition to neutral");
    return ESP_OK;
}

esp_err_t face_fsm_init(void)
{
    ESP_LOGI(TAG, "Initialize face FSM");

    s_fsm.state = FACE_STATE_NEUTRAL;
    s_fsm.target_state = FACE_STATE_NEUTRAL;
    s_fsm.mode = FACE_FSM_MODE_IDLE;
#if FACE_FSM_ENABLE_DEMO_CYCLE
    s_fsm.demo_state_index = 0;
#endif

    face_renderer_set_animation_done_callback(animation_done_cb, NULL);
    ESP_RETURN_ON_ERROR(face_renderer_play_animation(face_assets_idle_for_state(s_fsm.state)),
                        TAG,
                        "failed to start initial face animation");

    s_fsm.blink_timer = xTimerCreate("face_blink",
                                     pdMS_TO_TICKS(FACE_BLINK_PERIOD_MS),
                                     pdTRUE,
                                     NULL,
                                     blink_timer_cb);
    ESP_RETURN_ON_FALSE(s_fsm.blink_timer, ESP_ERR_NO_MEM, TAG, "failed to create blink timer");
    ESP_RETURN_ON_FALSE(xTimerStart(s_fsm.blink_timer, 0) == pdPASS,
                        ESP_FAIL,
                        TAG,
                        "failed to start blink timer");

#if FACE_FSM_ENABLE_DEMO_CYCLE
    s_fsm.demo_state_timer = xTimerCreate("face_demo_state",
                                          pdMS_TO_TICKS(FACE_DEMO_STATE_PERIOD_MS),
                                          pdTRUE,
                                          NULL,
                                          demo_state_timer_cb);
    ESP_RETURN_ON_FALSE(s_fsm.demo_state_timer, ESP_ERR_NO_MEM, TAG, "failed to create demo timer");
    ESP_RETURN_ON_FALSE(xTimerStart(s_fsm.demo_state_timer, 0) == pdPASS,
                        ESP_FAIL,
                        TAG,
                        "failed to start demo timer");
#else
    ESP_LOGI(TAG, "Demo state cycle disabled; waiting for GUI set_state commands");
#endif

    return ESP_OK;
}

face_state_t face_fsm_get_state(void)
{
    return s_fsm.state;
}
