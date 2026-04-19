#pragma once

#include "esp_err.h"
#include "face_assets.h"

esp_err_t face_fsm_init(void);
esp_err_t face_fsm_request_state(face_state_t target_state);
face_state_t face_fsm_get_state(void);
