@echo off
echo ============================================
echo   Resource Monitor - Build Script
echo ============================================
echo.

:: Try GCC (MinGW) first
where gcc >nul 2>&1
if %errorlevel% equ 0 (
    echo [BUILD] Using GCC...
    gcc -O2 -o ResourceMonitor.exe src\*.c -lgdi32 -luser32 -liphlpapi -lole32 -loleaut32 -lcomctl32 -lshell32 -mwindows
    if %errorlevel% equ 0 (
        echo.
        echo [OK] Build successful: ResourceMonitor.exe
        for %%A in (ResourceMonitor.exe) do echo [OK] Size: %%~zA bytes
    ) else (
        echo [ERROR] Build failed.
    )
    goto :done
)

:: Try MSVC
where cl >nul 2>&1
if %errorlevel% equ 0 (
    echo [BUILD] Using MSVC...
    cl /O2 src\*.c /link user32.lib gdi32.lib iphlpapi.lib ole32.lib oleaut32.lib comctl32.lib shell32.lib /SUBSYSTEM:WINDOWS /OUT:ResourceMonitor.exe
    if %errorlevel% equ 0 (
        echo.
        echo [OK] Build successful: ResourceMonitor.exe
    ) else (
        echo [ERROR] Build failed.
    )
    goto :done
)

echo [ERROR] No C compiler found. Install GCC (MinGW-w64) or MSVC.

:done
echo.
pause
