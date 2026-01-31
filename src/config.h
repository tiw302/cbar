/* src/config.h */
#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware ---
#define WIFI_INTERFACE  "wlan0"
#define ETH_INTERFACE   "eth0"
#define BATTERY_PATH    "/sys/class/power_supply/BAT0"

// --- Colors (Hex Code) ---
#define COLOR_WIFI   "#00FF00" // green
#define COLOR_ETH    "#FF0000" // red
#define COLOR_TEMP   "#ffffff" // white
#define COLOR_CPU    "#ffffff" // white
#define COLOR_GPU    "#ffffff" // white
#define COLOR_RAM    "#ffffff" // white
#define COLOR_DISK   "#ffffff" // white
#define COLOR_IO     "#ffffff" // white
#define COLOR_BAT    "#ffffff" // white
#define COLOR_TIME   "#ffffff" // White

// --- Settings ---
#define REFRESH_RATE_MS 1000
#define SLOW_UPDATE_CYCLE 5

#endif
