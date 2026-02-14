/* CBAR - Lightweight i3/Sway Status Bar | github.com/yourusername/cbar */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/statvfs.h>
#include "config.h"

#define THERMAL_ZONE "/sys/class/hwmon"
#define GPU_INTEL_AMD "/sys/class/drm/card0/device/gpu_busy_percent"

static int has_battery = 0, has_gpu = 0;
static char wifi_path[128], eth_path[128];

/* Inline helpers for better performance */
static inline void print_block(const char* text, const char* color) {
    printf("{\"full_text\":\"%s\",\"color\":\"%s\",\"separator\":true}", text, color);
}

static inline int read_file(const char* path, char* out, size_t size) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int ok = fgets(out, size, f) != NULL;
    if (ok) out[strcspn(out, "\n")] = 0;
    fclose(f);
    return ok;
}

static inline void run_cmd(const char* cmd, char* out, size_t size) {
    FILE* fp = popen(cmd, "r");
    if (fp && fgets(out, size, fp)) out[strcspn(out, "\n")] = 0;
    else out[0] = '\0';
    if (fp) pclose(fp);
}

void detect_hardware() {
    char path[256], test[16];
    snprintf(path, sizeof(path), "%s/capacity", BATTERY_PATH);
    has_battery = (access(path, R_OK) == 0);
    has_gpu = (access(GPU_INTEL_AMD, R_OK) == 0);
    if (!has_gpu) {
        run_cmd("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null", test, sizeof(test));
        has_gpu = (strlen(test) > 0);
    }
    snprintf(wifi_path, sizeof(wifi_path), "/sys/class/net/%s/operstate", WIFI_INTERFACE);
    snprintf(eth_path, sizeof(eth_path), "/sys/class/net/%s/operstate", ETH_INTERFACE);
}

void get_net(char* out, size_t size, char* color, const char* iface, int is_wifi, const char* path) {
    char state[16], ip[64] = "", cmd[128];
    if (!read_file(path, state, sizeof(state)) || strcmp(state, "up") != 0) {
        snprintf(out, size, is_wifi ? "WIFI: down" : "E: down");
        strcpy(color, COLOR_NET_DOWN);
        return;
    }
    strcpy(color, COLOR_NET_UP);
    snprintf(cmd, sizeof(cmd), "ip -4 addr show %s 2>/dev/null | grep -oP 'inet \\K[\\d.]+'", iface);
    run_cmd(cmd, ip, sizeof(ip));
    if (is_wifi) {
        char ssid[64];
        snprintf(cmd, sizeof(cmd), "iwgetid -r %s 2>/dev/null", iface);
        run_cmd(cmd, ssid, sizeof(ssid));
        snprintf(out, size, "WIFI: %s (%s)", strlen(ssid) ? ssid : "?", strlen(ip) ? ip : "No IP");
    } else snprintf(out, size, "E: %s", strlen(ip) ? ip : "Up");
}

void get_temp(char* out, size_t size) {
    DIR* dir = opendir(THERMAL_ZONE);
    if (!dir) { snprintf(out, size, "TEMP: N/A"); return; }
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "hwmon", 5) != 0) continue;
        char path[256], name[32], temp_str[16];
        snprintf(path, sizeof(path), "%s/%s/name", THERMAL_ZONE, entry->d_name);
        if (read_file(path, name, sizeof(name)) && (strstr(name, "core") || strstr(name, "k10") || strstr(name, "zen"))) {
            snprintf(path, sizeof(path), "%s/%s/temp1_input", THERMAL_ZONE, entry->d_name);
            if (read_file(path, temp_str, sizeof(temp_str))) {
                int temp = atoi(temp_str) / 1000;
#if TEMP_FAHRENHEIT
                snprintf(out, size, "TEMP: %dF", (temp * 9 / 5) + 32);
#else
                snprintf(out, size, "TEMP: %dC", temp);
#endif
                closedir(dir); return;
            }
        }
    }
    closedir(dir);
    snprintf(out, size, "TEMP: N/A");
}

void get_cpu(char* out, size_t size) {
    static unsigned long long prev_total = 0, prev_idle = 0;
    unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal;
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return;
    fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
    fclose(f);
    unsigned long long total = user + nice + sys + idle + iowait + irq + softirq + steal;
    unsigned long long idle_total = idle + iowait;
    if (prev_total > 0) {
        double usage = 100.0 * (double)(total - prev_total - (idle_total - prev_idle)) / (total - prev_total);
        snprintf(out, size, "CPU: %.1f%%", usage);
    } else snprintf(out, size, "CPU: 0.0%%");
    prev_total = total; prev_idle = idle_total;
}

void get_gpu(char* out, size_t size) {
    char util[16];
    if (read_file(GPU_INTEL_AMD, util, sizeof(util))) snprintf(out, size, "GPU: %s%%", util);
    else {
        run_cmd("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null", util, sizeof(util));
        snprintf(out, size, strlen(util) > 0 ? "GPU: %s%%" : "GPU: N/A", util);
    }
}

void get_ram(char* out, size_t size) {
    FILE* f = fopen("/proc/meminfo", "r");
    long total = 0, available = 0;
    char line[128];
    while (f && fgets(line, sizeof(line), f)) {
        sscanf(line, "MemTotal: %ld", &total);
        if (sscanf(line, "MemAvailable: %ld", &available)) break;
    }
    if (f) fclose(f);
    snprintf(out, size, "RAM: %ldMB", (total - available) / 1024);
}

void get_disk(char* out, size_t size) {
    struct statvfs buf;
    if (statvfs("/", &buf) == 0) {
        unsigned long percent = 100 * (buf.f_blocks - buf.f_bavail) / buf.f_blocks;
        snprintf(out, size, "DISK: %lu%%", percent);
    } else snprintf(out, size, "DISK: N/A");
}

void get_io(char* out, size_t size) {
    static unsigned long long prev_sectors = 0;
    static time_t prev_time = 0;
    unsigned long long sectors = 0;
    char line[256], dev[32];
    FILE* f = fopen("/proc/diskstats", "r");
    if (!f) { snprintf(out, size, "IO: N/A"); return; }
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, " %*d %*d %s %*d %*d %*d %*d %*d %*d %llu", dev, &sectors) == 2) {
            if (strstr(dev, "nvme") || (dev[0] == 's' && isalpha(dev[1]) && !dev[2])) break;
        }
    }
    fclose(f);
    time_t now = time(NULL);
    if (prev_time && now > prev_time) snprintf(out, size, "IO: %.1f KB/s", (sectors - prev_sectors) * 512.0 / 1024.0 / (now - prev_time));
    else snprintf(out, size, "IO: 0.0 KB/s");
    prev_sectors = sectors; prev_time = now;
}

void get_bat(char* out, size_t size) {
    char cap[16], status[16], path[256];
    snprintf(path, sizeof(path), "%s/capacity", BATTERY_PATH);
    if (!read_file(path, cap, sizeof(cap))) { snprintf(out, size, "PWR: AC"); return; }
    snprintf(path, sizeof(path), "%s/status", BATTERY_PATH);
    read_file(path, status, sizeof(status));
    const char* state = strcmp(status, "Charging") == 0 ? "CHR" : strcmp(status, "Discharging") == 0 ? "DIS" : "FULL";
    snprintf(out, size, "BAT: %s%% %s", cap, state);
}

void get_time(char* out, size_t size) {
    time_t t = time(NULL);
    strftime(out, size, TIME_FORMAT, localtime(&t));
}

/* Macros for cleaner main loop */
#define UPDATE_IF(cond, interval, func, idx, color_val, ...) \
    if ((cond) && (tick % (interval) == 0)) { func(modules[idx], sizeof(modules[idx]), ##__VA_ARGS__); strcpy(colors[idx], color_val); }

#define UPDATE_NET_IF(cond, interval, idx, iface, is_wifi, path) \
    if ((cond) && (tick % (interval) == 0)) get_net(modules[idx], sizeof(modules[idx]), colors[idx], iface, is_wifi, path);

#define PRINT_IF(cond, idx) if (cond) { if (need_comma) printf(","); print_block(modules[idx], colors[idx]); need_comma = 1; }

int main() {
    char modules[10][64], colors[10][16];
    int counter = 0;
    
    detect_hardware();
    memset(modules, 0, sizeof(modules));
    for (int i = 0; i < 10; i++) strcpy(colors[i], "#FFFFFF");
    setvbuf(stdout, NULL, _IOLBF, 0);
    
    printf("{\"version\":1}\n[\n[");
    print_block("Loading...", "#FFFFFF");
    printf("],\n");
    
    while (1) {
        int tick = counter * REFRESH_RATE_SEC, need_comma = 0;
        
        UPDATE_NET_IF(SHOW_WIFI, UPDATE_WIFI_SEC, 0, WIFI_INTERFACE, 1, wifi_path);
        UPDATE_NET_IF(SHOW_ETH, UPDATE_ETH_SEC, 1, ETH_INTERFACE, 0, eth_path);
        UPDATE_IF(SHOW_TEMP, UPDATE_TEMP_SEC, get_temp, 2, COLOR_TEMP);
        UPDATE_IF(SHOW_CPU, UPDATE_CPU_SEC, get_cpu, 3, COLOR_CPU);
        UPDATE_IF(SHOW_GPU && has_gpu, UPDATE_GPU_SEC, get_gpu, 4, COLOR_GPU);
        UPDATE_IF(SHOW_RAM, UPDATE_RAM_SEC, get_ram, 5, COLOR_RAM);
        UPDATE_IF(SHOW_DISK, UPDATE_DISK_SEC, get_disk, 6, COLOR_DISK);
        UPDATE_IF(SHOW_IO, UPDATE_IO_SEC, get_io, 7, COLOR_IO);
        UPDATE_IF(SHOW_BAT && has_battery, UPDATE_BAT_SEC, get_bat, 8, COLOR_BAT);
        UPDATE_IF(SHOW_TIME, UPDATE_TIME_SEC, get_time, 9, COLOR_TIME);
        
        printf("[");
        PRINT_IF(SHOW_WIFI, 0);
        PRINT_IF(SHOW_ETH, 1);
        PRINT_IF(SHOW_TEMP, 2);
        PRINT_IF(SHOW_CPU, 3);
        PRINT_IF(SHOW_GPU && has_gpu, 4);
        PRINT_IF(SHOW_RAM, 5);
        PRINT_IF(SHOW_DISK, 6);
        PRINT_IF(SHOW_IO, 7);
        PRINT_IF(SHOW_BAT && has_battery, 8);
        PRINT_IF(SHOW_TIME, 9);
        printf("],\n");
        
        counter++;
        sleep(REFRESH_RATE_SEC);
    }
    return 0;
}
