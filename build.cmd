@echo off
setlocal EnableExtensions
pushd "%~dp0"

rem ------------------------------------------------------------
rem Xploder PSX Converter native MSVC build script
rem
rem Expected layout:
rem   build-gui-msvc.cmd
rem   src\XploderConverterGui.cpp
rem   src\MultiFormatCodeConverter.hpp
rem   src\CodeTypeCommon.hpp
rem   src\GameSharkActionReplayCodeTypes.hpp
rem   src\XploderCodeTypes.hpp
rem   src\DuckStationCodeTypes.hpp
rem   src\CaetlaCodeTypes.hpp
rem   src\Ps1MipsCodeTypes.hpp
rem   src\XploderCmpConverter.hpp
rem   src\XploderMemoryCryptEngine.hpp
rem
rem Optional icon/resource:
rem   src\XploderConverterGui.rc
rem   src\resource.h
rem   src\XploderNeonX.ico
rem
rem Output is placed beside this .cmd file:
rem   XploderConverterGui-Win32.exe
rem   XploderConverterGui-Win64.exe
rem ------------------------------------------------------------

set "SRC_DIR=src"
set "MAIN_CPP=%SRC_DIR%\XploderConverterGui.cpp"
set "RC_FILE=%SRC_DIR%\XploderConverterGui.rc"

if not exist "%MAIN_CPP%" (
    echo.
    echo ERROR: Could not find "%MAIN_CPP%".
    echo Make sure this build script is beside the src folder.
    echo Current folder: %CD%
    goto fail
)

rem ------------------------------------------------------------
rem Target selection
rem You can also run:
rem   build-gui-msvc.cmd win32
rem   build-gui-msvc.cmd win64
rem ------------------------------------------------------------

set "TARGET_CHOICE=%~1"

if /I "%TARGET_CHOICE%"=="x86"   set "TARGET_CHOICE=win32"
if /I "%TARGET_CHOICE%"=="32"    set "TARGET_CHOICE=win32"
if /I "%TARGET_CHOICE%"=="win32" set "TARGET_CHOICE=win32"

if /I "%TARGET_CHOICE%"=="x64"   set "TARGET_CHOICE=win64"
if /I "%TARGET_CHOICE%"=="64"    set "TARGET_CHOICE=win64"
if /I "%TARGET_CHOICE%"=="win64" set "TARGET_CHOICE=win64"

if not defined TARGET_CHOICE (
    echo.
    echo Select build target:
    echo   1. Win32 / x86
    echo   2. Win64 / x64
    echo.
    set /p "TARGET_CHOICE=Enter 1 or 2: "
)

if "%TARGET_CHOICE%"=="1" set "TARGET_CHOICE=win32"
if "%TARGET_CHOICE%"=="2" set "TARGET_CHOICE=win64"

if /I "%TARGET_CHOICE%"=="win32" (
    set "TARGET_ARCH=x86"
    set "TARGET_NAME=Win32 / x86"
    set "MACHINE=X86"
    set "OUT_EXE=XploderConverterGui-Win32.exe"
    set "RES_FILE=%SRC_DIR%\XploderConverterGui-Win32.res"
) else if /I "%TARGET_CHOICE%"=="win64" (
    set "TARGET_ARCH=x64"
    set "TARGET_NAME=Win64 / x64"
    set "MACHINE=X64"
    set "OUT_EXE=XploderConverterGui-Win64.exe"
    set "RES_FILE=%SRC_DIR%\XploderConverterGui-Win64.res"
) else (
    echo.
    echo ERROR: Invalid target "%TARGET_CHOICE%".
    echo Use 1, 2, win32, win64, x86, or x64.
    goto fail
)

echo.
echo Target selected: %TARGET_NAME%

rem ------------------------------------------------------------
rem Initialize Visual Studio build environment for selected target.
rem This is done even if cl.exe is already in PATH, because the user
rem may be switching between Win32 and Win64 builds.
rem ------------------------------------------------------------

call :SetupVisualStudioDevCmd "%TARGET_ARCH%"
if errorlevel 1 (
    where cl.exe >nul 2>nul
    if errorlevel 1 (
        echo.
        echo ERROR: Could not find a Visual Studio C++ build environment.
        echo Install Visual Studio or Visual Studio Build Tools with the "Desktop development with C++" workload.
        echo.
        goto fail
    ) else (
        echo.
        echo WARNING: Could not auto-switch Visual Studio target environment.
        echo Using the cl.exe already available in PATH.
        echo If the wrong EXE type is produced, open the matching Developer Command Prompt and rerun this script.
        echo.
    )
)

where cl.exe >nul 2>nul
if errorlevel 1 (
    echo.
    echo ERROR: cl.exe is not available.
    goto fail
)

rem ------------------------------------------------------------
rem Optional resource/icon compile.
rem The .res is only linked if it was actually created.
rem ------------------------------------------------------------

set "EXTRA_INPUTS="
if exist "%RC_FILE%" (
    echo.
    echo Compiling resource file: %RC_FILE%
    if exist "%RES_FILE%" del /q "%RES_FILE%" >nul 2>nul

    rc /nologo /i "%SRC_DIR%" /fo"%RES_FILE%" "%RC_FILE%"
    if errorlevel 1 (
        echo.
        echo ERROR: Resource compile failed.
        echo Check paths inside "%RC_FILE%".
        goto fail
    )

    if exist "%RES_FILE%" (
        set "EXTRA_INPUTS=%RES_FILE%"
    ) else (
        echo.
        echo WARNING: Resource compiler finished, but no .res file was created.
        echo Building without embedded icon/resource.
    )
) else (
    echo.
    echo No resource file found at "%RC_FILE%". Building without embedded icon.
)

rem ------------------------------------------------------------
rem Build.
rem Output goes directly beside this .cmd file.
rem ------------------------------------------------------------

echo.
echo Building native Windows desktop GUI for %TARGET_NAME%...
cl /nologo /std:c++17 /EHsc /O2 /W3 /I "%SRC_DIR%" /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN "%MAIN_CPP%" %EXTRA_INPUTS% /Fe:"%OUT_EXE%" /link /SUBSYSTEM:WINDOWS /MACHINE:%MACHINE% user32.lib gdi32.lib comctl32.lib shell32.lib comdlg32.lib ole32.lib

if errorlevel 1 (
    echo.
    echo ERROR: Build failed.
    goto fail
)

echo.
echo Build succeeded:
echo   %CD%\%OUT_EXE%

rem ------------------------------------------------------------
rem Optional verification using dumpbin, if available.
rem Win64 should show "machine (x64)".
rem Win32 should show "machine (x86)".
rem ------------------------------------------------------------

where dumpbin.exe >nul 2>nul
if not errorlevel 1 (
    echo.
    echo EXE machine check:
    dumpbin /headers "%OUT_EXE%" | findstr /I /C:"machine"
) else (
    echo.
    echo Note: dumpbin.exe was not found, so the EXE machine type was not printed.
    echo To verify manually, open a Visual Studio Developer Command Prompt and run:
    echo   dumpbin /headers "%OUT_EXE%" ^| findstr /I machine
)

goto done

rem ------------------------------------------------------------
rem Visual Studio discovery
rem ------------------------------------------------------------
:SetupVisualStudioDevCmd
set "REQ_ARCH=%~1"
set "VSINSTALL="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
    if defined VSINSTALL (
        if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
            echo Found Visual Studio: %VSINSTALL%
            call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=%REQ_ARCH% -host_arch=x64 >nul
            exit /b %errorlevel%
        )
    )

    set "VSINSTALL="
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -prerelease -products * -property installationPath`) do set "VSINSTALL=%%I"
    if defined VSINSTALL (
        if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
            echo Found Visual Studio: %VSINSTALL%
            call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=%REQ_ARCH% -host_arch=x64 >nul
            exit /b %errorlevel%
        )
    )
)

for %%P in (
    "%ProgramFiles%\Microsoft Visual Studio\18\Insiders"
    "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\18\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\18\Community"
    "%ProgramFiles%\Microsoft Visual Studio\18\BuildTools"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Insiders"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Enterprise"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Professional"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Community"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\2026\Insiders"
    "%ProgramFiles%\Microsoft Visual Studio\2026\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\2026\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\2026\Community"
    "%ProgramFiles%\Microsoft Visual Studio\2026\BuildTools"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2026\Insiders"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2026\Enterprise"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2026\Professional"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2026\Community"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2026\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools"
) do (
    if exist "%%~P\Common7\Tools\VsDevCmd.bat" (
        echo Found Visual Studio: %%~P
        call "%%~P\Common7\Tools\VsDevCmd.bat" -arch=%REQ_ARCH% -host_arch=x64 >nul
        exit /b %errorlevel%
    )
)

exit /b 1

:fail
echo.
pause
popd
endlocal
exit /b 1

:done
echo.
pause
popd
endlocal
exit /b 0
