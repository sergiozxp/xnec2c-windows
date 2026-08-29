# Empaquetado de Windows

Esta carpeta concentra todo lo específico de distribución para no mezclarlo
con el motor NEC2.

- `upstream.env`: procedencia inmutable consumida por scripts y CI.
- `package-portable.sh`: reúne el ejecutable, DLL transitivas, módulos GTK,
  datos de runtime, manifiesto de paquetes y hashes.
- `xnec2c.cmd`: lanzador relocatable del paquete portable.

Los directorios reservados `installer/` y `scripts/` contienen marcadores para
las etapas futuras. Un instalador deberá consumir la salida de
`package-portable.sh`, no reconstruir una segunda lista independiente de DLL.

El script sólo reemplaza automáticamente destinos bajo `dist/` dentro del
checkout. Esta restricción evita borrar por accidente un directorio arbitrario
cuando se introduce una ruta incorrecta.
