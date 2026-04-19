#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    const char *name;
    const lv_image_dsc_t * const *frames;
    size_t frame_count;
    uint32_t frame_duration_ms;
    bool loop;
} face_animation_t;

typedef void (*face_renderer_animation_done_cb_t)(const face_animation_t *animation, void *user_ctx);

esp_err_t face_renderer_init(lv_obj_t *parent);
esp_err_t face_renderer_play_animation(const face_animation_t *animation);
esp_err_t face_renderer_play_animation_from_lvgl(const face_animation_t *animation);
void face_renderer_set_animation_done_callback(face_renderer_animation_done_cb_t callback, void *user_ctx);
bool face_renderer_is_animation_finished(void);
lv_obj_t *face_renderer_get_image_obj(void);
