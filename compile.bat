@echo off
echo Compiling Indra APC Plugin...

where cl >nul 2>nul
if errorlevel 1 (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    )
)

where cl >nul 2>nul
if errorlevel 1 (
    echo MSVC compiler not found. Install Visual Studio Build Tools, or run from a Developer Command Prompt.
    exit /b 2
)

set SOURCES=src\Plugin.cpp src\AIRACChecker\Version.cpp
set OUTPUT=build\MagPlugin.dll
set ES_LIB=
set ES_INCLUDE=

if exist "EuroScopePlugIn.lib"        set ES_LIB=EuroScopePlugIn.lib
if exist "EuroScopePlugInDll.lib"     set ES_LIB=EuroScopePlugInDll.lib
if exist "lib\EuroScopePlugIn.lib"    set ES_LIB=lib\EuroScopePlugIn.lib
if exist "lib\EuroScopePlugInDll.lib" set ES_LIB=lib\EuroScopePlugInDll.lib

if exist "include\EuroScopePlugIn.h" set ES_INCLUDE=include
if exist "src\EuroScopePlugIn.h"     set ES_INCLUDE=src
if exist "lib\EuroScopePlugIn.h"     set ES_INCLUDE=lib
if exist "EuroScopePlugIn.h"         set ES_INCLUDE=.

if "%ES_LIB%"=="" (
    echo Missing EuroScope SDK import library.
    echo Copy EuroScopePlugIn.lib or EuroScopePlugInDll.lib into this folder,
    echo or put it in a lib subfolder, then run compile.bat again.
    exit /b 2
)

if "%ES_INCLUDE%"=="" (
    echo Missing EuroScope SDK header: EuroScopePlugIn.h
    echo Copy EuroScopePlugIn.h from the EuroScope plugin SDK into:
    echo   %CD%\lib
    echo The matching import library is already present at: %ES_LIB%
    exit /b 2
)

echo Using EuroScope library: %ES_LIB%
echo Using EuroScope headers: %ES_INCLUDE%

if not exist build mkdir build

cl /nologo /LD /MD /O2 /Oi /GL /EHsc ^
    /std:c++14 ^
    /D "_CRT_SECURE_NO_WARNINGS" ^
    /D "WIN32_LEAN_AND_MEAN" ^
    /I "%ES_INCLUDE%" ^
    /I "include" ^
    /I "src" ^
    /Fe"%OUTPUT%" ^
    %SOURCES% ^
    /link /SUBSYSTEM:WINDOWS /MACHINE:X86 /LTCG /OPT:REF /OPT:ICF /DLL ^
    /EXPORT:EuroScopePlugInInit=?EuroScopePlugInInit@@YAXPAPAVCPlugIn@EuroScopePlugIn@@@Z ^
    /EXPORT:EuroScopePlugInExit=?EuroScopePlugInExit@@YAXXZ ^
    "%ES_LIB%" user32.lib gdi32.lib ws2_32.lib winhttp.lib

if %errorlevel% == 0 (
    echo Compilation successful! Output: %OUTPUT%
) else (
    echo Compilation failed with error code %errorlevel%
    exit /b %errorlevel%
)
