@echo off
setlocal

set "ROOT=%~dp0"
set "RUN_DIR=%ROOT%build3"
set "STABLE_EXE=%ROOT%build3\RelWithDebInfo\openttd.exe"
set "FALLBACK_EXE=%ROOT%build3\Release\openttd.exe"

if exist "%STABLE_EXE%" (
	start "" /D "%RUN_DIR%" "%STABLE_EXE%" %*
	exit /b 0
)

if exist "%FALLBACK_EXE%" (
	start "" /D "%RUN_DIR%" "%FALLBACK_EXE%" %*
	exit /b 0
)

echo Could not find the Windows OpenTTD build to launch.
echo Expected one of:
echo   %STABLE_EXE%
echo   %FALLBACK_EXE%
exit /b 1
