@echo off
setlocal enabledelayedexpansion

:: ════════════════════════════════════════════════════════════════
::  PHANTOM EFI Bootkit — Windows Build Script (MSVC)
:: ════════════════════════════════════════════════════════════════
::
::  REQUISITOS:
::    - Visual Studio (2022, 2024, 2026, cualquier version)
::      Workload: "Desktop development with C++"
::    - Ejecutar desde "x64 Native Tools Command Prompt for VS"
::      o dejar que el script lo detecte automaticamente
::
::  USO:
::    build.bat          — compila la cheat DLL
::    build.bat all      — compila todo (DLL + kernel + EFI)
::    build.bat clean    — limpia build/
::
:: ════════════════════════════════════════════════════════════════

:: Check MSVC — if cl.exe is not in PATH, try to find vcvars64.bat automatically
where cl >nul 2>&1
if errorlevel 1 (
    echo [*] cl.exe no encontrado en PATH, buscando Visual Studio...
    set "FOUND_VS="

    :: Check common VS install paths (newest first)
    for %%Y in (2026 2025 2024 2022) do (
        for %%E in (Community Professional Enterprise) do (
            set "VCVARS=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
            if exist "!VCVARS!" (
                echo [*] Encontrado VS %%Y %%E
                call "!VCVARS!" >nul 2>&1
                set "FOUND_VS=1"
                goto :vs_found
            )
        )
    )

    :: Also check vswhere (works with any VS version)
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" (
            echo [*] Encontrado VS via vswhere: %%i
            call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
            set "FOUND_VS=1"
            goto :vs_found
        )
    )

    echo [ERROR] No se encontro Visual Studio.
    echo.
    echo Opciones:
    echo   1. Abre "x64 Native Tools Command Prompt" desde el menu de VS
    echo   2. Instala Visual Studio con "Desktop development with C++"
    exit /b 1
)
:vs_found

set TARGET=%1
if "%TARGET%"=="" set TARGET=cheatdll

:: Dirs
set SRC_CHEAT=src\cheat
set BUILD=build
set BUILD_CHEAT=%BUILD%\cheat
set BUILD_KERN=%BUILD%\kernel
set BUILD_EFI=%BUILD%\efi
set BUILD_USB=%BUILD%\usb
set INC=include

:: DirectX SDK (bundled in repo)
set DXSDK_INC=%SRC_CHEAT%\FrameWork\Dependencies\DirectX\Include
set DXSDK_LIB=%SRC_CHEAT%\FrameWork\Dependencies\DirectX\Lib

if "%TARGET%"=="clean" goto :clean

:: ═══════════════════════════════════════════════════
:: Create dirs
:: ═══════════════════════════════════════════════════
if not exist %BUILD_CHEAT% mkdir %BUILD_CHEAT%
if not exist %BUILD_KERN% mkdir %BUILD_KERN%
if not exist %BUILD_EFI% mkdir %BUILD_EFI%
if not exist %BUILD_USB% mkdir %BUILD_USB%
if not exist %INC% mkdir %INC%

:: ═══════════════════════════════════════════════════
:: STEP 1: Compile Cheat DLL
:: ═══════════════════════════════════════════════════
echo.
echo ══════════════════════════════════════════════
echo  [1/3] Compilando Cheat DLL...
echo ══════════════════════════════════════════════

:: Collect source files (main cheat + ImGui — NOT crypto)
set SRCS=
set SRCS=%SRCS% %SRC_CHEAT%\PhantomComm.cpp
set SRCS=%SRCS% %SRC_CHEAT%\dllmain.cpp
set SRCS=%SRCS% %SRC_CHEAT%\main.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Cheat.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\FivemSDK\Fivem.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\LegitBot\AimBot.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\LegitBot\SilentAim.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\LegitBot\TriggerBot.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\Misc\Bypass.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\Misc\Exploits.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\Misc\KernelCloak.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\Misc\Protection.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\Visuals\PlayerESP.cpp
set SRCS=%SRCS% %SRC_CHEAT%\Cheat\Features\Visuals\VehicleESP.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Render\Assets.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Render\Interface.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Render\Overlay.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Utilities\Memory.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Utilities\Misc.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Utilities\SigScanner.cpp

:: ImGui sources
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui_draw.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui_impl_dx11.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui_impl_win32.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui_tables.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui_widgets.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui_stdlib.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\imgui_tricks.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\custom.cpp
set SRCS=%SRCS% %SRC_CHEAT%\FrameWork\Dependencies\ImGui\slider.cpp

:: Compiler flags
set CFLAGS=/O2 /std:c++17 /EHa /DUNICODE /D_UNICODE /DWIN32 /D_WINDOWS
set CFLAGS=%CFLAGS% /DIMGUI_DEFINE_MATH_OPERATORS /wd4005 /wd4838
set CFLAGS=%CFLAGS% /I%INC% /I%SRC_CHEAT% /I%DXSDK_INC%

:: Linker flags
set LDFLAGS=/Fe:%BUILD_CHEAT%\phantom_cheat.dll
set LIBS=kernel32.lib user32.lib gdi32.lib shell32.lib shlwapi.lib ole32.lib advapi32.lib ntdll.lib dwmapi.lib winhttp.lib
set LIBS=%LIBS% /LIBPATH:%DXSDK_LIB% d3d11.lib d3dx11.lib dxgi.lib

echo [*] cl.exe compilando %BUILD_CHEAT%\phantom_cheat.dll ...
cl %CFLAGS% %SRCS% /link %LDFLAGS% %LIBS% /DLL /MACHINE:X64
if errorlevel 1 (
    echo [FAIL] Error compilando cheat DLL
    exit /b 1
)
echo [OK] Cheat DLL: %BUILD_CHEAT%\phantom_cheat.dll

if "%TARGET%"=="cheatdll" goto :done

:: ═══════════════════════════════════════════════════
:: STEP 2: Generate cheat blob header
:: ═══════════════════════════════════════════════════
echo.
echo ══════════════════════════════════════════════
echo  [2/3] Generando blob del cheat...
echo ══════════════════════════════════════════════

python -c "data=open('build/cheat/phantom_cheat.dll','rb').read();f=open('include/cheat_payload_blob.h','w');f.write('#ifndef _CHEAT_PAYLOAD_BLOB_H_\n#define _CHEAT_PAYLOAD_BLOB_H_\n\nstatic const unsigned char gCheatPayloadBlob[] = {\n');[f.write('    '+', '.join('0x%%02x'%%b for b in data[i:i+16])+',\n') for i in range(0,len(data),16)];f.write('};\n\n#endif\n');f.close();print('[OK] Cheat blob: %%d bytes' %% len(data))"
if errorlevel 1 (
    echo [FAIL] Python no encontrado o error generando blob
    echo Instala Python 3: https://www.python.org/downloads/
    exit /b 1
)

if "%TARGET%"=="payload" goto :build_kernel
if "%TARGET%"=="all" goto :build_kernel
goto :done

:: ═══════════════════════════════════════════════════
:: STEP 3: Build kernel payload + EFI
:: ═══════════════════════════════════════════════════
:build_kernel
echo.
echo ══════════════════════════════════════════════
echo  [3/3] Kernel + EFI (requiere WSL o Linux)
echo ══════════════════════════════════════════════
echo.
echo El kernel payload y el EFI bootkit se compilan con GCC (gnu-efi).
echo Desde WSL ejecuta:
echo.
echo   cd /mnt/c/ruta/al/proyecto
echo   make payload
echo   make efi
echo.
echo O si tienes MSYS2:
echo   pacman -S mingw-w64-x86_64-gcc
echo   make payload
echo   make efi
echo.
echo El output final es: build/efi/bootx64.efi
goto :done

:: ═══════════════════════════════════════════════════
:clean
echo [*] Limpiando build/...
if exist %BUILD% rd /s /q %BUILD%
echo [OK] Limpio
goto :eof

:done
echo.
echo ══════════════════════════════════════════════
echo  BUILD COMPLETO
echo ══════════════════════════════════════════════
echo  Cheat DLL:  %BUILD_CHEAT%\phantom_cheat.dll
if exist %INC%\cheat_payload_blob.h echo  Cheat blob: %INC%\cheat_payload_blob.h
echo ══════════════════════════════════════════════
