# Procedencia upstream

## Snapshot usado

| Campo | Valor |
|---|---|
| Proyecto | Xnec2c |
| URL oficial | `https://github.com/KJ7LNW/xnec2c` |
| Remoto Git local | `upstream` |
| Referencia observada | `upstream/master` |
| Commit fijado | `a0245e6c0ed4c44909993a83db9e2a162fa81a60` |
| Fecha | `2026-08-24T14:10:47-07:00` |
| Asunto | `render: generalize vector pipeline to draw the animated far-zone field` |
| Versión en `AC_INIT` | `4.4.18` |

Este snapshot es posterior a la última release etiquetada cuando se inició el
port. El tag `v4.4.18` resuelve al commit
`2408f783cf012f41c776c9e8ec52817ae3fe8294`; ese tag no incluye el conmutador
de configuración `--disable-opengl`, por lo que no satisface el objetivo de la
primera etapa sin incorporar una serie amplia de commits por separado.

`packaging/windows/upstream.env` contiene estos valores en formato apto para
shell. El workflow comprueba que el commit fijado siga siendo ancestro del
checkout antes de compilar.

## Política de cambios locales

Los parches Windows deben permanecer pequeños, documentados y fuera de la
lógica NEC2. Se prefieren abstracciones ya requeridas por Xnec2c (GLib,
GModule, pthreads) y el camino serial existente antes que implementaciones
paralelas específicas de Windows.

## Incorporar una versión futura

1. Asegurar que el remoto apunta al origen oficial:

   ```sh
   git remote set-url upstream https://github.com/KJ7LNW/xnec2c.git
   git fetch upstream --tags --prune
   ```

2. Inspeccionar releases y commits, y elegir una referencia inmutable:

   ```sh
   git log --oneline --decorate --max-count=20 upstream/master
   git tag --sort=-version:refname | head
   git diff --stat HEAD..upstream/master
   ```

3. Crear una rama de actualización y fusionar o rebasar los commits locales de
   integración sobre el nuevo commit. Resolver conflictos sin mezclar cambios
   funcionales de NEC2 con compatibilidad de plataforma.

4. Actualizar este archivo y `packaging/windows/upstream.env`, ejecutar el build
   UCRT64 y revisar que GitHub Actions produzca el artefacto portable.

5. Registrar por separado la importación upstream, los parches Windows y los
   cambios de empaquetado para que una futura actualización sea auditable.
