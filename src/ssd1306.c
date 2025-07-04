/**
 * @file      ssd1306.c
 * @author    Muhamad Arif Hidayat
 * @brief     Implementation of the I2C-based SSD1306 OLED display driver.
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


// Helper macros for math operations and variable swapping.
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    } /**< Swaps two int16_t variables. */
#define _abs(a) ((a) < 0 ? -(a) : (a))     /**< Computes the absolute value of a number. */
#define _min(a, b) (((a) < (b)) ? (a) : (b)) /**< Returns the minimum of two values. */

// I2C Control Byte definitions for SSD1306
#define OLED_CONTROL_BYTE_CMD_STREAM 0x00  /**< Control byte for a command stream. */
#define OLED_CONTROL_BYTE_DATA_STREAM 0x40 /**< Control byte for a data stream. */

// SSD1306 Command Definitions
#define OLED_CMD_SET_CONTRAST 0x81                   /**< Sets display contrast. */
#define OLED_CMD_DISPLAY_RAM 0xA4                      /**< Resumes display from RAM content. */
#define OLED_CMD_DISPLAY_NORMAL 0xA6                   /**< Sets normal display mode. */
#define OLED_CMD_INVERTDISPLAY 0xA7                    /**< Inverts display colors. */
#define OLED_CMD_DISPLAY_OFF 0xAE                      /**< Turns off the display. */
#define OLED_CMD_DISPLAY_ON 0xAF                       /**< Turns on the display. */
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20             /**< Sets memory addressing mode. */
#define OLED_CMD_SET_COLUMN_RANGE 0x21                 /**< Sets column address range. */
#define OLED_CMD_SET_PAGE_RANGE 0x22                   /**< Sets page address range. */
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40           /**< Sets display start line. */
#define OLED_CMD_SET_SEGMENT_REMAP 0xA0                  /**< Sets segment remapping (horizontal flip). */
#define OLED_CMD_SET_MUX_RATIO 0xA8                    /**< Sets multiplex ratio. */
#define OLED_CMD_SET_COM_SCAN_MODE 0xC0                  /**< Sets COM scan direction (vertical flip). */
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3               /**< Sets display offset. */
#define OLED_CMD_SET_DISPLAY_CLK_DIV 0xD5              /**< Sets display clock divider. */
#define OLED_CMD_SET_PRECHARGE 0xD9                    /**< Sets pre-charge period. */
#define OLED_CMD_SET_COM_PIN_MAP 0xDA                    /**< Sets COM pin configuration. */
#define OLED_CMD_SET_VCOMH_DESELCT 0xDB                /**< Sets VCOMH deselect level. */
#define OLED_CMD_SET_CHARGE_PUMP 0x8D                  /**< Sets charge pump configuration. */
#define OLED_CMD_DEACTIVATE_SCROLL 0x2E                /**< Deactivates scrolling. */
#define OLED_CMD_ACTIVATE_SCROLL 0x2F                  /**< Activates scrolling. */
#define OLED_CMD_RIGHT_HORIZONTAL_SCROLL 0x26          /**< Right horizontal scroll. */
#define OLED_CMD_LEFT_HORIZONTAL_SCROLL 0x27           /**< Left horizontal scroll. */
#define OLED_CMD_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 /**< Vertical and right horizontal scroll. */
#define OLED_CMD_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A  /**< Vertical and left horizontal scroll. */
#define OLED_CMD_SET_VERTICAL_SCROLL_AREA 0xA3           /**< Sets vertical scroll area. */


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
    bool needs_update; /**< Flag indicating if an update is required. */
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


/**
 * @brief Sends a list of commands to the SSD1306 display via I2C.
 *
 * @param handle SSD1306 device handle.
 * @param cmd_list Array of commands to send.
 * @param size Size of the command array in bytes.
 * @return esp_err_t Operation status.
 */
#define I2C_CMD_BUFFER_SIZE 256 // Static buffer size for the I2C link. Adjust if needed for very long command lists.

static uint8_t i2c_cmd_buffer[I2C_CMD_BUFFER_SIZE]; // Static buffer for the I2C link to avoid repeated dynamic memory allocation.
static i2c_cmd_handle_t cmd_cache = NULL;           // Cache the I2C link handle for reuse, improving efficiency.

static esp_err_t _ssd1306_send_cmd_list(ssd1306_handle_t handle, const uint8_t *cmd_list, size_t size)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    // Initialize the static I2C link if this is the first call.
    // This is an optimization to avoid recreating the handle every time.
    if (!cmd_cache)
    {
        cmd_cache = i2c_cmd_link_create_static(i2c_cmd_buffer, I2C_CMD_BUFFER_SIZE);
        ESP_RETURN_ON_FALSE(cmd_cache, ESP_ERR_NO_MEM, TAG, "Failed to create static I2C command link");
    }

    // Reset and recreate the link from the existing static buffer.
    // This is faster than `i2c_cmd_link_create()` followed by `i2c_cmd_link_delete()`.
    i2c_cmd_link_delete(cmd_cache);
    cmd_cache = i2c_cmd_link_create_static(i2c_cmd_buffer, I2C_CMD_BUFFER_SIZE);

    // Build the I2C transmission sequence.
    i2c_master_start(cmd_cache);
    i2c_master_write_byte(cmd_cache, (handle->config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_cache, OLED_CONTROL_BYTE_CMD_STREAM, true); // Indicate that the following data is a command.
    i2c_master_write(cmd_cache, cmd_list, size, true);
    i2c_master_stop(cmd_cache);

    esp_err_t ret = ESP_FAIL;
    // Retry mechanism to handle potential temporary glitches on the I2C bus.
    for (int retry = 0; retry < 3; retry++)
    { 
        ret = i2c_master_cmd_begin(handle->config.i2c_port, cmd_cache, pdMS_TO_TICKS(100));
        if (ret == ESP_OK)
            break; // If successful, exit the loop.
        vTaskDelay(pdMS_TO_TICKS(10)); // Wait a short time before retrying.
    }

    // `cmd_cache` is not deleted so it can be reused on the next call.
    return ret;
}

/**
 * @brief Resets the dirty area for partial updates.
 * This is called after a screen update is successful.
 * @param handle SSD1306 device handle.
 */
static void _ssd1306_reset_dirty_area(ssd1306_handle_t handle)
{
    handle->needs_update = false;
    // Reset the area boundaries to "inverted" values so that any new pixel drawn
    // will automatically set the correct boundaries.
    handle->min_col = handle->config.screen_width;
    handle->max_col = 0;
    handle->min_page = handle->config.screen_height / 8;
    handle->max_page = 0;
}

/**
 * @brief Marks an area as dirty for partial updates.
 * Whenever a drawing operation occurs, the affected area is marked.
 * The screen update will then only refresh the combined dirty area.
 *
 * @param handle SSD1306 device handle.
 * @param x Starting x-coordinate.
 * @param y Starting y-coordinate.
 * @param w Width of the area.
 * @param h Height of the area.
 */
static void _ssd1306_mark_dirty(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h)
{
    // Ignore if completely off-screen.
    if (!handle || x >= handle->config.screen_width || y >= handle->config.screen_height ||
        x + w <= 0 || y + h <= 0)
        return;

    // Clip the area to the screen dimensions.
    int16_t x1 = x > 0 ? x : 0;
    int16_t y1 = y > 0 ? y : 0;
    int16_t x2 = (x + w - 1) < handle->config.screen_width ? (x + w - 1) : (handle->config.screen_width - 1);
    int16_t y2 = (y + h - 1) < handle->config.screen_height ? (y + h - 1) : (handle->config.screen_height - 1);

    // Convert Y coordinates to "page" units. One page is 8 pixel rows.
    uint8_t page1 = y1 >> 3; // y1 / 8
    uint8_t page2 = y2 >> 3; // y2 / 8

    // Update the dirty area boundaries. `min` will shrink and `max` will grow.
    handle->min_col = x1 < handle->min_col ? x1 : handle->min_col;
    handle->max_col = x2 > handle->max_col ? x2 : handle->max_col;
    handle->min_page = page1 < handle->min_page ? page1 : handle->min_page;
    handle->max_page = page2 > handle->max_page ? page2 : handle->max_page;
    handle->needs_update = true; // Flag that a pending update exists.
}


/**
 * @brief Helper function to draw a circle quadrant.
 * Used by `ssd1306_draw_round_rect`.
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
    // Midpoint circle algorithm (Bresenham's)
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
        // Draw pixels in the appropriate quadrant based on the bitmask.
        if (cornername & 0x4) // Bottom right
        {
            ssd1306_draw_pixel(handle, x0 + x, y0 + y, color);
            ssd1306_draw_pixel(handle, x0 + y, y0 + x, color);
        }
        if (cornername & 0x2) // Top right
        {
            ssd1306_draw_pixel(handle, x0 + x, y0 - y, color);
            ssd1306_draw_pixel(handle, x0 + y, y0 - x, color);
        }
        if (cornername & 0x8) // Bottom left
        {
            ssd1306_draw_pixel(handle, x0 - y, y0 + x, color);
            ssd1306_draw_pixel(handle, x0 - x, y0 + y, color);
        }
        if (cornername & 0x1) // Top left
        {
            ssd1306_draw_pixel(handle, x0 - y, y0 - x, color);
            ssd1306_draw_pixel(handle, x0 - x, y0 - y, color);
        }
    }
}

/**
 * @brief Helper function to fill a circle quadrant.
 * Used by `ssd1306_fill_circle` and `ssd1306_fill_round_rect`.
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
    // Midpoint circle algorithm (Bresenham's) adapted for filling.
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    int16_t px = x;
    int16_t py = y;

    delta++;

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
        
        // Draw vertical lines to fill the area between points on the circle's arc.
        if (x < (y + 1))
        {
            if (corners & 1) // Right side
                ssd1306_draw_fast_vline(handle, x0 + x, y0 - y, 2 * y + delta, color);
            if (corners & 2) // Left side
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
 * This function does not draw, it only calculates dimensions and position.
 *
 * @param handle SSD1306 device handle.
 * @param c Character to measure.
 * @param x Pointer to the current cursor x-coordinate (will be updated).
 * @param y Pointer to the current cursor y-coordinate (will be updated).
 * @param minx Pointer to the minimum x-coordinate (output).
 * @param miny Pointer to the minimum y-coordinate (output).
 * @param maxx Pointer to the maximum x-coordinate (output).
 * @param maxy Pointer to the maximum y-coordinate (output).
 */
static void _ssd1306_char_bounds(ssd1306_handle_t handle, unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy)
{
    if (!handle->gfxFont)
        return;

    // Handle newline character.
    if (c == '\n')
    {
        *x = 0; // Move cursor x to the beginning of the line.
        const GFXfont *font = (const GFXfont *)handle->gfxFont->font_data;
        *y += handle->textsize_y * font->yAdvance; // Move cursor y to the next line.
    }
    else if (c != '\r') // Ignore carriage return.
    {
        const GFXfont *font = (const GFXfont *)handle->gfxFont->font_data;
        // Ensure character is within the font range.
        if (c >= font->first && c <= font->last)
        {
            const GFXglyph *glyph = &font->glyph[c - font->first]; // Get glyph data for the character.
            uint8_t gw = glyph->width, gh = glyph->height, xa = glyph->xAdvance;
            int8_t xo = glyph->xOffset, yo = glyph->yOffset;

            // Handle text wrapping if the character exceeds screen width.
            if (handle->wrap && ((*x + ((int16_t)xo + gw) * handle->textsize_x) > handle->config.screen_width))
            {
                *x = 0;
                *y += handle->textsize_y * font->yAdvance;
            }
            // Calculate absolute bounding box coordinates.
            int16_t x1 = *x + xo * handle->textsize_x;
            int16_t y1 = *y + yo * handle->textsize_y;
            int16_t x2 = x1 + gw * handle->textsize_x - 1;
            int16_t y2 = y1 + gh * handle->textsize_y - 1;

            // Update the global min/max boundaries.
            if (x1 < *minx) *minx = x1;
            if (y1 < *miny) *miny = y1;
            if (x2 > *maxx) *maxx = x2;
            if (y2 > *maxy) *maxy = y2;

            // Advance cursor x for the next character.
            *x += xa * handle->textsize_x;
        }
    }
}


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

    // Allocate memory for the driver handle.
    ssd1306_handle_t handle = calloc(1, sizeof(struct ssd1306_dev_t));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "Failed to allocate handle");

    handle->config = *config;
    // Allocate memory for the framebuffer. Size is (width * height) / 8 because 1 byte represents 8 vertical pixels.
    handle->buffer_size = (config->screen_width * config->screen_height) / 8;
    handle->buffer = malloc(handle->buffer_size);
    if (!handle->buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    // Initialize default graphics state.
    handle->cursor_x = 0;
    handle->cursor_y = 0;
    handle->textsize_x = 1;
    handle->textsize_y = 1;
    handle->textcolor = OLED_COLOR_WHITE;
    handle->textbgcolor = OLED_COLOR_BLACK;
    handle->wrap = true;
    handle->gfxFont = &FONT_5x7; // Set default font.

    // Configure the I2C master driver.
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

    // Perform a hardware reset on the display if the RST pin is defined.
    if (handle->config.rst_pin != -1)
    {
        gpio_set_direction(handle->config.rst_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(handle->config.rst_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(50)); // Hold reset for 50ms.
        gpio_set_level(handle->config.rst_pin, 1);
    }

    // Initialization command sequence to configure the SSD1306 controller.
    const uint8_t init_cmds[] = {
        OLED_CMD_DISPLAY_OFF,                 // Turn display off during setup
        OLED_CMD_SET_DISPLAY_CLK_DIV, 0x80,   // Set clock
        OLED_CMD_SET_MUX_RATIO, (uint8_t)(config->screen_height - 1), // Set based on screen height
        OLED_CMD_SET_DISPLAY_OFFSET, 0x00,    // No offset
        OLED_CMD_SET_DISPLAY_START_LINE | 0x00, // Start at line 0
        OLED_CMD_SET_CHARGE_PUMP, 0x14,       // Enable charge pump
        OLED_CMD_SET_MEMORY_ADDR_MODE, 0x00,  // Horizontal addressing mode
        OLED_CMD_SET_SEGMENT_REMAP | 0x01,    // Remap segment (column 127 is at SEG0)
        OLED_CMD_SET_COM_SCAN_MODE | 0x08,    // Remap COM (scan from COM[N-1] to COM0)
        OLED_CMD_SET_COM_PIN_MAP, (config->screen_height == 64) ? (uint8_t)0x12 : (uint8_t)0x02, // COM pin config
        OLED_CMD_SET_CONTRAST, 0xCF,           // Set contrast
        OLED_CMD_SET_PRECHARGE, 0xF1,         // Set pre-charge period
        OLED_CMD_SET_VCOMH_DESELCT, 0x40,     // Set VCOMH level
        OLED_CMD_DISPLAY_RAM,                 // Display RAM content
        OLED_CMD_DISPLAY_NORMAL,              // Normal display mode (not inverted)
        OLED_CMD_DEACTIVATE_SCROLL,           // Deactivate scrolling
        OLED_CMD_DISPLAY_ON                   // Turn display on
    };
    ESP_RETURN_ON_ERROR(_ssd1306_send_cmd_list(handle, init_cmds, sizeof(init_cmds)), TAG, "Display initialization failed");

    // Prepare driver for use.
    _ssd1306_reset_dirty_area(handle);
    ssd1306_clear_buffer(handle);
    ESP_RETURN_ON_ERROR(ssd1306_update_screen(handle), TAG, "Initial screen update failed");

    *out_handle = handle;
    ESP_LOGI(TAG, "SSD1306 driver initialized successfully");
    return ESP_OK;
}

/**
 * @brief Deletes an SSD1306 driver instance and frees resources.
 *
 * @param handle_ptr Pointer to the device handle to be deleted.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_delete(ssd1306_handle_t *handle_ptr)
{
    ESP_RETURN_ON_FALSE(handle_ptr && *handle_ptr, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ssd1306_handle_t handle = *handle_ptr;
    i2c_driver_delete(handle->config.i2c_port); // Delete the I2C driver.
    free(handle->buffer);                      // Free the framebuffer memory.
    free(handle);                              // Free the handle memory.
    *handle_ptr = NULL;                        // Set pointer to NULL to prevent dangling pointers.
    return ESP_OK;
}

/**
 * @brief Updates the display with the contents of the internal buffer.
 * This function only sends the changed area (dirty area) for efficiency.
 *
 * @param handle SSD1306 device handle.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_update_screen(ssd1306_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    // If nothing has changed, do nothing.
    if (!handle->needs_update)
    {
        return ESP_OK;
    }

    // Set the update "window" on the display, corresponding to the dirty area.
    uint8_t cmds[] = {
        OLED_CMD_SET_COLUMN_RANGE, handle->min_col, handle->max_col,
        OLED_CMD_SET_PAGE_RANGE, handle->min_page, handle->max_page
    };
    ESP_RETURN_ON_ERROR(_ssd1306_send_cmd_list(handle, cmds, sizeof(cmds)), TAG, "Failed to set update window");

    // Create a new I2C link to send the framebuffer data.
    // Cache is not used here as data transfers can be larger than the static buffer.
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_RETURN_ON_FALSE(cmd, ESP_ERR_NO_MEM, TAG, "Failed to create I2C command link");

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true); // Indicate that the following is image data.

    // Send data page by page, only for columns within the dirty area.
    for (uint8_t page = handle->min_page; page <= handle->max_page; ++page)
    {
        // Calculate the starting offset for the data of the current page and column.
        uint16_t offset = (page * handle->config.screen_width) + handle->min_col;
        uint16_t len = handle->max_col - handle->min_col + 1;
        i2c_master_write(cmd, &handle->buffer[offset], len, true);
    }

    i2c_master_stop(cmd);

    // Send the data to the I2C bus.
    esp_err_t ret = i2c_master_cmd_begin(handle->config.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd); // Free the link after use.

    // After a successful update, reset the dirty area.
    _ssd1306_reset_dirty_area(handle);
    return ret;
}

/**
 * @brief Clears the internal buffer to black (pixels off).
 *
 * @param handle SSD1306 device handle.
 */
void ssd1306_clear_buffer(ssd1306_handle_t handle)
{
    if (!handle)
        return;
    // Call fill_buffer with black color.
    ssd1306_fill_buffer(handle, OLED_COLOR_BLACK);
}

/**
 * @brief Fills the entire internal buffer with a specified color.
 *
 * @param handle SSD1306 device handle.
 * @param color Color to fill the buffer (OLED_COLOR_BLACK or OLED_COLOR_WHITE).
 */
void ssd1306_fill_buffer(ssd1306_handle_t handle, ssd1306_color_t color)
{
    if (!handle)
        return;
    // Use memset for a fast buffer fill. 0x00 for black, 0xFF for white.
    memset(handle->buffer, (color == OLED_COLOR_BLACK) ? 0x00 : 0xFF, handle->buffer_size);
    // Mark the entire screen as dirty since it has all been changed.
    _ssd1306_mark_dirty(handle, 0, 0, handle->config.screen_width, handle->config.screen_height);
}


/**
 * @brief Sets a uniform text size for both x and y axes.
 *
 * @param handle SSD1306 device handle.
 * @param size Text scaling factor (e.g., 1 for normal, 2 for twice as large).
 */
void ssd1306_set_text_size(ssd1306_handle_t handle, uint8_t size)
{
    if (!handle)
        return;
    // Call the custom function with the same value for x and y.
    ssd1306_set_text_size_custom(handle, size, size);
}

/**
 * @brief Sets custom text size for x and y axes separately.
 *
 * @param handle SSD1306 device handle.
 * @param size_x Text scaling factor for the x-axis.
 * @param size_y Text scaling factor for the y-axis.
 */
void ssd1306_set_text_size_custom(ssd1306_handle_t handle, uint8_t size_x, uint8_t size_y)
{
    if (!handle)
        return;
    // Ensure minimum size is 1.
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
 * @param x Cursor x-coordinate.
 * @param y Cursor y-coordinate.
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
 * The background is not redrawn; only the text pixels are modified.
 *
 * @param handle SSD1306 device handle.
 * @param color Text color.
 */
void ssd1306_set_text_color(ssd1306_handle_t handle, ssd1306_color_t color)
{
    if (!handle)
        return;
    // Set background color to be the same as the foreground color.
    // This signifies that the background should not be drawn.
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
 * If enabled, text will automatically wrap to the next line if it exceeds the screen width.
 *
 * @param handle SSD1306 device handle.
 * @param wrap true to enable, false to disable.
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
 * @return int16_t Current cursor x-coordinate.
 */
int16_t ssd1306_get_cursor_x(ssd1306_handle_t handle)
{
    return handle ? handle->cursor_x : 0;
}

/**
 * @brief Gets the current y-coordinate of the text cursor.
 *
 * @param handle SSD1306 device handle.
 * @return int16_t Current cursor y-coordinate.
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
 * This is a wrapper function that calls ssd1306_write for each character.
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
 * @brief Writes a single character to the display buffer at the current cursor position.
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

    // Handle newline character: move to the next line.
    if (c == '\n')
    {
        handle->cursor_x = 0;
        handle->cursor_y += (int16_t)handle->textsize_y * font->yAdvance;
    }
    else if (c != '\r') // Ignore carriage return.
    {
        // Ensure the character exists in the font.
        if (c >= font->first && c <= font->last)
        {
            const GFXglyph *glyph = &font->glyph[c - font->first];
            uint8_t w = glyph->width;
            int16_t xo = glyph->xOffset;

            // --- Auto-adjust Cursor for First Character ---
            // If the cursor is at the default (0,0), automatically adjust the
            // y-position to make the first line visible, based on the current font's height.
            // This prevents the top of the font from being clipped at the screen edge.
            if (handle->cursor_x == 0 && handle->cursor_y == 0)
            {
                int8_t y_offset = glyph->yOffset;
                // If yOffset is negative (top of character is above the baseline),
                // shift the cursor down to make room.
                if (y_offset < 0)
                {
                    handle->cursor_y = -y_offset + 1;
                }
            }
            // --- End of addition ---

            // Handle text wrapping.
            if (handle->wrap && ((handle->cursor_x + handle->textsize_x * (xo + w)) > handle->config.screen_width))
            {
                handle->cursor_x = 0;
                handle->cursor_y += (int16_t)handle->textsize_y * font->yAdvance;
            }
            // Draw the character at the cursor position.
            ssd1306_draw_char(handle, handle->cursor_x, handle->cursor_y, c, handle->textcolor, handle->textbgcolor, handle->textsize_x, handle->textsize_y);
            // Advance the cursor for the next character.
            handle->cursor_x += glyph->xAdvance * handle->textsize_x;
        }
    }
    return 1;
}


/**
 * @brief Draws a single character to the display buffer at a specified position.
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
    // Validate input and ensure font is of GFX type.
    if (!handle || !handle->gfxFont || handle->gfxFont->type != FONT_TYPE_GFX)
        return;

    const GFXfont *font = (const GFXfont *)handle->gfxFont->font_data;
    if (!font || c < font->first || c > font->last)
        return; // Character not in font.

    // Get glyph metadata (size, offset, etc.) for the character.
    const GFXglyph *glyph = &font->glyph[c - font->first];
    const uint8_t *bitmap = font->bitmap;
    uint16_t bo = glyph->bitmapOffset;
    uint8_t w = glyph->width, h = glyph->height;
    int8_t xo = glyph->xOffset, yo = glyph->yOffset;

    // If character has no dimensions (e.g., a space), no need to draw.
    if (w == 0 || h == 0)
        return;

    uint8_t bits = 0, bit = 0;
    bool bg = (color != bg_color); // Determine if background needs to be drawn.

    // Optimization: Mark the entire character area as dirty just once.
    _ssd1306_mark_dirty(handle, x + xo * size_x, y + yo * size_y, w * size_x, h * size_y);

    // Loop through each row (y) and column (x) of the character's bitmap.
    for (uint8_t yy = 0; yy < h; yy++)
    {
        for (uint8_t xx = 0; xx < w; xx++)
        {
            // Read a new byte from the bitmap every 8 pixels.
            if (!(bit++ & 7))
            {
                bits = bitmap[bo++];
            }

            // If the most significant bit is 1, draw the foreground pixel.
            if (bits & 0x80)
            {
                if (size_x == 1 && size_y == 1) // If normal size, draw a single pixel.
                {
                    ssd1306_draw_pixel(handle, x + xo + xx, y + yo + yy, color);
                }
                else // If scaled, draw a rectangle.
                {
                    ssd1306_fill_rect(handle, x + (xo + xx) * size_x, y + (yo + yy) * size_y, size_x, size_y, color);
                }
            }
            // If the bit is 0 AND the background should be drawn.
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
            // Shift bits to read the next pixel.
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
 * @param x1 Pointer to store the minimum x-coordinate of the box.
 * @param y1 Pointer to store the minimum y-coordinate of the box.
 * @param w Pointer to store the total text width.
 * @param h Pointer to store the total text height.
 */
void ssd1306_get_text_bounds(ssd1306_handle_t handle, const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
{
    if (!handle || !str || !x1 || !y1 || !w || !h)
        return;

    *x1 = x;
    *y1 = y;
    *w = *h = 0;
    // Initialize boundaries to extreme values.
    int16_t minx = handle->config.screen_width, miny = handle->config.screen_height, maxx = -1, maxy = -1;

    unsigned char c;
    // Loop through each character and update the bounding box boundaries.
    while ((c = *str++))
    {
        _ssd1306_char_bounds(handle, c, &x, &y, &minx, &miny, &maxx, &maxy);
    }

    // Calculate final width and height from min/max boundaries.
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
 * This is the core function that directly manipulates the framebuffer.
 *
 * @param handle SSD1306 device handle.
 * @param x X-coordinate of the pixel.
 * @param y Y-coordinate of the pixel.
 * @param color Pixel color (white, black, or invert).
 */
void __attribute__((always_inline)) inline ssd1306_draw_pixel(ssd1306_handle_t handle, int16_t x, int16_t y, ssd1306_color_t color)
{
    // Ignore if the pixel is off-screen (basic clipping).
    if (x < 0 || x >= handle->config.screen_width || y < 0 || y >= handle->config.screen_height) return;

    // Calculate the byte index in the framebuffer. The screen is organized in 8-pixel-high "pages".
    // index = x + (y / 8) * screen_width
    size_t index = x + (y >> 3) * handle->config.screen_width;
    // Calculate the bit position within that byte (0-7).
    // bit_pos = y % 8
    uint8_t bit_pos = y & 0x07;

    // Manipulate the bit based on the color.
    switch (color)
    {
    case OLED_COLOR_WHITE:
        handle->buffer[index] |= (1 << bit_pos); // Set bit (OR)
        break;
    case OLED_COLOR_BLACK:
        handle->buffer[index] &= ~(1 << bit_pos); // Clear bit (AND with NOT)
        break;
    case OLED_COLOR_INVERT:
        handle->buffer[index] ^= (1 << bit_pos); // Invert bit (XOR)
        break;
    }
    // Mark this pixel as dirty.
    _ssd1306_mark_dirty(handle, x, y, 1, 1);
}

/**
 * @brief Draws a straight line from one point to another.
 * Uses Bresenham's line algorithm.
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
    // Optimization for vertical and horizontal lines.
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

    // Bresenham's algorithm
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
 * Optimized by manipulating bits within the same or sequential framebuffer bytes.
 *
 * @param handle SSD1306 device handle.
 * @param x X-coordinate of the line.
 * @param y Starting y-coordinate of the line.
 * @param h Height of the line.
 * @param color Line color.
 */
void ssd1306_draw_fast_vline(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t h, ssd1306_color_t color)
{
    // Basic clipping
    if (!handle || x < 0 || x >= handle->config.screen_width || h == 0)
        return;

    // Handle negative height
    if (h < 0)
    {
        y += h;
        h = -h;
    }
    if (y >= handle->config.screen_height)
        return;

    // Clip top and bottom boundaries
    int16_t y_end = y + h;
    if (y_end > handle->config.screen_height)
        y_end = handle->config.screen_height;
    if (y < 0)
        y = 0;

    // Mark the entire line area as dirty once
    _ssd1306_mark_dirty(handle, x, y, 1, y_end - y);

    // Loop through each vertical pixel
    for (int16_t i = y; i < y_end; ++i)
    {
        // Calculate index and bit position for the current pixel
        size_t index = x + (i >> 3) * handle->config.screen_width;
        uint8_t bit_pos = i & 0x07;
        // Manipulate the bit in the framebuffer
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
 * Optimized by setting the same bit on multiple adjacent bytes.
 *
 * @param handle SSD1306 device handle.
 * @param x Starting x-coordinate of the line.
 * @param y Y-coordinate of the line.
 * @param w Width of the line.
 * @param color Line color.
 */
void ssd1306_draw_fast_hline(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, ssd1306_color_t color)
{
    // Basic clipping
    if (!handle || y < 0 || y >= handle->config.screen_height || w == 0)
        return;

    // Handle negative width
    if (w < 0)
    {
        x += w;
        w = -w;
    }
    if (x >= handle->config.screen_width)
        return;

    // Clip left and right boundaries
    int16_t x_end = x + w;
    if (x_end > handle->config.screen_width)
        x_end = handle->config.screen_width;
    if (x < 0)
        x = 0;

    // Mark the entire line area as dirty once
    _ssd1306_mark_dirty(handle, x, y, x_end - x, 1);

    // Calculate page and bit mask (since y is constant, this is only calculated once)
    int16_t page = y >> 3;
    uint8_t bit_mask = 1 << (y & 0x07);
    uint16_t index_start = x + page * handle->config.screen_width;
    uint16_t index_end = (x_end - 1) + page * handle->config.screen_width;

    // Loop through the relevant bytes in the framebuffer
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
 * Built from four calls to horizontal and vertical line functions.
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
    // Top and bottom lines
    ssd1306_draw_fast_hline(handle, x, y, w, color);
    ssd1306_draw_fast_hline(handle, x, y + h - 1, w, color);
    // Left and right lines
    ssd1306_draw_fast_vline(handle, x, y, h, color);
    ssd1306_draw_fast_vline(handle, x + w - 1, y, h, color);
}

/**
 * @brief Fills a rectangle with a specified color.
 * Implemented by drawing a series of vertical lines.
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

    // Mark the entire dirty area once for efficiency.
    _ssd1306_mark_dirty(handle, x, y, x_end - x, h);

    // Loop and draw adjacent vertical lines.
    for (int16_t i = x; i < x_end; i++)
    {
        ssd1306_draw_fast_vline(handle, i, y, h, color);
    }
}

/**
 * @brief Draws an empty circle.
 * Uses the Midpoint circle algorithm (Bresenham's).
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
    // Draw the 4 cardinal points.
    ssd1306_draw_pixel(handle, x0, y0 + r, color);
    ssd1306_draw_pixel(handle, x0, y0 - r, color);
    ssd1306_draw_pixel(handle, x0 + r, y0, color);
    ssd1306_draw_pixel(handle, x0 - r, y0, color);
    // Use 8-way symmetry to draw the rest of the circle.
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
    // Start by drawing a vertical line at the center.
    ssd1306_draw_fast_vline(handle, x0, y0 - r, 2 * r + 1, color);
    // Use the helper to fill the remaining quadrants.
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
    // Simply draw three lines connecting the three points.
    ssd1306_draw_line(handle, x0, y0, x1, y1, color);
    ssd1306_draw_line(handle, x1, y1, x2, y2, color);
    ssd1306_draw_line(handle, x2, y2, x0, y0, color);
}

/**
 * @brief Fills a triangle with a specified color.
 * Uses a scanline fill algorithm.
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
    // Sort vertices by y-coordinate (y0 <= y1 <= y2).
    if (y0 > y1) { _swap_int16_t(y0, y1); _swap_int16_t(x0, x1); }
    if (y1 > y2) { _swap_int16_t(y2, y1); _swap_int16_t(x2, x1); }
    if (y0 > y1) { _swap_int16_t(y0, y1); _swap_int16_t(x0, x1); }
    if (y0 == y2) return; // Degenerate triangle.

    int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;

    // Top part of the triangle
    last = (y1 == y2) ? y1 : y1 - 1;
    for (y = y0; y <= last; y++)
    {
        // Calculate left (a) and right (b) intersection points for the current scanline.
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        if (a > b) _swap_int16_t(a, b);
        // Draw a horizontal line between the intersection points.
        ssd1306_draw_fast_hline(handle, a, y, b - a + 1, color);
    }

    // Bottom part of the triangle
    sa = (int32_t)dx12 * (y - y1);
    sb = (int32_t)dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        if (a > b) _swap_int16_t(a, b);
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
    // Limit radius to half of the shortest side.
    int16_t max_radius = ((w < h) ? w : h) / 2;
    if (r > max_radius)
        r = max_radius;
    // Draw the straight sides.
    ssd1306_draw_fast_hline(handle, x + r, y, w - 2 * r, color);         // Top
    ssd1306_draw_fast_hline(handle, x + r, y + h - 1, w - 2 * r, color); // Bottom
    ssd1306_draw_fast_vline(handle, x, y + r, h - 2 * r, color);         // Left
    ssd1306_draw_fast_vline(handle, x + w - 1, y + r, h - 2 * r, color); // Right
    // Draw the rounded corners using the circle helper.
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
    // Limit radius.
    int16_t max_radius = ((w < h) ? w : h) / 2;
    if (r > max_radius)
        r = max_radius;
    // Fill the center part (rectangle).
    ssd1306_fill_rect(handle, x + r, y, w - 2 * r, h, color);
    // Fill the corner parts using the circle helper.
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
    // Call the version with background, setting background color same as foreground
    // so the background is not drawn.
    ssd1306_draw_bitmap_bg(handle, x, y, bitmap, w, h, color, color);
}

/**
 * @brief Draws a monochrome bitmap with foreground and background colors.
 * This version is optimized to manipulate the framebuffer directly.
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

    // Stop if the bitmap is completely off-screen.
    if (x >= handle->config.screen_width || y >= handle->config.screen_height || (x + w) <= 0 || (y + h) <= 0)
    {
        return;
    }

    int16_t byte_width = (w + 7) / 8; // Bitmap width in bytes.
    uint8_t byte = 0;
    bool bg = (color != bg_color); // Determine if background needs to be drawn.

    // Mark the dirty area once for the entire bitmap.
    _ssd1306_mark_dirty(handle, x, y, w, h);

    // Loop through each row (j) and column (i) of the bitmap.
    for (int16_t j = 0; j < h; j++)
    {
        int16_t current_y = y + j;
        // Skip rows that are off-screen.
        if (current_y < 0 || current_y >= handle->config.screen_height)
            continue;

        for (int16_t i = 0; i < w; i++)
        {
            int16_t current_x = x + i;
            // Skip columns that are off-screen.
            if (current_x < 0 || current_x >= handle->config.screen_width)
                continue;

            // Read a new byte from bitmap data every 8 pixels.
            if (i & 7)
            {
                byte <<= 1;
            }
            else
            {
                byte = bitmap[j * byte_width + i / 8];
            }

            // Determine the color for the current pixel.
            ssd1306_color_t pixel_color = (byte & 0x80) ? color : bg_color;

            // Directly manipulate the framebuffer (inlined draw_pixel logic).
            // Only draw if background is enabled OR if the pixel is the foreground color.
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
                byte >>= 1; // Shift for the next bit (LSB-first format).
            else
                byte = bitmap[j * byteWidth + i / 8]; // Read a new byte.
            // If the bit is 1, draw the pixel.
            if (byte & 0x01)
                ssd1306_draw_pixel(handle, x + i, y, color);
        }
    }
}

/**
 * @brief Inverts the display colors (black becomes white and vice-versa).
 * This is a hardware operation.
 *
 * @param handle SSD1306 device handle.
 * @param invert true to invert, false to restore to normal.
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
 * @brief Stops any active hardware scrolling effect.
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
    ssd1306_stop_scroll(handle); // Stop any previous scroll.
    vTaskDelay(pdMS_TO_TICKS(10)); // Short delay.
    // Send scroll setup command.
    uint8_t cmds[] = {scroll_cmd, 0x00, start_page, 0x00, end_page, 0x00, 0xFF};
    _ssd1306_send_cmd_list(handle, cmds, sizeof(cmds));
    // Activate scroll.
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
 * @brief Turns on the display (exits sleep mode).
 *
 * @param handle SSD1306 device handle.
 * @return esp_err_t Operation status.
 */
esp_err_t ssd1306_display_on(ssd1306_handle_t handle)
{
    return _ssd1306_send_cmd_list(handle, (uint8_t[]){OLED_CMD_DISPLAY_ON}, 1);
}

/**
 * @brief Turns off the display (enters sleep mode). RAM content is preserved.
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

    // Set the vertical scroll area to encompass the whole screen.
    uint8_t setup_cmds[] = {
        OLED_CMD_SET_VERTICAL_SCROLL_AREA,
        0, // Number of fixed rows at the top
        handle->config.screen_height // Number of rows to scroll
    };
    _ssd1306_send_cmd_list(handle, setup_cmds, sizeof(setup_cmds));

    // Send diagonal scroll setup command.
    uint8_t scroll_cmds[] = {
        scroll_cmd,
        0x00,
        start_page,
        speed,
        end_page,
        offset // Vertical offset
    };
    _ssd1306_send_cmd_list(handle, scroll_cmds, sizeof(scroll_cmds));

    // Activate scroll.
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
    // Scrolling up is implemented by providing a negative offset (relative to screen height).
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

    // Ensure the end angle is always greater than the start angle.
    while (end_angle < start_angle)
    {
        end_angle += 360;
    }

    // Loop through each degree from the start to the end angle.
    for (int16_t angle = start_angle; angle <= end_angle; angle++)
    {
        // Convert angle to radians and calculate x, y coordinates.
        float rad = angle * M_PI / 180.0f;
        int16_t x = x0 + (int16_t)(r * cosf(rad));
        int16_t y = y0 + (int16_t)(r * sinf(rad));
        // Draw the pixel at the calculated position.
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

    // Loop and draw a line between each sequential pair of points.
    for (uint8_t i = 0; i < num_points - 1; i++)
    {
        ssd1306_draw_line(handle, x[i], y[i], x[i + 1], y[i + 1], color);
    }
}


/**
 * @brief Shifts the framebuffer contents by the specified offsets.
 * Useful for scrolling effects or simple animations.
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

    // --- Common Case Optimization: Fast Horizontal Shift Without Wrap ---
    // This is a very common case and can be significantly optimized by moving memory blocks.
    // It is much faster than moving pixels one by one.
    // Note: This optimization applies only to horizontal shifts because the framebuffer data
    // is organized horizontally (per pixel row, not per page).
    if (dy == 0 && !wrap)
    {
        int16_t shift_abs = (dx > 0) ? dx : -dx;
        if (shift_abs < width)
        {
            if (dx > 0) // Shift to the right
            {
                // Move the memory block to the right, leaving an empty space on the left.
                memmove(handle->buffer + dx, handle->buffer, handle->buffer_size - dx);
                // Clear the newly empty area on the left side.
                memset(handle->buffer, 0, dx); 
            }
            else // Shift to the left
            {
                // Move the memory block to the left, leaving an empty space on the right.
                memmove(handle->buffer, handle->buffer + shift_abs, handle->buffer_size - shift_abs);
                // Clear the newly empty area on the right side.
                memset(handle->buffer + (handle->buffer_size - shift_abs), 0, shift_abs);
            }
        }
        else // If the shift is larger than the screen width, just clear the buffer.
        {
            memset(handle->buffer, 0, handle->buffer_size);
        }
        _ssd1306_mark_dirty(handle, 0, 0, width, height); // Mark the entire screen as dirty.
        return;
    }

    // --- Advanced Implementation for Complex Cases (Vertical or Wrap) ---
    // For these cases, we need to process pixel by pixel.

    // A static buffer is used to avoid heap allocation (malloc), which can cause fragmentation.
    // The size is compile-time defined for a common 128x64 screen.
    static uint8_t temp_buffer[128 * 64 / 8];
    if (handle->buffer_size > sizeof(temp_buffer))
    {
        ESP_LOGE(TAG, "Buffer size exceeds temp_buffer capacity for shift operation");
        return;
    }

    // 1. Copy the original framebuffer to a temporary buffer.
    memcpy(temp_buffer, handle->buffer, handle->buffer_size);

    // 2. Clear the main framebuffer to be filled with the shifted data.
    memset(handle->buffer, 0x00, handle->buffer_size);

    // 3. Iterate through each pixel of the ORIGINAL (source) framebuffer in the temp buffer.
    for (int16_t src_y = 0; src_y < height; src_y++)
    {
        for (int16_t src_x = 0; src_x < width; src_x++)
        {
            // Check if the source pixel is 'on' (white).
            size_t src_idx = src_x + (src_y >> 3) * width;
            uint8_t src_bit_pos = src_y & 0x07;

            if ((temp_buffer[src_idx] >> src_bit_pos) & 1)
            {
                // If the pixel is 'on', calculate its new (destination) position.
                int16_t dst_x = src_x + dx;
                int16_t dst_y = src_y + dy;

                // Apply wrapping if enabled.
                if (wrap)
                {
                    // A safe modulo operation for negative values in C.
                    dst_x = (dst_x % width + width) % width;
                    dst_y = (dst_y % height + height) % height;
                }

                // Draw the pixel at its destination in the main framebuffer if it's still within the screen.
                if (dst_x >= 0 && dst_x < width && dst_y >= 0 && dst_y < height)
                {
                    // Direct manipulation of the destination buffer (inlined draw_pixel for efficiency).
                    size_t dst_idx = dst_x + (dst_y >> 3) * width;
                    uint8_t dst_bit_pos = dst_y & 0x07;
                    handle->buffer[dst_idx] |= (1 << dst_bit_pos);
                }
            }
        }
    }

    // 4. Mark the entire screen as 'dirty' as the whole content may have changed.
    _ssd1306_mark_dirty(handle, 0, 0, width, height);
}

/**
 * @brief Sets the hardware scan orientation (flip and remap).
 *
 * @warning This function does not rotate the library's coordinate system. It only
 * changes how the hardware scans RAM to the screen. It is useful for flipping the
 * display 180 degrees or correcting an inverted mount. For full rotation (e.g., 90 degrees),
 * a more complex coordinate system implementation would be needed.
 *
 * @param handle SSD1306 device handle.
 * @param rotation Orientation: 0 (normal), 1 (horizontal flip), 2 (vertical flip), 3 (180-degree flip).
 */
void ssd1306_set_orientation(ssd1306_handle_t handle, uint8_t rotation)
{
    if (!handle)
        return;

    // Set Segment Remap command for horizontal flip.
    // rotation & 1: if bit 0 is set (value 1 or 3), enable remap.
    uint8_t seg_cmd = (rotation & 1) ? OLED_CMD_SET_SEGMENT_REMAP | 0x01 : OLED_CMD_SET_SEGMENT_REMAP | 0x00;
    
    // Set COM Scan Mode command for vertical flip.
    // rotation & 2: if bit 1 is set (value 2 or 3), enable reverse scan.
    uint8_t com_cmd = (rotation & 2) ? OLED_CMD_SET_COM_SCAN_MODE | 0x08 : OLED_CMD_SET_COM_SCAN_MODE | 0x00;

    _ssd1306_send_cmd_list(handle, &seg_cmd, 1);
    _ssd1306_send_cmd_list(handle, &com_cmd, 1);
    
    // This function currently does not fully adjust the internal coordinate system
    // for `draw_` functions. The cursor adjustment below is an initial step
    // for `print` based functions. For full rotation functionality,
    // all drawing functions would need to be updated to transform coordinates.
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
    
    // Clear buffer and update screen to ensure the new orientation is visible
    // and there are no artifacts from the previous orientation.
    ssd1306_clear_buffer(handle);
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
}

/**
 * @brief Sets the display start line in RAM.
 * This shifts the entire display vertically without altering RAM content.
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
    // Get the text width to calculate the center position.
    ssd1306_get_text_bounds(handle, text, 0, 0, &x1, &y1, &w, &h);

    // Calculate the x-coordinate to center the text.
    int16_t x = (handle->config.screen_width - w) / 2;

    // Set cursor and print the text.
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
    // Get text dimensions to calculate the center position.
    ssd1306_get_text_bounds(handle, text, 0, 0, &x1, &y1, &w, &h);

    // Calculate x and y coordinates to center the text on screen.
    int16_t x = (handle->config.screen_width - w) / 2;
    int16_t y = (handle->config.screen_height + h) / 2; // Y adjustment for GFX font vertical centering.

    // Set cursor and print the text.
    ssd1306_set_cursor(handle, x, y);
    ssd1306_print(handle, text);
}

/**
 * @brief Draws left-aligned text at a specified y-coordinate.
 *
 * @param handle SSD1306 device handle.
 * @param text Text string to draw.
 * @param y Y-coordinate for the text baseline.
 */
void ssd1306_print_h(ssd1306_handle_t handle, const char *text, int16_t y)
{
    if (!handle || !text)
        return;

    // Set cursor to x=0 (left-aligned) and the specified y.
    ssd1306_set_cursor(handle, 0, y);
    ssd1306_print(handle, text);
}