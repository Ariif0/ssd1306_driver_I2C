/**
 * @file      ssd1306_fonts.h
 * @author    Muhamad Arif Hidayat
 * @brief     Defines data structures and handles for the SSD1306 font driver system.
 * @version   1.0
 * @date      2025-06-30
 * @copyright Copyright (c) 2025
 *
 * @details
 * This file is a core component for text rendering. It provides data structures
 * compatible with the Adafruit GFX font format, enabling portability and the use
 * of thousands of existing fonts. It also utilizes a unified handle
 * (`ssd1306_font_handle_t`) to abstract different font formats for future scalability.
 */

#ifndef SSD1306_FONTS_H
#define SSD1306_FONTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct GFXglyph
 * @brief  Defines metrics and location data for a single character (glyph).
 *
 * Each character in a font has this descriptor, which acts as a map for locating
 * and rendering its bitmap.
 */
typedef struct {
    uint16_t bitmapOffset; ///< Byte offset from the start of the bitmap to the glyph data.
    uint8_t  width;        ///< Width of the character bitmap in pixels.
    uint8_t  height;       ///< Height of the character bitmap in pixels.
    uint8_t  xAdvance;     ///< Horizontal distance to advance the cursor to the next character.
    int8_t   xOffset;      ///< Horizontal offset from the cursor to the top-left corner of the bitmap.
    int8_t   yOffset;      ///< Vertical offset from the baseline to the top-left corner of the bitmap.
} GFXglyph;

/**
 * @struct GFXfont
 * @brief  Main structure defining an entire font set.
 *
 * This structure contains all necessary data for a font, including pointers to
 * bitmap data, glyph descriptors, and character range information.
 */
typedef struct {
    const uint8_t* bitmap;   ///< Pointer to the bitmap data array for all characters.
    const GFXglyph* glyph;   ///< Pointer to the array of GFXglyph descriptors for each character.
    uint8_t         first;   ///< ASCII value of the first supported character.
    uint8_t         last;    ///< ASCII value of the last supported character.
    uint8_t         yAdvance; ///< Total line height in pixels; vertical distance to the next baseline.
} GFXfont;

/**
 * @enum ssd1306_font_type_t
 * @brief Identifies the base format of a font.
 *
 * This enum is designed for future scalability, allowing new font formats to be
 * added without altering the main API.
 */
typedef enum {
    FONT_TYPE_GFX, ///< Indicates a font using the GFXfont structure.
} ssd1306_font_type_t;

/**
 * @struct ssd1306_font_handle_t
 * @brief  Universal wrapper (handle) structure for font API.
 *
 * This handle is exposed to high-level API functions like `ssd1306_set_font`.
 * It abstracts the actual font format, enhancing modularity and manageability.
 */
typedef struct {
    ssd1306_font_type_t type;       ///< Font format type from the `ssd1306_font_type_t` enum.
    const void* font_data;          ///< Generic pointer to the actual font data (e.g., `GFXfont*`).
} ssd1306_font_handle_t;

/**
 * @defgroup Font_Declarations External Font Declarations
 * @brief Declarations of fonts available for the project.
 *
 * These fonts have their data definitions in a separate file, such as `ssd1306_font_data.c`.
 * @{
 */

/**
 * add all new font declarations here 
 */
#include "fonts/font5x7.h"
#include "fonts/FreeMono12pt7b.h"
#include "fonts/FreeSans9pt7b.h"


#ifdef __cplusplus
}
#endif

#endif // SSD1306_FONTS_H