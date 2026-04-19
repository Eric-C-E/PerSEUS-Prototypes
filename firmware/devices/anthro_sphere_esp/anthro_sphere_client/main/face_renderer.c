#include "face_renderer.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "face_renderer";

typedef struct {
    lv_obj_t *image;
    lv_timer_t *timer;
    const face_animation_t *animation;
    size_t frame_index;
    bool finished;
    face_renderer_animation_done_cb_t done_cb;
    void *done_user_ctx;
} face_renderer_ctx_t;

static face_renderer_ctx_t s_renderer;

static void face_renderer_show_frame(size_t frame_index)
{
    const face_animation_t *animation = s_renderer.animation;

    if (!animation || frame_index >= animation->frame_count) {
        return;
    }

    lv_image_set_src(s_renderer.image, animation->frames[frame_index]);
    lv_obj_center(s_renderer.image);
}

static esp_err_t face_renderer_play_animation_locked(const face_animation_t *animation)
{
    ESP_RETURN_ON_FALSE(animation, ESP_ERR_INVALID_ARG, TAG, "animation is required");
    ESP_RETURN_ON_FALSE(animation->frames, ESP_ERR_INVALID_ARG, TAG, "animation frames are required");
    ESP_RETURN_ON_FALSE(animation->frame_count > 0, ESP_ERR_INVALID_ARG, TAG, "animation has no frames");
    ESP_RETURN_ON_FALSE(animation->frame_duration_ms > 0, ESP_ERR_INVALID_ARG, TAG, "animation duration is invalid");
    ESP_RETURN_ON_FALSE(s_renderer.image && s_renderer.timer, ESP_ERR_INVALID_STATE, TAG, "renderer is not initialized");

    s_renderer.animation = animation;
    s_renderer.frame_index = 0;
    s_renderer.finished = false;

    face_renderer_show_frame(0);
    lv_timer_set_period(s_renderer.timer, animation->frame_duration_ms);

    lv_timer_resume(s_renderer.timer);

    return ESP_OK;
}

static void face_renderer_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    const face_animation_t *animation = s_renderer.animation;
    if (!animation || animation->frame_count == 0 || s_renderer.finished) {
        return;
    }

    size_t next_frame = s_renderer.frame_index + 1;
    if (next_frame >= animation->frame_count) {
        if (animation->loop) {
            next_frame = 0;
        } else {
            s_renderer.finished = true;
            lv_timer_pause(s_renderer.timer);
            ESP_LOGI(TAG, "Animation finished: %s", animation->name);
            if (s_renderer.done_cb) {
                s_renderer.done_cb(animation, s_renderer.done_user_ctx);
            }
            return;
        }
    }

    s_renderer.frame_index = next_frame;
    face_renderer_show_frame(s_renderer.frame_index);
}

esp_err_t face_renderer_init(lv_obj_t *parent)
{
    ESP_RETURN_ON_FALSE(parent, ESP_ERR_INVALID_ARG, TAG, "parent is required");

    ESP_LOGI(TAG, "Create face image widget");
    ESP_RETURN_ON_FALSE(lvgl_port_lock(0), ESP_ERR_TIMEOUT, TAG, "LVGL lock failed");

    s_renderer.image = lv_image_create(parent);
    s_renderer.timer = lv_timer_create(face_renderer_timer_cb, 125, NULL);

    if (s_renderer.image) {
        lv_obj_center(s_renderer.image);
    }
    if (s_renderer.timer) {
        lv_timer_pause(s_renderer.timer);
    }

    lvgl_port_unlock();

    ESP_RETURN_ON_FALSE(s_renderer.image && s_renderer.timer, ESP_FAIL, TAG, "face renderer create failed");
    return ESP_OK;
}

esp_err_t face_renderer_play_animation(const face_animation_t *animation)
{
    ESP_RETURN_ON_FALSE(animation, ESP_ERR_INVALID_ARG, TAG, "animation is required");

    ESP_LOGI(TAG, "Play animation: %s (%u frames, %u ms/frame, loop=%d)",
             animation->name,
             (unsigned)animation->frame_count,
             (unsigned)animation->frame_duration_ms,
             animation->loop);

    ESP_RETURN_ON_FALSE(lvgl_port_lock(0), ESP_ERR_TIMEOUT, TAG, "LVGL lock failed");
    esp_err_t ret = face_renderer_play_animation_locked(animation);
    lvgl_port_unlock();

    return ret;
}

esp_err_t face_renderer_play_animation_from_lvgl(const face_animation_t *animation)
{
    ESP_RETURN_ON_FALSE(animation, ESP_ERR_INVALID_ARG, TAG, "animation is required");

    ESP_LOGI(TAG, "Play animation from LVGL context: %s", animation->name);
    return face_renderer_play_animation_locked(animation);
}

void face_renderer_set_animation_done_callback(face_renderer_animation_done_cb_t callback, void *user_ctx)
{
    s_renderer.done_cb = callback;
    s_renderer.done_user_ctx = user_ctx;
}

bool face_renderer_is_animation_finished(void)
{
    return s_renderer.finished;
}

lv_obj_t *face_renderer_get_image_obj(void)
{
    return s_renderer.image;
}
