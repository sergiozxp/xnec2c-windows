@echo off
setlocal
pushd "%~dp0"
set "PATH=%~dp0bin;%PATH%"
if not defined HOME if defined USERPROFILE set "HOME=%USERPROFILE%"
if exist "%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders.cache" set "GDK_PIXBUF_MODULE_FILE=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders.cache"
if exist "%~dp0lib\gtk-3.0\3.0.0\immodules.cache" set "GTK_IM_MODULE_FILE=%~dp0lib\gtk-3.0\3.0.0\immodules.cache"
set "GSETTINGS_SCHEMA_DIR=%~dp0share\glib-2.0\schemas"
set "XNEC2C_LOCALEDIR=%~dp0share\locale"
"%~dp0bin\xnec2c.exe" %*
set "xnec2c_exit=%ERRORLEVEL%"
popd
exit /b %xnec2c_exit%
