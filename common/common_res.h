// Resource IDs shared by every screensaver. The numbers match the ones found
// in sspipes.scr so the resource layout mirrors the original.
#pragma once

#define IDS_DESCRIPTION            1     // Name shown in Display Properties

#define IDI_MAIN                   101   // Icon (the Desktop applet shows it)

#define IDD_DISPLAY_SETTINGS       200
#define IDC_MONITOR_LIST           1100
#define IDC_MONITOR_INFO           1101
#define IDC_SHOW_SAVER             1102
#define IDC_LEAVE_BLACK            1103
#define IDC_SAME_ON_ALL            1104
#define IDC_FRAME_GRAPH            1105
#define IDC_MSAA                   1106
#define IDC_STYLE_BUTTON           1107

#define IDD_STYLE                  210   // Themes & Effects
#define IDC_STYLE_THEME            1200
#define IDC_STYLE_NAME             1201
#define IDC_STYLE_PREV             1202
#define IDC_STYLE_NEXT             1203
#define IDC_STYLE_RANDOM           1204
#define IDC_STYLE_PATTERN          1205
#define IDC_STYLE_EFFECT           1206
#define IDC_STYLE_MOTION           1207
#define IDC_STYLE_SPEED            1208
#define IDC_STYLE_STRENGTH         1209
#define IDC_STYLE_ALL              1210
#define IDC_STYLE_RESET            1211
#define IDC_STYLE_RANGE            1212

#define IDD_SIMPLE_CONFIG          300   // generic settings dialog (simplecfg.cpp)

// D3DSaver error strings (2100..2112 in the original)
#define IDS_ERR_GENERIC            2100
#define IDS_ERR_NOGL               2101
#define IDS_ERR_INIT               2102
#define IDS_ERR_CREATE_DEVICE      2103
#define IDS_ERR_NO_PIXEL_FORMAT    2104
#define IDS_ERR_OUT_OF_MEMORY      2110
#define IDS_NO_PREVIEW             2112
