@echo off
setlocal
cd /d "%~dp0"
set "COMPAT_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%COMPAT_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "COMPAT_VS=%%i"
if not defined COMPAT_VS exit /b 1
call "%COMPAT_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\compatibility mkdir build\compatibility
cl /nologo /LD /std:c++17 /O2 /W4 /WX /MT /EHsc /Fo:build\compatibility\runtime.obj src\runtime.cpp /link /DEF:src\exports.def /OUT:build\compatibility\dinput8.dll /IMPLIB:build\compatibility\compat.lib /PDB:build\compatibility\compat.pdb /DYNAMICBASE /NXCOMPAT /IGNORE:4222 dxguid.lib ole32.lib bcrypt.lib user32.lib
if errorlevel 1 exit /b 1
copy /y AC8InputMapper.example.ini build\compatibility\AC8InputMapper.ini >nul
cl /nologo /std:c++17 /O2 /W4 /WX /MT /EHsc /Fo:build\compatibility\test.obj /Fe:build\compatibility\compat-test.exe tests\compat-test.cpp dxguid.lib ole32.lib bcrypt.lib user32.lib
if errorlevel 1 exit /b 1
build\compatibility\compat-test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /WX /MT /EHsc /Fo:build\compatibility\multi-test.obj /Fe:build\compatibility\multisource-test.exe tests\multisource-test.cpp dxguid.lib ole32.lib bcrypt.lib user32.lib
if errorlevel 1 exit /b 1
build\compatibility\multisource-test.exe
if errorlevel 1 exit /b 1
dumpbin /exports build\compatibility\dinput8.dll >build\compatibility\exports.txt
powershell -NoProfile -ExecutionPolicy Bypass -File tests\verify-compatibility.ps1
if errorlevel 1 exit /b 1
echo Compatibility prototype built and locally tested. No game files modified.

