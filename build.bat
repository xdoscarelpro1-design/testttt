@echo off
setlocal enabledelayedexpansion

:: ════════════════════════════════════════════════════════════════
::  PHANTOM EFI Bootkit — Windows Build Script (MSVC)
:: ════════════════════════════════════════════════════════════════
::
::  REQUISITOS:
::    - Visual Studio 2022 (Community es gratis)
::      Workload: "Desktop development with C++"
::    - Ejecutar desde "x64 Native Tools Command Prompt for VS 2022"
::      o "Developer Command Prompt for VS 2022"
::
::  USO:
::    build.bat          — compila la cheat DLL
::    build.bat all      — compila todo (DLL + kernel + EFI)
::    build.bat clean    — limpia build/
::
:: ════════════════════════════════════════════════════════════════

:: Check MSVC
where cl >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cl.exe no encontrado.
    echo.
    echo Abre "x64 Native Tools Command Prompt for VS 2022"
    echo   Start ^> Visual Studio 2022 ^> x64 Native Tools Command Prompt
    echo.
    echo O ejecuta esto primero:
    echo   "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    exit /b 1
)

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
set CFLAGS=/O2 /std:c++17 /EHsc /DUNICODE /D_UNICODE /DWIN32 /D_WINDOWS
set CFLAGS=%CFLAGS% /I%INC% /I%SRC_CHEAT% /I%DXSDK_INC%

:: Linker flags
set LDFLAGS=/LD /Fe:%BUILD_CHEAT%\phantom_cheat.dll
set LIBS=kernel32.lib user32.lib gdi32.lib shell32.lib shlwapi.lib ole32.lib advapi32.lib ntdll.lib dwmapi.lib
set LIBS=%LIBS% /LIBPATH:%DXSDK_LIB% d3d11.lib d3dx11.lib dxgi.lib

echo [*] cl.exe compilando %BUILD_CHEAT%\phantom_cheat.dll ...
cl %CFLAGS% %SRCS% /link %LDFLAGS% %LIBS% /ENTRY:DllMain /DLL /MACHINE:X64
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
