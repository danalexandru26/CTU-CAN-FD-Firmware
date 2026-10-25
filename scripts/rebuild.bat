@echo off
rem Rebuild the Vitis platform + app from git into a fresh workspace.
rem Usage: scripts\rebuild.bat [workspace_dir]   (default: .\_rebuild_ws)
rem Override the Vitis install with: set XILINX_VITIS=C:\path\to\Vitis

if "%XILINX_VITIS%"=="" set XILINX_VITIS=C:\AMDDesignTools\2025.2.1\Vitis
pushd "%~dp0.."
call "%XILINX_VITIS%\bin\vitis.bat" -s scripts\rebuild.py %*
set RC=%ERRORLEVEL%
popd
exit /b %RC%
