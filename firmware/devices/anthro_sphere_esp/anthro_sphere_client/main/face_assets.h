#pragma once

#include "face_renderer.h"
#include "lvgl.h"

/*
 * Asset registry for full-screen face frames.
 *
 * Source art:
 *   assets/faces/src/ with PNG files
 *
 * Generated LVGL C assets:
 *   main/assets/faces/generated/ with C files
 *
 * Formats:
 *   loading_screen: LV_COLOR_FORMAT_RGB565, 320x240
 *   face frames:    LV_COLOR_FORMAT_I1, 320x240, 1 bit per pixel
 *
 * Monochrome convention:
 *   Pixel value 0 maps to white.
 *   Pixel value 1 maps to black.
 */

#define FACE_FRAME_WIDTH             320
#define FACE_FRAME_HEIGHT            240
#define FACE_DEFAULT_FRAME_TIME_MS   125
#define FACE_BLINK_PERIOD_MS         3000
#define FACE_DEMO_STATE_PERIOD_MS    10000

typedef enum {
    FACE_STATE_NEUTRAL = 0,
    FACE_STATE_LOW_NEG,
    FACE_STATE_LOW_POS,
    FACE_STATE_HIGH_NEG,
    FACE_STATE_HIGH_POS,
    FACE_STATE_COUNT,
} face_state_t;

LV_IMAGE_DECLARE(face_loading_screen);

LV_IMAGE_DECLARE(face_neutral_idle);
LV_IMAGE_DECLARE(face_neutral_blink);
LV_IMAGE_DECLARE(face_low_neg_idle);
LV_IMAGE_DECLARE(face_low_neg_blink);
LV_IMAGE_DECLARE(face_low_pos_idle);
LV_IMAGE_DECLARE(face_low_pos_blink);
LV_IMAGE_DECLARE(face_high_neg_idle);
LV_IMAGE_DECLARE(face_high_neg_blink);
LV_IMAGE_DECLARE(face_high_pos_idle);
LV_IMAGE_DECLARE(face_high_pos_blink);

LV_IMAGE_DECLARE(face_neutral_to_low_neg);
LV_IMAGE_DECLARE(face_neutral_to_low_pos);
LV_IMAGE_DECLARE(face_neutral_to_high_neg);
LV_IMAGE_DECLARE(face_neutral_to_high_pos);

extern const face_animation_t g_face_anim_loading_screen;

const char *face_assets_state_name(face_state_t state);
const face_animation_t *face_assets_idle_for_state(face_state_t state);
const face_animation_t *face_assets_blink_for_state(face_state_t state);
const face_animation_t *face_assets_neutral_to_state(face_state_t state);
const face_animation_t *face_assets_state_to_neutral(face_state_t state);
const face_animation_t *face_assets_neutral_hold(void);
