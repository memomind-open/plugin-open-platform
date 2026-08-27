#ifndef LV_SYMBOL_DEF_H
#define LV_SYMBOL_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lv_conf_internal.h"

/*-------------------------------
 * Symbols from "normal" font
 *-----------------------------*/
#if !defined LV_SYMBOL_BULLET
#define LV_SYMBOL_BULLET          "\xE2\x80\xA2" /*20042, 0x2022*/
#endif

/*-------------------------------
 * Symbols from FontAwesome font
 *-----------------------------*/

/*In the font converter use this list as range:
      61441, 61448, 61451, 61452, 61453, 61457, 61459, 61461, 61465, 61468,
      61473, 61478, 61479, 61480, 61502, 61507, 61512, 61515, 61516, 61517,
      61521, 61522, 61523, 61524, 61543, 61544, 61550, 61552, 61553, 61556,
      61559, 61560, 61561, 61563, 61587, 61589, 61636, 61637, 61639, 61641,
      61664, 61671, 61674, 61683, 61724, 61732, 61787, 61931, 62016, 62017,
      62018, 62019, 62020, 62087, 62099, 62189, 62212, 62810, 63426, 63650
*/

/* These symbols can be prefined in the lv_conf.h file.
 * If they are not predefined, they will use the following values
 */

#if !defined LV_SYMBOL_CHECK
#define LV_SYMBOL_CHECK        "\xEF\x80\x8c" /*61452, 0xF00C*/
#endif

#if !defined LV_SYMBOL_UP_ARROW
#define LV_SYMBOL_UP_ARROW        "\xEF\x81\xA2" /*61441, 0xF062*/
#endif

#if !defined LV_SYMBOL_DOWN_ARROW
#define LV_SYMBOL_DOWN_ARROW       "\xEF\x81\xA3" /*61441, 0xF063*/
#endif

#if !defined LV_SYMBOL_TRIANGLE_DOWN_ARROW
#define LV_SYMBOL_TRIANGLE_DOWN_ARROW        "\xEF\x83\x97" /*61655, 0xF0D7*/
#endif

#if !defined LV_SYMBOL_TRIANGLE_UP_ARROW
#define LV_SYMBOL_TRIANGLE_UP_ARROW       "\xEF\x83\x98" /*61656, 0xF0D8*/
#endif

#if !defined LV_SYMBOL_AUDIO
#define LV_SYMBOL_AUDIO           "\xEF\x80\x81" /*61441, 0xF001*/
#endif

#if !defined LV_SYMBOL_VIDEO
#define LV_SYMBOL_VIDEO           "\xEF\x80\x88" /*61448, 0xF008*/
#endif

#if !defined LV_SYMBOL_LIST
#define LV_SYMBOL_LIST            "\xEF\x80\x8B" /*61451, 0xF00B*/
#endif

#if !defined LV_SYMBOL_OK
#define LV_SYMBOL_OK              "\xEF\x80\x8C" /*61452, 0xF00C*/
#endif

#if !defined LV_SYMBOL_CLOSE
#define LV_SYMBOL_CLOSE           "\xEF\x80\x8D" /*61453, 0xF00D*/
#endif

#if !defined LV_SYMBOL_POWER
#define LV_SYMBOL_POWER           "\xEF\x80\x91" /*61457, 0xF011*/
#endif

#if !defined LV_SYMBOL_SETTINGS
#define LV_SYMBOL_SETTINGS        "\xEF\x80\x93" /*61459, 0xF013*/
#endif

#if !defined LV_SYMBOL_HOME
#define LV_SYMBOL_HOME            "\xEF\x80\x95" /*61461, 0xF015*/
#endif

#if !defined LV_SYMBOL_DOWNLOAD
#define LV_SYMBOL_DOWNLOAD        "\xEF\x80\x99" /*61465, 0xF019*/
#endif

#if !defined LV_SYMBOL_DRIVE
#define LV_SYMBOL_DRIVE           "\xEF\x80\x9C" /*61468, 0xF01C*/
#endif

#if !defined LV_SYMBOL_REFRESH
#define LV_SYMBOL_REFRESH         "\xEF\x80\xA1" /*61473, 0xF021*/
#endif

#if !defined LV_SYMBOL_MUTE
#define LV_SYMBOL_MUTE            "\xEF\x80\xA6" /*61478, 0xF026*/
#endif

#if !defined LV_SYMBOL_VOLUME_MID
#define LV_SYMBOL_VOLUME_MID      "\xEF\x80\xA7" /*61479, 0xF027*/
#endif

#if !defined LV_SYMBOL_VOLUME_MAX
#define LV_SYMBOL_VOLUME_MAX      "\xEF\x80\xA8" /*61480, 0xF028*/
#endif

#if !defined LV_SYMBOL_IMAGE
#define LV_SYMBOL_IMAGE           "\xEF\x80\xBE" /*61502, 0xF03E*/
#endif

#if !defined LV_SYMBOL_TINT
#define LV_SYMBOL_TINT            "\xEF\x81\x83" /*61507, 0xF043*/
#endif

#if !defined LV_SYMBOL_PREV
#define LV_SYMBOL_PREV            "\xEF\x81\x88" /*61512, 0xF048*/
#endif

#if !defined LV_SYMBOL_PLAY
#define LV_SYMBOL_PLAY            "\xEF\x81\x8B" /*61515, 0xF04B*/
#endif

#if !defined LV_SYMBOL_PAUSE
#define LV_SYMBOL_PAUSE           "\xEF\x81\x8C" /*61516, 0xF04C*/
#endif

#if !defined LV_SYMBOL_STOP
#define LV_SYMBOL_STOP            "\xEF\x81\x8D" /*61517, 0xF04D*/
#endif

#if !defined LV_SYMBOL_NEXT
#define LV_SYMBOL_NEXT            "\xEF\x81\x91" /*61521, 0xF051*/
#endif

#if !defined LV_SYMBOL_EJECT
#define LV_SYMBOL_EJECT           "\xEF\x81\x92" /*61522, 0xF052*/
#endif

#if !defined LV_SYMBOL_LEFT
#define LV_SYMBOL_LEFT            "\xEF\x81\x93" /*61523, 0xF053*/
#endif

#if !defined LV_SYMBOL_RIGHT
#define LV_SYMBOL_RIGHT           "\xEF\x81\x94" /*61524, 0xF054*/
#endif

#if !defined LV_SYMBOL_PLUS
#define LV_SYMBOL_PLUS            "\xEF\x81\xA7" /*61543, 0xF067*/
#endif

#if !defined LV_SYMBOL_MINUS
#define LV_SYMBOL_MINUS           "\xEF\x81\xA8" /*61544, 0xF068*/
#endif

#if !defined LV_SYMBOL_EYE_OPEN
#define LV_SYMBOL_EYE_OPEN        "\xEF\x81\xAE" /*61550, 0xF06E*/
#endif

#if !defined LV_SYMBOL_EYE_CLOSE
#define LV_SYMBOL_EYE_CLOSE       "\xEF\x81\xB0" /*61552, 0xF070*/
#endif

#if !defined LV_SYMBOL_WARNING
#define LV_SYMBOL_WARNING         "\xEF\x81\xB1" /*61553, 0xF071*/
#endif

#if !defined LV_SYMBOL_SHUFFLE
#define LV_SYMBOL_SHUFFLE         "\xEF\x81\xB4" /*61556, 0xF074*/
#endif

#if !defined LV_SYMBOL_UP
#define LV_SYMBOL_UP              "\xEF\x81\xB7" /*61559, 0xF077*/
#endif

#if !defined LV_SYMBOL_DOWN
#define LV_SYMBOL_DOWN            "\xEF\x81\xB8" /*61560, 0xF078*/
#endif

#if !defined LV_SYMBOL_LOOP
#define LV_SYMBOL_LOOP            "\xEF\x81\xB9" /*61561, 0xF079*/
#endif

#if !defined LV_SYMBOL_DIRECTORY
#define LV_SYMBOL_DIRECTORY       "\xEF\x81\xBB" /*61563, 0xF07B*/
#endif

#if !defined LV_SYMBOL_UPLOAD
#define LV_SYMBOL_UPLOAD          "\xEF\x82\x93" /*61587, 0xF093*/
#endif

#if !defined LV_SYMBOL_CALL
#define LV_SYMBOL_CALL            "\xEF\x82\x95" /*61589, 0xF095*/
#endif

#if !defined LV_SYMBOL_CUT
#define LV_SYMBOL_CUT             "\xEF\x83\x84" /*61636, 0xF0C4*/
#endif

#if !defined LV_SYMBOL_COPY
#define LV_SYMBOL_COPY            "\xEF\x83\x85" /*61637, 0xF0C5*/
#endif

#if !defined LV_SYMBOL_SAVE
#define LV_SYMBOL_SAVE            "\xEF\x83\x87" /*61639, 0xF0C7*/
#endif

#if !defined LV_SYMBOL_BARS
#define LV_SYMBOL_BARS            "\xEF\x83\x89" /*61641, 0xF0C9*/
#endif

#if !defined LV_SYMBOL_ENVELOPE
#define LV_SYMBOL_ENVELOPE        "\xEF\x83\xA0" /*61664, 0xF0E0*/
#endif

#if !defined LV_SYMBOL_CHARGE
#define LV_SYMBOL_CHARGE          "\xEF\x83\xA7" /*61671, 0xF0E7*/
#endif

#if !defined LV_SYMBOL_PASTE
#define LV_SYMBOL_PASTE           "\xEF\x83\xAA" /*61674, 0xF0EA*/
#endif

#if !defined LV_SYMBOL_BELL
#define LV_SYMBOL_BELL            "\xEF\x83\xB3" /*61683, 0xF0F3*/
#endif

#if !defined LV_SYMBOL_KEYBOARD
#define LV_SYMBOL_KEYBOARD        "\xEF\x84\x9C" /*61724, 0xF11C*/
#endif

#if !defined LV_SYMBOL_GPS
#define LV_SYMBOL_GPS             "\xEF\x84\xA4" /*61732, 0xF124*/
#endif

#if !defined LV_SYMBOL_FILE
#define LV_SYMBOL_FILE            "\xEF\x85\x9B" /*61787, 0xF158*/
#endif

#if !defined LV_SYMBOL_WIFI
#define LV_SYMBOL_WIFI            "\xEF\x87\xAB" /*61931, 0xF1EB*/
#endif

#if !defined LV_SYMBOL_BATTERY_FULL
#define LV_SYMBOL_BATTERY_FULL    "\xEF\x89\x80" /*62016, 0xF240*/
#endif

#if !defined LV_SYMBOL_BATTERY_3
#define LV_SYMBOL_BATTERY_3       "\xEF\x89\x81" /*62017, 0xF241*/
#endif

#if !defined LV_SYMBOL_BATTERY_2
#define LV_SYMBOL_BATTERY_2       "\xEF\x89\x82" /*62018, 0xF242*/
#endif

#if !defined LV_SYMBOL_BATTERY_1
#define LV_SYMBOL_BATTERY_1       "\xEF\x89\x83" /*62019, 0xF243*/
#endif

#if !defined LV_SYMBOL_BATTERY_EMPTY
#define LV_SYMBOL_BATTERY_EMPTY   "\xEF\x89\x84" /*62020, 0xF244*/
#endif

#if !defined LV_SYMBOL_USB
#define LV_SYMBOL_USB             "\xEF\x8a\x87" /*62087, 0xF287*/
#endif

#if !defined LV_SYMBOL_BLUETOOTH
#define LV_SYMBOL_BLUETOOTH       "\xEF\x8a\x93" /*62099, 0xF293*/
#endif

#if !defined LV_SYMBOL_TRASH
#define LV_SYMBOL_TRASH           "\xEF\x8B\xAD" /*62189, 0xF2ED*/
#endif

#if !defined LV_SYMBOL_EDIT
#define LV_SYMBOL_EDIT            "\xEF\x8C\x84" /*62212, 0xF304*/
#endif

#if !defined LV_SYMBOL_BACKSPACE
#define LV_SYMBOL_BACKSPACE       "\xEF\x95\x9A" /*62810, 0xF55A*/
#endif

#if !defined LV_SYMBOL_SD_CARD
#define LV_SYMBOL_SD_CARD         "\xEF\x9F\x82" /*63426, 0xF7C2*/
#endif

#if !defined LV_SYMBOL_NEW_LINE
#define LV_SYMBOL_NEW_LINE        "\xEF\xA2\xA2" /*63650, 0xF8A2*/
#endif

#if !defined LV_SYMBOL_DUMMY
/** Invalid symbol at (U+F8FF). If written before a string then `lv_img` will show it as a label*/
#define LV_SYMBOL_DUMMY           "\xEF\xA3\xBF"
#endif

#if LV_FONT_XGIMI_WEATHER
#define LV_WEATHER_100       "\xef\x84\x81" /* 0xF101*/
#define LV_WEATHER_101       "\xef\x84\x82" /* 0xF102*/
#define LV_WEATHER_102       "\xef\x84\x83" /* 0xF103*/
#define LV_WEATHER_103       "\xef\x84\x84" /* 0xF104*/
#define LV_WEATHER_104       "\xef\x84\x85" /* 0xF105*/
#define LV_WEATHER_150       "\xef\x84\x86" /* 0xF106*/
#define LV_WEATHER_151       "\xef\x84\x87" /* 0xF107*/
#define LV_WEATHER_152       "\xef\x84\x88" /* 0xF108*/
#define LV_WEATHER_153       "\xef\x84\x89" /* 0xF109*/

#define LV_WEATHER_300       "\xef\x84\x8a" /* 0xf10a*/
#define LV_WEATHER_301       "\xef\x84\x8b" /* 0xf10b*/
#define LV_WEATHER_302       "\xef\x84\x8c" /* 0xf10c*/
#define LV_WEATHER_303       "\xef\x84\x8d" /* 0xf10d*/
#define LV_WEATHER_304       "\xef\x84\x8e" /* 0xf10e*/
#define LV_WEATHER_305       "\xef\x84\x8f" /* 0xf10f*/
#define LV_WEATHER_306       "\xef\x84\x90" /* 0xf110*/
#define LV_WEATHER_307       "\xef\x84\x91" /* 0xf111*/
#define LV_WEATHER_308       "\xef\x84\x92" /* 0xf112*/
#define LV_WEATHER_309       "\xef\x84\x93" /* 0xf113*/
#define LV_WEATHER_310       "\xef\x84\x94" /* 0xf114*/
#define LV_WEATHER_311       "\xef\x84\x95" /* 0xf115*/
#define LV_WEATHER_312       "\xef\x84\x96" /* 0xf116*/
#define LV_WEATHER_313       "\xef\x84\x97" /* 0xf117*/
#define LV_WEATHER_314       "\xef\x84\x98" /* 0xf118*/
#define LV_WEATHER_315       "\xef\x84\x99" /* 0xf119*/
#define LV_WEATHER_316       "\xef\x84\x9a" /* 0xf11a*/
#define LV_WEATHER_317       "\xef\x84\x9b" /* 0xf11b*/
#define LV_WEATHER_318       "\xef\x84\x9c" /* 0xf11c*/

#define LV_WEATHER_350       "\xef\x84\x9d" /* 0xf11d*/
#define LV_WEATHER_351       "\xef\x84\x9e" /* 0xf11e*/

#define LV_WEATHER_399       "\xef\x84\x9f" /* 0xf11f*/
#define LV_WEATHER_400       "\xef\x84\xa0" /* 0xf120*/
#define LV_WEATHER_401       "\xef\x84\xa1" /* 0xf121*/
#define LV_WEATHER_402       "\xef\x84\xa2" /* 0xf122*/
#define LV_WEATHER_403       "\xef\x84\xa3" /* 0xf123*/
#define LV_WEATHER_404       "\xef\x84\xa4" /* 0xf124*/
#define LV_WEATHER_405       "\xef\x84\xa5" /* 0xf125*/
#define LV_WEATHER_406       "\xef\x84\xa6" /* 0xf126*/
#define LV_WEATHER_407       "\xef\x84\xa7" /* 0xf127*/
#define LV_WEATHER_408       "\xef\x84\xa8" /* 0xf128*/
#define LV_WEATHER_409       "\xef\x84\xa9" /* 0xf129*/
#define LV_WEATHER_410       "\xef\x84\xaa" /* 0xf12a*/
#define LV_WEATHER_456       "\xef\x84\xab" /* 0xf12b*/
#define LV_WEATHER_457       "\xef\x84\xac" /* 0xf12c*/
#define LV_WEATHER_499       "\xef\x84\xad" /* 0xf12d*/
#define LV_WEATHER_500       "\xef\x84\xae" /* 0xf12e*/
#define LV_WEATHER_501       "\xef\x84\xaf" /* 0xf12f*/
#define LV_WEATHER_502       "\xef\x84\xb0" /* 0xf130*/
#define LV_WEATHER_503       "\xef\x84\xb1" /* 0xf131*/
#define LV_WEATHER_504       "\xef\x84\xb2" /* 0xf132*/
#define LV_WEATHER_507       "\xef\x84\xb3" /* 0xf133*/
#define LV_WEATHER_508       "\xef\x84\xb4" /* 0xf134*/
#define LV_WEATHER_509       "\xef\x84\xb5" /* 0xf135*/
#define LV_WEATHER_510       "\xef\x84\xb6" /* 0xf136*/
#define LV_WEATHER_511       "\xef\x84\xb7" /* 0xf137*/
#define LV_WEATHER_512       "\xef\x84\xb8" /* 0xf138*/
#define LV_WEATHER_513       "\xef\x84\xb9" /* 0xf139*/
#define LV_WEATHER_514       "\xef\x84\xba" /* 0xf13a*/
#define LV_WEATHER_515       "\xef\x84\xbb" /* 0xf13b*/
#define LV_WEATHER_800       "\xef\x84\xbc" /* 0xf13c*/
#define LV_WEATHER_801       "\xef\x84\xbd" /* 0xf13d*/
#define LV_WEATHER_802       "\xef\x84\xbe" /* 0xf13e*/
#define LV_WEATHER_803       "\xef\x84\xbf" /* 0xf13f*/
#define LV_WEATHER_804       "\xef\x85\x80" /* 0xf140*/
#define LV_WEATHER_805       "\xef\x85\x81" /* 0xf141*/
#define LV_WEATHER_806       "\xef\x85\x82" /* 0xf142*/
#define LV_WEATHER_807       "\xef\x85\x83" /* 0xf143*/
#define LV_WEATHER_900       "\xef\x85\x84" /* 0xf144*/
#define LV_WEATHER_901       "\xef\x85\x85" /* 0xf145*/
#define LV_WEATHER_999       "\xef\x85\x86" /* 0xf146*/
#endif /*LV_FONT_XGIMI_WEATHER*/

static inline const char* get_lv_weather_symbol(int code)
{
#if LV_FONT_XGIMI_WEATHER
    switch(code) {
        case 100: return LV_WEATHER_100;
        case 101: return LV_WEATHER_101;
        case 102: return LV_WEATHER_102;
        case 103: return LV_WEATHER_103;
        case 104: return LV_WEATHER_104;
        case 150: return LV_WEATHER_150;
        case 151: return LV_WEATHER_151;
        case 152: return LV_WEATHER_152;
        case 153: return LV_WEATHER_153;
        case 300: return LV_WEATHER_300;
        case 301: return LV_WEATHER_301;
        case 302: return LV_WEATHER_302;
        case 303: return LV_WEATHER_303;
        case 304: return LV_WEATHER_304;
        case 305: return LV_WEATHER_305;
        case 306: return LV_WEATHER_306;
        case 307: return LV_WEATHER_307;
        case 308: return LV_WEATHER_308;
        case 309: return LV_WEATHER_309;
        case 310: return LV_WEATHER_310;
        case 311: return LV_WEATHER_311;
        case 312: return LV_WEATHER_312;
        case 313: return LV_WEATHER_313;
        case 314: return LV_WEATHER_314;
        case 315: return LV_WEATHER_315;
        case 316: return LV_WEATHER_316;
        case 317: return LV_WEATHER_317;
        case 318: return LV_WEATHER_318;
        case 350: return LV_WEATHER_350;
        case 351: return LV_WEATHER_351;
        case 399: return LV_WEATHER_399;
        case 400: return LV_WEATHER_400;
        case 401: return LV_WEATHER_401;
        case 402: return LV_WEATHER_402;
        case 403: return LV_WEATHER_403;
        case 404: return LV_WEATHER_404;
        case 405: return LV_WEATHER_405;
        case 406: return LV_WEATHER_406;
        case 407: return LV_WEATHER_407;
        case 408: return LV_WEATHER_408;
        case 409: return LV_WEATHER_409;
        case 410: return LV_WEATHER_410;
        case 456: return LV_WEATHER_456;
        case 457: return LV_WEATHER_457;
        case 499: return LV_WEATHER_499;
        case 500: return LV_WEATHER_500;
        case 501: return LV_WEATHER_501;
        case 502: return LV_WEATHER_502;
        case 503: return LV_WEATHER_503;
        case 504: return LV_WEATHER_504;
        case 507: return LV_WEATHER_507;
        case 508: return LV_WEATHER_508;
        case 509: return LV_WEATHER_509;
        case 510: return LV_WEATHER_510;
        case 511: return LV_WEATHER_511;
        case 512: return LV_WEATHER_512;
        case 513: return LV_WEATHER_513;
        case 514: return LV_WEATHER_514;
        case 515: return LV_WEATHER_515;
        case 800: return LV_WEATHER_800;
        case 801: return LV_WEATHER_801;
        case 802: return LV_WEATHER_802;
        case 803: return LV_WEATHER_803;
        case 804: return LV_WEATHER_804;
        case 805: return LV_WEATHER_805;
        case 806: return LV_WEATHER_806;
        case 807: return LV_WEATHER_807;
        case 900: return LV_WEATHER_900;
        case 901: return LV_WEATHER_901;
        default:  return LV_WEATHER_999;
    }
#else
    (void)code;
    return "";
#endif
}

/*
 * The following list is generated using
 * cat src/font/lv_symbol_def.h | sed -E -n 's/^#define\s+LV_(SYMBOL_\w+).*".*$/    _LV_STR_\1,/p'
 */
enum {
    _LV_STR_SYMBOL_BULLET,
    _LV_STR_SYMBOL_AUDIO,
    _LV_STR_SYMBOL_VIDEO,
    _LV_STR_SYMBOL_LIST,
    _LV_STR_SYMBOL_OK,
    _LV_STR_SYMBOL_CLOSE,
    _LV_STR_SYMBOL_POWER,
    _LV_STR_SYMBOL_SETTINGS,
    _LV_STR_SYMBOL_HOME,
    _LV_STR_SYMBOL_DOWNLOAD,
    _LV_STR_SYMBOL_DRIVE,
    _LV_STR_SYMBOL_REFRESH,
    _LV_STR_SYMBOL_MUTE,
    _LV_STR_SYMBOL_VOLUME_MID,
    _LV_STR_SYMBOL_VOLUME_MAX,
    _LV_STR_SYMBOL_IMAGE,
    _LV_STR_SYMBOL_TINT,
    _LV_STR_SYMBOL_PREV,
    _LV_STR_SYMBOL_PLAY,
    _LV_STR_SYMBOL_PAUSE,
    _LV_STR_SYMBOL_STOP,
    _LV_STR_SYMBOL_NEXT,
    _LV_STR_SYMBOL_EJECT,
    _LV_STR_SYMBOL_LEFT,
    _LV_STR_SYMBOL_RIGHT,
    _LV_STR_SYMBOL_PLUS,
    _LV_STR_SYMBOL_MINUS,
    _LV_STR_SYMBOL_EYE_OPEN,
    _LV_STR_SYMBOL_EYE_CLOSE,
    _LV_STR_SYMBOL_WARNING,
    _LV_STR_SYMBOL_SHUFFLE,
    _LV_STR_SYMBOL_UP,
    _LV_STR_SYMBOL_DOWN,
    _LV_STR_SYMBOL_LOOP,
    _LV_STR_SYMBOL_DIRECTORY,
    _LV_STR_SYMBOL_UPLOAD,
    _LV_STR_SYMBOL_CALL,
    _LV_STR_SYMBOL_CUT,
    _LV_STR_SYMBOL_COPY,
    _LV_STR_SYMBOL_SAVE,
    _LV_STR_SYMBOL_BARS,
    _LV_STR_SYMBOL_ENVELOPE,
    _LV_STR_SYMBOL_CHARGE,
    _LV_STR_SYMBOL_PASTE,
    _LV_STR_SYMBOL_BELL,
    _LV_STR_SYMBOL_KEYBOARD,
    _LV_STR_SYMBOL_GPS,
    _LV_STR_SYMBOL_FILE,
    _LV_STR_SYMBOL_WIFI,
    _LV_STR_SYMBOL_BATTERY_FULL,
    _LV_STR_SYMBOL_BATTERY_3,
    _LV_STR_SYMBOL_BATTERY_2,
    _LV_STR_SYMBOL_BATTERY_1,
    _LV_STR_SYMBOL_BATTERY_EMPTY,
    _LV_STR_SYMBOL_USB,
    _LV_STR_SYMBOL_BLUETOOTH,
    _LV_STR_SYMBOL_TRASH,
    _LV_STR_SYMBOL_EDIT,
    _LV_STR_SYMBOL_BACKSPACE,
    _LV_STR_SYMBOL_SD_CARD,
    _LV_STR_SYMBOL_NEW_LINE,
    _LV_STR_SYMBOL_DUMMY,
};

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_SYMBOL_DEF_H*/
