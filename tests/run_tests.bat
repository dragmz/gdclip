@echo off
REM Build and run the gdclip unit tests with the MSVC toolchain.
REM Run from a "Developer Command Prompt for VS" so cl.exe is on PATH.
setlocal
set ROOT=%~dp0..

cl /nologo /EHsc /std:c++14 /O2 /I "%ROOT%" ^
	"%ROOT%\tests\test_gdclip.cpp" ^
	"%ROOT%\gdclip_core.cpp" ^
	"%ROOT%\clipper\clipper.cpp" ^
	/Fe:"%ROOT%\tests\test_gdclip.exe" /Fo:"%ROOT%\tests\\"
if errorlevel 1 exit /b 1

"%ROOT%\tests\test_gdclip.exe"
