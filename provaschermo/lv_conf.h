/**
 * lv_conf.h - configurazione base per ESP32-S3 + JC3248W535C-I-Y (AXS15231B)
 * Risoluzione: 320x480
 * Colori: RGB565
 * Touch: opzionale
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/

#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0

/*====================
   RESOLUTION
 *====================*/

#define LV_HOR_RES_MAX     320
#define LV_VER_RES_MAX     480

/*====================
   PERFORMANCE
 *====================*/

#define LV_MEM_CUSTOM      0
#define LV_MEM_SIZE        (48U * 1024U)
#define LV_MEM_ADDR        0

#define LV_DRAW_BUF_DOUBLE 0

/*====================
   FEATURE CONFIG
 *====================*/

#define LV_USE_LOG         0
#define LV_USE_ASSERT_NULL 1

#define LV_USE_PERF_MONITOR 1

#define LV_USE_LABEL       1
#define LV_USE_BTN         1
#define LV_USE_SLIDER      1
#define LV_USE_SWITCH      1

#define LV_USE_THEME_DEFAULT 1

/*====================
   MISC
 *====================*/

#define LV_TICK_CUSTOM     0
#define LV_USE_USER_DATA   0

#endif // LV_CONF_H
