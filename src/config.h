#pragma once
#include <windows.h>

enum MetricID
{
    M_NET_DL,
    M_NET_UL,
    M_NET_ALL,
    M_NET_TOT,
    M_CPU_TEMP,
    M_CPU_USAGE,
    M_CPU_FREQ,
    M_RAM_USAGE,
    M_GPU_TEMP,
    M_GPU_USAGE,
    M_GPU_VRAM,
    M_DISK_USAGE,
    M_MAX
};

typedef struct
{
    int id;
    int active;
} MetricCfg;

extern MetricCfg cfg_items[32];
extern int cfg_count;

const wchar_t *get_metric_name(int id);
const wchar_t *get_metric_friendly(int id);

void load_config(void);
void save_config(void);
void load_default_config(void);
