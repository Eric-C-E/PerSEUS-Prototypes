#include "display_init.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/lcd_types.h"

static const char *TAG = "display_init";

static uint64_t gpio_pin_mask(gpio_num_t pin)
{
    return 1ULL << (uint32_t)pin;
}

static void configure_backlight(bool enabled)
{
    const gpio_num_t backlight_gpio = BOARD_LCD_PIN_NUM_BK_LIGHT;

    if (backlight_gpio == GPIO_NUM_NC) {
        ESP_LOGW(TAG, "No LCD backlight GPIO configured");
        return;
    }

    const gpio_config_t bk_gpio_config = {
        .pin_bit_mask = gpio_pin_mask(backlight_gpio),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    ESP_ERROR_CHECK(gpio_set_level(backlight_gpio,
                                   enabled ? BOARD_LCD_BACKLIGHT_ON_LEVEL : BOARD_LCD_BACKLIGHT_OFF_LEVEL));
}

static esp_lcd_panel_handle_t init_panel(esp_lcd_panel_io_handle_t io_handle)
{
    ESP_LOGI(TAG, "Install ILI9341 panel driver");

    /*
     * TODO: If the target panel is not ILI9341-compatible, switch this file to
     * the matching esp_lcd_* component and panel constructor. Keep the public
     * display_init() flow unchanged for later face animation work.
     */
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_PIN_NUM_RST,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        .rgb_ele_order = BOARD_LCD_BGR_ORDER ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
#else
        .rgb_endian = BOARD_LCD_BGR_ORDER ? LCD_RGB_ENDIAN_BGR : LCD_RGB_ENDIAN_RGB,
#endif
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    ESP_LOGI(TAG, "Reset and initialize LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, BOARD_LCD_COLOR_INVERT));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, BOARD_LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, BOARD_LCD_MIRROR_X, BOARD_LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    return panel_handle;
}

lv_disp_t *display_init(void)
{
    ESP_LOGI(TAG, "Configure LCD backlight off for init");
    configure_backlight(false);

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t bus_config = {
        .sclk_io_num = BOARD_LCD_PIN_NUM_SCLK,
        .mosi_io_num = BOARD_LCD_PIN_NUM_MOSI,
        .miso_io_num = BOARD_LCD_PIN_NUM_MISO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUFFER_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(BOARD_LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install LCD panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_PIN_NUM_DC,
        .cs_gpio_num = BOARD_LCD_PIN_NUM_CS,
        .pclk_hz = BOARD_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_SPI_HOST,
                                             &io_config,
                                             &io_handle));

    esp_lcd_panel_handle_t panel_handle = init_panel(io_handle);

    ESP_LOGI(TAG, "Initialize LVGL port");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    ESP_LOGI(TAG, "Register LCD display with LVGL");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUFFER_LINES,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .monochrome = false,
        .mipi_dsi = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = BOARD_LCD_SWAP_XY,
            .mirror_x = BOARD_LCD_MIRROR_X,
            .mirror_y = BOARD_LCD_MIRROR_Y,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = BOARD_LCD_SWAP_COLOR_BYTES,
        },
    };

    lv_disp_t *display = lvgl_port_add_disp(&disp_cfg);
    ESP_ERROR_CHECK(display == NULL ? ESP_FAIL : ESP_OK);

    ESP_LOGI(TAG, "Turn LCD backlight on");
    configure_backlight(true);

    ESP_LOGI(TAG, "Display initialization complete");
    return display;
}
