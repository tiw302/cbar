/* ============================================================================
 * CBAR Configuration File
 * ============================================================================
 * Customize your status bar appearance and behavior here.
 * All changes require recompilation: gcc main.c -o cbar && ./cbar
 * ========================================================================= */

#ifndef CONFIG_H
#define CONFIG_H

/* ============================================================================
 * HARDWARE CONFIGURATION
 * ========================================================================= */

// Network interface names (run 'ip link' to find yours)
#define WIFI_INTERFACE  "wlan0"
#define ETH_INTERFACE   "eth0"

// Battery path (usually BAT0 or BAT1, set to "" to disable battery module)
#define BATTERY_PATH    "/sys/class/power_supply/BAT0"

/* ============================================================================
 * UPDATE INTERVALS (in seconds)
 * ========================================================================= */

// How often to refresh the entire bar
#define REFRESH_RATE_SEC    1

// Individual module update intervals (must be multiples of REFRESH_RATE_SEC)
#define UPDATE_WIFI_SEC     5    // WiFi status and SSID
#define UPDATE_ETH_SEC      5    // Ethernet status
#define UPDATE_TEMP_SEC     5    // CPU temperature
#define UPDATE_CPU_SEC      1    // CPU usage (recommended: 1-2 sec)
#define UPDATE_GPU_SEC      5    // GPU usage (nvidia-smi is slow, use 5+ sec)
#define UPDATE_RAM_SEC      1    // RAM usage
#define UPDATE_DISK_SEC     10   // Disk usage percentage
#define UPDATE_IO_SEC       1    // Disk I/O speed
#define UPDATE_BAT_SEC      10   // Battery status
#define UPDATE_TIME_SEC     1    // Date and time

/* ============================================================================
 * COLORS (Hex codes: #RRGGBB) - Grayscale/White Minimal Theme
 * ========================================================================= */

// Network colors (dynamic based on connection status)
#define COLOR_NET_UP        "#666666"  // Dark gray when connected
#define COLOR_NET_DOWN      "#CCCCCC"  // Light gray when disconnected

// Module colors
#define COLOR_TEMP          "#FFFFFF"  // White
#define COLOR_CPU           "#FFFFFF"  // White
#define COLOR_GPU           "#FFFFFF"  // White
#define COLOR_RAM           "#FFFFFF"  // White
#define COLOR_DISK          "#FFFFFF"  // White
#define COLOR_IO            "#FFFFFF"  // White
#define COLOR_BAT           "#FFFFFF"  // White
#define COLOR_TIME          "#FFFFFF"  // White

/* ============================================================================
 * MODULE VISIBILITY
 * ========================================================================= */

// Set to 1 to show module, 0 to hide (battery/GPU auto-hide if not detected)
#define SHOW_WIFI           1
#define SHOW_ETH            1
#define SHOW_TEMP           1
#define SHOW_CPU            1
#define SHOW_GPU            1  // Auto-hides if no GPU found
#define SHOW_RAM            1
#define SHOW_DISK           1
#define SHOW_IO             1
#define SHOW_BAT            1  // Auto-hides if no battery found
#define SHOW_TIME           1

/* ============================================================================
 * DISPLAY FORMAT
 * ========================================================================= */

// Date/time format (see 'man strftime' for options)
#define TIME_FORMAT         "%Y-%m-%d %H:%M:%S"

// Temperature unit (0 = Celsius, 1 = Fahrenheit)
#define TEMP_FAHRENHEIT     0

#endif
