/*
 * Cbar: A lightweight and efficient status bar for i3/sway.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#define BATTERY_NAME "BAT0"
#define WIFI_INTERFACE "wlan0"

// Runs a command and captures the first line of output.
static void run_cmd(char* out, size_t size, const char* cmd) {
    FILE* fp = popen(cmd, "r");
    if (fp && fgets(out, size, fp)) {
        out[strcspn(out, "\n")] = 0; // Remove trailing newline
    } else if (out) {
        out[0] = '\0';
    }
    if (fp) pclose(fp);
}

// Reads a long integer from a file.
static long read_long(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    long val = -1;
    fscanf(f, "%ld", &val);
    fclose(f);
    return val;
}

// Gets root partition usage percentage.
static void get_disk(char* out, size_t size) {
    char usage[16];
    run_cmd(usage, sizeof(usage), "df --output=pcent / | tail -n 1");
    
    // Trim leading whitespace from df output.
    char* p = usage;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    snprintf(out, size, "DISK: %s", p);
}

// Calculates disk write speed by reading /proc/diskstats.
static void get_disk_write(char* out, size_t size) {
    static unsigned long long prev_sectors = 0;
    static time_t prev_time = 0;
    unsigned long long sectors = 0;
    
    FILE* f = fopen("/proc/diskstats", "r");
    if (!f) {
        snprintf(out, size, "IO: N/A");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char dev[32];
        sscanf(line, " %*d %*d %s %*d %*d %*d %*d %*d %*d %llu", dev, &sectors);
        if (strstr(dev, "nvme") || ( (strstr(dev, "sd") || strstr(dev, "hd") || strstr(dev, "vd")) && isalpha(dev[2]) && dev[3] == '\0') ) {
            break;
        }
    }
    fclose(f);

    time_t now = time(NULL);
    double kbs = 0.0;

    if (prev_time > 0 && now > prev_time) {
        unsigned long long sector_diff = sectors - prev_sectors;
        time_t time_diff = now - prev_time;
        kbs = (double)(sector_diff * 512) / 1024.0 / (double)time_diff;
    }

    snprintf(out, size, "IO: %.1f KB/s", kbs);
    prev_sectors = sectors;
    prev_time = now;
}

// Gets GPU utilization.
static void get_gpu(char* out, size_t size) {
    char util[16];
    run_cmd(util, sizeof(util), "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits");
    if (strlen(util) > 0) {
        char* p = util;
        while (*p && isspace((unsigned char)*p)) p++; // Trim whitespace
        snprintf(out, size, "GPU: %s%%", p);
        return;
    }

    FILE* f = fopen("/sys/class/drm/card0/device/gpu_busy_percent", "r");
    if (f) {
        int percent = 0;
        fscanf(f, "%d", &percent);
        fclose(f);
        snprintf(out, size, "GPU: %d%%", percent);
        return;
    }

    // If neither is found, report N/A.
    snprintf(out, size, "GPU: N/A");
}

// Gets Wi-Fi SSID and IP address.
static void get_wifi(char* out, size_t size) {
    char ssid[64], ipv4[64], cmd[128];
    snprintf(cmd, sizeof(cmd), "iwgetid -r %s", WIFI_INTERFACE);
    run_cmd(ssid, sizeof(ssid), cmd);
    if (strlen(ssid) == 0) {
        snprintf(out, size, "WIFI: Down");
        return;
    }
    snprintf(cmd, sizeof(cmd), "ip -4 addr show %s | grep -oP 'inet \\K[\\d.]+'", WIFI_INTERFACE);
    run_cmd(ipv4, sizeof(ipv4), cmd);
    snprintf(out, size, "WIFI: %s (%s)", ssid, strlen(ipv4) > 0 ? ipv4 : "No IP");
}

// Calculates CPU usage percentage by reading /proc/stat.
static void get_cpu(char* out, size_t size) {
    static unsigned long long prev_total = 0, prev_idle = 0;
    unsigned long long total, idle, user, nice, system, iowait, irq, softirq, steal;
    
    FILE* f = fopen("/proc/stat", "r");
    if (!f) { snprintf(out, size, "CPU: N/A"); return; }
    fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    fclose(f);

    total = user + nice + system + idle + iowait + irq + softirq + steal;
    idle = idle + iowait;

    double percent = 0.0;
    if (prev_total > 0) {
        unsigned long long total_d = total - prev_total;
        unsigned long long idle_d = idle - prev_idle;
        if (total_d > 0) percent = 100.0 * (total_d - idle_d) / total_d;
    }
    snprintf(out, size, "CPU: %.2f%%", percent);
    prev_total = total;
    prev_idle = idle;
}

// Gets CPU/system temperature from hwmon.
static void get_temp(char* out, size_t size) {
    DIR* hwmon_dir = opendir("/sys/class/hwmon");
    if (!hwmon_dir) { snprintf(out, size, "TEMP: N/A"); return; }
    struct dirent* de;
    while ((de = readdir(hwmon_dir))) {
        if (strncmp(de->d_name, "hwmon", 5) != 0) continue;
        
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/hwmon/%s/name", de->d_name);
        FILE* f_name = fopen(path, "r");
        if (!f_name) continue;

        char name[32];
        fscanf(f_name, "%31s", name);
        fclose(f_name);

        // Check for common CPU temperature sensors.
        if (strcmp(name, "coretemp") == 0 || strcmp(name, "k10temp") == 0 || strcmp(name, "zenpower") == 0) {
            snprintf(path, sizeof(path), "/sys/class/hwmon/%s/temp1_input", de->d_name);
            long temp = read_long(path);
            if (temp != -1) {
                snprintf(out, size, "TEMP: %.1fC", (double)temp / 1000.0);
                closedir(hwmon_dir);
                return;
            }
        }
    }
    closedir(hwmon_dir);
    snprintf(out, size, "TEMP: N/A");
}

// Calculates used RAM by reading /proc/meminfo.
static void get_ram(char* out, size_t size) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) { snprintf(out, size, "RAM: N/A"); return; }
    long total = 0, free = 0, buffers = 0, cached = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "MemTotal: %ld kB", &total);
        sscanf(line, "MemFree: %ld kB", &free);
        sscanf(line, "Buffers: %ld kB", &buffers);
        sscanf(line, "Cached: %ld kB", &cached);
        if (total && free && buffers && cached) break; // Stop reading once we have all values
    }
    fclose(f);
    snprintf(out, size, "RAM: %ld MB", (total - free - buffers - cached) / 1024);
}

// Gets battery status and percentage.
static void get_battery(char* out, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", BATTERY_NAME);
    if (access(path, F_OK) == -1) { snprintf(out, size, "PWR: AC"); return; }

    double capacity = 0.0;
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/energy_now", BATTERY_NAME);
    long now = read_long(path);
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/energy_full", BATTERY_NAME);
    long full = read_long(path);

    if (now != -1 && full > 0) {
        capacity = ((double)now / full) * 100.0;
    } else {
        // Fallback to capacity file if energy files are not available.
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", BATTERY_NAME);
        capacity = (double)read_long(path);
    }

    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", BATTERY_NAME);
    FILE* f_status = fopen(path, "r");
    char status[16] = "";
    if (f_status) { fscanf(f_status, "%15s", status); fclose(f_status); }
    
    char* s_text = (strcmp(status, "Charging")==0)?"CHR":(strcmp(status, "Discharging")==0)?"DIS":(strcmp(status, "Full")==0)?"FULL":"";
    snprintf(out, size, "BAT: %.2f%% %s", capacity, s_text);
}

// Gets the current date and time.
static void get_datetime(char* out, size_t size) {
    time_t t = time(NULL);
    strftime(out, size, "%Y-%m-%d %H:%M:%S", localtime(&t));
}

int main() {
    char status_line[1024];
    char modules[9][128];
    unsigned int counter = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);

    while (1) {
        // Modules that update every second for real-time stats.
        get_cpu(modules[2], sizeof(modules[2]));
        get_gpu(modules[3], sizeof(modules[3]));
        get_ram(modules[4], sizeof(modules[4]));
        get_disk_write(modules[6], sizeof(modules[6]));
        get_datetime(modules[8], sizeof(modules[8]));

        // Modules that update every 5 seconds to save resources.
        if (counter % 5 == 0) {
            get_wifi(modules[0], sizeof(modules[0]));
            get_temp(modules[1], sizeof(modules[1]));
            get_battery(modules[7], sizeof(modules[7]));
            get_disk(modules[5], sizeof(modules[5]));
        }

        snprintf(status_line, sizeof(status_line), "%s | %s | %s | %s | %s | %s | %s | %s | %s",
                 modules[0], // WIFI
                 modules[1], // TEMP
                 modules[2], // CPU
                 modules[3], // GPU
                 modules[4], // RAM
                 modules[5], // DISK
                 modules[6], // IO
                 modules[7], // BAT
                 modules[8]  // DATETIME
                 );
        
        puts(status_line);
        
        counter++;
        sleep(1);
    }
    return 0;
}
