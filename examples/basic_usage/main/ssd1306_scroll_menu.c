/**
 * @file main.c
 * @author Muhamad Arif Hidayat
 * @brief Interactive menu application for ESP32 (ESP-IDF) with SSD1306 OLED display.
 * @version 1.0
 * @date 2025-06-30
 *
 * @details
 * This program displays a list of food menu items on a 128x64 OLED display.
 * Users can navigate the menu up and down using two buttons.
 * Visual indicators are shown to denote additional items outside the visible area.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ssd1306.h"

//==================================================================================
// PROJECT CONFIGURATION & DEFINITIONS
//==================================================================================

static const char *TAG = "MENU_APP";

// --- Hardware Configuration ---
#define I2C_SDA_PIN         GPIO_NUM_21     ///< I2C Data Pin
#define I2C_SCL_PIN         GPIO_NUM_22     ///< I2C Clock Pin
#define BUTTON_DOWN_PIN     GPIO_NUM_16     ///< Button Pin for Down Navigation
#define BUTTON_UP_PIN       GPIO_NUM_17     ///< Button Pin for Up Navigation
#define OLED_RST_PIN        -1              ///< OLED Reset Pin (-1 if unused)

// --- Application Behavior Settings ---
#define DEBOUNCE_TIME_MS    200             ///< Button debounce time in milliseconds (200ms is reliable)
#define MAX_MENU_ITEMS      14              ///< Total number of menu items
#define MAX_VISIBLE_ITEMS   4               ///< Number of menu items visible on screen at once

//==================================================================================
// DATA STRUCTURES & GLOBAL VARIABLES
//==================================================================================

/**
 * @struct menu_item_t
 * @brief Structure defining a single menu item.
 */
typedef struct {
    const char *name; ///< Name of the menu item
} menu_item_t;

// Initialize constant array of menu items
const menu_item_t menu_items[MAX_MENU_ITEMS] = {
    {"Nasi Goreng"}, {"Mie Ayam"},    {"Sate Ayam"},  {"Bakso"},
    {"Penyet"},      {"Tahu Tempe"},  {"Rendang"},    {"Soto Ayam"},
    {"Nasi Padang"}, {"Ayam Penyet"}, {"Nasi Uduk"},  {"Nasi Kuning"},
    {"Nasi Campur"}, {"Sop Buntut"}
};

// --- Application State Variables ---
static ssd1306_handle_t oled_handle = NULL;         ///< Handle for OLED display driver
static volatile int selected_item = 0;              ///< Index of currently selected menu item
static volatile bool menu_needs_update = true;      ///< Flag to trigger screen redraw
static QueueHandle_t button_evt_queue = NULL;       ///< Queue for button press events from ISR

//==================================================================================
// FUNCTIONS & TASKS
//==================================================================================

/**
 * @brief Interrupt Service Routine (ISR) for handling button presses.
 * @details This ISR is triggered on the falling edge of the button GPIO pins.
 *          It sends the triggered pin number to a queue for processing by another task,
 *          keeping the ISR short and efficient.
 * @param arg GPIO pin number configured for this ISR.
 */
static void IRAM_ATTR button_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(button_evt_queue, &gpio_num, NULL);
}

/**
 * @brief Task to process button logic and menu navigation.
 * @details This task waits for button events from the queue, applies debouncing,
 *          and updates the `selected_item` state based on the pressed button.
 * @param arg Task argument (unused in this implementation).
 */
static void button_task(void* arg) {
    uint32_t io_num;
    TickType_t last_press_time = 0;

    for (;;) {
        // Wait for button events from ISR indefinitely
        if (xQueueReceive(button_evt_queue, &io_num, portMAX_DELAY)) {
            TickType_t current_time = xTaskGetTickCount();

            // Debounce logic: Process event only if sufficient time has passed
            if ((current_time - last_press_time) * portTICK_PERIOD_MS > DEBOUNCE_TIME_MS) {
                // Brief delay to ensure stable button contact
                vTaskDelay(pdMS_TO_TICKS(20));

                // Confirm button is still pressed (active LOW)
                if (gpio_get_level(io_num) == 0) {
                    last_press_time = current_time; // Record valid press time
                    ESP_LOGI(TAG, "Valid button press detected on pin %ld", io_num);

                    // Navigation logic based on pressed button
                    if (io_num == BUTTON_DOWN_PIN) {
                        selected_item++;
                        // Wrap to first item if at the end
                        if (selected_item >= MAX_MENU_ITEMS) {
                            selected_item = 0;
                        }
                    } else if (io_num == BUTTON_UP_PIN) {
                        // Wrap to last item if at the beginning
                        if (selected_item == 0) {
                            selected_item = MAX_MENU_ITEMS - 1;
                        } else {
                            selected_item--;
                        }
                    }

                    menu_needs_update = true; // Set flag to redraw screen
                    ESP_LOGI(TAG, "Selected item: %d -> %s", selected_item, menu_items[selected_item].name);
                }
            }
        }
    }
}

/**
 * @brief Draws the entire menu interface to the display buffer.
 * @details This function handles all rendering operations, including the title,
 *          visible menu items, and scroll indicators.
 */
void draw_menu() {
    // Determine visible item window based on selected item
    int start_item = 0;
    if (selected_item >= MAX_VISIBLE_ITEMS) {
        start_item = selected_item - MAX_VISIBLE_ITEMS + 1;
    }

    ssd1306_clear_buffer(oled_handle);
    ssd1306_set_font(oled_handle, &FONT_5x7); // Switch to custom font if needed

    // --- Layout Constants for easy modification ---
    const int title_y = 7;                    ///< Y position for title
    const int menu_start_y = 20;              ///< Starting Y position for menu items
    const int menu_line_spacing = 12;         ///< Line spacing between menu items
    const int indicator_x = 122;              ///< X position for scroll indicators on right side

    // 1. Draw Menu Title
    // Center-align text by calculating its width (char_width * num_chars)
    const char* title_text = "MENU LIST";
    int title_x = (128 - (strlen(title_text) * 6)) / 2; // Font 5x7 has ~6px effective width
    ssd1306_set_cursor(oled_handle, title_x, title_y);
    ssd1306_print(oled_handle, title_text);
    ssd1306_draw_fast_hline(oled_handle, 0, 10, 128, OLED_COLOR_WHITE);

    // 2. Draw Visible Menu Items
    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        int current_item_index = start_item + i;
        if (current_item_index >= MAX_MENU_ITEMS) {
            break;
        }
        
        int y_pos = menu_start_y + (i * menu_line_spacing);
        ssd1306_set_cursor(oled_handle, 0, y_pos);

        if (current_item_index == selected_item) {
            ssd1306_print(oled_handle, "> ");
        } else {
            ssd1306_print(oled_handle, "  ");
        }
        
        ssd1306_print(oled_handle, menu_items[current_item_index].name);
    }

    // 3. Draw Scroll Indicators (if needed)
    // Show 'up' indicator if there are items before the visible list
    if (start_item > 0) {
        ssd1306_set_cursor(oled_handle, indicator_x, title_y);
        ssd1306_print(oled_handle, "^"); // Up arrow
    }
    // Show 'down' indicator if there are items after the visible list
    if (start_item + MAX_VISIBLE_ITEMS < MAX_MENU_ITEMS) {
        ssd1306_set_cursor(oled_handle, indicator_x, 56); // Y position at screen bottom
        ssd1306_print(oled_handle, "v"); // Down arrow
    }
    
    // Send rendered buffer to physical display
    ssd1306_update_screen(oled_handle);
}

/**
 * @brief Main application entry point.
 * @details Initializes hardware (OLED, GPIO), creates tasks, and enters the main loop
 *          to render the display.
 */
void app_main(void) {
    ESP_LOGI(TAG, "Starting Menu Application ...");

    // 1. Initialize OLED Display
    ssd1306_config_t oled_config = {
        .i2c_port = I2C_NUM_0,
        .i2c_addr = 0x3C,
        .sda_pin = I2C_SDA_PIN,
        .scl_pin = I2C_SCL_PIN,
        .rst_pin = OLED_RST_PIN,
        .screen_width = 128,
        .screen_height = 64,
        .i2c_clk_speed_hz = 400000
    };
    ESP_ERROR_CHECK(ssd1306_create(&oled_config, &oled_handle));
    ESP_LOGI(TAG, "SSD1306 driver initialized successfully.");

    // 2. Configure Buttons
    button_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = ((1ULL << BUTTON_DOWN_PIN) | (1ULL << BUTTON_UP_PIN)),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);
    
    // Install global ISR service
    gpio_install_isr_service(0);
    // Add ISR handlers for each button
    gpio_isr_handler_add(BUTTON_DOWN_PIN, button_isr_handler, (void*) BUTTON_DOWN_PIN);
    gpio_isr_handler_add(BUTTON_UP_PIN, button_isr_handler, (void*) BUTTON_UP_PIN);
    ESP_LOGI(TAG, "Buttons and ISR configured successfully.");

    // 3. Create Button Logic Task
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    // 4. Main Application Loop
    while (1) {
        // Redraw display only if state has changed (efficient)
        if (menu_needs_update) {
            ESP_LOGD(TAG, "Triggering menu redraw.");
            draw_menu();
            menu_needs_update = false; // Reset flag after update
        }
        // Delay to allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}