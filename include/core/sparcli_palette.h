#pragma once

#include "core/sparcli_core.h"

/**
 * @defgroup sc_palette Named RGB Palette
 * @brief Curated truecolor palette, additional to the ANSI colors.
 *
 * Each `SC_COLOR_*` is a 24-bit RGB `ScColor` (`index == -1`), distinct
 * from the terminal-palette `SC_ANSI_COLOR_*` macros. Zero-init is still
 * `SC_ANSI_COLOR_NONE`; these are explicit values you opt into.
 *
 * Each color is defined once as a brace-initializer `SC_COLOR_*_INIT` (usable
 * in a static/constant initializer - MSVC's C compiler rejects a compound
 * literal there); the value macro `SC_COLOR_*` casts it to a compound literal
 * for use in expressions. Single source of truth, both spellings available.
 * @{
 */

/* Standard colors */
#define SC_COLOR_RED_INIT              { -1, 243, 139, 139 } /* #f38b8b */
#define SC_COLOR_ORANGE_INIT           { -1, 248, 178, 136 } /* #f8b288 */
#define SC_COLOR_YELLOW_INIT           { -1, 249, 230, 175 } /* #f9e6af */
#define SC_COLOR_GREEN_INIT            { -1, 165, 227, 164 } /* #a5e3a4 */
#define SC_COLOR_CYAN_INIT             { -1, 148, 225, 239 } /* #94e1ef */
#define SC_COLOR_BLUE_INIT             { -1, 186, 213, 255 } /* #bad5ff */
#define SC_COLOR_PURPLE_INIT           { -1, 207, 173, 247 } /* #cfadf7 */
#define SC_COLOR_MAGENTA_INIT          { -1, 245, 159, 224 } /* #f59fe0 */
#define SC_COLOR_BLACK_INIT            { -1, 0, 0, 0 }       /* #000000 */
#define SC_COLOR_WHITE_INIT            { -1, 255, 255, 255 } /* #ffffff */

/* Vivid variants */
#define SC_COLOR_RED_VIVID_INIT        { -1, 255, 69, 87 }   /* #ff4557 */
#define SC_COLOR_ORANGE_VIVID_INIT     { -1, 255, 140, 58 }  /* #ff8c3a */
#define SC_COLOR_YELLOW_VIVID_INIT     { -1, 255, 213, 81 }  /* #ffd551 */
#define SC_COLOR_GREEN_VIVID_INIT      { -1, 0, 227, 53 }    /* #00e335 */
#define SC_COLOR_CYAN_VIVID_INIT       { -1, 64, 230, 255 }  /* #40e6ff */
#define SC_COLOR_BLUE_VIVID_INIT       { -1, 108, 165, 255 } /* #6ca5ff */
#define SC_COLOR_PURPLE_VIVID_INIT     { -1, 175, 77, 255 }  /* #af4dff */
#define SC_COLOR_MAGENTA_VIVID_INIT    { -1, 255, 81, 223 }  /* #ff51df */

/* Dark variants */
#define SC_COLOR_RED_DARK_INIT         { -1, 65, 11, 16 }    /* #410b10 */
#define SC_COLOR_ORANGE_DARK_INIT      { -1, 63, 30, 7 }     /* #3f1e07 */
#define SC_COLOR_YELLOW_DARK_INIT      { -1, 60, 48, 12 }    /* #3c300c */
#define SC_COLOR_GREEN_DARK_INIT       { -1, 0, 49, 5 }      /* #003105 */
#define SC_COLOR_CYAN_DARK_INIT        { -1, 8, 54, 61 }     /* #08363d */
#define SC_COLOR_BLUE_DARK_INIT        { -1, 21, 38, 64 }    /* #152640 */
#define SC_COLOR_PURPLE_DARK_INIT      { -1, 43, 13, 67 }    /* #2b0d43 */
#define SC_COLOR_MAGENTA_DARK_INIT     { -1, 64, 14, 55 }    /* #400e37 */

/* Background and foreground */
#define SC_COLOR_BG_INIT               { -1, 21, 21, 21 }    /* #151515 */
#define SC_COLOR_BG_DARKEN_1_INIT      { -1, 17, 17, 17 }    /* #111111 */
#define SC_COLOR_BG_DARKEN_2_INIT      { -1, 12, 12, 12 }    /* #0c0c0c */
#define SC_COLOR_BG_LIGHTEN_1_INIT     { -1, 26, 26, 26 }    /* #1a1a1a */
#define SC_COLOR_BG_LIGHTEN_2_INIT     { -1, 37, 37, 37 }    /* #252525 */
#define SC_COLOR_BG_LIGHTEN_3_INIT     { -1, 43, 43, 43 }    /* #2b2b2b */
#define SC_COLOR_BG_SELECTED_INIT      { -1, 3, 101, 198 }   /* #0365c6 */
#define SC_COLOR_FG_INIT               { -1, 212, 212, 212 } /* #d4d4d4 */
#define SC_COLOR_FG_DARKEN_1_INIT      { -1, 204, 204, 204 } /* #cccccc */
#define SC_COLOR_FG_DARKEN_2_INIT      { -1, 187, 187, 187 } /* #bbbbbb */
#define SC_COLOR_FG_DARKEN_3_INIT      { -1, 170, 170, 170 } /* #aaaaaa */
#define SC_COLOR_FG_LIGHTEN_1_INIT     { -1, 238, 238, 238 } /* #eeeeee */
#define SC_COLOR_FG_LIGHTEN_2_INIT     { -1, 253, 253, 253 } /* #fdfdfd */

/* Accent colors */
#define SC_COLOR_ACCENT_INIT           { -1, 140, 210, 204 } /* #8cd2cc */
#define SC_COLOR_ACCENT_DIM_INIT       { -1, 113, 162, 157 } /* #71a29d */
#define SC_COLOR_ACCENT_DARKER_INIT    { -1, 79, 114, 111 }  /* #4f726f */
#define SC_COLOR_ACCENT_DARK_INIT      { -1, 49, 73, 71 }    /* #314947 */
#define SC_COLOR_ACCENT_IMPORTANT_INIT { -1, 255, 236, 150 } /* #ffec96 */

/* Status colors */
#define SC_COLOR_ENABLED_INIT          { -1, 112, 223, 129 } /* #70df81 */
#define SC_COLOR_DISABLED_INIT         { -1, 224, 108, 117 } /* #e06c75 */
#define SC_COLOR_DISABLED_DIM_INIT     { -1, 128, 128, 128 } /* #808080 */

/* Diagnostics */
#define SC_COLOR_ERROR_INIT            { -1, 244, 135, 113 } /* #f48771 */
#define SC_COLOR_WARNING_INIT          { -1, 255, 185, 84 }  /* #ffb954 */
#define SC_COLOR_SUCCESS_INIT          { -1, 166, 227, 161 } /* #a6e3a1 */
#define SC_COLOR_INFO_INIT             { -1, 148, 225, 239 } /* #94e1ef */
#define SC_COLOR_HINT_INIT             { -1, 170, 170, 170 } /* #aaaaaa */
#define SC_COLOR_UNUSED_INIT           { -1, 98, 98, 98 }    /* #626262 */

#define SC_COLOR_RED              ((ScColor)SC_COLOR_RED_INIT)
#define SC_COLOR_ORANGE           ((ScColor)SC_COLOR_ORANGE_INIT)
#define SC_COLOR_YELLOW           ((ScColor)SC_COLOR_YELLOW_INIT)
#define SC_COLOR_GREEN            ((ScColor)SC_COLOR_GREEN_INIT)
#define SC_COLOR_CYAN             ((ScColor)SC_COLOR_CYAN_INIT)
#define SC_COLOR_BLUE             ((ScColor)SC_COLOR_BLUE_INIT)
#define SC_COLOR_PURPLE           ((ScColor)SC_COLOR_PURPLE_INIT)
#define SC_COLOR_MAGENTA          ((ScColor)SC_COLOR_MAGENTA_INIT)
#define SC_COLOR_BLACK            ((ScColor)SC_COLOR_BLACK_INIT)
#define SC_COLOR_WHITE            ((ScColor)SC_COLOR_WHITE_INIT)
#define SC_COLOR_RED_VIVID        ((ScColor)SC_COLOR_RED_VIVID_INIT)
#define SC_COLOR_ORANGE_VIVID     ((ScColor)SC_COLOR_ORANGE_VIVID_INIT)
#define SC_COLOR_YELLOW_VIVID     ((ScColor)SC_COLOR_YELLOW_VIVID_INIT)
#define SC_COLOR_GREEN_VIVID      ((ScColor)SC_COLOR_GREEN_VIVID_INIT)
#define SC_COLOR_CYAN_VIVID       ((ScColor)SC_COLOR_CYAN_VIVID_INIT)
#define SC_COLOR_BLUE_VIVID       ((ScColor)SC_COLOR_BLUE_VIVID_INIT)
#define SC_COLOR_PURPLE_VIVID     ((ScColor)SC_COLOR_PURPLE_VIVID_INIT)
#define SC_COLOR_MAGENTA_VIVID    ((ScColor)SC_COLOR_MAGENTA_VIVID_INIT)
#define SC_COLOR_RED_DARK         ((ScColor)SC_COLOR_RED_DARK_INIT)
#define SC_COLOR_ORANGE_DARK      ((ScColor)SC_COLOR_ORANGE_DARK_INIT)
#define SC_COLOR_YELLOW_DARK      ((ScColor)SC_COLOR_YELLOW_DARK_INIT)
#define SC_COLOR_GREEN_DARK       ((ScColor)SC_COLOR_GREEN_DARK_INIT)
#define SC_COLOR_CYAN_DARK        ((ScColor)SC_COLOR_CYAN_DARK_INIT)
#define SC_COLOR_BLUE_DARK        ((ScColor)SC_COLOR_BLUE_DARK_INIT)
#define SC_COLOR_PURPLE_DARK      ((ScColor)SC_COLOR_PURPLE_DARK_INIT)
#define SC_COLOR_MAGENTA_DARK     ((ScColor)SC_COLOR_MAGENTA_DARK_INIT)
#define SC_COLOR_BG               ((ScColor)SC_COLOR_BG_INIT)
#define SC_COLOR_BG_DARKEN_1      ((ScColor)SC_COLOR_BG_DARKEN_1_INIT)
#define SC_COLOR_BG_DARKEN_2      ((ScColor)SC_COLOR_BG_DARKEN_2_INIT)
#define SC_COLOR_BG_LIGHTEN_1     ((ScColor)SC_COLOR_BG_LIGHTEN_1_INIT)
#define SC_COLOR_BG_LIGHTEN_2     ((ScColor)SC_COLOR_BG_LIGHTEN_2_INIT)
#define SC_COLOR_BG_LIGHTEN_3     ((ScColor)SC_COLOR_BG_LIGHTEN_3_INIT)
#define SC_COLOR_BG_SELECTED      ((ScColor)SC_COLOR_BG_SELECTED_INIT)
#define SC_COLOR_FG               ((ScColor)SC_COLOR_FG_INIT)
#define SC_COLOR_FG_DARKEN_1      ((ScColor)SC_COLOR_FG_DARKEN_1_INIT)
#define SC_COLOR_FG_DARKEN_2      ((ScColor)SC_COLOR_FG_DARKEN_2_INIT)
#define SC_COLOR_FG_DARKEN_3      ((ScColor)SC_COLOR_FG_DARKEN_3_INIT)
#define SC_COLOR_FG_LIGHTEN_1     ((ScColor)SC_COLOR_FG_LIGHTEN_1_INIT)
#define SC_COLOR_FG_LIGHTEN_2     ((ScColor)SC_COLOR_FG_LIGHTEN_2_INIT)
#define SC_COLOR_ACCENT           ((ScColor)SC_COLOR_ACCENT_INIT)
#define SC_COLOR_ACCENT_DIM       ((ScColor)SC_COLOR_ACCENT_DIM_INIT)
#define SC_COLOR_ACCENT_DARKER    ((ScColor)SC_COLOR_ACCENT_DARKER_INIT)
#define SC_COLOR_ACCENT_DARK      ((ScColor)SC_COLOR_ACCENT_DARK_INIT)
#define SC_COLOR_ACCENT_IMPORTANT ((ScColor)SC_COLOR_ACCENT_IMPORTANT_INIT)
#define SC_COLOR_ENABLED          ((ScColor)SC_COLOR_ENABLED_INIT)
#define SC_COLOR_DISABLED         ((ScColor)SC_COLOR_DISABLED_INIT)
#define SC_COLOR_DISABLED_DIM     ((ScColor)SC_COLOR_DISABLED_DIM_INIT)
#define SC_COLOR_ERROR            ((ScColor)SC_COLOR_ERROR_INIT)
#define SC_COLOR_WARNING          ((ScColor)SC_COLOR_WARNING_INIT)
#define SC_COLOR_SUCCESS          ((ScColor)SC_COLOR_SUCCESS_INIT)
#define SC_COLOR_INFO             ((ScColor)SC_COLOR_INFO_INIT)
#define SC_COLOR_HINT             ((ScColor)SC_COLOR_HINT_INIT)
#define SC_COLOR_UNUSED           ((ScColor)SC_COLOR_UNUSED_INIT)

/** @} */
