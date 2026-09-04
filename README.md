# Xnec2c para Windows

Este repositorio mantiene una integración mínima y reproducible de
[Xnec2c](https://github.com/KJ7LNW/xnec2c) para Windows 11 x64. La primera
etapa produce una aplicación nativa con MSYS2/UCRT64, GTK 3 y el renderizador
Cairo; OpenGL queda desactivado expresamente.

La automatización de GitHub Actions compila el código y publica el artefacto
`xnec2c-windows-x64-ucrt64`. El paquete contiene `bin/xnec2c.exe`, sus DLL de
UCRT64, los módulos y datos de ejecución de GTK necesarios, un launcher Win32
nativo sin consola y metadatos de la compilación. `xnec2c.cmd` se conserva sólo
para diagnóstico.

## Base upstream fijada

- Repositorio oficial: <https://github.com/KJ7LNW/xnec2c>
- Commit: `a0245e6c0ed4c44909993a83db9e2a162fa81a60`
- Fecha del commit: 2026-08-24
- Versión declarada por `configure.ac`: `4.4.18`
- Configuración de esta etapa: `--disable-opengl`

El commit elegido es un snapshot posterior al tag `v4.4.18`: se usa porque el
tag publicado todavía no contiene la opción `--disable-opengl`. La procedencia,
la diferencia con el tag y el procedimiento de actualización están detallados
en [UPSTREAM.md](UPSTREAM.md). El mismo dato está disponible para scripts en
`packaging/windows/upstream.env`.

## Estado del port

Los cambios de plataforma están limitados a las fronteras incompatibles con
MinGW:

- carga dinámica mediante GModule en vez de `dlopen`/`dlsym`;
- ejecución serial en Windows usando el camino upstream de `-j0`, sin
  `fork`, pipes ni señales POSIX;
- pthreads/winpthreads conservado para el bucle de frecuencia y optimizadores;
- asignación alineada y sincronización de archivos adaptadas a UCRT;
- exportación de callbacks para `GtkBuilder` en PE/COFF;
- optimizador externo basado en inotify deshabilitado por la detección que ya
  existe en upstream.

No se modifica la lógica numérica de NEC2. En esta etapa se usa por defecto el
solver NEC2 incorporado; el soporte de bibliotecas matemáticas externas queda
preparado a través de GModule, pero su distribución se abordará por separado.

## Compilar

El procedimiento local completo está en [README-WINDOWS.md](README-WINDOWS.md).
En síntesis, desde una terminal **MSYS2 UCRT64**:

```sh
autoreconf --force --install
./configure --disable-opengl --disable-silent-rules
make -j"$(nproc)"
make -C t check -j"$(nproc)"
./src/xnec2c.exe --version
./packaging/windows/package-portable.sh
```

El paquete local se crea en `dist/xnec2c-windows-x64-ucrt64/`.
Se inicia normalmente con `xnec2c-launcher.exe`, también aceptando una ruta
`.nec` como argumento. El portable no requiere MSYS2, WSL ni Cygwin en el
equipo de destino.

Después de crear el portable, el instalador por usuario se genera desde
PowerShell con:

```powershell
.\packaging\windows\build-installer.ps1
```

El Setup se escribe en
`dist/installer/Xnec2c-4.4.18-Windows-x64-Setup.exe` e instala sin UAC bajo
`%LOCALAPPDATA%\Programs\Xnec2c`.

El paquete para Microsoft Store se genera, a partir del mismo portable, con:

```powershell
.\packaging\windows\build-msix.ps1
```

El resultado es `dist/msix/Xnec2c-4.4.18-Windows-x64.msix`. Antes de enviarlo
hay que reservar la aplicación en Partner Center y completar la identidad exacta
de la Store; consulte [README-WINDOWS.md](README-WINDOWS.md).

## Alcance pendiente

- comprobar y habilitar bibliotecas BLAS/LAPACK nativas de Windows;
- evaluar paralelismo multiproceso nativo sin trasladar la arquitectura POSIX;
- incorporar OpenGL después de estabilizar el build Cairo;
- validar y habilitar la asociación `.nec` y evaluar un icono Windows dedicado;
- completar la identidad de Microsoft Store y publicar el MSIX;\n- incorporar firma de código para las descargas directas fuera de la Store;
- añadir pruebas funcionales con escritorio interactivo de Windows.

Xnec2c se distribuye bajo GPL; consulte [COPYING](COPYING). El historial y la
autoría upstream se conservan íntegramente.
