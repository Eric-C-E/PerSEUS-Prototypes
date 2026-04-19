#include "face_assets.h"

static const lv_image_dsc_t * const s_loading_frames[] = {
    &face_loading_screen,
};

static const lv_image_dsc_t * const s_neutral_idle_frames[] = {
    &face_neutral_idle,
};

static const lv_image_dsc_t * const s_neutral_blink_frames[] = {
    &face_neutral_blink,
};

static const lv_image_dsc_t * const s_low_neg_idle_frames[] = {
    &face_low_neg_idle,
};

static const lv_image_dsc_t * const s_low_neg_blink_frames[] = {
    &face_low_neg_blink,
};

static const lv_image_dsc_t * const s_low_pos_idle_frames[] = {
    &face_low_pos_idle,
};

static const lv_image_dsc_t * const s_low_pos_blink_frames[] = {
    &face_low_pos_blink,
};

static const lv_image_dsc_t * const s_high_neg_idle_frames[] = {
    &face_high_neg_idle,
};

static const lv_image_dsc_t * const s_high_neg_blink_frames[] = {
    &face_high_neg_blink,
};

static const lv_image_dsc_t * const s_high_pos_idle_frames[] = {
    &face_high_pos_idle,
};

static const lv_image_dsc_t * const s_high_pos_blink_frames[] = {
    &face_high_pos_blink,
};

static const lv_image_dsc_t * const s_neutral_to_low_neg_frames[] = {
    &face_neutral_to_low_neg,
};

static const lv_image_dsc_t * const s_low_neg_to_neutral_frames[] = {
    &face_neutral_to_low_neg,
};

static const lv_image_dsc_t * const s_neutral_to_low_pos_frames[] = {
    &face_neutral_to_low_pos,
};

static const lv_image_dsc_t * const s_low_pos_to_neutral_frames[] = {
    &face_neutral_to_low_pos,
};

static const lv_image_dsc_t * const s_neutral_to_high_neg_frames[] = {
    &face_neutral_to_high_neg,
};

static const lv_image_dsc_t * const s_high_neg_to_neutral_frames[] = {
    &face_neutral_to_high_neg,
};

static const lv_image_dsc_t * const s_neutral_to_high_pos_frames[] = {
    &face_neutral_to_high_pos,
};

static const lv_image_dsc_t * const s_high_pos_to_neutral_frames[] = {
    &face_neutral_to_high_pos,
};

static const face_animation_t s_state_idle[FACE_STATE_COUNT] = {
    [FACE_STATE_NEUTRAL] = {
        .name = "neutral_idle",
        .frames = s_neutral_idle_frames,
        .frame_count = sizeof(s_neutral_idle_frames) / sizeof(s_neutral_idle_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = true,
    },
    [FACE_STATE_LOW_NEG] = {
        .name = "low_neg_idle",
        .frames = s_low_neg_idle_frames,
        .frame_count = sizeof(s_low_neg_idle_frames) / sizeof(s_low_neg_idle_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = true,
    },
    [FACE_STATE_LOW_POS] = {
        .name = "low_pos_idle",
        .frames = s_low_pos_idle_frames,
        .frame_count = sizeof(s_low_pos_idle_frames) / sizeof(s_low_pos_idle_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = true,
    },
    [FACE_STATE_HIGH_NEG] = {
        .name = "high_neg_idle",
        .frames = s_high_neg_idle_frames,
        .frame_count = sizeof(s_high_neg_idle_frames) / sizeof(s_high_neg_idle_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = true,
    },
    [FACE_STATE_HIGH_POS] = {
        .name = "high_pos_idle",
        .frames = s_high_pos_idle_frames,
        .frame_count = sizeof(s_high_pos_idle_frames) / sizeof(s_high_pos_idle_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = true,
    },
};

static const face_animation_t s_state_blink[FACE_STATE_COUNT] = {
    [FACE_STATE_NEUTRAL] = {
        .name = "neutral_blink",
        .frames = s_neutral_blink_frames,
        .frame_count = sizeof(s_neutral_blink_frames) / sizeof(s_neutral_blink_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_LOW_NEG] = {
        .name = "low_neg_blink",
        .frames = s_low_neg_blink_frames,
        .frame_count = sizeof(s_low_neg_blink_frames) / sizeof(s_low_neg_blink_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_LOW_POS] = {
        .name = "low_pos_blink",
        .frames = s_low_pos_blink_frames,
        .frame_count = sizeof(s_low_pos_blink_frames) / sizeof(s_low_pos_blink_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_HIGH_NEG] = {
        .name = "high_neg_blink",
        .frames = s_high_neg_blink_frames,
        .frame_count = sizeof(s_high_neg_blink_frames) / sizeof(s_high_neg_blink_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_HIGH_POS] = {
        .name = "high_pos_blink",
        .frames = s_high_pos_blink_frames,
        .frame_count = sizeof(s_high_pos_blink_frames) / sizeof(s_high_pos_blink_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
};

static const face_animation_t s_neutral_to_state[FACE_STATE_COUNT] = {
    [FACE_STATE_NEUTRAL] = {
        .name = "neutral_hold",
        .frames = s_neutral_idle_frames,
        .frame_count = sizeof(s_neutral_idle_frames) / sizeof(s_neutral_idle_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_LOW_NEG] = {
        .name = "neutral_to_low_neg",
        .frames = s_neutral_to_low_neg_frames,
        .frame_count = sizeof(s_neutral_to_low_neg_frames) / sizeof(s_neutral_to_low_neg_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_LOW_POS] = {
        .name = "neutral_to_low_pos",
        .frames = s_neutral_to_low_pos_frames,
        .frame_count = sizeof(s_neutral_to_low_pos_frames) / sizeof(s_neutral_to_low_pos_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_HIGH_NEG] = {
        .name = "neutral_to_high_neg",
        .frames = s_neutral_to_high_neg_frames,
        .frame_count = sizeof(s_neutral_to_high_neg_frames) / sizeof(s_neutral_to_high_neg_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_HIGH_POS] = {
        .name = "neutral_to_high_pos",
        .frames = s_neutral_to_high_pos_frames,
        .frame_count = sizeof(s_neutral_to_high_pos_frames) / sizeof(s_neutral_to_high_pos_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
};

static const face_animation_t s_state_to_neutral[FACE_STATE_COUNT] = {
    [FACE_STATE_NEUTRAL] = {
        .name = "neutral_hold",
        .frames = s_neutral_idle_frames,
        .frame_count = sizeof(s_neutral_idle_frames) / sizeof(s_neutral_idle_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_LOW_NEG] = {
        .name = "low_neg_to_neutral",
        .frames = s_low_neg_to_neutral_frames,
        .frame_count = sizeof(s_low_neg_to_neutral_frames) / sizeof(s_low_neg_to_neutral_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_LOW_POS] = {
        .name = "low_pos_to_neutral",
        .frames = s_low_pos_to_neutral_frames,
        .frame_count = sizeof(s_low_pos_to_neutral_frames) / sizeof(s_low_pos_to_neutral_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_HIGH_NEG] = {
        .name = "high_neg_to_neutral",
        .frames = s_high_neg_to_neutral_frames,
        .frame_count = sizeof(s_high_neg_to_neutral_frames) / sizeof(s_high_neg_to_neutral_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
    [FACE_STATE_HIGH_POS] = {
        .name = "high_pos_to_neutral",
        .frames = s_high_pos_to_neutral_frames,
        .frame_count = sizeof(s_high_pos_to_neutral_frames) / sizeof(s_high_pos_to_neutral_frames[0]),
        .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
        .loop = false,
    },
};

const face_animation_t g_face_anim_loading_screen = {
    .name = "loading_screen",
    .frames = s_loading_frames,
    .frame_count = sizeof(s_loading_frames) / sizeof(s_loading_frames[0]),
    .frame_duration_ms = FACE_DEFAULT_FRAME_TIME_MS,
    .loop = false,
};

const char *face_assets_state_name(face_state_t state)
{
    static const char *names[FACE_STATE_COUNT] = {
        [FACE_STATE_NEUTRAL] = "neutral",
        [FACE_STATE_LOW_NEG] = "low_neg",
        [FACE_STATE_LOW_POS] = "low_pos",
        [FACE_STATE_HIGH_NEG] = "high_neg",
        [FACE_STATE_HIGH_POS] = "high_pos",
    };

    return (state < FACE_STATE_COUNT) ? names[state] : "unknown";
}

const face_animation_t *face_assets_idle_for_state(face_state_t state)
{
    return (state < FACE_STATE_COUNT) ? &s_state_idle[state] : &s_state_idle[FACE_STATE_NEUTRAL];
}

const face_animation_t *face_assets_blink_for_state(face_state_t state)
{
    return (state < FACE_STATE_COUNT) ? &s_state_blink[state] : &s_state_blink[FACE_STATE_NEUTRAL];
}

const face_animation_t *face_assets_neutral_to_state(face_state_t state)
{
    return (state < FACE_STATE_COUNT) ? &s_neutral_to_state[state] : &s_neutral_to_state[FACE_STATE_NEUTRAL];
}

const face_animation_t *face_assets_state_to_neutral(face_state_t state)
{
    return (state < FACE_STATE_COUNT) ? &s_state_to_neutral[state] : &s_state_to_neutral[FACE_STATE_NEUTRAL];
}

const face_animation_t *face_assets_neutral_hold(void)
{
    return &s_neutral_to_state[FACE_STATE_NEUTRAL];
}
