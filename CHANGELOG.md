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

### Known limitations

- OpenGL desactivado.
- Sin multiproceso por `fork` en Windows; pthreads se mantienen.
- Bibliotecas matemáticas aceleradas externas no se incluyen todavía.
- Sin instalador ni firma de código.

### Upstream

- Snapshot: `a0245e6c0ed4c44909993a83db9e2a162fa81a60`.
- Versión declarada: `4.4.18`.
