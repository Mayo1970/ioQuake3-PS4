@echo off
REM ============================================================================
REM Build ioQuake3 QVM Tools using Visual Studio 2026
REM ============================================================================
REM Run this from the code/tools directory in a "Developer Command Prompt for VS 2026"
REM ============================================================================

setlocal EnableDelayedExpansion

REM Find VS2026 installation path
if exist "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VSVARS=C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VSVARS=C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VSVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else (
    echo ERROR: Visual Studio 2026 not found at expected path.
    echo Looking for: C:\Program Files\Microsoft Visual Studio\18\[Edition]\VC\Auxiliary\Build\vcvarsall.bat
    echo Please run this from a "Developer Command Prompt for VS 2026" instead.
    exit /b 1
)

REM Initialize VS environment for x64
if not defined VCINSTALLDIR (
    echo Initializing Visual Studio environment...
    call "%VSVARS%" x64
)

set CC=cl
REM NOTE: /DWIN32 (not /D_WIN32) is needed because cmdlib.c checks #ifdef WIN32
set CFLAGS=/O2 /W3 /TC /D_CRT_SECURE_NO_WARNINGS /DWIN32 /D_WIN32 /D__i386__ /D__LCC__
REM Link against user32.lib for FindWindow, PostMessage, RegisterWindowMessage
REM Use /link to pass libraries to the linker phase
set LDFLAGS=/link user32.lib

set BIN_DIR=bin

if not exist %BIN_DIR% mkdir %BIN_DIR%

echo.
echo === Building lburg ===
%CC% %CFLAGS% /I lcc\lburg lcc\lburg\lburg.c lcc\lburg\gram.c /Fe%BIN_DIR%\lburg.exe
if errorlevel 1 goto error

echo.
echo === Generating dagcheck.c ===
%BIN_DIR%\lburg.exe lcc\src\dagcheck.md lcc\src\dagcheck.c
if errorlevel 1 goto error

echo.
echo === Building q3asm ===
%CC% %CFLAGS% /I asm asm\q3asm.c asm\cmdlib.c /Fe%BIN_DIR%\q3asm.exe %LDFLAGS%
if errorlevel 1 goto error

echo.
echo === Building q3cpp ===
%CC% %CFLAGS% /I lcc\cpp lcc\cpp\cpp.c lcc\cpp\eval.c lcc\cpp\getopt.c lcc\cpp\hideset.c lcc\cpp\include.c lcc\cpp\lex.c lcc\cpp\macro.c lcc\cpp\nlist.c lcc\cpp\tokens.c lcc\cpp\unix.c /Fe%BIN_DIR%\q3cpp.exe
if errorlevel 1 goto error

echo.
echo === Building q3rcc ===
%CC% %CFLAGS% /I lcc\src lcc\src\alloc.c lcc\src\bind.c lcc\src\bytecode.c lcc\src\dag.c lcc\src\decl.c lcc\src\enode.c lcc\src\error.c lcc\src\event.c lcc\src\expr.c lcc\src\gen.c lcc\src\init.c lcc\src\inits.c lcc\src\input.c lcc\src\lex.c lcc\src\list.c lcc\src\main.c lcc\src\null.c lcc\src\output.c lcc\src\prof.c lcc\src\profio.c lcc\src\simp.c lcc\src\stmt.c lcc\src\string.c lcc\src\sym.c lcc\src\symbolic.c lcc\src\trace.c lcc\src\tree.c lcc\src\types.c lcc\src\dagcheck.c /Fe%BIN_DIR%\q3rcc.exe
if errorlevel 1 goto error

echo.
echo === Building q3lcc ===
%CC% %CFLAGS% /I lcc\src /I lcc\etc /I ..\..\qcommon lcc\etc\lcc.c lcc\etc\bytecode.c /Fe%BIN_DIR%\q3lcc.exe
if errorlevel 1 goto error

echo.
echo ============================================================================
echo SUCCESS! QVM tools built in %BIN_DIR%\
echo ============================================================================
echo.
echo Binaries:
echo   %BIN_DIR%\lburg.exe
echo   %BIN_DIR%\q3lcc.exe
echo   %BIN_DIR%\q3cpp.exe
echo   %BIN_DIR%\q3rcc.exe
echo   %BIN_DIR%\q3asm.exe
echo.
goto end

:error
echo.
echo ============================================================================
echo BUILD FAILED
echo ============================================================================
exit /b 1

:end
del *.obj
endlocal
