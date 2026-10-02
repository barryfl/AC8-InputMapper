@echo off
setlocal
cd /d "%~dp0"
set "LIST_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%LIST_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "LIST_VS=%%i"
if not defined LIST_VS exit /b 1
call "%LIST_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\controller-list mkdir build\controller-list
cl /nologo /std:c++17 /O2 /W4 /WX /MT /EHsc /Fo:build\controller-list\list.obj /Fe:build\controller-list\List-Controllers.exe src\list-controllers.cpp dxguid.lib ole32.lib dinput8.lib
if errorlevel 1 exit /b 1
copy /y List-Controllers.cmd build\controller-list\List-Controllers.cmd >nul
echo Controller inventory built. Run build\controller-list\List-Controllers.cmd.
