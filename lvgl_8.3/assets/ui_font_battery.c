/*******************************************************************************
 * Size: 32 px
 * Bpp: 1
 * Opts: --bpp 1 --size 32 --font C:/XingNian/ESP32/ChunFeng/lvgl_8.3/assets/battery.ttf -o C:/XingNian/ESP32/ChunFeng/lvgl_8.3/assets\ui_font_battery.c --format lvgl -r 0x31 -r 0x33 -r 0x33 --symbols 123 --no-compress --no-prefilter
 ******************************************************************************/

#include "../ui.h"

#ifndef UI_FONT_BATTERY
#define UI_FONT_BATTERY 1
#endif

#if UI_FONT_BATTERY

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0031 "1" */
    0x7f, 0xff, 0xff, 0xc8, 0x0, 0x0, 0x4, 0xbc,
    0x0, 0x0, 0x4a, 0x20, 0x0, 0x7, 0xa2, 0x0,
    0x0, 0x5a, 0x20, 0x0, 0x5, 0xa1, 0x0, 0x0,
    0x5a, 0x10, 0x0, 0x5, 0xa1, 0x0, 0x0, 0x7b,
    0xf0, 0x0, 0x4, 0x80, 0x0, 0x0, 0x47, 0xff,
    0xff, 0xfc,

    /* U+0032 "2" */
    0x7f, 0xff, 0xff, 0xc8, 0x0, 0x0, 0x4, 0xbf,
    0xf8, 0x0, 0x4a, 0x0, 0x80, 0x7, 0xa0, 0x4,
    0x0, 0x5a, 0x0, 0x40, 0x5, 0xa0, 0x4, 0x0,
    0x5a, 0x0, 0x40, 0x5, 0xa0, 0x2, 0x0, 0x7b,
    0xff, 0xc0, 0x4, 0x80, 0x0, 0x0, 0x47, 0xff,
    0xff, 0xfc,

    /* U+0033 "3" */
    0x7f, 0xff, 0xff, 0x88, 0x0, 0x0, 0x4, 0xbf,
    0xff, 0xff, 0x4a, 0x0, 0x0, 0x17, 0xa0, 0x0,
    0x1, 0x5a, 0x0, 0x0, 0x15, 0xa0, 0x0, 0x1,
    0x5a, 0x0, 0x0, 0x15, 0xa0, 0x0, 0x1, 0x7b,
    0xff, 0xff, 0xf4, 0x80, 0x0, 0x0, 0x47, 0xff,
    0xff, 0xf8
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 512, .box_w = 28, .box_h = 12, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 42, .adv_w = 512, .box_w = 28, .box_h = 12, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 84, .adv_w = 512, .box_w = 28, .box_h = 12, .ofs_x = 2, .ofs_y = 6}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 49, .range_length = 3, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_battery = {
#else
lv_font_t ui_font_battery = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = -6,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_BATTERY*/

