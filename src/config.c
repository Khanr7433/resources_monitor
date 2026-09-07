#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "common.h"
#include "config.h"

#define DEFAULT_CFG L"NET_DL:1,NET_UL:1,CPU_TEMP:1,CPU_USAGE:1,CPU_FREQ:1,RAM_USAGE:1,NET_ALL:1,NET_TOT:1,GPU_TEMP:1,GPU_USAGE:1,GPU_VRAM:1,DISK_USAGE:1"

MetricCfg cfg_items[32];
int cfg_count = 0;

static struct
{
    const wchar_t *name;
    const wchar_t *friendly;
    int id;
} m_map[] = {
    {L"NET_DL", L"Download speed", M_NET_DL}, {L"NET_UL", L"Upload speed", M_NET_UL}, {L"NET_ALL", L"Total speed", M_NET_ALL}, {L"NET_TOT", L"Total traffic", M_NET_TOT}, {L"CPU_TEMP", L"CPU Temperature", M_CPU_TEMP}, {L"CPU_USAGE", L"CPU usage", M_CPU_USAGE}, {L"CPU_FREQ", L"CPU Freq", M_CPU_FREQ}, {L"RAM_USAGE", L"Memory usage", M_RAM_USAGE}, {L"GPU_TEMP", L"GPU Temperature", M_GPU_TEMP}, {L"GPU_USAGE", L"GPU usage", M_GPU_USAGE}, {L"GPU_VRAM", L"GPU VRAM usage", M_GPU_VRAM}, {L"DISK_USAGE", L"Disk usage", M_DISK_USAGE}};

const wchar_t *get_metric_name(int id)
{
    for (int i = 0; i < sizeof(m_map) / sizeof(m_map[0]); i++)
        if (m_map[i].id == id)
            return m_map[i].name;
    return L"Unknown";
}

const wchar_t *get_metric_friendly(int id)
{
    for (int i = 0; i < sizeof(m_map) / sizeof(m_map[0]); i++)
        if (m_map[i].id == id)
            return m_map[i].friendly;
    return L"Unknown";
}

static void parse_config_string(const wchar_t *str)
{
    cfg_count = 0;
    wchar_t buf[1024];
    wcsncpy(buf, str, 1023);
    buf[1023] = 0;
    wchar_t *p = buf;
    while (*p && cfg_count < 32)
    {
        while (*p == L' ')
            p++;
        if (!*p)
            break;
        wchar_t *start = p;
        while (*p && *p != L',')
            p++;
        if (*p)
        {
            *p = 0;
            p++;
        }

        wchar_t *colon = wcschr(start, L':');
        int active = 1;
        if (colon)
        {
            *colon = 0;
            active = _wtoi(colon + 1);
        }

        for (int i = 0; i < sizeof(m_map) / sizeof(m_map[0]); i++)
        {
            if (_wcsicmp(start, m_map[i].name) == 0)
            {
                cfg_items[cfg_count].id = m_map[i].id;
                cfg_items[cfg_count].active = active;
                cfg_count++;
                break;
            }
        }
    }
}

void load_config(void)
{
    wchar_t ini_path[MAX_PATH];
    GetModuleFileNameW(NULL, ini_path, MAX_PATH);
    wchar_t *p = wcsrchr(ini_path, L'\\');
    if (p)
        wcscpy(p + 1, L"config.ini");

    if (GetFileAttributesW(ini_path) == INVALID_FILE_ATTRIBUTES)
    {
        WritePrivateProfileStringW(L"Settings", L"Items", DEFAULT_CFG, ini_path);
    }
    wchar_t items[1024] = {0};
    GetPrivateProfileStringW(L"Settings", L"Items", DEFAULT_CFG, items, 1024, ini_path);
    parse_config_string(items);
}

void load_default_config(void) { parse_config_string(DEFAULT_CFG); }

void save_config(void)
{
    wchar_t buf[1024] = {0};
    for (int i = 0; i < cfg_count; i++)
    {
        wchar_t tmp[64];
        swprintf(tmp, 64, L"%s:%d%s", get_metric_name(cfg_items[i].id), cfg_items[i].active, i == cfg_count - 1 ? L"" : L",");
        wcscat(buf, tmp);
    }
    wchar_t ini_path[MAX_PATH];
    GetModuleFileNameW(NULL, ini_path, MAX_PATH);
    wchar_t *p = wcsrchr(ini_path, L'\\');
    if (p)
        wcscpy(p + 1, L"config.ini");
    WritePrivateProfileStringW(L"Settings", L"Items", buf, ini_path);
}
