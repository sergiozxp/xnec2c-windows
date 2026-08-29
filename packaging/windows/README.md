# Empaquetado de Windows

Esta carpeta concentra todo lo específico de distribución para no mezclarlo
con el motor NEC2.

- `upstream.env`: procedencia inmutable consumida por scripts y CI.
- `launcher/`: fuente C, recursos de versión y manifiesto del launcher Win32.
- `build-launcher.sh`: compila un PE de subsistema GUI reproducible en UCRT64.
- `package-portable.sh`: reúne el ejecutable, DLL transitivas, módulos GTK,
  datos de runtime, manifiesto de paquetes y hashes.
- `xnec2c.cmd`: inicio alternativo con consola, reservado para diagnóstico.
- `xnec2c.iss`: definición del instalador por usuario de Inno Setup.
- `build-installer.ps1`: localiza `ISCC.exe`, valida el portable y compila el
  Setup.

El instalador consume la salida de `package-portable.sh`; no mantiene una
segunda lista independiente de DLL. La asociación `.nec` está incluida detrás
de `EnableNecAssociation=0` y no se compila en esta etapa.

El script sólo reemplaza automáticamente destinos bajo `dist/` dentro del
checkout. Esta restricción evita borrar por accidente un directorio arbitrario
cuando se introduce una ruta incorrecta.
