#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PERSEUS_DEVICE_ANTHRO_SPHERE = 0,
    PERSEUS_DEVICE_ABSTRACT_SPHERE,
    PERSEUS_DEVICE_FLOWER,
} perseus_device_type_t;

const char *perseus_device_type_name(perseus_device_type_t device_type);
void perseus_common_init(perseus_device_type_t device_type);

#ifdef __cplusplus
}
#endif
