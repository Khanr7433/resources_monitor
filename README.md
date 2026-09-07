# 🖥️ Resource Monitor Widget

An ultra-lightweight (under 5MB RAM), always-on-top desktop widget for Windows that displays real-time system resource usage near the taskbar.

![Widget Preview](docs/images/preview.png)

---

## ✨ Features

- **CPU**: Usage %, Temperature (°C), Clock Speed (GHz)
- **Memory**: RAM usage %
- **GPU**: Usage %, VRAM, Temperature (NVIDIA only)
- **Disk**: Overall Storage Usage % (Across all drives)
- **Network**: Live Download/Upload speed + Total cumulative traffic
- **Taskbar-docked**: Fixed position left of the system tray
- **1-second refresh**: Stats update in real time natively via Windows APIs
- **Ultra-lightweight**: Written in pure C (No Python, No heavy UI frameworks)

> **Note**: CPU temperature requires [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases) running in the background.  
> GPU stats require NVIDIA drivers. Otherwise, these show `N/A`.

---

## 📋 Quick Start

### Option 1: Run the Pre-built `.exe`

1. Download or locate `ResourceMonitor.exe`
1. Double-click to run
1. The widget appears automatically docked to the taskbar

### Option 2: Build from Source (GCC)

Since this app is written in pure C, you only need a C compiler (like GCC/MinGW or MSVC).

```powershell
# Navigate to the project
cd d:\Projects\resourse_moniter

# Run the automated build script
.\build.bat
```

The output `.exe` will be generated in the root directory.

---

## 🎮 Controls & Settings

| Action                           | Effect                            |
| -------------------------------- | --------------------------------- |
| **Right-click widget**           | Open context menu                 |
| **Context Menu -> Task Manager** | Opens Windows Task Manager        |
| **Context Menu -> Settings**     | Opens the Display Settings dialog |
| **Context Menu -> Exit**         | Closes the widget completely      |

### Customizing the Display

Open the **Settings** menu by right-clicking the widget. From there, you can:

- **Toggle** metrics on or off.
- **Reorder** metrics by selecting them and using the **Move Up** and **Move Down** buttons.
- Click **OK** to instantly apply and save your layout to `config.ini`.

---

## ⚙️ Configuration

The widget automatically saves its layout state to a `config.ini` file in the same directory as the executable. It is human-readable, but we recommend using the GUI Settings dialog to modify it.

---

## Project Structure

```text
resources_monitor/
├── src/
│   ├── main.c           # Entry point and widget lifecycle
│   ├── metrics.c        # Hardware data collection (WMI, NVML, IP Helper)
│   ├── config.c         # Layout state and config.ini parsing
│   ├── gui.c            # GDI rendering and Settings Dialog UI
│   └── common.h         # Shared constants and macros
├── build.bat            # Automated C compiler script
└── README.md            # This file
```
