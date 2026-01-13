/*
 * Cbar: A lightweight, dependency-free status bar for i3/sway.
 * Written in pure C to minimize memory footprint (~1-2MB).
 *
 * Author: [Your Name/Handle]
 * Repository: github.com/[your-username]/cbar
 */

#define _POSIX_C_SOURCE 200809L // Required for popen, strdup, etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/statvfs.h>

/* ================================================================== *
 * CONFIGURATION                               *
 * Customize these values to match your hardware and preferences.    *
 * ================================================================== */

// Network Interfaces (Run 'ip link' to find yours)
#define WIFI_INTERFACE  "wlan0"
#define ETH_INTERFACE   "eth0"

// Battery Configuration
#define BATTERY_PATH    "/sys/class/power_supply/BAT0"

// System Paths (Usually don't need changing on standard Linux)
#define THERMAL_ZONE    "/sys/class/hwmon"
#define GPU_INTEL_AMD   "/sys/class/drm/card0/device/gpu_busy_percent"

// Update Intervals
#define REFRESH_RATE_MS 1000  // Main loop delay (1 second)
#define SLOW_UPDATE_CYCLE 5   // Update slow modules every 5 cycles (5 seconds)

/* ================================================================== *
 * HELPER FUNCTIONS                            *
 * ================================================================== */

// Reads the first line from a file. Returns 1 on success, 0 on failure.
int read_file(const char* path, char* out, size_t size) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    
    if (fgets(out, size, f)) {
        out[strcspn(out, "\n")] = 0; // Remove trailing newline
        fclose(f);
        return 1;
    }
    
    fclose(f);
    return 0;
}

// Executes a shell command and captures the first line of output.
// Used sparingly to keep memory usage low.
void run_command(const char* cmd, char* out, size_t size) {
    FILE* fp = popen(cmd, "r");
    if (fp) {
        if (fgets(out, size, fp)) {
            out[strcspn(out, "\n")] = 0;
        } else {
            out[0] = '\0';
        }
        pclose(fp);
    } else {
        out[0] = '\0';
    }
}

/* ================================================================== *
 * MODULES                                   *
 * ================================================================== */

// Calculates Root Partition (/) usage using syscalls (efficient).
void get_disk_usage(char* out, size_t size) {
    struct statvfs buf;
    if (statvfs("/", &buf) == 0) {
        unsigned long total = buf.f_blocks * buf.f_frsize;
        unsigned long available = buf.f_bavail * buf.f_frsize;
        int percent = 100 * (total - available) / total;
        snprintf(out, size, "DISK: %d%%", percent);
    } else {
        snprintf(out, size, "DISK: N/A");
    }
}

// Reads /proc/diskstats to calculate I/O write speed.
void get_disk_io(char* out, size_t size) {
    static unsigned long long prev_sectors = 0;
    static time_t prev_time = 0;
    unsigned long long sectors = 0;
    char line[256], device_name[32];
    
    FILE* f = fopen("/proc/diskstats", "r");
    if (!f) { snprintf(out, size, "IO: N/A"); return; }

    // Parse diskstats to find the main drive (nvme or sda)
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, " %*d %*d %s %*d %*d %*d %*d %*d %*d %llu", device_name, &sectors) == 2) {
             // Check for common main drive names
             if (strstr(device_name, "nvme") || 
                (device_name[0] == 's' && isalpha(device_name[1]) && !device_name[2])) {
                 break;
             }
        }
    }
    fclose(f);

    time_t now = time(NULL);
    if (prev_time && now > prev_time) {
        double speed_kb = (double)((sectors - prev_sectors) * 512) / 1024.0 / (now - prev_time);
        snprintf(out, size, "IO: %.1f KB/s", speed_kb);
    } else {
        snprintf(out, size, "IO: 0.0 KB/s");
    }
    
    prev_sectors = sectors;
    prev_time = now;
}

// Gets GPU usage. Supports Intel/AMD (sysfs) and NVIDIA (nvidia-smi).
void get_gpu_usage(char* out, size_t size) {
    // 1. Try generic sysfs (Intel/AMD) - Extremely fast
    if (read_file(GPU_INTEL_AMD, out, size)) {
        char temp[16]; 
        snprintf(temp, sizeof(temp), "GPU: %s%%", out); 
        strcpy(out, temp);
        return;
    }

    // 2. Fallback to nvidia-smi - Slower but necessary for NVIDIA
    char util[16];
    run_command("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits", util, sizeof(util));
    if (strlen(util) > 0) {
        snprintf(out, size, "GPU: %s%%", util);
    } else {
        snprintf(out, size, "GPU: N/A");
    }
}

// Handles both WiFi and Ethernet status.
// mode: 1 for WiFi (shows SSID), 0 for Ethernet (shows only Status/IP).
void get_network_status(char* out, size_t size, const char* interface, int is_wifi) {
    char path[128], operstate[16], ip_address[64] = "";
    
    // Check if interface is UP via sysfs (avoids spawning processes if down)
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", interface);
    
    if (!read_file(path, operstate, sizeof(operstate)) || strcmp(operstate, "up") != 0) {
        snprintf(out, size, is_wifi ? "WIFI: Down" : "E: down");
        return;
    }

    // Get IP Address
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip -4 addr show %s | grep -oP 'inet \\K[\\d.]+'", interface);
    run_command(cmd, ip_address, sizeof(ip_address));

    if (is_wifi) {
        char ssid[64];
        snprintf(cmd, sizeof(cmd), "iwgetid -r %s", interface);
        run_command(cmd, ssid, sizeof(ssid));
        // Format: WIFI: SSID (IP)
        snprintf(out, size, "WIFI: %s (%s)", strlen(ssid) ? ssid : "?", strlen(ip_address) ? ip_address : "No IP");
    } else {
        // Format: E: IP
        snprintf(out, size, "E: %s", strlen(ip_address) ? ip_address : "Up");
    }
}

// Calculates CPU usage from /proc/stat
void get_cpu_usage(char* out, size_t size) {
    static unsigned long long prev_total = 0, prev_idle = 0;
    unsigned long long user, nice, sys, idle, iowait, irq, softirq, steal, total;
    
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return;
    fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", 
           &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
    fclose(f);
    
    total = user + nice + sys + idle + iowait + irq + softirq + steal;
    idle += iowait; // Treat iowait as idle time
    
    double usage = 0.0;
    if (prev_total > 0) {
        unsigned long long total_diff = total - prev_total;
        unsigned long long idle_diff = idle - prev_idle;
        usage = 100.0 * (double)(total_diff - idle_diff) / total_diff;
    }
    
    snprintf(out, size, "CPU: %.1f%%", usage);
    prev_total = total;
    prev_idle = idle;
}

// Scans hwmon for CPU temperature.
void get_temperature(char* out, size_t size) {
    DIR* dir = opendir(THERMAL_ZONE);
    struct dirent* entry;
    
    while (dir && (entry = readdir(dir))) {
        if (strncmp(entry->d_name, "hwmon", 5) != 0) continue;
        
        char path[256], name[32], temp_str[16];
        snprintf(path, sizeof(path), "%s/%s/name", THERMAL_ZONE, entry->d_name);
        
        // Check for common CPU sensor names
        if (read_file(path, name, sizeof(name)) && 
           (strstr(name, "core") || strstr(name, "k10") || strstr(name, "zen"))) {
            
            snprintf(path, sizeof(path), "%s/%s/temp1_input", THERMAL_ZONE, entry->d_name);
            if (read_file(path, temp_str, sizeof(temp_str))) {
                snprintf(out, size, "TEMP: %.0fC", atoi(temp_str) / 1000.0);
                closedir(dir); 
                return;
            }
        }
    }
    if (dir) closedir(dir);
    snprintf(out, size, "TEMP: N/A");
}

// Reads MemAvailable from /proc/meminfo.
void get_ram_usage(char* out, size_t size) {
    FILE* f = fopen("/proc/meminfo", "r");
    long total = 0, available = 0;
    char line[128];
    
    while (f && fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %ld", &total)) continue;
        if (sscanf(line, "MemAvailable: %ld", &available)) break;
    }
    if (f) fclose(f);
    
    // Convert kB to MB
    snprintf(out, size, "RAM: %ldMB", (total - available) / 1024);
}

// Reads battery status and capacity from sysfs.
void get_battery_status(char* out, size_t size) {
    char capacity[16], status[16], path[256];
    
    snprintf(path, sizeof(path), "%s/capacity", BATTERY_PATH);
    if (!read_file(path, capacity, sizeof(capacity))) { 
        snprintf(out, size, "PWR: AC"); 
        return; 
    }
    
    snprintf(path, sizeof(path), "%s/status", BATTERY_PATH);
    read_file(path, status, sizeof(status));
    
    // Shorten status text
    char* state_str = "FULL";
    if (strcmp(status, "Charging") == 0) state_str = "CHR";
    else if (strcmp(status, "Discharging") == 0) state_str = "DIS";
    
    snprintf(out, size, "BAT: %s%% %s", capacity, state_str);
}

// Gets current system time.
void get_datetime(char* out, size_t size) {
    time_t t = time(NULL);
    strftime(out, size, "%Y-%m-%d %H:%M:%S", localtime(&t));
}

/* ================================================================== *
 * MAIN LOOP                               *
 * ================================================================== */

int main() {
    char status_line[1024];
    // Module buffers: 0=WiFi, 1=Eth, 2=CPU, 3=GPU, 4=RAM, 5=Temp, 6=IO, 7=Disk, 8=Bat, 9=Time
    char modules[10][64]; 
    int counter = 0;

    // Line buffering ensures output is sent immediately to the bar
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (1) {
        // 1. Fast updates (Every cycle/1s)
        get_cpu_usage(modules[2], sizeof(modules[2]));
        get_gpu_usage(modules[3], sizeof(modules[3]));
        get_ram_usage(modules[4], sizeof(modules[4]));
        get_disk_io(modules[6], sizeof(modules[6]));
        get_datetime(modules[9], sizeof(modules[9]));

        // 2. Slow updates (Every N cycles/5s)
        if (counter % SLOW_UPDATE_CYCLE == 0) {
            get_network_status(modules[0], sizeof(modules[0]), WIFI_INTERFACE, 1); // WiFi
            get_network_status(modules[1], sizeof(modules[1]), ETH_INTERFACE, 0);  // Ethernet
            get_temperature(modules[5], sizeof(modules[5]));
            get_disk_usage(modules[7], sizeof(modules[7]));
            get_battery_status(modules[8], sizeof(modules[8]));
        }

        // 3. Construct the status bar string
        snprintf(status_line, sizeof(status_line), 
                 "%s | %s | %s | %s | %s | %s | %s | %s | %s | %s",
                 modules[0], // Wifi
                 modules[1], // Ethernet
                 modules[5], // Temp
                 modules[2], // CPU
                 modules[3], // GPU
                 modules[4], // RAM
                 modules[7], // Disk
                 modules[6], // IO
                 modules[8], // Battery
                 modules[9]  // Time
                 );
        
        puts(status_line);
        
        counter++;
        sleep(REFRESH_RATE_MS / 1000);
    }
    return 0;
}
