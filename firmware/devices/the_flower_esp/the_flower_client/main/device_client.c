#include "device_client.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "cJSON.h"
#include "client_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "flower_servo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

static const char *TAG = "device_client";

#define DEVICE_CLIENT_RX_BUF_SIZE 512
#define DEVICE_CLIENT_LINE_BUF_SIZE 512

static int s_socket_fd = -1;
static SemaphoreHandle_t s_socket_lock;

static esp_err_t send_json_line(const char *json_line)
{
    ESP_RETURN_ON_FALSE(json_line, ESP_ERR_INVALID_ARG, TAG, "json_line is NULL");
    ESP_RETURN_ON_FALSE(s_socket_lock, ESP_ERR_INVALID_STATE, TAG, "socket lock is not initialized");

    esp_err_t ret = ESP_OK;
    xSemaphoreTake(s_socket_lock, portMAX_DELAY);

    if (s_socket_fd < 0) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        const size_t len = strlen(json_line);
        if (send(s_socket_fd, json_line, len, 0) < 0 || send(s_socket_fd, "\n", 1, 0) < 0) {
            ESP_LOGW(TAG, "send failed: errno=%d", errno);
            ret = ESP_FAIL;
        }
    }

    xSemaphoreGive(s_socket_lock);
    return ret;
}

static esp_err_t send_hello(void)
{
    char msg[160];
    snprintf(msg,
             sizeof(msg),
             "{\"type\":\"hello\",\"device_id\":\"%s\",\"device_kind\":\"%s\"}",
             DEVICE_CLIENT_ID,
             DEVICE_CLIENT_KIND);
    return send_json_line(msg);
}

static esp_err_t send_heartbeat(void)
{
    char msg[128];
    snprintf(msg,
             sizeof(msg),
             "{\"type\":\"heartbeat\",\"device_id\":\"%s\"}",
             DEVICE_CLIENT_ID);
    return send_json_line(msg);
}

esp_err_t device_client_send_event(const char *event_name)
{
    ESP_RETURN_ON_FALSE(event_name, ESP_ERR_INVALID_ARG, TAG, "event_name is NULL");

    char msg[160];
    snprintf(msg,
             sizeof(msg),
             "{\"type\":\"event\",\"device_id\":\"%s\",\"event\":\"%s\"}",
             DEVICE_CLIENT_ID,
             event_name);
    return send_json_line(msg);
}

static bool json_string_equals(const cJSON *obj, const char *name, const char *expected)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, name);
    return cJSON_IsString(item) && item->valuestring && strcmp(item->valuestring, expected) == 0;
}

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static bool map_state_string(const char *state, flower_state_t *out_state)
{
    if (!state || !out_state) {
        return false;
    }

    if (strcmp(state, "neutral") == 0) {
        *out_state = FLOWER_STATE_NEUTRAL;
    } else if (strcmp(state, "low_negative") == 0) {
        *out_state = FLOWER_STATE_LOW_NEGATIVE;
    } else if (strcmp(state, "low_positive") == 0) {
        *out_state = FLOWER_STATE_LOW_POSITIVE;
    } else if (strcmp(state, "high_negative") == 0) {
        *out_state = FLOWER_STATE_HIGH_NEGATIVE;
    } else if (strcmp(state, "high_positive") == 0) {
        *out_state = FLOWER_STATE_HIGH_POSITIVE;
    } else {
        return false;
    }

    return true;
}

static void handle_set_state(const cJSON *root)
{
    const cJSON *state_item = cJSON_GetObjectItemCaseSensitive(root, "state");
    if (!cJSON_IsString(state_item) || !state_item->valuestring) {
        ESP_LOGW(TAG, "set_state missing string state");
        return;
    }

    flower_state_t target_state;
    if (!map_state_string(state_item->valuestring, &target_state)) {
        ESP_LOGW(TAG, "Unknown state '%s'", state_item->valuestring);
        return;
    }

    ESP_LOGI(TAG, "Command set_state -> %s", state_item->valuestring);
    ESP_ERROR_CHECK_WITHOUT_ABORT(flower_servo_set_state(target_state));
}

static void handle_set_raw(const cJSON *root)
{
    const cJSON *run_item = cJSON_GetObjectItemCaseSensitive(root, "run");
    const cJSON *speed_item = cJSON_GetObjectItemCaseSensitive(root, "speed");
    const cJSON *amplitude_item = cJSON_GetObjectItemCaseSensitive(root, "amplitude");

    if (!cJSON_IsBool(run_item) || !cJSON_IsNumber(speed_item) || !cJSON_IsNumber(amplitude_item)) {
        ESP_LOGW(TAG, "set_raw requires run: bool, speed: number, amplitude: number");
        return;
    }

    const bool run = cJSON_IsTrue(run_item);
    const float speed = clamp_float((float)speed_item->valuedouble, 0.0f, 1.0f);
    const float amplitude = clamp_float((float)amplitude_item->valuedouble, 0.0f, 1.0f);

    ESP_LOGI(TAG, "Command set_raw -> run=%d speed=%.2f amplitude=%.2f", run, speed, amplitude);
    ESP_ERROR_CHECK_WITHOUT_ABORT(flower_servo_set_raw(run, speed, amplitude));
}

static void handle_set_tilt(const cJSON *root)
{
    const cJSON *tilt_item = cJSON_GetObjectItemCaseSensitive(root, "tilt");
    if (!cJSON_IsNumber(tilt_item)) {
        ESP_LOGW(TAG, "set_tilt requires tilt: number");
        return;
    }

    const float tilt = clamp_float((float)tilt_item->valuedouble, 0.0f, 1.0f);

    ESP_LOGI(TAG, "Command set_tilt -> tilt=%.2f", tilt);
    ESP_ERROR_CHECK_WITHOUT_ABORT(flower_servo_set_tilt(tilt));
}

static void handle_command_line(const char *line)
{
    cJSON *root = cJSON_Parse(line);
    if (!root) {
        ESP_LOGW(TAG, "Ignoring malformed JSON line");
        return;
    }

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }

    if (!json_string_equals(root, "type", "command")) {
        cJSON_Delete(root);
        return;
    }

    if (!json_string_equals(root, "device_id", DEVICE_CLIENT_ID)) {
        cJSON_Delete(root);
        return;
    }

    const cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "command");
    if (!cJSON_IsString(command) || !command->valuestring) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(command->valuestring, "set_state") == 0) {
        handle_set_state(root);
    } else if (strcmp(command->valuestring, "set_raw") == 0) {
        handle_set_raw(root);
    } else if (strcmp(command->valuestring, "set_tilt") == 0) {
        handle_set_tilt(root);
    } else {
        ESP_LOGI(TAG, "Ignoring unsupported flower command '%s'", command->valuestring);
    }

    cJSON_Delete(root);
}

static int connect_to_gui(void)
{
    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DEVICE_CLIENT_GUI_PORT),
    };

    int inet_ok = inet_pton(AF_INET, DEVICE_CLIENT_GUI_HOST, &dest_addr.sin_addr);
    if (inet_ok != 1) {
        ESP_LOGE(TAG, "DEVICE_CLIENT_GUI_HOST must be an IPv4 address: %s", DEVICE_CLIENT_GUI_HOST);
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket create failed: errno=%d", errno);
        return -1;
    }

    struct timeval recv_timeout = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    ESP_LOGI(TAG, "Connecting to GUI %s:%d", DEVICE_CLIENT_GUI_HOST, DEVICE_CLIENT_GUI_PORT);
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGW(TAG, "connect failed: errno=%d", errno);
        close(sock);
        return -1;
    }

    return sock;
}

static void close_current_socket(void)
{
    xSemaphoreTake(s_socket_lock, portMAX_DELAY);
    if (s_socket_fd >= 0) {
        shutdown(s_socket_fd, SHUT_RDWR);
        close(s_socket_fd);
        s_socket_fd = -1;
    }
    xSemaphoreGive(s_socket_lock);
}

static void device_client_task(void *arg)
{
    (void)arg;

    char rx_buf[DEVICE_CLIENT_RX_BUF_SIZE];
    char line_buf[DEVICE_CLIENT_LINE_BUF_SIZE];

    while (true) {
        int sock = connect_to_gui();
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(DEVICE_CLIENT_RECONNECT_MS));
            continue;
        }

        xSemaphoreTake(s_socket_lock, portMAX_DELAY);
        s_socket_fd = sock;
        xSemaphoreGive(s_socket_lock);

        ESP_LOGI(TAG, "Connected to GUI");
        ESP_ERROR_CHECK_WITHOUT_ABORT(send_hello());

        size_t line_len = 0;
        TickType_t last_heartbeat = 0;

        while (true) {
            TickType_t now = xTaskGetTickCount();
            if (last_heartbeat == 0 ||
                (now - last_heartbeat) >= pdMS_TO_TICKS(DEVICE_CLIENT_HEARTBEAT_MS)) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(send_heartbeat());
                last_heartbeat = now;
            }

            int len = recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
            if (len == 0) {
                ESP_LOGW(TAG, "GUI socket closed");
                break;
            }
            if (len < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                ESP_LOGW(TAG, "recv failed: errno=%d", errno);
                break;
            }

            for (int i = 0; i < len; i++) {
                char ch = rx_buf[i];
                if (ch == '\n') {
                    line_buf[line_len] = '\0';
                    if (line_len > 0) {
                        handle_command_line(line_buf);
                    }
                    line_len = 0;
                } else if (line_len < sizeof(line_buf) - 1) {
                    line_buf[line_len++] = ch;
                } else {
                    ESP_LOGW(TAG, "Dropping overlong JSON line");
                    line_len = 0;
                }
            }
        }

        close_current_socket();
        ESP_ERROR_CHECK_WITHOUT_ABORT(flower_servo_stop());
        ESP_LOGI(TAG, "Reconnect in %d ms", DEVICE_CLIENT_RECONNECT_MS);
        vTaskDelay(pdMS_TO_TICKS(DEVICE_CLIENT_RECONNECT_MS));
    }
}

esp_err_t device_client_start(void)
{
    ESP_LOGI(TAG, "Start Flower TCP client");

    if (!s_socket_lock) {
        s_socket_lock = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(s_socket_lock, ESP_ERR_NO_MEM, TAG, "socket lock allocation failed");
    }

    BaseType_t task_ok = xTaskCreate(device_client_task, "device_client", 6144, NULL, 5, NULL);
    ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create device client task");

    return ESP_OK;
}
