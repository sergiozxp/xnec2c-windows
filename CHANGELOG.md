# Changelog

Este archivo registra los cambios de la integración Windows. El historial de
cambios del proyecto original se conserva en `ChangeLog` y `doc/xnec2c.html`.

## [Unreleased]

### Added

- Build reproducible para Windows 11 x64 con MSYS2/UCRT64.
- Workflow de GitHub Actions que compila sin OpenGL y publica un paquete
  portable con sus DLL y datos de GTK.
- Documentación de compilación, procedencia upstream y actualización futura.
- Estructura `packaging/windows/`, manifiesto de upstream, lanzador portable y
  script de recolección de dependencias.
- Launcher Win32 nativo `xnec2c-launcher.exe`, de subsistema GUI, que resuelve
  el portable de forma relativa, reenvía argumentos y muestra errores mediante
  cuadros de diálogo de Windows.
- Base de instalador Inno Setup por usuario, compilación automatizada y
  artefacto `Xnec2c-4.4.18-Windows-x64-Setup.exe`.

### Changed

- Carga de bibliotecas matemáticas abstraída mediante GModule en lugar de la
  API POSIX `dlopen`/`dlsym`.
- Builds nativos de Windows usan el camino serial existente y excluyen la
  inicialización y recolección POSIX basada en `fork`, pipes y señales.
- Asignación alineada, sincronización de archivos y exportación de callbacks
  GTK adaptadas a MinGW/UCRT.
- Creación de directorios y pruebas de upstream adaptadas a las APIs
  disponibles en MinGW, sin alterar los casos comprobados.
- El build Cairo incorpora el stub no-OpenGL que faltaba para la invalidación
  de geometría.
- El archivo upstream `PACKAGING` se conserva como `PACKAGING-UPSTREAM.md` para
  permitir el directorio `packaging/` en sistemas de archivos de Windows, que
  no distinguen mayúsculas de minúsculas.
- El inicio normal del portable pasa a `xnec2c-launcher.exe`; `xnec2c.cmd`
  queda disponible únicamente para diagnóstico con consola.
- GitHub Actions valida launcher, relocación, dependencias y 12/12 pruebas, y
  publica por separado el portable y el instalador.

### Known limitations

- OpenGL desactivado.
- Sin multiproceso por `fork` en Windows; pthreads se mantienen.
- Bibliotecas matemáticas aceleradas externas no se incluyen todavía.
- Sin firma de código.
- Asociación `.nec` preparada pero desactivada; sin menú contextual moderno de
  Windows 11.

### Upstream

- Snapshot: `a0245e6c0ed4c44909993a83db9e2a162fa81a60`.
- Versión declarada: `4.4.18`.
