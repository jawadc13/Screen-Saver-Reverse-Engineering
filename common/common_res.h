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

// D3DSaver error strings (2100..2112 in the original)
#define IDS_ERR_GENERIC            2100
#define IDS_ERR_NOGL               2101
#define IDS_ERR_INIT               2102
#define IDS_ERR_CREATE_DEVICE      2103
#define IDS_ERR_NO_PIXEL_FORMAT    2104
#define IDS_ERR_OUT_OF_MEMORY      2110
#define IDS_NO_PREVIEW             2112
