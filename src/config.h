/* src/config.h */
#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware ---
#define WIFI_INTERFACE  "wlan0"
#define ETH_INTERFACE   "eth0"
#define BATTERY_PATH    "/sys/class/power_supply/BAT0"

// --- Colors (Hex Code) ---
#define COLOR_WIFI   "#adadad" // Green
#define COLOR_ETH    "#adadad" // Blue
#define COLOR_TEMP   "#d1d1d1" // Yellow
#define COLOR_CPU    "#d1d1d1" // Red
#define COLOR_GPU    "#d1d1d1" // Magenta
#define COLOR_RAM    "#d1d1d1" // Cyan
#define COLOR_DISK   "#d1d1d1" // Green
#define COLOR_IO     "#d1d1d1" // Orange
#define COLOR_BAT    "#ffffff" // Teal
#define COLOR_TIME   "#ffffff" // White

// --- Settings ---
#define REFRESH_RATE_MS 1000
#define SLOW_UPDATE_CYCLE 5

#endif
