/**
 * @file      ssd1306.h
 * @author    Muhamad Arif Hidayat
 * @brief Public header file for the I2C-based SSD1306 OLED display driver.
 * @version   1.0
 * @date      2025-06-30
 * @copyright Copyright (c) 2025
 *
 * This header provides a clean and unified C interface for controlling the SSD1306
 * OLED display, with compatibility for both C and C++ environments. The driver
 * incorporates core functionality inspired by the Adafruit GFX library, supporting
 * text rendering, primitive graphics, and display control. It features flexible I2C
 * configuration and custom font support via the `ssd1306_fonts.h` module.
 *
 * @note Ensure I2C configuration and GPIO pin assignments match the hardware
 * specifications before using this API. The default I2C address is typically 0x3C,
 * but it may vary depending on the device.
 */

#ifndef SSD1306_H
#define SSD1306_H

#include "driver/i2c.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ssd1306_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumeration of colors for pixel settings on the SSD1306 monochrome display.
 *
 * Defines the color states for individual pixels on the OLED display. As the SSD1306
 * is a monochrome display, it supports only two primary colors (black and white) and
 * an inversion option.
 */
typedef enum {
    OLED_COLOR_BLACK = 0,  ///< Pixel off (background color).
    OLED_COLOR_WHITE = 1,  ///< Pixel on (foreground color).
    OLED_COLOR_INVERT = 2, ///< Invert the current pixel state (toggle).
} ssd1306_color_t;

/**
 * @brief Configuration structure for SSD1306 display initialization.
 *
 * Stores essential parameters for setting up I2C communication and display
 * specifications, such as resolution and control pins. This structure must be
 * populated before calling the initialization function.
 *
 * @see ssd1306_create
 */
typedef struct {
    i2c_port_t i2c_port;         ///< I2C port used (e.g., I2C_NUM_0 or I2C_NUM_1).
    gpio_num_t sda_pin;          ///< GPIO pin number for the SDA line.
    gpio_num_t scl_pin;          ///< GPIO pin number for the SCL line.
    uint32_t i2c_clk_speed_hz;   ///< I2C clock speed in Hertz (e.g., 400000 for 400 kHz).
    uint8_t i2c_addr;            ///< I2C device address (default: 0x3C, refer to datasheet).
    int screen_width;            ///< Display width in pixels (e.g., 128).
    int screen_height;           ///< Display height in pixels (e.g., 64).
    gpio_num_t rst_pin;          ///< GPIO pin number for reset (use -1 if not used).
} ssd1306_config_t;

/**
 * @brief Opaque handle for an SSD1306 display instance.
 *
 * A pointer to an internal structure that manages the display's state. This handle
 * is returned by the initialization function and used for all subsequent operations.
 *
 * @see ssd1306_create
 */
typedef struct ssd1306_dev_t* ssd1306_handle_t;


/**
 * @brief Creates a new SSD1306 driver instance.
 *
 * Initializes the OLED display based on the provided configuration, allocates
 * internal resources, and sets up I2C communication.
 *
 * @param[in] config Pointer to the SSD1306 configuration structure.
 * @param[out] out_handle Pointer to store the created display instance handle.
 * @return esp_err_t Operation status (ESP_OK on success, otherwise error code).
 *
 * @note Ensure I2C pins and address are correctly configured before calling this function.
 */
esp_err_t ssd1306_create(const ssd1306_config_t *config, ssd1306_handle_t *out_handle);

/**
 * @brief Deletes an SSD1306 driver instance.
 *
 * Frees allocated resources (e.g., internal buffers) and invalidates the display handle.
 *
 * @param[in,out] handle Pointer to the display handle to be deleted (set to NULL after deletion).
 * @return esp_err_t Operation status (ESP_OK on success).
 */
esp_err_t ssd1306_delete(ssd1306_handle_t *handle);

/**
 * @brief Updates the display with the contents of the internal buffer.
 *
 * Transmits the internal buffer data to the SSD1306 display via I2C for rendering.
 *
 * @param[in] handle Display instance handle.
 * @return esp_err_t Operation status (ESP_OK on success).
 */
esp_err_t ssd1306_update_screen(ssd1306_handle_t handle);

/**
 * @brief Clears the internal display buffer.
 *
 * Sets all pixels in the internal buffer to black. The display is not updated until
 * ssd1306_update_screen is called.
 *
 * @param[in] handle Display instance handle.
 */
void ssd1306_clear_buffer(ssd1306_handle_t handle);

/**
 * @brief Fills the internal buffer with a specified color.
 *
 * Sets all pixels in the internal buffer to the specified color. The display is not
 * updated until ssd1306_update_screen is called.
 *
 * @param[in] handle Display instance handle.
 * @param[in] color Color to fill the buffer (OLED_COLOR_BLACK, OLED_COLOR_WHITE, or OLED_COLOR_INVERT).
 */
void ssd1306_fill_buffer(ssd1306_handle_t handle, ssd1306_color_t color);


/**
 * @brief Sets uniform text size (same scale for x and y).
 *
 * Scales text rendering uniformly in both horizontal and vertical directions.
 * The default size is 1 (no scaling).
 *
 * @param[in] handle Display instance handle.
 * @param[in] size Text scaling factor (1 = default, 2 = double size, etc.).
 */
void ssd1306_set_text_size(ssd1306_handle_t handle, uint8_t size);

/**
 * @brief Sets custom text size (separate x and y scaling).
 *
 * Allows independent scaling of text in horizontal and vertical directions.
 *
 * @param[in] handle Display instance handle.
 * @param[in] size_x Horizontal scaling factor.
 * @param[in] size_y Vertical scaling factor.
 */
void ssd1306_set_text_size_custom(ssd1306_handle_t handle, uint8_t size_x, uint8_t size_y);

/**
 * @brief Sets the font for text rendering.
 *
 * Replaces the default font with a custom font defined in `ssd1306_fonts.h`.
 *
 * @param[in] handle Display instance handle.
 * @param[in] font_handle Pointer to the font structure to be used.
 */
void ssd1306_set_font(ssd1306_handle_t handle, const ssd1306_font_handle_t *font_handle);

/**
 * @brief Sets the text cursor position.
 *
 * Specifies the (x, y) coordinates for the starting point of subsequent text rendering.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x X-coordinate (in pixels, from the left).
 * @param[in] y Y-coordinate (in pixels, from the top).
 */
void ssd1306_set_cursor(ssd1306_handle_t handle, int16_t x, int16_t y);

/**
 * @brief Sets the text color without a background color.
 *
 * Specifies the color for subsequent text rendering.
 *
 * @param[in] handle Display instance handle.
 * @param[in] color Text color (OLED_COLOR_BLACK, OLED_COLOR_WHITE, or OLED_COLOR_INVERT).
 */
void ssd1306_set_text_color(ssd1306_handle_t handle, ssd1306_color_t color);

/**
 * @brief Sets the text color with a background color.
 *
 * Specifies both the text and background colors for controlled text rendering.
 *
 * @param[in] handle Display instance handle.
 * @param[in] color Text color.
 * @param[in] bg_color Background color.
 */
void ssd1306_set_text_color_bg(ssd1306_handle_t handle, ssd1306_color_t color, ssd1306_color_t bg_color);

/**
 * @brief Enables or disables text wrapping.
 *
 * Controls whether text wraps to the next line when it exceeds the display width.
 *
 * @param[in] handle Display instance handle.
 * @param[in] wrap True to enable wrapping, false to disable.
 */
void ssd1306_set_text_wrap(ssd1306_handle_t handle, bool wrap);

/**
 * @brief Gets the current x-coordinate of the text cursor.
 *
 * Returns the last x-coordinate of the text cursor.
 *
 * @param[in] handle Display instance handle.
 * @return int16_t X-coordinate of the cursor.
 */
int16_t ssd1306_get_cursor_x(ssd1306_handle_t handle);

/**
 * @brief Gets the current y-coordinate of the text cursor.
 *
 * Returns the last y-coordinate of the text cursor.
 *
 * @param[in] handle Display instance handle.
 * @return int16_t Y-coordinate of the cursor.
 */
int16_t ssd1306_get_cursor_y(ssd1306_handle_t handle);

/**
 * @brief Gets the screen width of the display.
 * @param handle SSD1306 device handle.
 * @return Screen width in pixels.
 */
int16_t ssd1306_get_screen_width(ssd1306_handle_t handle);

/**
 * @brief Gets the screen height of the display.
 * @param handle SSD1306 device handle.
 * @return Screen height in pixels.
 */
int16_t ssd1306_get_screen_height(ssd1306_handle_t handle);


/**
 * @brief Writes a single character to the buffer.
 *
 * Adds a character to the buffer at the current cursor position.
 *
 * @param[in] handle Display instance handle.
 * @param[in] c Character to write.
 * @return size_t Number of characters successfully written.
 */
size_t ssd1306_write(ssd1306_handle_t handle, uint8_t c);

/**
 * @brief Prints a string to the buffer.
 *
 * Writes a null-terminated string to the buffer at the current cursor position.
 *
 * @param[in] handle Display instance handle.
 * @param[in] str Pointer to a null-terminated string.
 * @return size_t Number of characters successfully printed.
 */
size_t ssd1306_print(ssd1306_handle_t handle, const char* str);

/**
 * @brief Draws a single character at a specified position.
 *
 * Renders a character at the given (x, y) coordinates with specified size and color.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Starting x-coordinate.
 * @param[in] y Starting y-coordinate.
 * @param[in] c Character to draw.
 * @param[in] color Character color.
 * @param[in] bg_color Background color (optional).
 * @param[in] size_x Horizontal scaling factor.
 * @param[in] size_y Vertical scaling factor.
 */
void ssd1306_draw_char(ssd1306_handle_t handle, int16_t x, int16_t y, unsigned char c, ssd1306_color_t color, ssd1306_color_t bg_color, uint8_t size_x, uint8_t size_y);

/**
 * @brief Calculates the text bounding box.
 *
 * Computes the coordinates and dimensions of the bounding box for a given string.
 *
 * @param[in] handle Display instance handle.
 * @param[in] str String to measure.
 * @param[in] x Starting x-coordinate for calculation.
 * @param[in] y Starting y-coordinate for calculation.
 * @param[out] x1 Pointer to the x-coordinate of the bottom-left corner.
 * @param[out] y1 Pointer to the y-coordinate of the bottom-left corner.
 * @param[out] w Pointer to the bounding box width.
 * @param[out] h Pointer to the bounding box height.
 */
void ssd1306_get_text_bounds(ssd1306_handle_t handle, const char* str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);

/**
 * @brief Draws a single pixel.
 *
 * Sets the color of a pixel at the specified (x, y) coordinates in the internal buffer.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x X-coordinate of the pixel.
 * @param[in] y Y-coordinate of the pixel.
 * @param[in] color Pixel color.
 */
void ssd1306_draw_pixel(ssd1306_handle_t handle, int16_t x, int16_t y, ssd1306_color_t color);

/**
 * @brief Draws a straight line.
 *
 * Draws a line from (x0, y0) to (x1, y1) with the specified color.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x0 Starting x-coordinate.
 * @param[in] y0 Starting y-coordinate.
 * @param[in] x1 Ending x-coordinate.
 * @param[in] y1 Ending y-coordinate.
 * @param[in] color Line color.
 */
void ssd1306_draw_line(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, ssd1306_color_t color);

/**
 * @brief Draws a fast vertical line.
 *
 * Draws a vertical line from (x, y) downward with the specified height.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Starting x-coordinate.
 * @param[in] y Starting y-coordinate.
 * @param[in] h Line height in pixels.
 * @param[in] color Line color.
 */
void ssd1306_draw_fast_vline(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t h, ssd1306_color_t color);

/**
 * @brief Draws a fast horizontal line.
 *
 * Draws a horizontal line from (x, y) to the right with the specified width.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Starting x-coordinate.
 * @param[in] y Starting y-coordinate.
 * @param[in] w Line width in pixels.
 * @param[in] color Line color.
 */
void ssd1306_draw_fast_hline(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, ssd1306_color_t color);

/**
 * @brief Draws a rectangular outline.
 *
 * Draws the outline of a rectangle at (x, y) with the specified width and height.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Top-left x-coordinate.
 * @param[in] y Top-left y-coordinate.
 * @param[in] w Rectangle width.
 * @param[in] h Rectangle height.
 * @param[in] color Outline color.
 */
void ssd1306_draw_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, ssd1306_color_t color);

/**
 * @brief Draws a filled rectangle.
 *
 * Draws a filled rectangle at (x, y) with the specified width and height.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Top-left x-coordinate.
 * @param[in] y Top-left y-coordinate.
 * @param[in] w Rectangle width.
 * @param[in] h Rectangle height.
 * @param[in] color Fill color.
 */
void ssd1306_fill_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, ssd1306_color_t color);

/**
 * @brief Draws a circular outline.
 *
 * Draws the outline of a circle with center (x0, y0) and radius r.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x0 Center x-coordinate.
 * @param[in] y0 Center y-coordinate.
 * @param[in] r Circle radius.
 * @param[in] color Outline color.
 */
void ssd1306_draw_circle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, ssd1306_color_t color);

/**
 * @brief Draws a filled circle.
 *
 * Draws a filled circle with center (x0, y0) and radius r.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x0 Center x-coordinate.
 * @param[in] y0 Center y-coordinate.
 * @param[in] r Circle radius.
 * @param[in] color Fill color.
 */
void ssd1306_fill_circle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, ssd1306_color_t color);

/**
 * @brief Draws a triangular outline.
 *
 * Draws the outline of a triangle defined by three points (x0, y0), (x1, y1), and (x2, y2).
 *
 * @param[in] handle Display instance handle.
 * @param[in] x0 First point x-coordinate.
 * @param[in] y0 First point y-coordinate.
 * @param[in] x1 Second point x-coordinate.
 * @param[in] y1 Second point y-coordinate.
 * @param[in] x2 Third point x-coordinate.
 * @param[in] y2 Third point y-coordinate.
 * @param[in] color Outline color.
 */
void ssd1306_draw_triangle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, ssd1306_color_t color);

/**
 * @brief Draws a filled triangle.
 *
 * Draws a filled triangle defined by three points (x0, y0), (x1, y1), and (x2, y2).
 *
 * @param[in] handle Display instance handle.
 * @param[in] x0 First point x-coordinate.
 * @param[in] y0 First point y-coordinate.
 * @param[in] x1 Second point x-coordinate.
 * @param[in] y1 Second point y-coordinate.
 * @param[in] x2 Third point x-coordinate.
 * @param[in] y2 Third point y-coordinate.
 * @param[in] color Fill color.
 */
void ssd1306_fill_triangle(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, ssd1306_color_t color);

/**
 * @brief Draws a rounded rectangle outline.
 *
 * Draws the outline of a rectangle with rounded corners at (x, y).
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Top-left x-coordinate.
 * @param[in] y Top-left y-coordinate.
 * @param[in] w Rectangle width.
 * @param[in] h Rectangle height.
 * @param[in] r Corner radius.
 * @param[in] color Outline color.
 */
void ssd1306_draw_round_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, ssd1306_color_t color);

/**
 * @brief Draws a filled rounded rectangle.
 *
 * Draws a filled rectangle with rounded corners at (x, y).
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Top-left x-coordinate.
 * @param[in] y Top-left y-coordinate.
 * @param[in] w Rectangle width.
 * @param[in] h Rectangle height.
 * @param[in] r Corner radius.
 * @param[in] color Fill color.
 */
void ssd1306_fill_round_rect(ssd1306_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, ssd1306_color_t color);

/**
 * @brief Draws a monochrome bitmap.
 *
 * Renders a bitmap from an array at (x, y) with the specified color.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Top-left x-coordinate.
 * @param[in] y Top-left y-coordinate.
 * @param[in] bitmap Pointer to the bitmap data array.
 * @param[in] w Bitmap width in pixels.
 * @param[in] h Bitmap height in pixels.
 * @param[in] color Color for active pixels.
 */
void ssd1306_draw_bitmap(ssd1306_handle_t handle, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, ssd1306_color_t color);

/**
 * @brief Draws a monochrome bitmap with a background color.
 *
 * Renders a bitmap with separate foreground and background colors.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Top-left x-coordinate.
 * @param[in] y Top-left y-coordinate.
 * @param[in] bitmap Pointer to the bitmap data array.
 * @param[in] w Bitmap width in pixels.
 * @param[in] h Bitmap height in pixels.
 * @param[in] color Color for active pixels.
 * @param[in] bg_color Background color.
 */
void ssd1306_draw_bitmap_bg(ssd1306_handle_t handle, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, ssd1306_color_t color, ssd1306_color_t bg_color);

/**
 * @brief Draws an XBM (X BitMap) format bitmap.
 *
 * Renders a bitmap in XBM format at (x, y) with the specified color.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Top-left x-coordinate.
 * @param[in] y Top-left y-coordinate.
 * @param[in] bitmap Pointer to the XBM data array.
 * @param[in] w Bitmap width in pixels.
 * @param[in] h Bitmap height in pixels.
 * @param[in] color Color for active pixels.
 */
void ssd1306_draw_xbitmap(ssd1306_handle_t handle, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, ssd1306_color_t color);


/**
 * @brief Sets the display inversion mode.
 *
 * Inverts the color of all pixels on the display (white becomes black and vice versa).
 *
 * @param[in] handle Display instance handle.
 * @param[in] invert True to enable inversion, false to disable.
 */
void ssd1306_invert_display(ssd1306_handle_t handle, bool invert);

/**
 * @brief Sets the display contrast.
 *
 * Adjusts the brightness level of the display pixels (0-255).
 *
 * @param[in] handle Display instance handle.
 * @param[in] contrast Contrast value (0 = minimum, 255 = maximum).
 */
void ssd1306_set_contrast(ssd1306_handle_t handle, uint8_t contrast);

/**
 * @brief Starts horizontal scrolling to the right.
 *
 * Activates a horizontal scrolling animation to the right for a specified page range.
 *
 * @param[in] handle Display instance handle.
 * @param[in] start_page Starting page for scrolling (0-7).
 * @param[in] end_page Ending page for scrolling (0-7).
 */
void ssd1306_start_scroll_right(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page);

/**
 * @brief Starts horizontal scrolling to the left.
 *
 * Activates a horizontal scrolling animation to the left for a specified page range.
 *
 * @param[in] handle Display instance handle.
 * @param[in] start_page Starting page for scrolling (0-7).
 * @param[in] end_page Ending page for scrolling (0-7).
 */
void ssd1306_start_scroll_left(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page);

/**
 * @brief Stops all scrolling.
 *
 * Deactivates any active scrolling animation.
 *
 * @param[in] handle Display instance handle.
 */
void ssd1306_stop_scroll(ssd1306_handle_t handle);

/**
 * @brief Turns the display on.
 *
 * Activates the OLED display to show content.
 *
 * @param[in] handle Display instance handle.
 * @return esp_err_t Operation status (ESP_OK on success).
 */
esp_err_t ssd1306_display_on(ssd1306_handle_t handle);

/**
 * @brief Turns the display off.
 *
 * Deactivates the OLED display while preserving the internal buffer contents.
 *
 * @param[in] handle Display instance handle.
 * @return esp_err_t Operation status (ESP_OK on success).
 */
esp_err_t ssd1306_display_off(ssd1306_handle_t handle);

/**
 * @brief Draws an arc at the specified coordinates.
 *
 * Renders an arc with center (x0, y0), radius r, and specified start and end angles.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x0 Center x-coordinate.
 * @param[in] y0 Center y-coordinate.
 * @param[in] r Arc radius.
 * @param[in] start_angle Starting angle in degrees (0 degrees = 3 o'clock).
 * @param[in] end_angle Ending angle in degrees.
 * @param[in] color Arc color.
 */
void ssd1306_draw_arc(ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, int16_t start_angle, int16_t end_angle, ssd1306_color_t color);

/**
 * @brief Draws a polyline (connected line segments).
 *
 * Renders a series of connected lines defined by arrays of x and y coordinates.
 *
 * @param[in] handle Display instance handle.
 * @param[in] x Pointer to an array of x-coordinates.
 * @param[in] y Pointer to an array of y-coordinates.
 * @param[in] num_points Number of points in the polyline.
 * @param[in] color Line color.
 */
void ssd1306_draw_polyline(ssd1306_handle_t handle, const int16_t *x, const int16_t *y, uint8_t num_points, ssd1306_color_t color);


/**
 * @brief Sets the hardware scan orientation (flip and remap).
 *
 * Adjusts the display orientation without altering the library's coordinate system.
 * Useful for flipping the display 180 degrees or correcting an inverted mount.
 *
 * @param[in] handle Display instance handle.
 * @param[in] rotation Orientation: 0 (normal), 1 (horizontal flip), 2 (vertical flip), 3 (180-degree flip).
 */
void ssd1306_set_orientation(ssd1306_handle_t handle, uint8_t rotation);

/**
 * @brief Shifts the framebuffer contents by the specified offsets.
 *
 * Moves the internal buffer contents by dx and dy pixels. This is a computationally
 * intensive operation.
 *
 * @param[in] handle Display instance handle.
 * @param[in] dx Horizontal shift (positive = right, negative = left).
 * @param[in] dy Vertical shift (positive = down, negative = up).
 * @param[in] wrap If true, pixels exiting one side reappear on the opposite side.
 */
void ssd1306_shift_framebuffer(ssd1306_handle_t handle, int16_t dx, int16_t dy, bool wrap);

/**
 * @brief Sets the display start line in RAM.
 *
 * Specifies the display RAM line to be shown at the top (Y=0), enabling software-based
 * vertical scrolling.
 *
 * @param[in] handle Display instance handle.
 * @param[in] line Starting line to display (0-63).
 */
void ssd1306_set_display_start_line(ssd1306_handle_t handle, uint8_t line);

/**
 * @brief Starts diagonal scrolling downward to the right.
 *
 * Activates a diagonal scrolling animation to the right and downward for a specified
 * page range.
 *
 * @param[in] handle Display instance handle.
 * @param[in] start_page Starting page for scrolling (0-7).
 * @param[in] end_page Ending page for scrolling (0-7).
 * @param[in] offset Number of lines to move downward per scroll step (1-63).
 * @param[in] speed Horizontal scroll speed (0 = fastest, 7 = slowest).
 */
void ssd1306_start_scroll_diag_right_down(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page, uint8_t offset, uint8_t speed);

/**
 * @brief Starts diagonal scrolling upward to the left.
 *
 * Activates a diagonal scrolling animation to the left and upward for a specified
 * page range.
 *
 * @param[in] handle Display instance handle.
 * @param[in] start_page Starting page for scrolling (0-7).
 * @param[in] end_page Ending page for scrolling (0-7).
 * @param[in] offset Number of lines to move upward per scroll step (1-63).
 * @param[in] speed Horizontal scroll speed (0 = fastest, 7 = slowest).
 */
void ssd1306_start_scroll_diag_left_up(ssd1306_handle_t handle, uint8_t start_page, uint8_t end_page, uint8_t offset, uint8_t speed);

/**
 * @brief Draws horizontally centered text at a specified y-coordinate.
 *
 * Renders text centered horizontally at the given y-coordinate (baseline).
 *
 * @param[in] handle Display instance handle.
 * @param[in] text Text to render.
 * @param[in] y Y-coordinate for the text baseline.
 */
void ssd1306_print_centered_h(ssd1306_handle_t handle, const char *text, int16_t y);

/**
 * @brief Draws text centered both horizontally and vertically on the screen.
 *
 * Renders text at the exact center of the display.
 *
 * @param[in] handle Display instance handle.
 * @param[in] text Text to render.
 */
void ssd1306_print_screen_center(ssd1306_handle_t handle, const char *text);

/**
 * @brief  Draws horizontally text at a specified y-coordinate.
 *
 * Renders text horizontally at the given y-coordinate (baseline).
 *
 * @param[in] handle Display instance handle.
 * @param[in] text Text to render.
 */
void ssd1306_print_h(ssd1306_handle_t handle,const char *text, int16_t y);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H