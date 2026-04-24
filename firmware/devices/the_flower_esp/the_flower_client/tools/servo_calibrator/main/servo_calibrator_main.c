#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"

static const char *TAG = "servo_calibrator";

#define CAL_TILT_GPIO                 GPIO_NUM_6
#define CAL_ROTATION_GPIO             GPIO_NUM_7

#define CAL_PWM_FREQ_HZ               50
#define CAL_PWM_RES_BITS              14
#define CAL_PERIOD_US                 20000

#define CAL_DEFAULT_MIN_PULSE_US      500
#define CAL_DEFAULT_CENTER_PULSE_US   1500
#define CAL_DEFAULT_MAX_PULSE_US      2500

#define CAL_LINE_BUF_SIZE             160
#define CAL_STEP_US                   10
#define CAL_SWEEP_STEP_US             25
#define CAL_SWEEP_DELAY_MS            120

#ifdef CONFIG_ESP_CONSOLE_UART_NUM
#define CAL_CONSOLE_UART_NUM          CONFIG_ESP_CONSOLE_UART_NUM
#else
#define CAL_CONSOLE_UART_NUM          UART_NUM_0
#endif

typedef enum {
    AXIS_TILT = 0,
    AXIS_ROTATION,
    AXIS_COUNT,
} axis_id_t;

typedef struct {
    const char *name;
    gpio_num_t gpio;
    ledc_channel_t channel;
    int current_us;
    int min_us;
    int center_us;
    int max_us;
    bool reversed;
} axis_state_t;

static axis_state_t s_axes[AXIS_COUNT] = {
    [AXIS_TILT] = {
        .name = "tilt",
        .gpio = CAL_TILT_GPIO,
        .channel = LEDC_CHANNEL_0,
        .current_us = CAL_DEFAULT_CENTER_PULSE_US,
        .min_us = CAL_DEFAULT_MIN_PULSE_US,
        .center_us = CAL_DEFAULT_CENTER_PULSE_US,
        .max_us = CAL_DEFAULT_MAX_PULSE_US,
        .reversed = false,
    },
    [AXIS_ROTATION] = {
        .name = "rotation",
        .gpio = CAL_ROTATION_GPIO,
        .channel = LEDC_CHANNEL_1,
        .current_us = CAL_DEFAULT_CENTER_PULSE_US,
        .min_us = CAL_DEFAULT_MIN_PULSE_US,
        .center_us = CAL_DEFAULT_CENTER_PULSE_US,
        .max_us = CAL_DEFAULT_MAX_PULSE_US,
        .reversed = false,
    },
};

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint32_t pulse_us_to_duty(int pulse_us)
{
    const uint32_t max_duty = (1U << CAL_PWM_RES_BITS) - 1U;
    return (uint32_t)(((uint64_t)pulse_us * max_duty) / CAL_PERIOD_US);
}

static esp_err_t write_axis_us(axis_state_t *axis, int pulse_us)
{
    pulse_us = clamp_int(pulse_us, CAL_DEFAULT_MIN_PULSE_US, CAL_DEFAULT_MAX_PULSE_US);
    const uint32_t duty = pulse_us_to_duty(pulse_us);

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, axis->channel, duty), TAG, "set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, axis->channel), TAG, "update duty failed");

    axis->current_us = pulse_us;
    printf("%s -> %d us\n", axis->name, axis->current_us);
    return ESP_OK;
}

static axis_state_t *find_axis(const char *name)
{
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < AXIS_COUNT; i++) {
        if (strcmp(name, s_axes[i].name) == 0) {
            return &s_axes[i];
        }
    }
    return NULL;
}

static void print_help(void)
{
    printf("\n");
    printf("Flower Servo Calibrator\n");
    printf("=======================\n");
    printf("This accessory firmware is for finding values to copy into main/board_config.h.\n");
    printf("It is standalone and is not built into the Flower client firmware.\n\n");
    printf("Safety first:\n");
    printf("- Power MG995 servos from a suitable external 5-6 V supply, not the ESP32 3V3 pin.\n");
    printf("- Connect servo ground and ESP32 ground together.\n");
    printf("- Start near 1500 us, then move in small steps.\n");
    printf("- If the mechanism binds, buzzes hard, or hits an end stop, move back immediately.\n\n");
    printf("Wiring defaults in this calibrator:\n");
    printf("- tilt     signal: GPIO%d\n", CAL_TILT_GPIO);
    printf("- rotation signal: GPIO%d\n\n", CAL_ROTATION_GPIO);
    printf("Commands:\n");
    printf("  help                         Show this help\n");
    printf("  status                       Show current recorded values\n");
    printf("  set <axis> <us>              Drive axis to pulse width, e.g. set tilt 1500\n");
    printf("  + <axis> [us]                Increase pulse by step, default %d us\n", CAL_STEP_US);
    printf("  - <axis> [us]                Decrease pulse by step, default %d us\n", CAL_STEP_US);
    printf("  mark <axis> min|center|max   Record current pulse for that point\n");
    printf("  reverse <axis> 0|1           Record whether logical direction is reversed\n");
    printf("  sweep <axis>                 Sweep recorded min..max..center slowly\n");
    printf("  neutral                      Drive both axes to recorded centers\n");
    printf("  print                        Print board_config.h values\n");
    printf("\n");
    printf("Suggested workflow:\n");
    printf("1. Run: neutral\n");
    printf("2. For tilt: use set/+/- to find safe min, center, max; use mark after each.\n");
    printf("3. Repeat for rotation.\n");
    printf("4. If GUI direction feels backwards later, use reverse axis 1 and print again.\n");
    printf("5. Run: print\n\n");
}

static void print_status(void)
{
    printf("\nCurrent calibration state:\n");
    for (int i = 0; i < AXIS_COUNT; i++) {
        const axis_state_t *axis = &s_axes[i];
        printf("  %-8s gpio=%d current=%dus min=%dus center=%dus max=%dus reversed=%d\n",
               axis->name,
               axis->gpio,
               axis->current_us,
               axis->min_us,
               axis->center_us,
               axis->max_us,
               axis->reversed ? 1 : 0);
    }
    printf("\n");
}

static void print_board_config(void)
{
    const axis_state_t *tilt = &s_axes[AXIS_TILT];
    const axis_state_t *rotation = &s_axes[AXIS_ROTATION];
    const int global_min = (tilt->min_us > rotation->min_us) ? tilt->min_us : rotation->min_us;
    const int global_max = (tilt->max_us < rotation->max_us) ? tilt->max_us : rotation->max_us;

    if (global_min >= global_max) {
        printf("\nRecorded ranges do not overlap safely:\n");
        printf("  tilt:     min=%dus max=%dus\n", tilt->min_us, tilt->max_us);
        printf("  rotation: min=%dus max=%dus\n", rotation->min_us, rotation->max_us);
        printf("The current Flower firmware uses one shared pulse range. Recheck marks or\n");
        printf("update flower_servo.c to support per-axis pulse min/max before proceeding.\n\n");
        return;
    }

    const float pulse_span = (float)(global_max - global_min);
    const float tilt_center_deg = ((float)(tilt->center_us - global_min) * 180.0f) / pulse_span;
    const float rotation_center_deg = ((float)(rotation->center_us - global_min) * 180.0f) / pulse_span;

    printf("\nPaste/update these in main/board_config.h:\n\n");
    printf("#define BOARD_SERVO_TILT_GPIO             GPIO_NUM_%d\n", tilt->gpio);
    printf("#define BOARD_SERVO_ROTATION_GPIO         GPIO_NUM_%d\n\n", rotation->gpio);
    printf("#define BOARD_SERVO_PWM_FREQ_HZ           %d\n", CAL_PWM_FREQ_HZ);
    printf("#define BOARD_SERVO_PWM_RES_BITS          %d\n\n", CAL_PWM_RES_BITS);
    printf("#define BOARD_SERVO_MIN_PULSE_US          %d\n", global_min);
    printf("#define BOARD_SERVO_MAX_PULSE_US          %d\n", global_max);
    printf("#define BOARD_SERVO_PERIOD_US             %d\n\n", CAL_PERIOD_US);
    printf("#define BOARD_SERVO_TILT_MIN_DEG          0.0f\n");
    printf("#define BOARD_SERVO_TILT_CENTER_DEG       %.1ff\n", tilt_center_deg);
    printf("#define BOARD_SERVO_TILT_MAX_DEG          180.0f\n");
    printf("#define BOARD_SERVO_TILT_REVERSED         %d\n\n", tilt->reversed ? 1 : 0);
    printf("#define BOARD_SERVO_ROTATION_MIN_DEG      0.0f\n");
    printf("#define BOARD_SERVO_ROTATION_CENTER_DEG   %.1ff\n", rotation_center_deg);
    printf("#define BOARD_SERVO_ROTATION_MAX_DEG      180.0f\n");
    printf("#define BOARD_SERVO_ROTATION_REVERSED     %d\n\n", rotation->reversed ? 1 : 0);
    printf("Recorded raw pulse notes:\n");
    printf("  tilt:     min=%dus center=%dus max=%dus\n", tilt->min_us, tilt->center_us, tilt->max_us);
    printf("  rotation: min=%dus center=%dus max=%dus\n\n", rotation->min_us, rotation->center_us, rotation->max_us);
    printf("This output uses the shared safe pulse intersection because the production\n");
    printf("firmware currently has one global pulse range. If the servos need different\n");
    printf("pulse ranges, update flower_servo.c to support per-axis pulse min/max.\n\n");
}

static void mark_axis(axis_state_t *axis, const char *point)
{
    if (strcmp(point, "min") == 0) {
        axis->min_us = axis->current_us;
    } else if (strcmp(point, "center") == 0) {
        axis->center_us = axis->current_us;
    } else if (strcmp(point, "max") == 0) {
        axis->max_us = axis->current_us;
    } else {
        printf("Unknown mark point '%s'. Use min, center, or max.\n", point);
        return;
    }

    printf("Recorded %s %s = %d us\n", axis->name, point, axis->current_us);
}

static void sweep_axis(axis_state_t *axis)
{
    printf("Sweeping %s from min to max and back to center. Press Ctrl+] in monitor if unsafe.\n", axis->name);

    for (int pulse = axis->min_us; pulse <= axis->max_us; pulse += CAL_SWEEP_STEP_US) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(write_axis_us(axis, pulse));
        vTaskDelay(pdMS_TO_TICKS(CAL_SWEEP_DELAY_MS));
    }
    for (int pulse = axis->max_us; pulse >= axis->min_us; pulse -= CAL_SWEEP_STEP_US) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(write_axis_us(axis, pulse));
        vTaskDelay(pdMS_TO_TICKS(CAL_SWEEP_DELAY_MS));
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_axis_us(axis, axis->center_us));
}

static char *next_token(char **cursor)
{
    char *s = *cursor;
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        *cursor = s;
        return NULL;
    }

    char *start = s;
    while (*s && !isspace((unsigned char)*s)) {
        s++;
    }
    if (*s) {
        *s = '\0';
        s++;
    }
    *cursor = s;
    return start;
}

static bool parse_int_token(const char *token, int *out_value)
{
    if (!token || !out_value) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long value = strtol(token, &end, 10);
    if (errno != 0 || end == token || *end != '\0') {
        return false;
    }

    *out_value = (int)value;
    return true;
}

static void handle_command(char *line)
{
    char *cursor = line;
    char *cmd = next_token(&cursor);
    if (!cmd) {
        return;
    }

    for (char *p = cmd; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        print_help();
        return;
    }

    if (strcmp(cmd, "status") == 0) {
        print_status();
        return;
    }

    if (strcmp(cmd, "print") == 0) {
        print_board_config();
        return;
    }

    if (strcmp(cmd, "neutral") == 0) {
        for (int i = 0; i < AXIS_COUNT; i++) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(write_axis_us(&s_axes[i], s_axes[i].center_us));
        }
        return;
    }

    char *axis_name = next_token(&cursor);
    axis_state_t *axis = find_axis(axis_name);
    if (!axis) {
        printf("Missing or unknown axis. Use 'tilt' or 'rotation'. Type help.\n");
        return;
    }

    if (strcmp(cmd, "set") == 0) {
        int pulse_us = 0;
        if (!parse_int_token(next_token(&cursor), &pulse_us)) {
            printf("Usage: set <tilt|rotation> <pulse_us>\n");
            return;
        }
        ESP_ERROR_CHECK_WITHOUT_ABORT(write_axis_us(axis, pulse_us));
        return;
    }

    if (strcmp(cmd, "+") == 0 || strcmp(cmd, "-") == 0) {
        int step_us = CAL_STEP_US;
        char *step_token = next_token(&cursor);
        if (step_token && !parse_int_token(step_token, &step_us)) {
            printf("Usage: %s <tilt|rotation> [step_us]\n", cmd);
            return;
        }
        if (step_us < 1) {
            step_us = CAL_STEP_US;
        }
        const int direction = (strcmp(cmd, "+") == 0) ? 1 : -1;
        ESP_ERROR_CHECK_WITHOUT_ABORT(write_axis_us(axis, axis->current_us + direction * step_us));
        return;
    }

    if (strcmp(cmd, "mark") == 0) {
        char *point = next_token(&cursor);
        if (!point) {
            printf("Usage: mark <tilt|rotation> min|center|max\n");
            return;
        }
        mark_axis(axis, point);
        return;
    }

    if (strcmp(cmd, "reverse") == 0) {
        int reversed = 0;
        if (!parse_int_token(next_token(&cursor), &reversed)) {
            printf("Usage: reverse <tilt|rotation> 0|1\n");
            return;
        }
        axis->reversed = reversed != 0;
        printf("Recorded %s reversed = %d\n", axis->name, axis->reversed ? 1 : 0);
        return;
    }

    if (strcmp(cmd, "sweep") == 0) {
        sweep_axis(axis);
        return;
    }

    printf("Unknown command '%s'. Type help.\n", cmd);
}

static esp_err_t pwm_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = CAL_PWM_RES_BITS,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CAL_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "LEDC timer config failed");

    for (int i = 0; i < AXIS_COUNT; i++) {
        const ledc_channel_config_t channel_config = {
            .gpio_num = s_axes[i].gpio,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = s_axes[i].channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "LEDC channel config failed");
    }

    return ESP_OK;
}

static esp_err_t console_stdin_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    esp_err_t ret = uart_driver_install(CAL_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    return ESP_OK;
}

static bool console_read_line(char *line, size_t line_size)
{
    if (!line || line_size == 0) {
        return false;
    }

    size_t len = 0;
    while (true) {
        uint8_t ch = 0;
        int read_len = uart_read_bytes(CAL_CONSOLE_UART_NUM, &ch, 1, portMAX_DELAY);
        if (read_len <= 0) {
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            line[len] = '\0';
            printf("\n");
            return true;
        }

        if (ch == '\b' || ch == 0x7f) {
            if (len > 0) {
                len--;
                printf("\b \b");
            }
            continue;
        }

        if (isprint((unsigned char)ch) && len < line_size - 1) {
            line[len++] = (char)ch;
            putchar(ch);
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(console_stdin_init());
    ESP_ERROR_CHECK(pwm_init());

    for (int i = 0; i < AXIS_COUNT; i++) {
        ESP_ERROR_CHECK(write_axis_us(&s_axes[i], s_axes[i].center_us));
    }

    print_help();

    char line[CAL_LINE_BUF_SIZE];
    while (true) {
        printf("cal> ");
        fflush(stdout);

        if (!console_read_line(line, sizeof(line))) {
            continue;
        }

        handle_command(line);
    }
}
