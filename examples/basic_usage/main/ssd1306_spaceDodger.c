
/**
 * @file      main.c
 * @author    Muhamad Arif Hidayat
 * @brief     "Space Dodger" game for SSD1306 OLED display.
 * @version   1.0
 * @date      2025-07-03
 *
 * @details
 * A simple space-themed dodging game where the player controls a spaceship to avoid
 * asteroids and collect bonus points. Uses two buttons: GPIO 16 (Jump) to move up,
 * GPIO 17 (Restart) to start a new game.
 *
 * Hardware Setup:
 * - OLED Display (SSD1306 128x64): Connected via I2C (SDA: GPIO 21, SCL: GPIO 22).
 * - Buttons: Jump (GPIO 16), Restart (GPIO 17) with internal pull-up resistors.
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "ssd1306.h"

static const char *TAG = "SPACE_DODGER";

// -- Hardware Definitions --
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_PORT I2C_NUM_0
#define OLED_ADDR 0x3C

#define JUMP_BUTTON_PIN GPIO_NUM_16
#define RESTART_BUTTON_PIN GPIO_NUM_17

// -- Game Constants --
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PLAYER_WIDTH 8
#define PLAYER_HEIGHT 8
#define ASTEROID_SIZE 6
#define BONUS_SIZE 4
#define MAX_ASTEROIDS 3
#define STAR_COUNT 5

#define GRAVITY 0.5f
#define JUMP_STRENGTH -3.5f
#define INITIAL_GAME_SPEED 2.0f
#define MAX_GAME_SPEED 4.0f

// -- Game State Enumeration --
typedef enum {
    GAME_STATE_START_SCREEN,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER
} game_state_t;

// -- Game Object Structures --
typedef struct {
    float x, y;
    float vy; // Vertical velocity
    bool is_on_screen;
} Player;

typedef struct {
    float x, y;
    bool active;
} Asteroid;

typedef struct {
    float x, y;
    bool active;
} Bonus;

typedef struct {
    float x, y;
} Star;

const uint8_t player_sprite[] = {
    0b00011000, 0b00111100, 0b01111110, 0b11111111,
    0b11111111, 0b01111110, 0b00111100, 0b00011000,
};


const uint8_t bonus_sprite[] = {
    0b0110, 0b1111, 0b1111, 0b0110,
};

// Reset game state
void reset_game_state(Player *player, Asteroid *asteroids, Bonus *bonus, Star *stars, int *score, float *game_speed) {
    player->x = 10;
    player->y = SCREEN_HEIGHT / 2;
    player->vy = 0;
    player->is_on_screen = true;

    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        asteroids[i].x = SCREEN_WIDTH + i * (SCREEN_WIDTH / MAX_ASTEROIDS + (rand() % 20));
        asteroids[i].y = 10 + (rand() % (SCREEN_HEIGHT - 20));
        asteroids[i].active = true;
    }

    bonus->x = SCREEN_WIDTH + (rand() % 50);
    bonus->y = 10 + (rand() % (SCREEN_HEIGHT - 20));
    bonus->active = true;

    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x = rand() % SCREEN_WIDTH;
        stars[i].y = rand() % SCREEN_HEIGHT;
    }

    *score = 0;
    *game_speed = INITIAL_GAME_SPEED;
}

// Draw start screen animation (moving star)
void draw_start_screen_animation(ssd1306_handle_t handle, int offset) {
    int x = (offset % (SCREEN_WIDTH - 20)) + 10;
    ssd1306_draw_fast_hline(handle, x, 40, 10, OLED_COLOR_WHITE);
    ssd1306_draw_fast_vline(handle, x + 5, 35, 10, OLED_COLOR_WHITE);
}

// Game logic task
void game_task(void *pvParameters) {
    // 1. Configure the display
    ssd1306_config_t oled_config = {
        .i2c_port = I2C_MASTER_PORT,
        .i2c_addr = OLED_ADDR,
        .sda_pin = I2C_MASTER_SDA_IO,
        .scl_pin = I2C_MASTER_SCL_IO,
        .rst_pin = -1,
        .screen_width = SCREEN_WIDTH,
        .screen_height = SCREEN_HEIGHT,
        .i2c_clk_speed_hz = I2C_MASTER_FREQ_HZ
    };

    ssd1306_handle_t oled_handle = NULL;
    esp_err_t ret = ssd1306_create(&oled_config, &oled_handle);
    if (ret != ESP_OK || oled_handle == NULL) {
        ESP_LOGE(TAG, "OLED initialization failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "OLED initialized, free heap: %" PRIu32 " bytes", esp_get_free_heap_size());

    // 2. Configure Buttons
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << JUMP_BUTTON_PIN) | (1ULL << RESTART_BUTTON_PIN),
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    gpio_config(&io_conf);

    // 3. Initialize Game Variables
    Player player;
    Asteroid asteroids[MAX_ASTEROIDS];
    Bonus bonus;
    Star stars[STAR_COUNT];
    int score = 0;
    float game_speed = INITIAL_GAME_SPEED;
    game_state_t current_state = GAME_STATE_START_SCREEN;
    int animation_offset = 0;

    reset_game_state(&player, asteroids, &bonus, stars, &score, &game_speed);

    // 4. Main Game Loop
    while (1) {
        ssd1306_clear_buffer(oled_handle);

        switch (current_state) {
            case GAME_STATE_START_SCREEN:
                ssd1306_set_font(oled_handle, &FONT_5x7);
                ssd1306_set_text_size(oled_handle, 2);
                ssd1306_print_screen_center(oled_handle, "Space Dodger");
                ssd1306_set_text_size(oled_handle, 1);
                ssd1306_set_cursor(oled_handle, 10, 50);
                ssd1306_print(oled_handle, "Press JUMP to start");
                draw_start_screen_animation(oled_handle, animation_offset);
                animation_offset = (animation_offset + 2) % (SCREEN_WIDTH - 20);

                if (gpio_get_level(JUMP_BUTTON_PIN) == 0) {
                    reset_game_state(&player, asteroids, &bonus, stars, &score, &game_speed);
                    current_state = GAME_STATE_PLAYING;
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                break;

            case GAME_STATE_PLAYING:
                // Handle player input and physics
                if (gpio_get_level(JUMP_BUTTON_PIN) == 0) {
                    player.vy = JUMP_STRENGTH;
                }
                player.vy += GRAVITY;
                player.y += player.vy;

                // Keep player on screen
                if (player.y < 0) {
                    player.y = 0;
                    player.vy = 0;
                } else if (player.y > SCREEN_HEIGHT - PLAYER_HEIGHT) {
                    player.y = SCREEN_HEIGHT - PLAYER_HEIGHT;
                    player.vy = 0;
                }

                // Update asteroids
                for (int i = 0; i < MAX_ASTEROIDS; i++) {
                    if (asteroids[i].active) {
                        asteroids[i].x -= game_speed;
                        if (asteroids[i].x < -ASTEROID_SIZE) {
                            asteroids[i].x = SCREEN_WIDTH + (rand() % 40);
                            asteroids[i].y = 10 + (rand() % (SCREEN_HEIGHT - 20));
                            score++;
                            if (game_speed < MAX_GAME_SPEED) {
                                game_speed += 0.05f;
                            }
                        }
                    }
                }

                // Update bonus
                if (bonus.active) {
                    bonus.x -= game_speed;
                    if (bonus.x < -BONUS_SIZE) {
                        bonus.x = SCREEN_WIDTH + (rand() % 100);
                        bonus.y = 10 + (rand() % (SCREEN_HEIGHT - 20));
                    }
                }

                // Update stars (background)
                for (int i = 0; i < STAR_COUNT; i++) {
                    stars[i].x -= game_speed / 2;
                    if (stars[i].x < 0) {
                        stars[i].x = SCREEN_WIDTH;
                        stars[i].y = rand() % SCREEN_HEIGHT;
                    }
                }

                // Collision detection (asteroids)
                for (int i = 0; i < MAX_ASTEROIDS; i++) {
                    if (asteroids[i].active &&
                        player.x < asteroids[i].x + ASTEROID_SIZE &&
                        player.x + PLAYER_WIDTH > asteroids[i].x &&
                        player.y < asteroids[i].y + ASTEROID_SIZE &&
                        player.y + PLAYER_HEIGHT > asteroids[i].y) {
                        current_state = GAME_STATE_GAME_OVER;
                        break;
                    }
                }

                // Bonus collection
                if (bonus.active &&
                    player.x < bonus.x + BONUS_SIZE &&
                    player.x + PLAYER_WIDTH > bonus.x &&
                    player.y < bonus.y + BONUS_SIZE &&
                    player.y + PLAYER_HEIGHT > bonus.y) {
                    score += 5; // Bonus points
                    bonus.active = false;
                    bonus.x = SCREEN_WIDTH + (rand() % 100);
                    bonus.y = 10 + (rand() % (SCREEN_HEIGHT - 20));
                }

                // Draw game elements
                for (int i = 0; i < STAR_COUNT; i++) {
                    ssd1306_draw_fast_hline(oled_handle, (int)stars[i].x, (int)stars[i].y, 2, OLED_COLOR_WHITE);
                }
                ssd1306_draw_bitmap(oled_handle, (int)player.x, (int)player.y, player_sprite, PLAYER_WIDTH, PLAYER_HEIGHT, OLED_COLOR_WHITE);
                for (int i = 0; i < MAX_ASTEROIDS; i++) {
                    if (asteroids[i].active) {
                        ssd1306_draw_fast_hline(oled_handle, (int)asteroids[i].x, (int)asteroids[i].y, ASTEROID_SIZE, OLED_COLOR_WHITE);
                        ssd1306_draw_fast_vline(oled_handle, (int)asteroids[i].x, (int)asteroids[i].y, ASTEROID_SIZE, OLED_COLOR_WHITE);
                        ssd1306_draw_fast_vline(oled_handle, (int)asteroids[i].x + ASTEROID_SIZE - 1, (int)asteroids[i].y, ASTEROID_SIZE, OLED_COLOR_WHITE);
                    }
                }
                if (bonus.active) {
                    ssd1306_draw_bitmap(oled_handle, (int)bonus.x, (int)bonus.y, bonus_sprite, BONUS_SIZE, BONUS_SIZE, OLED_COLOR_WHITE);
                }

                // Draw score
                char score_str[16];
                snprintf(score_str, sizeof(score_str), "Score: %d", score);
                ssd1306_set_font(oled_handle, &FONT_5x7);
                ssd1306_set_cursor(oled_handle, 0, 0);
                ssd1306_print(oled_handle, score_str);
                break;

            case GAME_STATE_GAME_OVER:
                ssd1306_set_font(oled_handle, &FONT_5x7);
                ssd1306_set_text_size(oled_handle, 2);
                ssd1306_print_centered_h(oled_handle, "GAME OVER", 15);
                char final_score_str[16];
                snprintf(final_score_str, sizeof(final_score_str), "Score: %d", score);
                ssd1306_set_text_size(oled_handle, 1);
                ssd1306_print_centered_h(oled_handle, final_score_str, 35);
                ssd1306_print_centered_h(oled_handle, "Press RESTART", 50);

                if (gpio_get_level(RESTART_BUTTON_PIN) == 0) {
                    current_state = GAME_STATE_START_SCREEN;
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                break;
        }

        ssd1306_update_screen(oled_handle);
        vTaskDelay(pdMS_TO_TICKS(25)); // Control game speed
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Space Dodger Game, free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
    xTaskCreate(game_task, "game_task", 4096, NULL, 5, NULL);
}
