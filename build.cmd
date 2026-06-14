@echo off
setlocal EnableExtensions
pushd "%~dp0"

rem ------------------------------------------------------------
rem Xploder Converter native Win32 GUI build script
rem Expected layout:
rem   build-gui-msvc.cmd
rem   src\XploderConverterGui.cpp
rem   src\XploderCmpConverter.hpp
rem   src\XploderMemoryCryptEngine.hpp
rem Optional icon/resource:
rem   src\XploderConverterGui.rc
rem   src\resource.h
rem   src\XploderNeonX.ico
rem Output:
rem   XploderConverterGui.exe beside this .cmd file
rem ------------------------------------------------------------

set "SRC_DIR=src"
set "MAIN_CPP=%SRC_DIR%\XploderConverterGui.cpp"
set "RC_FILE=%SRC_DIR%\XploderConverterGui.rc"
set "RES_FILE=%SRC_DIR%\XploderConverterGui.res"
set "OUT_EXE=XploderConverterGui.exe"

if not exist "%MAIN_CPP%" goto missing_source

where cl.exe >nul 2>nul
if errorlevel 1 (
    echo cl.exe not found in PATH. Trying to initialize Visual Studio Developer Command Prompt...
    call :SetupVisualStudioDevCmd
    if errorlevel 1 goto missing_vs
)

where cl.exe >nul 2>nul
if errorlevel 1 goto missing_cl

if exist "%RES_FILE%" del /q "%RES_FILE%" >nul 2>nul
if exist "%OUT_EXE%" del /q "%OUT_EXE%" >nul 2>nul

set "EXTRA_INPUTS="
if exist "%RC_FILE%" (
    echo.
    echo Compiling resource file: %RC_FILE%
    rc /nologo /i "%SRC_DIR%" /fo"%RES_FILE%" "%RC_FILE%"
    if errorlevel 1 goto resource_fail

    if exist "%RES_FILE%" (
        set "EXTRA_INPUTS=%RES_FILE%"
    ) else (
        echo.
        echo WARNING: rc.exe completed, but "%RES_FILE%" was not created.
        echo Building without embedded icon/resource.
        set "EXTRA_INPUTS="
    )
) else (
    echo.
    echo No resource file found at "%RC_FILE%". Building without embedded icon.
)

echo.
echo Building native Win32 GUI...
if defined EXTRA_INPUTS (
    echo Including resource: %EXTRA_INPUTS%
)

cl /nologo /std:c++17 /EHsc /O2 /W3 /I "%SRC_DIR%" /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN "%MAIN_CPP%" %EXTRA_INPUTS% /Fe:"%OUT_EXE%" /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib shell32.lib comdlg32.lib ole32.lib
if errorlevel 1 goto build_fail

echo.
echo Build succeeded:
echo   %CD%\%OUT_EXE%
goto done

:missing_source
echo.
echo ERROR: Could not find "%MAIN_CPP%".
echo This script must be beside the src folder.
echo Current folder: %CD%
goto fail

:missing_vs
echo.
echo ERROR: Could not find a Visual Studio C++ build environment.
echo Install Visual Studio / Build Tools with "Desktop development with C++".
goto fail

:missing_cl
echo.
echo ERROR: Visual Studio was found, but cl.exe is still not available.
goto fail

:resource_fail
echo.
echo ERROR: Resource compile failed.
echo Check "%RC_FILE%" and make sure icon files referenced inside it are in the src folder.
goto fail

:build_fail
echo.
echo ERROR: Build failed.
goto fail

:done
echo.
pause
popd
endlocal
exit /b 0

:fail
echo.
pause
popd
endlocal
exit /b 1

:SetupVisualStudioDevCmd
rem Preferred: vswhere, installed with Visual Studio Installer.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
    if defined VSINSTALL (
        if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
            echo Found Visual Studio: %VSINSTALL%
            call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
            exit /b %errorlevel%
        )
    )

    rem Retry without the C++ workload filter. cl.exe is checked again afterward.
    set "VSINSTALL="
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -prerelease -products * -property installationPath`) do set "VSINSTALL=%%I"
    if defined VSINSTALL (
        if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
            echo Found Visual Studio: %VSINSTALL%
            call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
            exit /b %errorlevel%
        )
    )
)

rem Fallback common install paths, including Visual Studio 18 / 2026 Insiders.
for %%P in (
    "%ProgramFiles%\Microsoft Visual Studio\18\Insiders"
    "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\18\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\18\Community"
    "%ProgramFiles%\Microsoft Visual Studio\18\BuildTools"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Insiders"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\2026\Insiders"
    "%ProgramFiles%\Microsoft Visual Studio\2026\BuildTools"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2026\Insiders"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2026\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
) do (
    if exist "%%~P\Common7\Tools\VsDevCmd.bat" (
        echo Found Visual Studio: %%~P
        call "%%~P\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
        exit /b %errorlevel%
    )
)

exit /b 1
