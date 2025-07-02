/**
 * @file      ssd1306.c
 * @author    Muhamad Arif Hidayat
 * @brief Implementation of the I2C-based SSD1306 OLED display driver.
 * @version   1.0
 * @date      2025-06-30
 * @copyright Copyright (c) 2025
 *
 * This file contains the low-level logic, framebuffer management, partial update
 * mechanisms, and rendering functions for various font formats and graphic primitives.
 * It provides a robust and efficient interface for controlling the SSD1306 OLED display.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"

#include "ssd1306.h"

static const char *TAG = "SSD1306";

//=========================================================================================
// Internal Macros and Command Definitions
//=========================================================================================

#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    } /**< Swaps two int16_t variables. */
#define _abs(a) ((a) < 0 ? -(a) : (a))       /**< Computes the absolute value of a number. */
#define _min(a, b) (((a) < (b)) ? (a) : (b)) /**< Returns the minimum of two values. */

#define OLED_CONTROL_BYTE_CMD_STREAM 0x00  /**< Control byte for command stream. */
#define OLED_CONTROL_BYTE_DATA_STREAM 0x40 /**< Control byte for data stream. */

#define OLED_CMD_SET_CONTRAST 0x81                         /**< Sets display contrast. */
#define OLED_CMD_DISPLAY_RAM 0xA4                          /**< Displays RAM content. */
#define OLED_CMD_DISPLAY_NORMAL 0xA6                       /**< Sets normal display mode. */
#define OLED_CMD_INVERTDISPLAY 0xA7                        /**< Inverts display colors. */
#define OLED_CMD_DISPLAY_OFF 0xAE                          /**< Turns off the display. */
#define OLED_CMD_DISPLAY_ON 0xAF                           /**< Turns on the display. */
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20                 /**< Sets memory addressing mode. */
#define OLED_CMD_SET_COLUMN_RANGE 0x21                     /**< Sets column address range. */
#define OLED_CMD_SET_PAGE_RANGE 0x22                       /**< Sets page address range. */
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40               /**< Sets display start line. */
#define OLED_CMD_SET_SEGMENT_REMAP 0xA0                    /**< Sets segment remapping. */
#define OLED_CMD_SET_MUX_RATIO 0xA8                        /**< Sets multiplex ratio. */
#define OLED_CMD_SET_COM_SCAN_MODE 0xC0                    /**< Sets COM scan direction. */
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3                   /**< Sets display offset. */
#define OLED_CMD_SET_DISPLAY_CLK_DIV 0xD5                  /**< Sets display clock divider. */
#define OLED_CMD_SET_PRECHARGE 0xD9                        /**< Sets pre-charge period. */
#define OLED_CMD_SET_COM_PIN_MAP 0xDA                      /**< Sets COM pin configuration. */
#define OLED_CMD_SET_VCOMH_DESELCT 0xDB                    /**< Sets VCOMH deselect level. */
#define OLED_CMD_SET_CHARGE_PUMP 0x8D                      /**< Sets charge pump configuration. */
#define OLED_CMD_DEACTIVATE_SCROLL 0x2E                    /**< Deactivates scrolling. */
#define OLED_CMD_ACTIVATE_SCROLL 0x2F                      /**< Activates scrolling. */
#define OLED_CMD_RIGHT_HORIZONTAL_SCROLL 0x26              /**< Right horizontal scroll. */
#define OLED_CMD_LEFT_HORIZONTAL_SCROLL 0x27               /**< Left horizontal scroll. */
#define OLED_CMD_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 /**< Vertical and right horizontal scroll. */
#define OLED_CMD_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A  /**< Vertical and left horizontal scroll. */
#define OLED_CMD_SET_VERTICAL_SCROLL_AREA 0xA3             /**< Sets vertical scroll area. */

//=========================================================================================
// Internal Data Structure
//=========================================================================================

/**
 * @struct ssd1306_dev_t
 * @brief Internal structure to store the SSD1306 driver state.
 */
struct ssd1306_dev_t
{
    ssd1306_config_t config; /**< Display configuration parameters. */
    uint8_t *buffer;         /**< Framebuffer for display data. */
    size_t buffer_size;      /**< Size of the framebuffer. */

    // Partial update state
    bool needs_update; /**< Indicates if an update is required. */
    uint8_t min_page;  /**< Minimum page for partial update. */
    uint8_t max_page;  /**< Maximum page for partial update. */
    uint8_t min_col;   /**< Minimum column for partial update. */
    uint8_t max_col;   /**< Maximum column for partial update. */

    // Graphics state (adapted from Adafruit_GFX)
    int16_t cursor_x;                     /**< Current x-coordinate of the text cursor. */
    int16_t cursor_y;                     /**< Current y-coordinate of the text cursor. */
    uint8_t textsize_x;                   /**< Text size scaling factor for x-axis. */
    uint8_t textsize_y;                   /**< Text size scaling factor for y-axis. */
    ssd1306_color_t textcolor;            /**< Text foreground color. */
    ssd1306_color_t textbgcolor;          /**< Text background color. */
    bool wrap;                            /**< Text wrapping mode. */
    const ssd1306_font_handle_t *gfxFont; /**< Current font handle. */
};

//=========================================================================================
// Internal Helper Functions
//=========================================================================================

/**
 * @brief Sends a list of commands to the SSD1306 display via I2C.
 *
 * @param handle SSD1306 device handle.
 * @param cmd_list Array of commands to send.
 * @param size Size of the command array in bytes.
 * @return esp_err_t Operation status.
 */
#define I2C_CMD_BUFFER_SIZE 256 // Ukuran buffer statis untuk link I2C (sesuaikan jika perlu)

static uint8_t i2c_cmd_buffer[I2C_CMD_BUFFER_SIZE]; // Buffer statis untuk link I2C
static i2c_cmd_handle_t cmd_cache = NULL;           // Cache link I2C

static esp_err_t _ssd1306_send_cmd_list(ssd1306_handle_t handle, const uint8_t *cmd_list, size_t size)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    // Inisialisasi link I2C statis jika belum ada
    if (!cmd_cache)
    {
        cmd_cache = i2c_cmd_link_create_static(i2c_cmd_buffer, I2C_CMD_BUFFER_SIZE);
        ESP_RETURN_ON_FALSE(cmd_cache, ESP_ERR_NO_MEM, TAG, "Failed to create static I2C command link");
    }

    // Bersihkan link sebelum digunakan
    i2c_cmd_link_delete(cmd_cache);
    cmd_cache = i2c_cmd_link_create_static(i2c_cmd_buffer, I2C_CMD_BUFFER_SIZE);

    i2c_master_start(cmd_cache);
    i2c_master_write_byte(cmd_cache, (handle->config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_cache, OLED_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write(cmd_cache, cmd_list, size, true);
    i2c_master_stop(cmd_cache);

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++)
    { // Coba hingga 3 kali
        ret = i2c_master_cmd_begin(handle->config.i2c_port, cmd_cache, pdMS_TO_TICKS(100));
        if (ret == ESP_OK)
            break;
        vTaskDelay(pdMS_TO_TICKS(10)); // Tunggu 10ms sebelum retry
    }

    // Jangan hapus cmd_cache agar dapat digunakan kembali
    return ret;
}

/**
 * @brief Resets the dirty area for partial updates.
 *
 * @param handle SSD1306 device handle.
 */
static void _ssd1306_reset_dirty_area(ssd1306_handle_t handle)
{
    handle->needs_update = false;
    handle->min_col = handle->config.screen_width;
    handle->max_col = 0;
    handle->min_page = handle->config.screen_height / 8;
    handle->max_page = 0;
}

/**
 * @brief Marks an area as dirty for partial updates.
 *
 * @param handle SSD1306 device handle.
 * @param x Starting x-coordinate.
 * @param y Starting y-coordinate.
 * @param w Width of the area.
 * @param h Height of the area.
 */
static void _ssd1306_mark_dirty(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (!handle || x >= handle->config.screen_width || y >= handle->config.screen_height ||
        x + w <= 0 || y + h <= 0)
        return;

    int16_t x1 = x > 0 ? x : 0;
    int16_t y1 = y > 0 ? y : 0;
    int16_t x2 = (x + w - 1) < handle->config.screen_width ? (x + w - 1) : (handle->config.screen_width - 1);
    int16_t y2 = (y + h - 1) < handle->config.screen_height ? (y + h - 1) : (handle->config.screen_height - 1);

    uint8_t page1 = y1 >> 3; // Pembagian dengan 8 dioptimalkan dengan shift
    uint8_t page2 = y2 >> 3;

    handle->min_col = x1 < handle->min_col ? x1 : handle->min_col;
    handle->max_col = x2 > handle->max_col ? x2 : handle->max_col;
    handle->min_page = page1 < handle->min_page ? page1 : handle->min_page;
    handle->max_page = page2 > handle->max_page ? page2 : handle->max_page;
    handle->needs_update = true;
}

/**
 * @brief Helper function to draw a circle quadrant.
 *
 * @param handle SSD1306 device handle.
 * @param x0 Center x-coordinate.
 * @param y0 Center y-coordinate.
 * @param r Circle radius.
 * @param cornername Bitmask indicating which quadrant to draw.
 * @param color Drawing color.
 */
static void _ssd1306_draw_circle_helper(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, uint8_t cornername, ssd1306_color_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x4)
        {
            ssd1306_draw_pixel(handle, x0 + x, y0 + y, color);
            ssd1306_draw_pixel(handle, x0 + y, y0 + x, color);
        }
        if (cornername & 0x2)
        {
            ssd1306_draw_pixel(handle, x0 + x, y0 - y, color);
            ssd1306_draw_pixel(handle, x0 + y, y0 - x, color);
        }
        if (cornername & 0x8)
        {
            ssd1306_draw_pixel(handle, x0 - y, y0 + x, color);
            ssd1306_draw_pixel(handle, x0 - x, y0 + y, color);
        }
        if (cornername & 0x1)
        {
            ssd1306_draw_pixel(handle, x0 - y, y0 - x, color);
            ssd1306_draw_pixel(handle, x0 - x, y0 - y, color);
        }
    }
}

/**
 * @brief Helper function to fill a circle quadrant.
 *
 * @param handle SSD1306 device handle.
 * @param x0 Center x-coordinate.
 * @param y0 Center y-coordinate.
 * @param r Circle radius.
 * @param corners Bitmask indicating which quadrants to fill.
 * @param delta Adjustment for vertical line height.
 * @param color Fill color.
 */
static void _ssd1306_fill_circle_helper(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, ssd1306_color_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    int16_t px = x;
    int16_t py = y;

    delta++; // Avoid some +1 operations in the loop

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (x < (y + 1))
        {
            if (corners & 1)
                ssd1306_draw_fast_vline(handle, x0 + x, y0 - y, 2 * y + delta, color);
            if (corners & 2)
                ssd1306_draw_fast_vline(handle, x0 - x, y0 - y, 2 * y + delta, color);
        }
        if (y != py)
        {
            if (corners & 1)
                ssd1306_draw_fast_vline(handle, x0 + py, y0 - px, 2 * px + delta, color);
            if (corners & 2)
                ssd1306_draw_fast_vline(handle, x0 - py, y0 - px, 2 * px + delta, color);
            py = y;
        }
        px = x;
    }
}

/**
 * @brief Calculates the bounding box for a single character.
 *
 * @param handle SSD1306 device handle.
 * @param c Character to measure.
 * @param x Pointer to the current x-coordinate of the cursor.
 * @param y Pointer to the current y-coordinate of the cursor.
 * @param minx Pointer to the minimum x-coordinate.
 * @param miny Pointer to the minimum y-coordinate.
 * @param maxx Pointer to the maximum x-coordinate.
 * @param maxy Pointer to the maximum y-coordinate.
 */
static void _ssd1306_char_bounds(ssd1306_handle_t handle, unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy)
{
    if (!handle->gfxFont)
        return;

    if (c == '\n')
    {           // Baris baru
        *x = 0; // Reset x, geser y
        const GFXfont *font = (const GFXfont *)handle->gfxFont->font_data;
        *y += handle->textsize_y * font->yAdvance;
    }
    else if (c != '\r')
    {
        const GFXfont *font = (const GFXfont *)handle->gfxFont->font_data;
        if (c >= font->first && c <= font->last)
        {
            const GFXglyph *glyph = &font->glyph[c - font->first];
            uint8_t gw = glyph->width, gh = glyph->height, xa = glyph->xAdvance;
            int8_t xo = glyph->xOffset, yo = glyph->yOffset;

            if (handle->wrap && ((*x + ((int16_t)xo + gw) * handle->textsize_x) > handle->config.screen_width))
            {
                *x = 0;
                *y += handle->textsize_y * font->yAdvance;
            }
            int16_t x1 = *x + xo * handle->textsize_x;
            int16_t y1 = *y + yo * handle->textsize_y;
            int16_t x2 = x1 + gw * handle->textsize_x - 1;
            int16_t y2 = y1 + gh * handle->textsize_y - 1;

            if (x1 < *minx)
                *minx = x1;
            if (y1 < *miny)
                *miny = y1;
            if (x2 > *maxx)
                *maxx = x2;
            if (y2 > *maxy)
                *maxy = y2;
            *x += xa * handle->textsize_x;
        }
    }
}

//=========================================================================================
// Public Function Implementations
//=========================================================================================

/**
 * @brief Creates and initializes an SSD1306 driver instance.
 *
 * @param config Pointer to the SSD1306 configuration structure.
 * @param out_handle Pointer to store the created device handle.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_create(const ssd1306_config_t *config, ssd1306_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle && *out_handle == NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    ssd1306_handle_t handle = calloc(1, sizeof(struct ssd1306_dev_t));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "Failed to allocate handle");

    handle->config = *config;
    handle->buffer_size = (config->screen_width * config->screen_height) / 8;
    handle->buffer = malloc(handle->buffer_size);
    if (!handle->buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    // Initialize graphics state
    handle->cursor_x = 0;
    handle->cursor_y = 0;
    handle->textsize_x = 1;
    handle->textsize_y = 1;
    handle->textcolor = OLED_COLOR_WHITE;
    handle->textbgcolor = OLED_COLOR_BLACK;
    handle->wrap = true;
    handle->gfxFont = &FONT_5x7; // Default font

    // Configure I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->i2c_clk_speed_hz,
    };
    ESP_ERROR_CHECK(i2c_param_config(config->i2c_port, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(config->i2c_port, I2C_MODE_MASTER, 0, 0, 0));

    // Reset display if reset pin is defined
    if (handle->config.rst_pin != -1)
    {
        gpio_set_direction(handle->config.rst_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(handle->config.rst_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(handle->config.rst_pin, 1);
    }

    // Display initialization command sequence
    const uint8_t init_cmds[] = {
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_DISPLAY_CLK_DIV, 0x80,
        OLED_CMD_SET_MUX_RATIO, (uint8_t)(config->screen_height - 1),
        OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
        OLED_CMD_SET_DISPLAY_START_LINE | 0x00,
        OLED_CMD_SET_CHARGE_PUMP, 0x14,
        OLED_CMD_SET_MEMORY_ADDR_MODE, 0x00,
        OLED_CMD_SET_SEGMENT_REMAP | 0x01,
        OLED_CMD_SET_COM_SCAN_MODE | 0x08,
        OLED_CMD_SET_COM_PIN_MAP, (config->screen_height == 64) ? (uint8_t)0x12 : (uint8_t)0x02,
        OLED_CMD_SET_CONTRAST, 0xCF,
        OLED_CMD_SET_PRECHARGE, 0xF1,
        OLED_CMD_SET_VCOMH_DESELCT, 0x40,
        OLED_CMD_DISPLAY_RAM,
        OLED_CMD_DISPLAY_NORMAL,
        OLED_CMD_DEACTIVATE_SCROLL,
        OLED_CMD_DISPLAY_ON};
    ESP_RETURN_ON_ERROR(_ssd1306_send_cmd_list(handle, init_cmds, sizeof(init_cmds)), TAG, "Display initialization failed");

    _ssd1306_reset_dirty_area(handle);
    ssd1306_clear_buffer(handle);
    ESP_RETURN_ON_ERROR(ssd1306_update_screen(handle), TAG, "Initial screen update failed");

    *out_handle = handle;
    ESP_LOGI(TAG, "SSD1306 driver initialized successfully");
    return ESP_OK;
}

/**
 * @brief Deletes and frees resources of an SSD1306 driver instance.
 *
 * @param handle_ptr Pointer to the device handle to be deleted.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_delete(ssd1306_handle_t *handle_ptr)
{
    ESP_RETURN_ON_FALSE(handle_ptr && *handle_ptr, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ssd1306_handle_t handle = *handle_ptr;
    i2c_driver_delete(handle->config.i2c_port);
    free(handle->buffer);
    free(handle);
    *handle_ptr = NULL;
    return ESP_OK;
}

/**
 * @brief Updates the display with the contents of the internal buffer.
 *
 * @param handle SSD1306 device handle.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_update_screen(ssd1306_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    if (!handle->needs_update)
    {
        return ESP_OK;
    }

    uint8_t cmds[] = {
        OLED_CMD_SET_COLUMN_RANGE, handle->min_col, handle->max_col,
        OLED_CMD_SET_PAGE_RANGE, handle->min_page, handle->max_page};
    ESP_RETURN_ON_ERROR(_ssd1306_send_cmd_list(handle, cmds, sizeof(cmds)), TAG, "Failed to set update window");

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_RETURN_ON_FALSE(cmd, ESP_ERR_NO_MEM, TAG, "Failed to create I2C command link");

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);

    // Send data page by page directly from the main buffer
    for (uint8_t page = handle->min_page; page <= handle->max_page; ++page)
    {
        uint16_t offset = (page * handle->config.screen_width) + handle->min_col;
        uint16_t len = handle->max_col - handle->min_col + 1;
        i2c_master_write(cmd, &handle->buffer[offset], len, true);
    }

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(handle->config.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    _ssd1306_reset_dirty_area(handle);
    return ret;
}
/**
 * @brief Clears the internal buffer to black.
 *
 * @param handle SSD1306 device handle.
 */
void ssd1306_clear_buffer(ssd1306_handle_t handle)
{
    if (!handle)
        return;
    ssd1306_fill_buffer(handle, OLED_COLOR_BLACK);
}

/**
 * @brief Fills the internal buffer with a specified color.
 *
 * @param handle SSD1306 device handle.
 * @param color Color to fill the buffer.
 */
void ssd1306_fill_buffer(ssd1306_handle_t handle, ssd1306_color_t color)
{
    if (!handle)
        return;
    memset(handle->buffer, (color == OLED_COLOR_BLACK) ? 0x00 : 0xFF, handle->buffer_size);
    _ssd1306_mark_dirty(handle, 0, 0, handle->config.screen_width, handle->config.screen_height);
}

/**
 * @brief Sets uniform text size for both x and y axes.
 *
 * @param handle SSD1306 device handle.
 * @param size Text scaling factor.
 */
void ssd1306_set_text_size(ssd1306_handle_t handle, uint8_t size)
{
    if (!handle)
        return;
    ssd1306_set_text_size_custom(handle, size, size);
}

/**
 * @brief Sets custom text size for x and y axes.
 *
 * @param handle SSD1306 device handle.
 * @param size_x Text scaling factor for x-axis.
 * @param size_y Text scaling factor for y-axis.
 */
void ssd1306_set_text_size_custom(ssd1306_handle_t handle, uint8_t size_x, uint8_t size_y)
{
    if (!handle)
        return;
    handle->textsize_x = (size_x > 0) ? size_x : 1;
    handle->textsize_y = (size_y > 0) ? size_y : 1;
}

/**
 * @brief Sets the font for text rendering.
 *
 * @param handle SSD1306 device handle.
 * @param font_handle Pointer to the font structure.
 */
void ssd1306_set_font(ssd1306_handle_t handle, const ssd1306_font_handle_t *font_handle)
{
    if (!handle)
        return;
    handle->gfxFont = font_handle;
}

/**
 * @brief Sets the text cursor position.
 *
 * @param handle SSD1306 device handle.
 * @param x X-coordinate of the cursor.
 * @param y Y-coordinate of the cursor.
 */
void ssd1306_set_cursor(ssd1306_handle_t handle, int16_t x, int16_t y)
{
    if (!handle)
        return;
    handle->cursor_x = x;
    handle->cursor_y = y;
}

/**
 * @brief Sets the text color with a transparent background.
 *
 * @param handle SSD1306 device handle.
 * @param color Text color.
 */
void ssd1306_set_text_color(ssd1306_handle_t handle, ssd1306_color_t color)
{
    if (!handle)
        return;
    ssd1306_set_text_color_bg(handle, color, color);
}

/**
 * @brief Sets the text and background colors.
 *
 * @param handle SSD1306 device handle.
 * @param color Text color.
 * @param bg_color Background color.
 */
void ssd1306_set_text_color_bg(ssd1306_handle_t handle, ssd1306_color_t color, ssd1306_color_t bg_color)
{
    if (!handle)
        return;
    handle->textcolor = color;
    handle->textbgcolor = bg_color;
}

/**
 * @brief Enables or disables text wrapping.
 *
 * @param handle SSD1306 device handle.
 * @param wrap True to enable text wrapping, false to disable.
 */
void ssd1306_set_text_wrap(ssd1306_handle_t handle, bool wrap)
{
    if (!handle)
        return;
    handle->wrap = wrap;
}

/**
 * @brief Gets the current x-coordinate of the text cursor.
 *
 * @param handle SSD1306 device handle.
 * @return int16_t Current x-coordinate of the cursor.
 */
int16_t ssd1306_get_cursor_x(ssd1306_handle_t handle)
{
    return handle ? handle->cursor_x : 0;
}

/**
 * @brief Gets the current y-coordinate of the text cursor.
 *
 * @param handle SSD1306 device handle.
 * @return int16_t Current y-coordinate of the cursor.
 */
int16_t ssd1306_get_cursor_y(ssd1306_handle_t handle)
{
    return handle ? handle->cursor_y : 0;
}

/**
 * @brief Gets the width of the display screen.
 *
 * @param handle SSD1306 device handle.
 * @return int16_t Width of the display screen.
 */
int16_t ssd1306_get_screen_width(ssd1306_handle_t handle)
{
    if (!handle)
    {
        ESP_LOGE(TAG, "Invalid handle");
        return 0;
    }
    return handle->config.screen_width;
}

/**
 * @brief Gets the height of the display screen.
 *
 * @param handle SSD1306 device handle.
 * @return int16_t Height of the display screen.
 */
int16_t ssd1306_get_screen_height(ssd1306_handle_t handle)
{
    if (!handle)
    {
        ESP_LOGE(TAG, "Invalid handle");
        return 0;
    }
    return handle->config.screen_height;
}

/**
 * @brief Writes a string to the display buffer.
 *
 * @param handle SSD1306 device handle.
 * @param str Null-terminated string to write.
 * @return size_t Number of characters written.
 */
size_t ssd1306_print(ssd1306_handle_t handle, const char *str)
{
    if (!handle || !str)
        return 0;
    size_t n = 0;
    while (*str)
    {
        if (ssd1306_write(handle, *str++))
            n++;
        else
            break;
    }
    return n;
}

/**
 * @brief Writes a single character to the display buffer.
 *
 * @param handle SSD1306 device handle.
 * @param c Character to write.
 * @return size_t Number of characters written (1 on success, 0 on failure).
 */
size_t ssd1306_write(ssd1306_handle_t handle, uint8_t c)
{
    if (!handle || !handle->gfxFont)
        return 0;

    const GFXfont *font = (const GFXfont *)handle->gfxFont->font_data;

    if (c == '\n')
    {
        handle->cursor_x = 0;
        handle->cursor_y += (int16_t)handle->textsize_y * font->yAdvance;
    }
    else if (c != '\r')
    {
        if (c >= font->first && c <= font->last)
        {
            const GFXglyph *glyph = &font->glyph[c - font->first];
            uint8_t w = glyph->width;
            int16_t xo = glyph->xOffset;

            // --- Auto-adjust cursor for the first character ---
            // If the cursor is at the default (0,0), automatically adjust the
            // y-position to make the first line visible, based on the current font's height.
            if (handle->cursor_x == 0 && handle->cursor_y == 0)
            {
                int8_t y_offset = glyph->yOffset;
                if (y_offset < 0)
                {
                    handle->cursor_y = -y_offset + 1;
                }
            }
            // --- End of addition ---

            if (handle->wrap && ((handle->cursor_x + handle->textsize_x * (xo + w)) > handle->config.screen_width))
            {
                handle->cursor_x = 0;
                handle->cursor_y += (int16_t)handle->textsize_y * font->yAdvance;
            }
            ssd1306_draw_char(handle, handle->cursor_x, handle->cursor_y, c, handle->textcolor, handle->textbgcolor, handle->textsize_x, handle->textsize_y);
            handle->cursor_x += glyph->xAdvance * handle->textsize_x;
        }
    }
    return 1;
}

/**
 * @brief Draws a single character to the display buffer.
 *
 * @param handle SSD1306 device handle.
 * @param x X-coordinate to draw the character.
 * @param y Y-coordinate to draw the character.
 * @param c Character to draw.
 * @param color Text color.
 * @param bg_color Background color.
 * @param size_x X-axis scaling factor.
 * @param size_y Y-axis scaling factor.
 */
void ssd1306_draw_char(ssd1306_handle_t handle, int16_t x, int16_t y, unsigned char c, ssd1306_color_t color, ssd1306_color_t bg_color, uint8_t size_x, uint8_t size_y)
{
    if (!handle || !handle->gfxFont || handle->gfxFont->type != FONT_TYPE_GFX)
        return;

    const GFXfont *font = (const GFXfont *)handle->gfxFont->font_data;
    if (!font || c < font->first || c > font->last)
        return;

    const GFXglyph *glyph = &font->glyph[c - font->first];
    const uint8_t *bitmap = font->bitmap;
    uint16_t bo = glyph->bitmapOffset;
    uint8_t w = glyph->width, h = glyph->height;
    int8_t xo = glyph->xOffset, yo = glyph->yOffset;

    if (w == 0 || h == 0)
        return;

    uint8_t bits = 0, bit = 0;
    bool bg = (color != bg_color);

    // Mark the entire character area as dirty once
    _ssd1306_mark_dirty(handle, x + xo * size_x, y + yo * size_y, w * size_x, h * size_y);

    for (uint8_t yy = 0; yy < h; yy++)
    {
        for (uint8_t xx = 0; xx < w; xx++)
        {
            if (!(bit++ & 7))
            {
                bits = bitmap[bo++];
            }

            if (bits & 0x80)
            {
                if (size_x == 1 && size_y == 1)
                {
                    ssd1306_draw_pixel(handle, x + xo + xx, y + yo + yy, color);
                }
                else
                {
                    ssd1306_fill_rect(handle, x + (xo + xx) * size_x, y + (yo + yy) * size_y, size_x, size_y, color);
                }
            }
            else if (bg)
            {
                if (size_x == 1 && size_y == 1)
                {
                    ssd1306_draw_pixel(handle, x + xo + xx, y + yo + yy, bg_color);
                }
                else
                {
                    ssd1306_fill_rect(handle, x + (xo + xx) * size_x, y + (yo + yy) * size_y, size_x, size_y, bg_color);
                }
            }
            bits <<= 1;
        }
    }
}

/**
 * @brief Calculates the bounding box for a given string.
 *
 * @param handle SSD1306 device handle.
 * @param str String to measure.
 * @param x Starting x-coordinate.
 * @param y Starting y-coordinate.
 * @param x1 Pointer to the minimum x-coordinate.
 * @param y1 Pointer to the minimum y-coordinate.
 * @param w Pointer to the text width.
 * @param h Pointer to the text height.
 */
void ssd1306_get_text_bounds(ssd1306_handle_t handle, const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
{
    if (!handle || !str || !x1 || !y1 || !w || !h)
        return;

    *x1 = x;
    *y1 = y;
    *w = *h = 0;
    int16_t minx = handle->config.screen_width, miny = handle->config.screen_height, maxx = -1, maxy = -1;

    unsigned char c;
    while ((c = *str++))
    {
        _ssd1306_char_bounds(handle, c, &x, &y, &minx, &miny, &maxx, &maxy);
    }

    if (maxx >= minx)
    {
        *x1 = minx;
        *w = maxx - minx + 1;
    }
    if (maxy >= miny)
    {
        *y1 = miny;
        *h = maxy - miny + 1;
    }
}

/**
 * @brief Draws a single pixel at the specified coordinates.
 *
 * @param handle SSD1306 device handle.
 * @param x X-coordinate of the pixel.
 * @param y Y-coordinate of the pixel.
 * @param color Pixel color.
 */
void __attribute__((always_inline)) inline ssd1306_draw_pixel(ssd1306_handle_t handle, int16_t x, int16_t y, ssd1306_color_t color)
{
    size_t index = x + (y >> 3) * handle->config.screen_width;
    uint8_t bit_pos = y & 0x07;
    switch (color)
    {
    case OLED_COLOR_WHITE:
        handle->buffer[index] |= (1 << bit_pos);
        break;
    case OLED_COLOR_BLACK:
        handle->buffer[index] &= ~(1 << bit_pos);
        break;
    case OLED_COLOR_INVERT:
        handle->buffer[index] ^= (1 << bit_pos);
        break;
    }
    _ssd1306_mark_dirty(handle, x, y, 1, 1);
}

/**
 * @brief Draws a straight line from one point to another.
 *
 * @param handle SSD1306 device handle.
 * @param x0 Starting x-coordinate.
 * @param y0 Starting y-coordinate.
 * @param x1 Ending x-coordinate.
 * @param y1 Ending y-coordinate.
 * @param color Line color.
 */
void ssd1306_draw_line(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, ssd1306_color_t color)
{
    if (!handle)
        return;
    if (x0 == x1)
    {
        ssd1306_draw_fast_vline(handle, x0, y0, y1 - y0 + 1, color);
        return;
    }
    if (y0 == y1)
    {
        ssd1306_draw_fast_hline(handle, x0, y0, x1 - x0 + 1, color);
        return;
    }

    bool steep = _abs(y1 - y0) > _abs(x1 - x0);
    if (steep)
    {
        _swap_int16_t(x0, y0);
        _swap_int16_t(x1, y1);
    }
    if (x0 > x1)
    {
        _swap_int16_t(x0, x1);
        _swap_int16_t(y0, y1);
    }

    int16_t dx = x1 - x0, dy = _abs(y1 - y0);
    int16_t err = dx >> 1;
    int16_t ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++)
    {
        if (steep)
            ssd1306_draw_pixel(handle, y0, x0, color);
        else
            ssd1306_draw_pixel(handle, x0, y0, color);
        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}
/**
 * @brief Draws a fast vertical line.
 *
 * @param handle SSD1306 device handle.
 * @param x X-coordinate of the line.
 * @param y Starting y-coordinate of the line.
 * @param h Height of the line.
 * @param color Line color.
 */
void ssd1306_draw_fast_vline(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t h, ssd1306_color_t color)
{
    if (!handle || x < 0 || x >= handle->config.screen_width || h == 0)
        return;

    if (h < 0)
    {
        y += h;
        h = -h;
    }
    if (y >= handle->config.screen_height)
        return;

    int16_t y_end = y + h;
    if (y_end > handle->config.screen_height)
        y_end = handle->config.screen_height;
    if (y < 0)
        y = 0;

    _ssd1306_mark_dirty(handle, x, y, 1, h);

    for (int16_t i = y; i < y_end; ++i)
    {
        size_t index = x + (i >> 3) * handle->config.screen_width;
        uint8_t bit_pos = i & 0x07;
        switch (color)
        {
        case OLED_COLOR_WHITE:
            handle->buffer[index] |= (1 << bit_pos);
            break;
        case OLED_COLOR_BLACK:
            handle->buffer[index] &= ~(1 << bit_pos);
            break;
        case OLED_COLOR_INVERT:
            handle->buffer[index] ^= (1 << bit_pos);
            break;
        }
    }
}

/**
 * @brief Draws a fast horizontal line.
 *
 * @param handle SSD1306 device handle.
 * @param x Starting x-coordinate of the line.
 * @param y Y-coordinate of the line.
 * @param w Width of the line.
 * @param color Line color.
 */
void ssd1306_draw_fast_hline(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, ssd1306_color_t color)
{
    if (!handle || y < 0 || y >= handle->config.screen_height || w == 0)
        return;

    if (w < 0)
    {
        x += w;
        w = -w;
    }
    if (x >= handle->config.screen_width)
        return;

    int16_t x_end = x + w;
    if (x_end > handle->config.screen_width)
        x_end = handle->config.screen_width;
    if (x < 0)
        x = 0;

    _ssd1306_mark_dirty(handle, x, y, w, 1);

    int16_t page = y >> 3;
    uint8_t bit_mask = 1 << (y & 0x07);
    uint16_t index_start = x + page * handle->config.screen_width;
    uint16_t index_end = x_end - 1 + page * handle->config.screen_width;

    for (uint16_t i = index_start; i <= index_end; ++i)
    {
        switch (color)
        {
        case OLED_COLOR_WHITE:
            handle->buffer[i] |= bit_mask;
            break;
        case OLED_COLOR_BLACK:
            handle->buffer[i] &= ~bit_mask;
            break;
        case OLED_COLOR_INVERT:
            handle->buffer[i] ^= bit_mask;
            break;
        }
    }
}

/**
 * @brief Draws an empty rectangle.
 *
 * @param handle SSD1306 device handle.
 * @param x Top-left x-coordinate.
 * @param y Top-left y-coordinate.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @param color Rectangle color.
 */
void ssd1306_draw_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, ssd1306_color_t color)
{
    if (!handle)
        return;
    ssd1306_draw_fast_hline(handle, x, y, w, color);
    ssd1306_draw_fast_hline(handle, x, y + h - 1, w, color);
    ssd1306_draw_fast_vline(handle, x, y, h, color);
    ssd1306_draw_fast_vline(handle, x + w - 1, y, h, color);
}

/**
 * @brief Fills a rectangle with a specified color.
 *
 * @param handle SSD1306 device handle.
 * @param x Top-left x-coordinate.
 * @param y Top-left y-coordinate.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @param color Fill color.
 */
void ssd1306_fill_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, ssd1306_color_t color)
{
    if (!handle || w <= 0 || h <= 0)
        return;

    // Clipping
    if (x >= handle->config.screen_width || y >= handle->config.screen_height)
        return;
    if (x + w < 0 || y + h < 0)
        return;

    int16_t x_end = x + w;

    if (x < 0)
        x = 0;
    if (x_end > handle->config.screen_width)
        x_end = handle->config.screen_width;

    // Mark dirty area once
    _ssd1306_mark_dirty(handle, x, y, x_end - x, h);

    for (int16_t i = x; i < x_end; i++)
    {
        ssd1306_draw_fast_vline(handle, i, y, h, color);
    }
}

/**
 * @brief Draws an empty circle.
 *
 * @param handle SSD1306 device handle.
 * @param x0 Center x-coordinate.
 * @param y0 Center y-coordinate.
 * @param r Circle radius.
 * @param color Circle color.
 */
void ssd1306_draw_circle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, ssd1306_color_t color)
{
    if (!handle)
        return;
    int16_t f = 1 - r;
    int16_t ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    ssd1306_draw_pixel(handle, x0, y0 + r, color);
    ssd1306_draw_pixel(handle, x0, y0 - r, color);
    ssd1306_draw_pixel(handle, x0 + r, y0, color);
    ssd1306_draw_pixel(handle, x0 - r, y0, color);
    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        ssd1306_draw_pixel(handle, x0 + x, y0 + y, color);
        ssd1306_draw_pixel(handle, x0 - x, y0 + y, color);
        ssd1306_draw_pixel(handle, x0 + x, y0 - y, color);
        ssd1306_draw_pixel(handle, x0 - x, y0 - y, color);
        ssd1306_draw_pixel(handle, x0 + y, y0 + x, color);
        ssd1306_draw_pixel(handle, x0 - y, y0 + x, color);
        ssd1306_draw_pixel(handle, x0 + y, y0 - x, color);
        ssd1306_draw_pixel(handle, x0 - y, y0 - x, color);
    }
}

/**
 * @brief Fills a circle with a specified color.
 *
 * @param handle SSD1306 device handle.
 * @param x0 Center x-coordinate.
 * @param y0 Center y-coordinate.
 * @param r Circle radius.
 * @param color Fill color.
 */
void ssd1306_fill_circle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, ssd1306_color_t color)
{
    if (!handle)
        return;
    ssd1306_draw_fast_vline(handle, x0, y0 - r, 2 * r + 1, color);
    _ssd1306_fill_circle_helper(handle, x0, y0, r, 3, 0, color);
}

/**
 * @brief Draws an empty triangle.
 *
 * @param handle SSD1306 device handle.
 * @param x0 First point x-coordinate.
 * @param y0 First point y-coordinate.
 * @param x1 Second point x-coordinate.
 * @param y1 Second point y-coordinate.
 * @param x2 Third point x-coordinate.
 * @param y2 Third point y-coordinate.
 * @param color Triangle color.
 */
void ssd1306_draw_triangle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, ssd1306_color_t color)
{
    if (!handle)
        return;
    ssd1306_draw_line(handle, x0, y0, x1, y1, color);
    ssd1306_draw_line(handle, x1, y1, x2, y2, color);
    ssd1306_draw_line(handle, x2, y2, x0, y0, color);
}

/**
 * @brief Fills a triangle with a specified color.
 *
 * @param handle SSD1306 device handle.
 * @param x0 First point x-coordinate.
 * @param y0 First point y-coordinate.
 * @param x1 Second point x-coordinate.
 * @param y1 Second point y-coordinate.
 * @param x2 Third point x-coordinate.
 * @param y2 Third point y-coordinate.
 * @param color Fill color.
 */
void ssd1306_fill_triangle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, ssd1306_color_t color)
{
    if (!handle)
        return;
    int16_t a, b, y, last;
    if (y0 > y1)
    {
        _swap_int16_t(y0, y1);
        _swap_int16_t(x0, x1);
    }
    if (y1 > y2)
    {
        _swap_int16_t(y2, y1);
        _swap_int16_t(x2, x1);
    }
    if (y0 > y1)
    {
        _swap_int16_t(y0, y1);
        _swap_int16_t(x0, x1);
    }
    if (y0 == y2)
    {
        return;
    }

    int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;
    last = (y1 == y2) ? y1 : y1 - 1;
    for (y = y0; y <= last; y++)
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        if (a > b)
            _swap_int16_t(a, b);
        ssd1306_draw_fast_hline(handle, a, y, b - a + 1, color);
    }
    sa = (int32_t)dx12 * (y - y1);
    sb = (int32_t)dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        if (a > b)
            _swap_int16_t(a, b);
        ssd1306_draw_fast_hline(handle, a, y, b - a + 1, color);
    }
}

/**
 * @brief Draws a rounded rectangle outline.
 *
 * @param handle SSD1306 device handle.
 * @param x Top-left x-coordinate.
 * @param y Top-left y-coordinate.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @param r Corner radius.
 * @param color Outline color.
 */
void ssd1306_draw_round_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, ssd1306_color_t color)
{
    if (!handle)
        return;
    int16_t max_radius = ((w < h) ? w : h) / 2;
    if (r > max_radius)
        r = max_radius;
    ssd1306_draw_fast_hline(handle, x + r, y, w - 2 * r, color);
    ssd1306_draw_fast_hline(handle, x + r, y + h - 1, w - 2 * r, color);
    ssd1306_draw_fast_vline(handle, x, y + r, h - 2 * r, color);
    ssd1306_draw_fast_vline(handle, x + w - 1, y + r, h - 2 * r, color);
    _ssd1306_draw_circle_helper(handle, x + r, y + r, r, 1, color);
    _ssd1306_draw_circle_helper(handle, x + w - r - 1, y + r, r, 2, color);
    _ssd1306_draw_circle_helper(handle, x + w - r - 1, y + h - r - 1, r, 4, color);
    _ssd1306_draw_circle_helper(handle, x + r, y + h - r - 1, r, 8, color);
}

/**
 * @brief Fills a rounded rectangle with a specified color.
 *
 * @param handle SSD1306 device handle.
 * @param x Top-left x-coordinate.
 * @param y Top-left y-coordinate.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @param r Corner radius.
 * @param color Fill color.
 */
void ssd1306_fill_round_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, ssd1306_color_t color)
{
    if (!handle)
        return;
    int16_t max_radius = ((w < h) ? w : h) / 2;
    if (r > max_radius)
        r = max_radius;
    ssd1306_fill_rect(handle, x + r, y, w - 2 * r, h, color);
    _ssd1306_fill_circle_helper(handle, x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    _ssd1306_fill_circle_helper(handle, x + r, y + r, r, 2, h - 2 * r - 1, color);
}

/**
 * @brief Draws a monochrome bitmap with a transparent background.
 *
 * @param handle SSD1306 device handle.
 * @param x Top-left x-coordinate.
 * @param y Top-left y-coordinate.
 * @param bitmap Bitmap data array.
 * @param w Bitmap width.
 * @param h Bitmap height.
 * @param color Foreground color.
 */
void ssd1306_draw_bitmap(ssd1306_handle_t handle, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, ssd1306_color_t color)
{
    ssd1306_draw_bitmap_bg(handle, x, y, bitmap, w, h, color, color);
}

/**
 * @brief Draws a monochrome bitmap with foreground and background colors.
 *
 * @param handle SSD1306 device handle.
 * @param x Top-left x-coordinate.
 * @param y Top-left y-coordinate.
 * @param bitmap Bitmap data array.
 * @param w Bitmap width.
 * @param h Bitmap height.
 * @param color Foreground color.
 * @param bg_color Background color.
 */
void ssd1306_draw_bitmap_bg(ssd1306_handle_t handle, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, ssd1306_color_t color, ssd1306_color_t bg_color)
{
    if (!handle || !bitmap)
        return;

    // Stop if bitmap is completely off-screen
    if (x >= handle->config.screen_width || y >= handle->config.screen_height || (x + w) <= 0 || (y + h) <= 0)
    {
        return;
    }

    int16_t byte_width = (w + 7) / 8;
    uint8_t byte = 0;
    bool bg = (color != bg_color);

    // Mark the dirty area just once
    _ssd1306_mark_dirty(handle, x, y, w, h);

    for (int16_t j = 0; j < h; j++)
    {
        int16_t current_y = y + j;
        if (current_y < 0 || current_y >= handle->config.screen_height)
            continue;

        for (int16_t i = 0; i < w; i++)
        {
            int16_t current_x = x + i;
            if (current_x < 0 || current_x >= handle->config.screen_width)
                continue;

            if (i & 7)
            {
                byte <<= 1;
            }
            else
            {
                byte = bitmap[j * byte_width + i / 8];
            }

            // Determine the color for the current pixel
            ssd1306_color_t pixel_color = (byte & 0x80) ? color : bg_color;

            // Direct framebuffer manipulation (logic from draw_pixel)
            if (bg || pixel_color == color)
            {
                size_t index = current_x + (current_y >> 3) * handle->config.screen_width;
                uint8_t bit_pos = current_y & 0x07;
                switch (pixel_color)
                {
                case OLED_COLOR_WHITE:
                    handle->buffer[index] |= (1 << bit_pos);
                    break;
                case OLED_COLOR_BLACK:
                    handle->buffer[index] &= ~(1 << bit_pos);
                    break;
                case OLED_COLOR_INVERT:
                    handle->buffer[index] ^= (1 << bit_pos);
                    break;
                }
            }
        }
    }
}

/**
 * @brief Draws an XBM (X BitMap) format bitmap.
 *
 * @param handle SSD1306 device handle.
 * @param x Top-left x-coordinate.
 * @param y Top-left y-coordinate.
 * @param bitmap XBM data array.
 * @param w Bitmap width.
 * @param h Bitmap height.
 * @param color Foreground color.
 */
void ssd1306_draw_xbitmap(ssd1306_handle_t handle, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, ssd1306_color_t color)
{
    if (!handle || !bitmap)
        return;
    int16_t byteWidth = (w + 7) / 8, byte = 0;
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                byte >>= 1;
            else
                byte = bitmap[j * byteWidth + i / 8];
            if (byte & 0x01)
                ssd1306_draw_pixel(handle, x + i, y, color);
        }
    }
}

/**
 * @brief Inverts the display colors.
 *
 * @param handle SSD1306 device handle.
 * @param invert True to invert the display, false to restore normal mode.
 */
void ssd1306_invert_display(ssd1306_handle_t handle, bool invert)
{
    _ssd1306_send_cmd_list(handle, (uint8_t[]){invert ? OLED_CMD_INVERTDISPLAY : OLED_CMD_DISPLAY_NORMAL}, 1);
}

/**
 * @brief Sets the display contrast.
 *
 * @param handle SSD1306 device handle.
 * @param contrast Contrast value (0-255).
 */
void ssd1306_set_contrast(ssd1306_handle_t handle, uint8_t contrast)
{
    _ssd1306_send_cmd_list(handle, (uint8_t[]){OLED_CMD_SET_CONTRAST, contrast}, 2);
}

/**
 * @brief Stops any active scrolling effect.
 *
 * @param handle SSD1306 device handle.
 */
void ssd1306_stop_scroll(ssd1306_handle_t handle)
{
    if (handle)
        _ssd1306_send_cmd_list(handle, (uint8_t[]){OLED_CMD_DEACTIVATE_SCROLL}, 1);
}

/**
 * @brief Internal helper to start a hardware scrolling effect.
 *
 * @param handle SSD1306 device handle.
 * @param scroll_cmd Scroll command (right or left).
 * @param start_page Starting page for scrolling (0-7).
 * @param end_page Ending page for scrolling (0-7).
 */
static void _ssd1306_start_scroll(ssd1306_handle_t handle, uint8_t scroll_cmd, uint8_t start_page, uint8_t end_page)
{
    if (!handle || start_page > 7 || end_page > 7 || start_page > end_page)
        return;
    ssd1306_stop_scroll(handle);
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t cmds[] = {scroll_cmd, 0x00, start_page, 0x00, end_page, 0x00, 0xFF};
    _ssd1306_send_cmd_list(handle, cmds, sizeof(cmds));
    _ssd1306_send_cmd_list(handle, (uint8_t[]){OLED_CMD_ACTIVATE_SCROLL}, 1);
}

/**
 * @brief Starts horizontal scrolling to the right.
 *
 * @param handle SSD1306 device handle.
 * @param start_page Starting page for scrolling (0-7).
 * @param end_page Ending page for scrolling (0-7).
 */
void ssd1306_start_scroll_right(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page)
{
    _ssd1306_start_scroll(handle, OLED_CMD_RIGHT_HORIZONTAL_SCROLL, start_page, end_page);
}

/**
 * @brief Starts horizontal scrolling to the left.
 *
 * @param handle SSD1306 device handle.
 * @param start_page Starting page for scrolling (0-7).
 * @param end_page Ending page for scrolling (0-7).
 */
void ssd1306_start_scroll_left(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page)
{
    _ssd1306_start_scroll(handle, OLED_CMD_LEFT_HORIZONTAL_SCROLL, start_page, end_page);
}

/**
 * @brief Turns on the display.
 *
 * @param handle SSD1306 device handle.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_display_on(ssd1306_handle_t handle)
{
    return _ssd1306_send_cmd_list(handle, (uint8_t[]){OLED_CMD_DISPLAY_ON}, 1);
}

/**
 * @brief Turns off the display.
 *
 * @param handle SSD1306 device handle.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_display_off(ssd1306_handle_t handle)
{
    return _ssd1306_send_cmd_list(handle, (uint8_t[]){OLED_CMD_DISPLAY_OFF}, 1);
}

/**
 * @brief Internal helper to configure and start diagonal hardware scrolling.
 *
 * @param handle SSD1306 device handle.
 * @param scroll_cmd Scroll command (0x29 for right, 0x2A for left).
 * @param start_page Starting page for scrolling (0-7).
 * @param end_page Ending page for scrolling (0-7).
 * @param offset Number of vertical lines to scroll per step (1-63).
 * @param speed Horizontal scroll speed (0-7, 0 is fastest).
 */
static void _ssd1306_start_diag_scroll(ssd1306_handle_t handle, uint8_t scroll_cmd, uint8_t start_page, uint8_t end_page, uint8_t offset, uint8_t speed)
{
    if (!handle || start_page > 7 || end_page > 7 || start_page > end_page || speed > 7 || offset == 0 || offset > 63)
        return;

    ssd1306_stop_scroll(handle);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t setup_cmds[] = {
        OLED_CMD_SET_VERTICAL_SCROLL_AREA,
        0,
        handle->config.screen_height};
    _ssd1306_send_cmd_list(handle, setup_cmds, sizeof(setup_cmds));

    uint8_t scroll_cmds[] = {
        scroll_cmd,
        0x00,
        start_page,
        speed,
        end_page,
        offset};
    _ssd1306_send_cmd_list(handle, scroll_cmds, sizeof(scroll_cmds));

    _ssd1306_send_cmd_list(handle, (uint8_t[]){OLED_CMD_ACTIVATE_SCROLL}, 1);
}

/**
 * @brief Starts diagonal scrolling downward to the right.
 *
 * @param handle SSD1306 device handle.
 * @param start_page Starting page for scrolling (0-7).
 * @param end_page Ending page for scrolling (0-7).
 * @param offset Number of lines to scroll downward per step (1-63).
 * @param speed Horizontal scroll speed (0-7, 0 is fastest).
 */
void ssd1306_start_scroll_diag_right_down(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page, uint8_t offset, uint8_t speed)
{
    _ssd1306_start_diag_scroll(handle, OLED_CMD_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL, start_page, end_page, offset, speed);
}

/**
 * @brief Starts diagonal scrolling upward to the left.
 *
 * @param handle SSD1306 device handle.
 * @param start_page Starting page for scrolling (0-7).
 * @param end_page Ending page for scrolling (0-7).
 * @param offset Number of lines to scroll upward per step (1-63).
 * @param speed Horizontal scroll speed (0-7, 0 is fastest).
 */
void ssd1306_start_scroll_diag_left_up(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page, uint8_t offset, uint8_t speed)
{
    uint8_t true_offset = handle->config.screen_height - offset;
    _ssd1306_start_diag_scroll(handle, OLED_CMD_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL, start_page, end_page, true_offset, speed);
}

/**
 * @brief Draws an arc at the specified coordinates.
 *
 * @param handle SSD1306 device handle.
 * @param x0 Center x-coordinate.
 * @param y0 Center y-coordinate.
 * @param r Arc radius.
 * @param start_angle Starting angle in degrees (0 degrees = 3 o'clock).
 * @param end_angle Ending angle in degrees.
 * @param color Arc color.
 */
void ssd1306_draw_arc(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, int16_t start_angle, int16_t end_angle, ssd1306_color_t color)
{
    if (!handle || r <= 0)
        return;

    while (end_angle < start_angle)
    {
        end_angle += 360;
    }

    for (int16_t angle = start_angle; angle <= end_angle; angle++)
    {
        float rad = angle * M_PI / 180.0f;
        int16_t x = x0 + (int16_t)(r * cosf(rad));
        int16_t y = y0 + (int16_t)(r * sinf(rad));
        ssd1306_draw_pixel(handle, x, y, color);
    }
}

/**
 * @brief Draws a polyline (connected line segments).
 *
 * @param handle SSD1306 device handle.
 * @param x Pointer to an array of x-coordinates.
 * @param y Pointer to an array of y-coordinates.
 * @param num_points Number of points in the polyline.
 * @param color Line color.
 */
void ssd1306_draw_polyline(ssd1306_handle_t handle, const int16_t *x, const int16_t *y, uint8_t num_points, ssd1306_color_t color)
{
    if (!handle || !x || !y || num_points < 2)
        return;

    for (uint8_t i = 0; i < num_points - 1; i++)
    {
        ssd1306_draw_line(handle, x[i], y[i], x[i + 1], y[i + 1], color);
    }
}

/**
 * @brief Shifts the framebuffer contents by the specified offsets.
 *
 * @param handle SSD1306 device handle.
 * @param dx Horizontal shift (positive = right, negative = left).
 * @param dy Vertical shift (positive = down, negative = up).
 * @param wrap If true, pixels exiting one side reappear on the opposite side.
 */
void ssd1306_shift_framebuffer(ssd1306_handle_t handle, int16_t dx, int16_t dy, bool wrap)
{
    if (!handle || !handle->buffer || (dx == 0 && dy == 0))
    {
        return;
    }

    const int16_t width = handle->config.screen_width;
    const int16_t height = handle->config.screen_height;

    // --- Optimasi Kasus Umum: Pergeseran Horizontal Cepat Tanpa Wrap ---
    if (dy == 0 && !wrap)
    {
        int16_t shift_abs = (dx > 0) ? dx : -dx;
        if (shift_abs < width)
        {
            if (dx > 0) // Geser ke kanan
            {
                // Pindahkan blok memori, sisakan ruang kosong di kiri
                memmove(handle->buffer + dx, handle->buffer, handle->buffer_size - dx);
                memset(handle->buffer, 0, dx); // Bersihkan area yang baru kosong
            }
            else // Geser ke kiri
            {
                // Pindahkan blok memori, sisakan ruang kosong di kanan
                memmove(handle->buffer, handle->buffer + shift_abs, handle->buffer_size - shift_abs);
                memset(handle->buffer + (handle->buffer_size - shift_abs), 0, shift_abs); // Bersihkan area yang baru kosong
            }
        }
        else // Jika pergeseran lebih besar dari lebar layar, cukup bersihkan
        {
            memset(handle->buffer, 0, handle->buffer_size);
        }
        _ssd1306_mark_dirty(handle, 0, 0, width, height);
        return;
    }

    // --- Implementasi Lanjutan untuk Kasus Kompleks (Vertikal atau Wrap) ---

    // Buffer statis untuk menghindari fragmentasi heap. Ukuran untuk layar 128x64.
    static uint8_t temp_buffer[128 * 64 / 8];
    if (handle->buffer_size > sizeof(temp_buffer))
    {
        ESP_LOGE(TAG, "Buffer size exceeds temp_buffer capacity for shift operation");
        return;
    }

    // 1. Salin framebuffer asli ke buffer sementara
    memcpy(temp_buffer, handle->buffer, handle->buffer_size);

    // 2. Bersihkan framebuffer utama untuk diisi data baru
    memset(handle->buffer, 0x00, handle->buffer_size);

    // 3. Iterasi melalui setiap piksel dari framebuffer ASLI (sumber)
    for (int16_t src_y = 0; src_y < height; src_y++)
    {
        for (int16_t src_x = 0; src_x < width; src_x++)
        {
            // Cek apakah piksel sumber 'on' (putih)
            size_t src_idx = src_x + (src_y >> 3) * width;
            uint8_t src_bit_pos = src_y & 0x07;

            if ((temp_buffer[src_idx] >> src_bit_pos) & 1)
            {
                // Jika 'on', hitung posisi barunya (tujuan)
                int16_t dst_x = src_x + dx;
                int16_t dst_y = src_y + dy;

                // Terapkan wrapping jika diaktifkan
                if (wrap)
                {
                    // Modulo yang aman untuk nilai negatif
                    dst_x = (dst_x % width + width) % width;
                    dst_y = (dst_y % height + height) % height;
                }

                // Gambar piksel di posisi tujuan jika masih di dalam layar
                if (dst_x >= 0 && dst_x < width && dst_y >= 0 && dst_y < height)
                {
                    // Manipulasi langsung buffer tujuan (inlined draw_pixel)
                    size_t dst_idx = dst_x + (dst_y >> 3) * width;
                    uint8_t dst_bit_pos = dst_y & 0x07;
                    handle->buffer[dst_idx] |= (1 << dst_bit_pos);
                }
            }
        }
    }

    // 4. Tandai seluruh layar sebagai 'dirty' untuk di-update
    _ssd1306_mark_dirty(handle, 0, 0, width, height);
}

/**
 * @brief Sets the hardware scan orientation (flip and remap).
 *
 * @warning This function does not rotate the library's coordinate system. It is
 * intended for flipping the display 180 degrees or correcting an inverted mount.
 * @param handle SSD1306 device handle.
 * @param rotation Orientation: 0 (normal), 1 (horizontal flip), 2 (vertical flip), 3 (180-degree flip).
 */
void ssd1306_set_orientation(ssd1306_handle_t handle, uint8_t rotation)
{
    if (!handle)
        return;

    uint8_t seg_cmd = (rotation & 1) ? OLED_CMD_SET_SEGMENT_REMAP | 0x01 : OLED_CMD_SET_SEGMENT_REMAP | 0x00;
    uint8_t com_cmd = (rotation & 2) ? OLED_CMD_SET_COM_SCAN_MODE | 0x08 : OLED_CMD_SET_COM_SCAN_MODE | 0x00;

    _ssd1306_send_cmd_list(handle, &seg_cmd, 1);
    _ssd1306_send_cmd_list(handle, &com_cmd, 1);

    // Reset internal coordinate system to match new orientation
    switch (rotation)
    {
    case 0: // Normal (0 degrees)
        handle->cursor_x = 0;
        handle->cursor_y = 0;
        break;
    case 1: // Horizontal flip
        handle->cursor_x = handle->config.screen_width - 1 - handle->cursor_x;
        break;
    case 2: // Vertical flip
        handle->cursor_y = handle->config.screen_height - 1 - handle->cursor_y;
        break;
    case 3: // 180-degree flip
        handle->cursor_x = handle->config.screen_width - 1 - handle->cursor_x;
        handle->cursor_y = handle->config.screen_height - 1 - handle->cursor_y;
        break;
    }

    // Ensure display is updated to reflect the new orientation
    ssd1306_clear_buffer(handle);
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
}

/**
 * @brief Sets the display start line in RAM.
 *
 * @param handle SSD1306 device handle.
 * @param line Starting line to display (0-63).
 */
void ssd1306_set_display_start_line(ssd1306_handle_t handle, uint8_t line)
{
    if (!handle || line > 63)
        return;

    uint8_t cmd = OLED_CMD_SET_DISPLAY_START_LINE | line;
    _ssd1306_send_cmd_list(handle, &cmd, 1);
}

/**
 * @brief Draws horizontally centered text at a specified y-coordinate.
 *
 * @param handle SSD1306 device handle.
 * @param text Text string to draw.
 * @param y Y-coordinate for the text baseline.
 */
void ssd1306_print_centered_h(ssd1306_handle_t handle, const char *text, int16_t y)
{
    if (!handle || !text)
        return;

    int16_t x1, y1;
    uint16_t w, h;
    ssd1306_get_text_bounds(handle, text, 0, 0, &x1, &y1, &w, &h);

    int16_t x = (handle->config.screen_width - w) / 2;

    ssd1306_set_cursor(handle, x, y);
    ssd1306_print(handle, text);
}

/**
 * @brief Draws text centered both horizontally and vertically on the screen.
 *
 * @param handle SSD1306 device handle.
 * @param text Text string to draw.
 */
void ssd1306_print_screen_center(ssd1306_handle_t handle, const char *text)
{
    if (!handle || !text)
        return;

    int16_t x1, y1;
    uint16_t w, h;
    ssd1306_get_text_bounds(handle, text, 0, 0, &x1, &y1, &w, &h);

    int16_t x = (handle->config.screen_width - w) / 2;
    int16_t y = (handle->config.screen_height + h) / 2;

    ssd1306_set_cursor(handle, x, y);
    ssd1306_print(handle, text);
}

/**
 * @brief Draws horizontally  text at a specified y-coordinate.
 *
 * @param handle SSD1306 device handle.
 * @param text Text string to draw.
 * @param y Y-coordinate for the text baseline.
 */
void ssd1306_print_h(ssd1306_handle_t handle, const char *text, int16_t y)
{
    if (!handle || !text)
        return;

    ssd1306_set_cursor(handle, 0, y);
    ssd1306_print(handle, text);
}