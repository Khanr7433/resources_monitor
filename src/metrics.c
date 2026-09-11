#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <objbase.h>
#include <oleauto.h>
#include "metrics.h"

SysMetrics g_metrics = {0};

/* ================================================================
 *  WMI COM & External APIs
 * ================================================================ */
static const GUID CLSID_WbemLocator = {0x4590f811, 0x1d3a, 0x11d0, {0x89, 0x1f, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24}};
static const GUID IID_IWbemLocator = {0xdc12a687, 0x737f, 0x11cf, {0x88, 0x4d, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24}};

#define VT(obj) (*(void ***)(obj))
typedef ULONG(STDMETHODCALLTYPE *pfn_Release)(void *);
typedef HRESULT(STDMETHODCALLTYPE *pfn_ConnectServer)(void *, BSTR, BSTR, BSTR, BSTR, LONG, BSTR, void *, void **);
typedef HRESULT(STDMETHODCALLTYPE *pfn_ExecQuery)(void *, BSTR, BSTR, LONG, void *, void **);
typedef HRESULT(STDMETHODCALLTYPE *pfn_EnumNext)(void *, LONG, ULONG, void **, ULONG *);
typedef HRESULT(STDMETHODCALLTYPE *pfn_ObjGet)(void *, LPCWSTR, LONG, VARIANT *, LONG *, LONG *);

#define COM_Release(obj) ((pfn_Release)(VT(obj)[2]))(obj)
#define WMI_Connect(loc, ns, svc) ((pfn_ConnectServer)(VT(loc)[3]))(loc, ns, NULL, NULL, NULL, 0, NULL, NULL, svc)
#define WMI_Query(svc, lang, qry, enm) ((pfn_ExecQuery)(VT(svc)[20]))(svc, lang, qry, 0x20, NULL, enm)
#define WMI_Next(enm, obj, cnt) ((pfn_EnumNext)(VT(enm)[4]))(enm, 3000, 1, obj, cnt)
#define WMI_Get(obj, name, val) ((pfn_ObjGet)(VT(obj)[4]))(obj, name, 0, val, NULL, NULL)

typedef struct
{
    unsigned int gpu;
    unsigned int mem;
} NvmlUtil;
typedef struct
{
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} NvmlMem;

typedef int (*pfn_nvmlInit)(void);
typedef int (*pfn_nvmlShutdown)(void);
typedef int (*pfn_nvmlGetHandle)(unsigned int, void **);
typedef int (*pfn_nvmlGetUtil)(void *, void *);
typedef int (*pfn_nvmlGetTemp)(void *, int, unsigned int *);
typedef int (*pfn_nvmlGetMem)(void *, NvmlMem *);

typedef struct
{
    ULONG Number, MaxMhz, CurrentMhz, MhzLimit, MaxIdleState, CurrentIdleState;
} CpuPowerInfo;
typedef LONG(WINAPI *pfn_NtPowerInfo)(INT, PVOID, ULONG, PVOID, ULONG);

/* ================================================================
 *  Internal State
 * ================================================================ */
static struct
{
    ULONGLONG prev_idle, prev_kern, prev_user, prev_recv, prev_sent;
    DWORD prev_tick;
    int first;

    void *wmi_svc;
    int wmi_ok;
    HMODULE nvml_dll;
    void *nvml_dev;
    pfn_nvmlGetUtil nvml_util;
    pfn_nvmlGetTemp nvml_temp;
    pfn_nvmlGetMem nvml_mem;
    int nvml_ok;
    pfn_NtPowerInfo nt_power;
    int ncpus;
} State = {0};

/* ================================================================
 *  Collectors
 * ================================================================ */
static void upd_cpu(void)
{
    FILETIME fi, fk, fu;
    if (!GetSystemTimes(&fi, &fk, &fu))
        return;
    ULONGLONG i = *(ULONGLONG *)&fi, k = *(ULONGLONG *)&fk, u = *(ULONGLONG *)&fu;
    if (!State.first)
    {
        ULONGLONG total = (k - State.prev_kern) + (u - State.prev_user);
        if (total > 0)
            g_metrics.cpu_pct = (1.0 - (double)(i - State.prev_idle) / total) * 100.0;
    }
    State.prev_idle = i;
    State.prev_kern = k;
    State.prev_user = u;
}
static void upd_freq(void)
{
    if (!State.nt_power || State.ncpus <= 0)
        return;
    int sz = State.ncpus * sizeof(CpuPowerInfo);
    CpuPowerInfo *info = (CpuPowerInfo *)malloc(sz);
    if (!info)
        return;
    memset(info, 0, sz);
    if (State.nt_power(11, NULL, 0, info, sz) == 0)
    {
        double sum = 0;
        for (int i = 0; i < State.ncpus; i++)
            sum += info[i].CurrentMhz;
        g_metrics.cpu_ghz = (sum / State.ncpus) / 1000.0;
    }
    free(info);
}
static void upd_temp(void)
{
    if (!State.wmi_ok || !State.wmi_svc)
    {
        g_metrics.cpu_temp = -1;
        return;
    }
    BSTR lang = SysAllocString(L"WQL"), qry = SysAllocString(L"SELECT Value FROM Sensor WHERE SensorType='Temperature' AND Name LIKE 'CPU%'");
    void *enm = NULL;
    if (SUCCEEDED(WMI_Query(State.wmi_svc, lang, qry, &enm)) && enm)
    {
        void *obj = NULL;
        ULONG cnt = 0;
        if (SUCCEEDED(WMI_Next(enm, &obj, &cnt)) && cnt > 0 && obj)
        {
            VARIANT v;
            VariantInit(&v);
            if (SUCCEEDED(WMI_Get(obj, L"Value", &v)))
            {
                if (v.vt == VT_R4)
                    g_metrics.cpu_temp = v.fltVal;
                else if (v.vt == VT_R8)
                    g_metrics.cpu_temp = v.dblVal;
                else if (v.vt == VT_I4)
                    g_metrics.cpu_temp = (double)v.lVal;
                else
                    g_metrics.cpu_temp = -1;
            }
            VariantClear(&v);
            COM_Release(obj);
        }
        COM_Release(enm);
    }
    SysFreeString(lang);
    SysFreeString(qry);
}
static void upd_mem(void)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
    {
        g_metrics.mem_pct = (double)ms.dwMemoryLoad;
        g_metrics.mem_total = (double)ms.ullTotalPhys;
        g_metrics.mem_used = (double)(ms.ullTotalPhys - ms.ullAvailPhys);
    }
}
static void upd_disk(void)
{
    DWORD drives = GetLogicalDrives();
    ULARGE_INTEGER total_bytes = {0}, free_bytes = {0};
    int valid_drives = 0;

    for (int i = 0; i < 26; i++)
    {
        if (drives & (1 << i))
        {
            wchar_t path[] = {(wchar_t)(L'A' + i), L':', L'\\', L'\0'};
            if (GetDriveTypeW(path) == DRIVE_FIXED)
            {
                ULARGE_INTEGER fa, t, ft;
                if (GetDiskFreeSpaceExW(path, &fa, &t, &ft))
                {
                    total_bytes.QuadPart += t.QuadPart;
                    free_bytes.QuadPart += ft.QuadPart;
                    valid_drives = 1;
                }
            }
        }
    }

    if (valid_drives && total_bytes.QuadPart > 0)
    {
        g_metrics.disk_pct = (1.0 - (double)free_bytes.QuadPart / total_bytes.QuadPart) * 100.0;
    }
    else
    {
        g_metrics.disk_pct = -1;
    }
}
static void upd_gpu(void)
{
    if (!State.nvml_ok)
    {
        g_metrics.gpu_pct = -1;
        g_metrics.gpu_temp = -1;
        g_metrics.gpu_vram_used = -1;
        return;
    }
    NvmlUtil u = {0};
    if (State.nvml_util && State.nvml_util(State.nvml_dev, &u) == 0)
        g_metrics.gpu_pct = (double)u.gpu;
    else
        g_metrics.gpu_pct = -1;
    unsigned int t = 0;
    if (State.nvml_temp && State.nvml_temp(State.nvml_dev, 0, &t) == 0)
        g_metrics.gpu_temp = (double)t;
    else
        g_metrics.gpu_temp = -1;
    NvmlMem m = {0};
    if (State.nvml_mem && State.nvml_mem(State.nvml_dev, &m) == 0)
    {
        g_metrics.gpu_vram_used = (double)m.used;
        g_metrics.gpu_vram_total = (double)m.total;
    }
    else
        g_metrics.gpu_vram_used = -1;
}
static void upd_net(void)
{
    MIB_IF_TABLE2 *tbl = NULL;
    if (GetIfTable2(&tbl) != NO_ERROR || !tbl)
        return;
    ULONGLONG recv = 0, sent = 0;
    for (ULONG i = 0; i < tbl->NumEntries; i++)
    {
        MIB_IF_ROW2 *r = &tbl->Table[i];
        if (r->Type == IF_TYPE_SOFTWARE_LOOPBACK || r->OperStatus != IfOperStatusUp)
            continue;
        recv += r->InOctets;
        sent += r->OutOctets;
    }
    FreeMibTable(tbl);
    DWORD now = GetTickCount();
    if (!State.first && now != State.prev_tick)
    {
        DWORD dt = now - State.prev_tick;
        if (dt > 0)
        {
            g_metrics.net_dl_speed = (double)(recv - State.prev_recv) * 1000.0 / dt;
            g_metrics.net_ul_speed = (double)(sent - State.prev_sent) * 1000.0 / dt;
            g_metrics.net_speed = g_metrics.net_dl_speed + g_metrics.net_ul_speed;
        }
    }
    g_metrics.total_traffic = (double)(recv + sent);
    State.prev_recv = recv;
    State.prev_sent = sent;
    State.prev_tick = now;
}

void update_metrics(void)
{
    upd_cpu();
    upd_freq();
    upd_temp();
    upd_mem();
    upd_disk();
    upd_gpu();
    upd_net();
    State.first = 0;
}

void init_metrics(void)
{
    g_metrics.cpu_temp = -1;
    g_metrics.gpu_pct = -1;
    State.first = 1;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    State.ncpus = (int)si.dwNumberOfProcessors;
    HMODULE hp = LoadLibraryA("powrprof.dll");
    if (hp)
        State.nt_power = (pfn_NtPowerInfo)GetProcAddress(hp, "CallNtPowerInformation");

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == (HRESULT)0x80010106L)
    {
        CoInitializeSecurity(NULL, -1, NULL, NULL, 0, 3, NULL, 0, NULL);
        void *loc = NULL;
        if (SUCCEEDED(CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, &loc)) && loc)
        {
            BSTR ns = SysAllocString(L"root\\LibreHardwareMonitor");
            if (SUCCEEDED(WMI_Connect(loc, ns, &State.wmi_svc)) && State.wmi_svc)
                State.wmi_ok = 1;
            SysFreeString(ns);
            COM_Release(loc);
        }
    }

    State.nvml_dll = LoadLibraryA("nvml.dll");
    if (State.nvml_dll)
    {
        pfn_nvmlInit init_fn = (pfn_nvmlInit)GetProcAddress(State.nvml_dll, "nvmlInit_v2");
        if (!init_fn)
            init_fn = (pfn_nvmlInit)GetProcAddress(State.nvml_dll, "nvmlInit");
        pfn_nvmlGetHandle get_h = (pfn_nvmlGetHandle)GetProcAddress(State.nvml_dll, "nvmlDeviceGetHandleByIndex_v2");
        if (!get_h)
            get_h = (pfn_nvmlGetHandle)GetProcAddress(State.nvml_dll, "nvmlDeviceGetHandleByIndex");
        State.nvml_util = (pfn_nvmlGetUtil)GetProcAddress(State.nvml_dll, "nvmlDeviceGetUtilizationRates");
        State.nvml_temp = (pfn_nvmlGetTemp)GetProcAddress(State.nvml_dll, "nvmlDeviceGetTemperature");
        State.nvml_mem = (pfn_nvmlGetMem)GetProcAddress(State.nvml_dll, "nvmlDeviceGetMemoryInfo");
        if (init_fn && get_h && State.nvml_util && init_fn() == 0 && get_h(0, &State.nvml_dev) == 0)
            State.nvml_ok = 1;
    }
    upd_cpu();
    upd_net();
    State.first = 0;
    update_metrics();
}

void shutdown_metrics(void)
{
    if (State.wmi_svc)
        COM_Release(State.wmi_svc);
    if (State.nvml_dll)
    {
        pfn_nvmlShutdown sd = (pfn_nvmlShutdown)GetProcAddress(State.nvml_dll, "nvmlShutdown");
        if (sd)
            sd();
        FreeLibrary(State.nvml_dll);
    }
    CoUninitialize();
}
