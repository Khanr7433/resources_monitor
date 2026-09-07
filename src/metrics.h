#pragma once
#include <windows.h>

typedef struct
{
    double cpu_pct, cpu_temp, cpu_ghz;
    double mem_pct, mem_used, mem_total;
    double disk_pct;
    double gpu_pct, gpu_temp, gpu_vram_used, gpu_vram_total;
    double net_speed, net_dl_speed, net_ul_speed, total_traffic;
} SysMetrics;

extern SysMetrics g_metrics;

void init_metrics(void);
void update_metrics(void);
void shutdown_metrics(void);
