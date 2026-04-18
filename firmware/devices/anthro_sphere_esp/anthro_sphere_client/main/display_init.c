#include "display_init.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "display_init";

static esp_lcd_panel_io_handle_t s_io_handle;
static esp_lcd_panel_handle_t s_panel_handle;
static lv_display_t *s_lvgl_display;

static uint64_t gpio_to_pin_bit_mask(gpio_num_t gpio_num)
{
    return 1ULL << (uint32_t)gpio_num;
}

static esp_err_t configure_backlight(void)
{
    const gpio_num_t backlight_gpio = BOARD_LCD_PIN_BACKLIGHT;

    if (backlight_gpio == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "Backlight GPIO not configured; assuming panel/backlight is externally powered");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Configure LCD backlight GPIO");
    const gpio_config_t backlight_config = {
        .pin_bit_mask = gpio_to_pin_bit_mask(backlight_gpio),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&backlight_config), TAG, "backlight GPIO config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(backlight_gpio, !BOARD_LCD_BACKLIGHT_ON_LEVEL),
                        TAG, "backlight off failed");

    return ESP_OK;
}

static esp_err_t set_backlight_on(void)
{
    const gpio_num_t backlight_gpio = BOARD_LCD_PIN_BACKLIGHT;

    if (backlight_gpio == GPIO_NUM_NC) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Turn on LCD backlight");
    return gpio_set_level(backlight_gpio, BOARD_LCD_BACKLIGHT_ON_LEVEL);
}

esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(configure_backlight(), TAG, "backlight setup failed");

    ESP_LOGI(TAG, "SPI bus init");
    const spi_bus_config_t bus_config = {
        .sclk_io_num = BOARD_LCD_PIN_SCLK,
        .mosi_io_num = BOARD_LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUFFER_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO),
                        TAG, "SPI bus init failed");

    ESP_LOGI(TAG, "Panel IO creation");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_PIN_DC,
        .cs_gpio_num = BOARD_LCD_PIN_CS,
        .pclk_hz = BOARD_LCD_SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_SPI_HOST,
                                                 &io_config,
                                                 &s_io_handle),
                        TAG, "panel IO creation failed");

    ESP_LOGI(TAG, "Panel creation");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_PIN_RST,
        .rgb_ele_order = BOARD_LCD_RGB_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(s_io_handle, &panel_config, &s_panel_handle),
                        TAG, "ILI9341 panel creation failed");

    ESP_LOGI(TAG, "Panel reset/init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel_handle), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel_handle), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel_handle, BOARD_LCD_COLOR_INVERT),
                        TAG, "panel invert config failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel_handle, BOARD_LCD_MIRROR_X, BOARD_LCD_MIRROR_Y),
                        TAG, "panel mirror config failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel_handle, BOARD_LCD_SWAP_XY),
                        TAG, "panel swap-xy config failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel_handle, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP),
                        TAG, "panel gap config failed");

    ESP_LOGI(TAG, "Panel display on");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel_handle, true),
                        TAG, "panel display-on failed");
    ESP_RETURN_ON_ERROR(set_backlight_on(), TAG, "backlight on failed");

    ESP_LOGI(TAG, "LVGL init");
    const lvgl_port_cfg_t lvgl_config = {
        .task_priority = 4,
        .task_stack = 7168,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
        .timer_period_ms = 5,
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_config), TAG, "LVGL port init failed");

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = s_io_handle,
        .panel_handle = s_panel_handle,
        .buffer_size = BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUFFER_LINES,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = BOARD_LCD_SWAP_XY,
            .mirror_x = BOARD_LCD_MIRROR_X,
            .mirror_y = BOARD_LCD_MIRROR_Y,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = BOARD_LCD_SWAP_BYTES,
        },
    };
    s_lvgl_display = lvgl_port_add_disp(&display_config);
    ESP_RETURN_ON_FALSE(s_lvgl_display, ESP_FAIL, TAG, "LVGL display add failed");

    ESP_LOGI(TAG, "Display init complete");
    return ESP_OK;
}

lv_display_t *display_init_get_lvgl_display(void)
{
    return s_lvgl_display;
}
