@echo off
rem run_capture.bat: the Release build with F3 frame captures on (out\build\release\captures\<HHMMSS>).
rem Double-click it instead of Darkwatch.exe when testing, so a restart keeps captures working.
cd /d "%~dp0..\out\build\release"
set XBE_CAPTURE_DIR=captures
start "" Darkwatch.exe
