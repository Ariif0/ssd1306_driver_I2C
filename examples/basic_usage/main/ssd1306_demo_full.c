/**
 * @file      main.c
 * @author    [Your Name]
 * @brief     Comprehensive automated showcase program for SSD1306 driver
 * @version   3.5
 * @date      2025-06-30
 *
 * @details   Demonstrates all public functions of the SSD1306 library through automated visual demos:
 *            - Primitive shapes with animations
 *            - Complex shape compositions
 *            - Text rendering (size, color, wrapping, custom fonts, cursor position)
 *            - Large centered characters
 *            - Dynamic bargraph animation
 *            - Full-screen bitmap display (including XBM format)
 *            - Display controls (inversion, contrast, scrolling, orientation)
 *            - Framebuffer shifting
 *            - Fast line drawing
 *
 * @note      Requires ssd1306.h and associated font definitions (FONT_5x7, FreeMono12pt7b, FreeSans9pt7b).
 *            Assumes a 128x64 SSD1306 OLED display connected via I2C.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ssd1306.h"

/**
 * @brief Logging tag for the OLED showcase application.
 */
#define TAG "OLED_SHOWCASE"

/**
 * @brief Display width in pixels.
 */
#define SCREEN_WIDTH 128

/**
 * @brief Display height in pixels.
 */
#define SCREEN_HEIGHT 64

/**
 * @brief Full-screen bitmap data (128x64, 1024 bytes) for ssd1306_draw_bitmap_bg.
 * @details Represents a predefined bitmap pattern for full-screen display demonstration.
 */
static const uint8_t fullscreen_bitmap[] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x1f, 0xff, 0xff, 0xf0, 0x41, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x7f, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x87, 0xff, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xff, 0xff, 0xff, 0xf8, 0x01, 0xf1, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 0xf8, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0xfc, 0x02, 0x78, 0x7f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfe, 0x03, 0x7c, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x07, 0xff, 0xff, 0xfe, 0x01, 0xfe, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xfd, 0xe0, 0x03, 0xff, 0xff, 0xfc, 0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xfe, 0x87, 0xe0, 0xff, 0xff, 0xfc, 0x00, 0x06, 0x07, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xfc, 0x1f, 0xf9, 0xff, 0xff, 0xfc, 0x00, 0x02, 0x07, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xff, 0xfc, 0x00, 0xc3, 0xc3, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xf0, 0x3f, 0xff, 0xff, 0xe0, 0x0c, 0x00, 0xe7, 0x81, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xf0, 0x0f, 0xff, 0xff, 0xe0, 0x02, 0x00, 0x02, 0x00, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xf0, 0x0f, 0xff, 0xff, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x3f, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x1e, 0x3f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0x3f, 0xf8, 0x00, 0x18, 0x7f, 0x1f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xf8, 0x01, 0x80, 0x03, 0xfc, 0x3f, 0xfc, 0x00, 0x70, 0xfe, 0x1f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xf0, 0x43, 0xff, 0xff, 0xf8, 0x7f, 0xf8, 0x00, 0x00, 0x7e, 0x1f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xf0, 0xff, 0xfc, 0x00, 0x00, 0x7c, 0x3f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xff, 0xf1, 0xef, 0xf8, 0x00, 0x01, 0xfc, 0x3f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xe4, 0xff, 0xff, 0xff, 0xf3, 0x80, 0xa0, 0x00, 0x07, 0xfc, 0xaf, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xec, 0x5f, 0xff, 0xff, 0xe7, 0xf0, 0x00, 0x00, 0x03, 0xfe, 0xdf, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xee, 0x7f, 0xff, 0xff, 0xc7, 0xf8, 0x00, 0x00, 0x03, 0xff, 0xdf, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xf7, 0xc7, 0xff, 0x06, 0x00, 0x03, 0xff, 0xbf, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfe, 0x5f, 0xff, 0xc7, 0x07, 0xff, 0x80, 0x00, 0x07, 0xdb, 0xbf, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xee, 0xff, 0xff, 0x80, 0x03, 0xff, 0xc0, 0x00, 0x03, 0xc3, 0x0f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0x98, 0x03, 0xff, 0xf8, 0x00, 0x07, 0xe0, 0x0f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xf8, 0x01, 0xff, 0xfc, 0x01, 0x07, 0xfc, 0x1f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xcf, 0xef, 0xff, 0xff, 0xe1, 0xff, 0xfc, 0x01, 0x07, 0xf8, 0x1f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0x7f, 0xf1, 0xff, 0xf8, 0x02, 0x07, 0x88, 0x3f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xcf, 0xef, 0xf8, 0x0f, 0xff, 0xff, 0xe0, 0x00, 0x07, 0x84, 0x3f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xe7, 0xef, 0xf0, 0x04, 0x7f, 0xff, 0xc0, 0x00, 0x07, 0x84, 0x7f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3f, 0xff, 0xe0, 0x00, 0x1f, 0xff, 0x80, 0x00, 0x06, 0x04, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3f, 0x7f, 0xe1, 0xf0, 0x07, 0xff, 0x80, 0x00, 0x07, 0x06, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc3, 0xfe, 0x03, 0xff, 0x00, 0x00, 0x03, 0x80, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xf2, 0x3f, 0xc6, 0x7f, 0x81, 0xce, 0x00, 0x00, 0x01, 0xc1, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xe0, 0x3f, 0xc0, 0x07, 0xc1, 0xfe, 0x00, 0x00, 0x0d, 0xc0, 0x7f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xe0, 0x3f, 0xc0, 0x01, 0xe0, 0xfc, 0x00, 0x00, 0x0f, 0xc0, 0x7f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0x3f, 0xc0, 0x00, 0x50, 0xfc, 0x00, 0x00, 0x0e, 0xc0, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0x3f, 0xc0, 0x00, 0x18, 0xf8, 0x00, 0x00, 0x0e, 0xc1, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0x3f, 0xc0, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x66, 0x81, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0x1f, 0xc7, 0x80, 0x00, 0xf8, 0x00, 0x01, 0xe0, 0x00, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0x1f, 0xc1, 0xe0, 0x01, 0xf8, 0x00, 0x03, 0xf0, 0x01, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x80, 0x1f, 0xc0, 0x3e, 0x03, 0xf0, 0x00, 0x00, 0xe0, 0x03, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x00, 0x1f, 0xe0, 0xe0, 0x03, 0xf2, 0x00, 0x00, 0xc0, 0x03, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x80, 0x1f, 0xf0, 0x00, 0x07, 0xe6, 0x00, 0x00, 0xc0, 0x03, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x80, 0x1f, 0xff, 0x00, 0x1f, 0xee, 0x00, 0x00, 0x80, 0x07, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xb8, 0x0f, 0xff, 0xf0, 0x3f, 0xdc, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xbc, 0x0f, 0xff, 0xff, 0xff, 0xdc, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x9e, 0x0f, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x08, 0x0f, 0xff, 0xff, 0xff, 0x70, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x00, 0x0b, 0xff, 0xff, 0xfe, 0xe0, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x00, 0x0b, 0xff, 0xff, 0xf9, 0xc0, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3c, 0x09, 0xff, 0xff, 0xf1, 0x80, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x1e, 0x08, 0x3f, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x1f, 0x08, 0x03, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xce, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfe, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff
};

/**
 * @brief XBM bitmap data (16x16) for ssd1306_draw_xbitmap.
 * @details Represents a small icon in XBM format for demonstration purposes.
 */
static const uint8_t xbm_icon[] = {
    0xFF, 0xFF, 0x81, 0x81, 0xBD, 0xBD, 0xA5, 0xA5,
    0xA5, 0xA5, 0xBD, 0xBD, 0x81, 0x81, 0xFF, 0xFF,
    0xFF, 0xFF, 0x81, 0x81, 0xBD, 0xBD, 0xA5, 0xA5,
    0xA5, 0xA5, 0xBD, 0xBD, 0x81, 0x81, 0xFF, 0xFF
};

/**
 * @brief Function prototypes for demonstration functions.
 * @details Each function showcases a specific feature of the SSD1306 driver.
 */
static void display_demo_title(ssd1306_handle_t handle, const char *title);
static void reset_display_state(ssd1306_handle_t handle);
static void run_demo_primitives(ssd1306_handle_t handle);
static void run_demo_shapes(ssd1306_handle_t handle);
static void run_demo_text(ssd1306_handle_t handle);
static void run_demo_text_alignment(ssd1306_handle_t handle);
static void run_demo_custom_fonts(ssd1306_handle_t handle);
static void run_demo_large_char(ssd1306_handle_t handle);
static void run_demo_bargraph(ssd1306_handle_t handle);
static void run_demo_clock(ssd1306_handle_t handle);
static void run_demo_fullscreen_bitmap(ssd1306_handle_t handle);
static void run_demo_display_control(ssd1306_handle_t handle);
static void run_demo_sine_wave(ssd1306_handle_t handle);
static void run_demo_spiral(ssd1306_handle_t handle);
static void run_demo_polyline_arc(ssd1306_handle_t handle);
static void run_demo_framebuffer_shift(ssd1306_handle_t handle);
static void run_demo_orientation(ssd1306_handle_t handle);
static void run_demo_advanced_scrolls(ssd1306_handle_t handle);
static void run_demo_fast_lines(ssd1306_handle_t handle);
static void run_demo_custom_text_size(ssd1306_handle_t handle);
static void run_demo_cursor_position(ssd1306_handle_t handle);
static void run_demo_single_char(ssd1306_handle_t handle);
static void run_demo_xbitmap(ssd1306_handle_t handle);
static void run_demo_left_scrolls(ssd1306_handle_t handle);
static void show_rects(ssd1306_handle_t handle);
static void show_circles(ssd1306_handle_t handle);
static void show_round_rects(ssd1306_handle_t handle);
static void show_triangles(ssd1306_handle_t handle);
static void run_anim_bouncing_ball(ssd1306_handle_t handle);

/**
 * @brief Main application entry point.
 * @details Initializes the SSD1306 OLED driver and runs a sequence of demonstration functions in a loop.
 */
void app_main(void) {
    // Initialize OLED driver
    ssd1306_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_pin = GPIO_NUM_21,
        .scl_pin = GPIO_NUM_22,
        .i2c_clk_speed_hz = 400000,
        .i2c_addr = 0x3C,
        .screen_width = SCREEN_WIDTH,
        .screen_height = SCREEN_HEIGHT,
        .rst_pin = -1
    };

    ssd1306_handle_t oled_handle = NULL;
    esp_err_t ret = ssd1306_create(&config, &oled_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OLED driver: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "OLED driver initialized successfully");

    // Demo sequence
    static const struct {
        void (*demo_func)(ssd1306_handle_t);
        const char *name;
    } demos[] = {
        {run_demo_primitives, "Primitives"},
        {run_demo_shapes, "Shapes"},
        {run_demo_text, "Text"},
        {run_demo_text_alignment, "Text Alignment"},
        {run_demo_custom_fonts, "Custom Fonts"},
        {run_demo_fullscreen_bitmap, "Fullscreen Bitmap"},
        {run_demo_large_char, "Large Character"},
        {run_demo_display_control, "Display Control"},
        {run_demo_bargraph, "Bargraph"},
        {run_demo_clock, "Clock"},
        {run_demo_sine_wave, "Sine Wave"},
        {run_demo_spiral, "Spiral"},
        {run_demo_polyline_arc, "Polyline Arc"},
        {run_demo_framebuffer_shift, "Framebuffer Shift"},
        {run_demo_orientation, "Orientation"},
        {run_demo_advanced_scrolls, "Advanced Scrolls"},
        {run_demo_fast_lines, "Fast Lines"},
        {run_demo_custom_text_size, "Custom Text Size"},
        {run_demo_cursor_position, "Cursor Position"},
        {run_demo_single_char, "Single Character"},
        {run_demo_xbitmap, "XBM Bitmap"},
        {run_demo_left_scrolls, "Left Scrolls"},
    };

    while (1) {
        reset_display_state(oled_handle);
        for (size_t i = 0; i < sizeof(demos) / sizeof(demos[0]); i++) {
            demos[i].demo_func(oled_handle);
        }
        ESP_LOGI(TAG, "Demo cycle completed. Restarting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // Cleanup (unreachable due to infinite loop)
    ssd1306_delete(&oled_handle);
}

/**
 * @brief Displays a centered demo title on the OLED screen.
 * @param handle SSD1306 display handle.
 * @param title Title text to display.
 */
static void display_demo_title(ssd1306_handle_t handle, const char *title) {
    ESP_LOGI(TAG, "Starting demo: %s", title);
    ssd1306_clear_buffer(handle);
    ssd1306_print_screen_center(handle, title);
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ssd1306_clear_buffer(handle);
}

/**
 * @brief Resets the display to its default state.
 * @details Stops scrolling, disables inversion, sets default contrast, and configures default font and text settings.
 * @param handle SSD1306 display handle.
 */
static void reset_display_state(ssd1306_handle_t handle) {
    ssd1306_stop_scroll(handle);
    ssd1306_invert_display(handle, false);
    ssd1306_set_contrast(handle, 0xCF);
    ssd1306_set_text_wrap(handle, false);
    ssd1306_set_text_size(handle, 1);
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_color(handle, OLED_COLOR_WHITE);
}

/**
 * @brief Demonstrates drawing of primitive shapes (pixels and lines) with animations.
 * @details Includes a spiderweb animation and a starfield effect.
 * @param handle SSD1306 display handle.
 */
static void run_demo_primitives(ssd1306_handle_t handle) {
    display_demo_title(handle, "Pixel & Lines");

    // Spiderweb animation
    for (int i = 0; i < SCREEN_WIDTH; i += 8) {
        ssd1306_clear_buffer(handle);
        ssd1306_draw_line(handle, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, i, 0, OLED_COLOR_WHITE);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    for (int i = 0; i < SCREEN_HEIGHT; i += 8) {
        ssd1306_clear_buffer(handle);
        ssd1306_draw_line(handle, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, SCREEN_WIDTH-1, i, OLED_COLOR_WHITE);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Starfield animation
    ssd1306_clear_buffer(handle);
    for (int i = 0; i < 200; i++) {
        ssd1306_draw_pixel(handle, rand() % SCREEN_WIDTH, rand() % SCREEN_HEIGHT, OLED_COLOR_WHITE);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Demonstrates various shapes with sub-demos for rectangles, circles, rounded rectangles, triangles, and a bouncing ball.
 * @param handle SSD1306 display handle.
 */
static void run_demo_shapes(ssd1306_handle_t handle) {
    display_demo_title(handle, "Basic Shapes");
    show_rects(handle);
    show_circles(handle);
    show_round_rects(handle);
    show_triangles(handle);
    run_anim_bouncing_ball(handle);
}

/**
 * @brief Displays filled and outlined rectangles.
 * @param handle SSD1306 display handle.
 */
static void show_rects(ssd1306_handle_t handle) {
    ssd1306_clear_buffer(handle);
    ssd1306_draw_rect(handle, 10, 10, 45, 35, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 12, 60);
    ssd1306_print(handle, "Outline");
    ssd1306_fill_rect(handle, 73, 10, 45, 35, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 79, 60);
    ssd1306_print(handle, "Filled");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(2500));
}

/**
 * @brief Displays filled and outlined circles.
 * @param handle SSD1306 display handle.
 */
static void show_circles(ssd1306_handle_t handle) {
    ssd1306_clear_buffer(handle);
    ssd1306_draw_circle(handle, 32, 28, 20, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 12, 60);
    ssd1306_print(handle, "Outline");
    ssd1306_fill_circle(handle, 96, 28, 20, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 79, 60);
    ssd1306_print(handle, "Filled");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(2500));
}

/**
 * @brief Displays filled and outlined rounded rectangles.
 * @param handle SSD1306 display handle.
 */
static void show_round_rects(ssd1306_handle_t handle) {
    ssd1306_clear_buffer(handle);
    ssd1306_draw_round_rect(handle, 10, 10, 50, 35, 8, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 12, 60);
    ssd1306_print(handle, "Outline");
    ssd1306_fill_round_rect(handle, 70, 10, 50, 35, 8, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 79, 60);
    ssd1306_print(handle, "Filled");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(2500));
}

/**
 * @brief Displays filled and outlined triangles.
 * @param handle SSD1306 display handle.
 */
static void show_triangles(ssd1306_handle_t handle) {
    ssd1306_clear_buffer(handle);
    ssd1306_draw_triangle(handle, 32, 5, 5, 45, 60, 45, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 12, 60);
    ssd1306_print(handle, "Outline");
    ssd1306_fill_triangle(handle, 96, 5, 69, 45, 123, 45, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 79, 60);
    ssd1306_print(handle, "Filled");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(2500));
}

/**
 * @brief Animates a bouncing ball within a rectangular boundary.
 * @param handle SSD1306 display handle.
 */
static void run_anim_bouncing_ball(ssd1306_handle_t handle) {
    display_demo_title(handle, "Bouncing Ball");
    float ball_x = 50.0f, ball_y = 20.0f;
    float vel_x = 1.5f, vel_y = 1.0f;
    const int radius = 4;
    const int box_x = 0, box_y = 0, box_w = SCREEN_WIDTH, box_h = SCREEN_HEIGHT;

    for (int i = 0; i < 150; i++) {
        ssd1306_clear_buffer(handle);
        ssd1306_draw_rect(handle, box_x, box_y, box_w, box_h, OLED_COLOR_WHITE);
        ball_x += vel_x;
        ball_y += vel_y;
        if (ball_x - radius < box_x || ball_x + radius > box_x + box_w) vel_x = -vel_x;
        if (ball_y - radius < box_y || ball_y + radius > box_y + box_h) vel_y = -vel_y;
        ssd1306_fill_circle(handle, (int)ball_x, (int)ball_y, radius, OLED_COLOR_WHITE);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Demonstrates text rendering with different sizes, colors, and wrapping.
 * @param handle SSD1306 display handle.
 */
static void run_demo_text(ssd1306_handle_t handle) {
    display_demo_title(handle, "Text Rendering");
    
    // Scene 1: Text sizes and colors
    ssd1306_clear_buffer(handle);
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_size(handle, 1);
    ssd1306_set_cursor(handle, 0, 15);
    ssd1306_print(handle, "Normal Size (1x)");
    ssd1306_set_text_size(handle, 2);
    ssd1306_set_cursor(handle, 0, 40);
    ssd1306_print(handle, "Large 2x");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Scene 2: Text wrapping
    ssd1306_clear_buffer(handle);
    ssd1306_set_text_size(handle, 1);
    ssd1306_set_text_wrap(handle, true);
    ssd1306_set_cursor(handle, 0, 8);
    ssd1306_print(handle, "With text wrap, this long sentence will automatically wrap to the next line when reaching the screen edge.");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(4000));
    ssd1306_set_text_wrap(handle, false);
}

/**
 * @brief Demonstrates text alignment (horizontal, vertical, and corner).
 * @param handle SSD1306 display handle.
 */
static void run_demo_text_alignment(ssd1306_handle_t handle) {
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_size(handle, 1);

    // Scene 1: Horizontal alignment
    display_demo_title(handle, "Horizontal Alignment");
    ssd1306_clear_buffer(handle);
    ssd1306_set_cursor(handle, 0, 15);
    ssd1306_print(handle, "Left Align");
    ssd1306_print_centered_h(handle, "Center Align", 35);
    int16_t x1, y1;
    uint16_t w, h;
    ssd1306_get_text_bounds(handle, "Right Align", 0, 0, &x1, &y1, &w, &h);
    ssd1306_set_cursor(handle, SCREEN_WIDTH - w, 55);
    ssd1306_print(handle, "Right Align");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Scene 2: Vertical alignment
    display_demo_title(handle, "Vertical Alignment");
    ssd1306_clear_buffer(handle);
    ssd1306_get_text_bounds(handle, "Top", 0, 0, &x1, &y1, &w, &h);
    ssd1306_print_centered_h(handle, "Top", h);
    ssd1306_print_centered_h(handle, "Middle", (SCREEN_HEIGHT + h) / 2);
    ssd1306_print_centered_h(handle, "Bottom", SCREEN_HEIGHT - 1);
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Scene 3: Corner alignment
    display_demo_title(handle, "Corner Alignment");
    ssd1306_clear_buffer(handle);
    ssd1306_get_text_bounds(handle, "Top Left", 0, 0, &x1, &y1, &w, &h);
    ssd1306_set_cursor(handle, 0, h);
    ssd1306_print(handle, "Top Left");
    ssd1306_get_text_bounds(handle, "Top Right", 0, 0, &x1, &y1, &w, &h);
    ssd1306_set_cursor(handle, SCREEN_WIDTH - w, h);
    ssd1306_print(handle, "Top Right");
    ssd1306_get_text_bounds(handle, "Bottom Left", 0, 0, &x1, &y1, &w, &h);
    ssd1306_set_cursor(handle, 0, SCREEN_HEIGHT - 1);
    ssd1306_print(handle, "Bottom Left");
    ssd1306_get_text_bounds(handle, "Bottom Right", 0, 0, &x1, &y1, &w, &h);
    ssd1306_set_cursor(handle, SCREEN_WIDTH - w, SCREEN_HEIGHT - 1);
    ssd1306_print(handle, "Bottom Right");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Demonstrates rendering with custom fonts (FreeMono12pt7b and FreeSans9pt7b).
 * @param handle SSD1306 display handle.
 */
static void run_demo_custom_fonts(ssd1306_handle_t handle) {
    display_demo_title(handle, "Custom Fonts");
    ssd1306_clear_buffer(handle);
    ssd1306_set_font(handle, &FreeMono12pt7b);
    ssd1306_print_centered_h(handle, "Mono 12pt", 18);
    ssd1306_set_font(handle, &FreeSans9pt7b);
    ssd1306_print_centered_h(handle, "Sans 9pt", 48);
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
    ssd1306_set_font(handle, &FONT_5x7);
}

/**
 * @brief Displays large characters with custom text size and color.
 * @param handle SSD1306 display handle.
 */
static void run_demo_large_char(ssd1306_handle_t handle) {
    display_demo_title(handle, "Large Character");
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_size(handle, 6);
    ssd1306_set_text_color_bg(handle, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    
    const char *chars = "ABCD123";
    for (int i = 0; i < strlen(chars); i++) {
        ssd1306_clear_buffer(handle);
        char str[2] = {chars[i], '\0'};
        int16_t x1, y1;
        uint16_t w, h;
        ssd1306_get_text_bounds(handle, str, 0, 0, &x1, &y1, &w, &h);
        ssd1306_set_cursor(handle, (SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT + h) / 2);
        ssd1306_print(handle, str);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ssd1306_set_text_size(handle, 1);
    ssd1306_set_text_color(handle, OLED_COLOR_WHITE);
}

/**
 * @brief Animates a dynamic bargraph with sinusoidal height variations.
 * @param handle SSD1306 display handle.
 */
static void run_demo_bargraph(ssd1306_handle_t handle) {
    display_demo_title(handle, "Bargraph Animation");
    const int num_bars = 8;
    const int bar_width = SCREEN_WIDTH / num_bars;

    for (int cycle = 0; cycle < 2; cycle++) {
        for (int h = 0; h <= SCREEN_HEIGHT; h += 4) {
            ssd1306_clear_buffer(handle);
            for (int i = 0; i < num_bars; i++) {
                float V_h = h * (sin(i * 0.8 + cycle * M_PI) * 0.5 + 0.5);
                ssd1306_fill_rect(handle, i * bar_width, SCREEN_HEIGHT - V_h, bar_width - 2, V_h, OLED_COLOR_WHITE);
            }
            ssd1306_update_screen(handle);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        for (int h = SCREEN_HEIGHT; h >= 0; h -= 4) {
            ssd1306_clear_buffer(handle);
            for (int i = 0; i < num_bars; i++) {
                float V_h = h * (sin(i * 0.8 + cycle * M_PI) * 0.5 + 0.5);
                ssd1306_fill_rect(handle, i * bar_width, SCREEN_HEIGHT - V_h, bar_width - 2, V_h, OLED_COLOR_WHITE);
            }
            ssd1306_update_screen(handle);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

/**
 * @brief Simulates a digital clock display with updating time.
 * @param handle SSD1306 display handle.
 */
static void run_demo_clock(ssd1306_handle_t handle) {
    display_demo_title(handle, "Digital Clock");
    ssd1306_set_font(handle, &FreeMono12pt7b);
    ssd1306_set_text_size(handle, 1);
    for (int i = 0; i < 10; i++) {
        ssd1306_clear_buffer(handle);
        char time_str[9];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", i / 3600, (i % 3600) / 60, i % 60);
        ssd1306_print_centered_h(handle, time_str, (SCREEN_HEIGHT + 12) / 2);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ssd1306_set_font(handle, &FONT_5x7);
}

/**
 * @brief Displays a full-screen bitmap using ssd1306_draw_bitmap_bg.
 * @param handle SSD1306 display handle.
 */
static void run_demo_fullscreen_bitmap(ssd1306_handle_t handle) {
    display_demo_title(handle, "Fullscreen Bitmap");
    ssd1306_clear_buffer(handle);
    ssd1306_draw_bitmap_bg(handle, 0, 0, fullscreen_bitmap, SCREEN_WIDTH, SCREEN_HEIGHT, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(4000));
}

/**
 * @brief Demonstrates display control features (blinking, inversion, and contrast).
 * @param handle SSD1306 display handle.
 */
static void run_demo_display_control(ssd1306_handle_t handle) {
    display_demo_title(handle, "Display Control");

    // Scene 1: Blinking effect with moving text
    ssd1306_set_font(handle, &FreeMono12pt7b);
    ssd1306_set_text_size(handle, 1);
    ssd1306_set_text_color_bg(handle, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    const char *text = "Blinking!";
    for (int i = 0; i < 30; i++) {
        ssd1306_clear_buffer(handle);
        int y = 32 + 10 * sin(i * M_PI / 15.0f);
        ssd1306_print_centered_h(handle, text, y);
        ssd1306_update_screen(handle);
        if (i % 10 == 0 && i < 20) {
            vTaskDelay(pdMS_TO_TICKS(300));
            ssd1306_display_off(handle);
            vTaskDelay(pdMS_TO_TICKS(200));
            ssd1306_display_on(handle);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_color(handle, OLED_COLOR_WHITE);

    // Scene 2: Color inversion with robot face
    for (int i = 0; i < 2; i++) {
        bool inverted = (i == 1);
        ssd1306_invert_display(handle, inverted);
        for (int t = 0; t < 20; t++) {
            ssd1306_clear_buffer(handle);
            ssd1306_draw_rect(handle, 49, 5, 30, 30, OLED_COLOR_WHITE);
            if (t % 4 < 2) {
                ssd1306_draw_circle(handle, 59, 20, 4, OLED_COLOR_WHITE);
                ssd1306_draw_circle(handle, 69, 20, 4, OLED_COLOR_WHITE);
            } else {
                ssd1306_draw_line(handle, 56, 20, 62, 20, OLED_COLOR_WHITE);
                ssd1306_draw_line(handle, 66, 20, 72, 20, OLED_COLOR_WHITE);
            }
            ssd1306_draw_round_rect(handle, 39, 37, 50, 26, 5, OLED_COLOR_WHITE);
            ssd1306_set_cursor(handle, 0, 0);
            ssd1306_print(handle, inverted ? "Inverted" : "Normal");
            if (i == 1 && t < 5) ssd1306_set_contrast(handle, 255 - t * 51);
            else if (i == 0 && t < 5) ssd1306_set_contrast(handle, t * 51);
            ssd1306_update_screen(handle);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    ssd1306_invert_display(handle, false);
    ssd1306_set_contrast(handle, 0xCF);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Scene 3: Contrast animation with rotating circles
    const int center_x = SCREEN_WIDTH / 2, center_y = SCREEN_HEIGHT / 2;
    for (int i = 0; i < 360; i += 5) {
        ssd1306_clear_buffer(handle);
        float rad = i * M_PI / 180;
        ssd1306_set_contrast(handle, 127 + 128 * sin(rad));
        for (int j = 0; j < 3; j++) {
            int r = 10 + j * 5;
            int x = center_x + r * cos(rad + j * 2 * M_PI / 3);
            int y = center_y + r * sin(rad + j * 2 * M_PI / 3);
            ssd1306_draw_circle(handle, x, y, 4, OLED_COLOR_WHITE);
        }
        ssd1306_print_centered_h(handle, "Contrast", 56);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ssd1306_set_contrast(handle, 0xCF);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Animates a sine wave pattern across the display.
 * @param handle SSD1306 display handle.
 */
static void run_demo_sine_wave(ssd1306_handle_t handle) {
    display_demo_title(handle, "Sine Wave");
    const int amplitude = 20;
    const int period = 50;
    const float speed = 0.1f;

    for (int t = 0; t < 200; t++) {
        ssd1306_clear_buffer(handle);
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            float rad = (x + t * speed) * 2 * M_PI / period;
            int y = SCREEN_HEIGHT/2 + amplitude * sin(rad);
            if (y >= 0 && y < SCREEN_HEIGHT) {
                ssd1306_draw_pixel(handle, x, y, OLED_COLOR_WHITE);
            }
        }
        ssd1306_draw_line(handle, 0, SCREEN_HEIGHT/2, SCREEN_WIDTH-1, SCREEN_HEIGHT/2, OLED_COLOR_WHITE);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Displays a rotating spiral pattern.
 * @param handle SSD1306 display handle.
 */
static void run_demo_spiral(ssd1306_handle_t handle) {
    display_demo_title(handle, "Rotating Spiral");
    const int center_x = SCREEN_WIDTH/2;
    const int center_y = SCREEN_HEIGHT/2;
    const int max_radius = 25;
    const float speed = 0.2f;

    for (int t = 0; t < 150; t++) {
        ssd1306_clear_buffer(handle);
        for (float theta = 0; theta < 4 * M_PI; theta += 0.2f) {
            float radius = max_radius * (1 - theta / (4 * M_PI)) * (1 + 0.5 * sin(t * speed));
            int x = center_x + radius * cos(theta + t * speed);
            int y = center_y + radius * sin(theta + t * speed);
            if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
                ssd1306_draw_pixel(handle, x, y, OLED_COLOR_WHITE);
            }
        }
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Demonstrates polyline and arc drawing with animation.
 * @param handle SSD1306 display handle.
 */
static void run_demo_polyline_arc(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in polyline_arc demo");
        return;
    }

    display_demo_title(handle, "Polyline & Arc");
    ssd1306_clear_buffer(handle);

    // Draw ECG-like polyline
    const int16_t points_x[] = {10, 20, 25, 35, 40, 50, 90, 95, 105, 110, 120};
    const int16_t points_y[] = {32, 32, 12, 52, 32, 32, 32, 52, 12, 32, 32};
    ssd1306_draw_polyline(handle, points_x, points_y, 11, OLED_COLOR_WHITE);
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Animated growing arc
    for (int angle = 0; angle <= 360; angle += 5) {
        ssd1306_clear_buffer(handle);
        ssd1306_draw_polyline(handle, points_x, points_y, 11, OLED_COLOR_WHITE);
        ssd1306_draw_arc(handle, ssd1306_get_screen_width(handle) / 2, 
                        ssd1306_get_screen_height(handle) / 2, 25, 0, angle, OLED_COLOR_WHITE);
        ESP_ERROR_CHECK(ssd1306_update_screen(handle));
        vTaskDelay(pdMS_TO_TICKS(15));
    }
     vTaskDelay(pdMS_TO_TICKS(1500));
}

/**
 * @brief Animates framebuffer shifting with wrap-around.
 * @param handle SSD1306 display handle.
 */
static void run_demo_framebuffer_shift(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in framebuffer_shift demo");
        return;
    }

    display_demo_title(handle, "Shift Framebuffer");
    ssd1306_clear_buffer(handle);
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_size(handle, 2);
    ssd1306_print_screen_center(handle, "SHIFT");
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Animate framebuffer shift with wrap-around
    ESP_LOGI(TAG, "Starting framebuffer shift animation...");
    for (int i = 0; i < 50; i++) {
        ssd1306_shift_framebuffer(handle, 2, 1, true);
        ESP_ERROR_CHECK(ssd1306_update_screen(handle));
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Draws an 'F' shape using rectangles.
 * @param handle SSD1306 display handle.
 */
static void draw_f_shape(ssd1306_handle_t handle) {
    ssd1306_clear_buffer(handle);
    ssd1306_fill_rect(handle, 10, 10, 10, 40, OLED_COLOR_WHITE); // Vertical bar
    ssd1306_fill_rect(handle, 20, 10, 25, 10, OLED_COLOR_WHITE); // Top horizontal bar
    ssd1306_fill_rect(handle, 20, 25, 20, 10, OLED_COLOR_WHITE); // Middle horizontal bar
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
}

/**
 * @brief Demonstrates display orientation changes (normal, horizontal flip, vertical flip, 180-degree flip).
 * @param handle SSD1306 display handle.
 */
static void run_demo_orientation(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in orientation demo");
        return;
    }

    display_demo_title(handle, "Orientation (Flip)");
    draw_f_shape(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Orientation: Horizontal Flip (1)");
    ssd1306_set_orientation(handle, 1);
    ssd1306_clear_buffer(handle); // Clear buffer before redraw
    draw_f_shape(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Orientation: Vertical Flip (2)");
    ssd1306_set_orientation(handle, 2);
    ssd1306_clear_buffer(handle); // Clear buffer before redraw
    draw_f_shape(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Orientation: 180 Degree Flip (3)");
    ssd1306_set_orientation(handle, 3);
    ssd1306_clear_buffer(handle); // Clear buffer before redraw
    draw_f_shape(handle);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

/**
 * @brief Demonstrates advanced scrolling features (diagonal and vertical).
 * @param handle SSD1306 display handle.
 */
static void run_demo_advanced_scrolls(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in advanced_scrolls demo");
        return;
    }

    display_demo_title(handle, "Advanced Scroll");
    ssd1306_clear_buffer(handle);
    // Draw grid pattern for scrolling visibility
    for (int i = 0; i < ssd1306_get_screen_width(handle); i += 8) {
        ssd1306_draw_line(handle, i, 0, i, ssd1306_get_screen_height(handle) - 1, OLED_COLOR_WHITE);
    }
    for (int i = 0; i < ssd1306_get_screen_height(handle); i += 8) {
        ssd1306_draw_line(handle, 0, i, ssd1306_get_screen_width(handle) - 1, i, OLED_COLOR_WHITE);
    }
    ssd1306_print_screen_center(handle, "DIAGONAL");
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Diagonal scroll right-down
    ssd1306_start_scroll_diag_right_down(handle, 0, 7, 1, 4);
    vTaskDelay(pdMS_TO_TICKS(5000));
    ssd1306_stop_scroll(handle);
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Software-based vertical scroll
    ssd1306_clear_buffer(handle);
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_size(handle, 1);
    ssd1306_print_screen_center(handle, "VERTICAL");
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Animate vertical scroll using display start line
    for (int line = 0; line < ssd1306_get_screen_height(handle); line += 2) {
        ssd1306_set_display_start_line(handle, line);
        ESP_ERROR_CHECK(ssd1306_update_screen(handle));
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    ssd1306_set_display_start_line(handle, 0);
    ESP_ERROR_CHECK(ssd1306_update_screen(handle));
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief A simple demo to show how to use fast line drawing functions.
 * @param handle Handle to the ssd1306 device.
 */
static void run_demo_fast_lines(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in fast_lines demo");
        return;
    }

    const int16_t width = ssd1306_get_screen_width(handle);
    const int16_t height = ssd1306_get_screen_height(handle);

    
    display_demo_title(handle, "Fast Lines demo");

    // --- Part 1: Animate drawing vertical lines ---
    // This loop draws vertical lines from left to right, one by one.
    // The screen is updated after each line is drawn to create an animation.
    ssd1306_clear_buffer(handle);
    for (int x = 0; x < width; x += 4) {
        ssd1306_draw_fast_vline(handle, x, 0, height, OLED_COLOR_WHITE);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // --- Part 2: Animate drawing horizontal lines over the vertical ones ---
    // This loop draws horizontal lines from top to bottom, creating a grid.
    // We don't clear the buffer, so it draws on top of the vertical lines.
    for (int y = 0; y < height; y += 4) {
        ssd1306_draw_fast_hline(handle, 0, y, width, OLED_COLOR_WHITE);
        ssd1306_update_screen(handle);
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // --- Part 3: Hold the final image for a few seconds ---
    // This gives time to see the final generated grid.
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Demonstrates custom text scaling (wide and tall text).
 * @param handle SSD1306 display handle.
 */
static void run_demo_custom_text_size(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in custom_text_size demo");
        return;
    }

    display_demo_title(handle, "Custom Text Size");
    ssd1306_clear_buffer(handle);
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_text_size_custom(handle, 2, 1); // Wide text
    ssd1306_print_centered_h(handle, "Wide Text", 20);
    ssd1306_set_text_size_custom(handle, 1, 2); // Tall text
    ssd1306_print_centered_h(handle, "Tall Text", 40);
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Demonstrates cursor position control and display.
 * @param handle SSD1306 display handle.
 */
static void run_demo_cursor_position(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in cursor_position demo");
        return;
    }

    display_demo_title(handle, "Cursor Position");
    ssd1306_clear_buffer( handle);
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_set_cursor(handle, 10, 20);
    ssd1306_print(handle, "Cursor Here");
    char pos_str[32];
    snprintf(pos_str, sizeof(pos_str), "X: %d, Y: %d", ssd1306_get_cursor_x(handle), ssd1306_get_cursor_y(handle));
    ssd1306_set_cursor(handle, 10, 40);
    ssd1306_print(handle, pos_str);
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Demonstrates rendering a single scaled character.
 * @param handle SSD1306 display handle.
 */
static void run_demo_single_char(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in single_char demo");
        return;
    }

    display_demo_title(handle, "Single Character");
    ssd1306_clear_buffer(handle);
    ssd1306_set_font(handle, &FONT_5x7);
    ssd1306_draw_char(handle, 50, 30, 'A', OLED_COLOR_WHITE, OLED_COLOR_BLACK, 3, 3);
    ssd1306_set_cursor(handle, 10, 50);
    ssd1306_print(handle, "Char 'A' (3x3)");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Displays an XBM format bitmap.
 * @param handle SSD1306 display handle.
 */
static void run_demo_xbitmap(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in xbitmap demo");
        return;
    }

    display_demo_title(handle, "XBM Bitmap");
    ssd1306_clear_buffer(handle);
    ssd1306_draw_xbitmap(handle, (SCREEN_WIDTH - 16) / 2, (SCREEN_HEIGHT - 16) / 2, xbm_icon, 16, 16, OLED_COLOR_WHITE);
    ssd1306_set_cursor(handle, 10, 50);
    ssd1306_print(handle, "XBM Icon (16x16)");
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Demonstrates left and diagonal left-up scrolling.
 * @param handle SSD1306 display handle.
 */
static void run_demo_left_scrolls(ssd1306_handle_t handle) {
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle in left_scrolls demo");
        return;
    }

    display_demo_title(handle, "Left Scrolls");
    ssd1306_clear_buffer(handle);
    ssd1306_print_screen_center(handle, "LEFT SCROLL");
    ssd1306_update_screen(handle);
    ssd1306_start_scroll_left(handle, 0, 7);
    vTaskDelay(pdMS_TO_TICKS(5000));
    ssd1306_stop_scroll(handle);
    ssd1306_clear_buffer(handle);
    ssd1306_print_screen_center(handle, "DIAG LEFT-UP");
    ssd1306_update_screen(handle);
    ssd1306_start_scroll_diag_left_up(handle, 0, 7, 1, 4);
    vTaskDelay(pdMS_TO_TICKS(5000));
    ssd1306_stop_scroll(handle);
    ssd1306_update_screen(handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
}