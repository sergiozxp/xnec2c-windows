# Compilar Xnec2c en Windows 11 x64

Este procedimiento reproduce el job de GitHub Actions en una instalación local
de Windows 11. Todos los comandos de build deben ejecutarse en la terminal
**MSYS2 UCRT64**, no en MSYS, MINGW64, PowerShell ni WSL.

## 1. Instalar MSYS2 y las dependencias

Instale MSYS2 desde <https://www.msys2.org/> y abra **MSYS2 UCRT64**. Actualice
el sistema según las indicaciones de MSYS2 y, tras reiniciar la terminal si se
solicita, instale:

```sh
pacman -Syu
pacman -S --needed \
  autoconf \
  automake \
  gettext-devel \
  git \
  libtool \
  make \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-gsl \
  mingw-w64-ucrt-x86_64-gtk3 \
  mingw-w64-ucrt-x86_64-librsvg \
  mingw-w64-ucrt-x86_64-pkgconf
```

Compruebe el entorno antes de continuar:

```sh
test "$MSYSTEM" = UCRT64
which gcc pkg-config glib-compile-resources
pkg-config --modversion gtk+-3.0 gsl gmodule-2.0
```

## 2. Preparar el árbol

Clone este repositorio o abra el checkout existente desde UCRT64. La fuente
upstream ya forma parte del historial; no hay una descarga implícita durante el
build.

```sh
cd /c/ruta/al/xnec2c-windows
git status
git rev-parse HEAD
```

La base exacta está registrada en [UPSTREAM.md](UPSTREAM.md). Para evitar que
Git convierta scripts a CRLF, el repositorio incluye `.gitattributes`.

## 3. Generar el sistema de build y compilar

```sh
autoreconf --force --install
./configure --disable-opengl --disable-silent-rules
make -j"$(nproc)"
make -C t check -j"$(nproc)"
```

Aspectos esperados de `configure`:

- host terminado en `mingw32` y detección `WINDOWS` activa;
- GTK 3, GSL, GIO y GModule encontrados bajo `/ucrt64`;
- OpenGL desactivado;
- inotify, `fork` y backtrace POSIX ausentes;
- pthread disponible mediante winpthreads.

Valide el ejecutable dentro del entorno de build:

```sh
./src/xnec2c.exe --version
ldd ./src/xnec2c.exe
```

La salida de versión conserva `4.4.18` porque ese es el valor declarado por el
snapshot upstream fijado.

## 4. Crear el paquete portable

```sh
./packaging/windows/package-portable.sh
```

El resultado aparece en:

```text
dist/xnec2c-windows-x64-ucrt64/
├── xnec2c.cmd
├── bin/
│   ├── xnec2c.exe
│   └── *.dll
├── lib/                     # módulos cargados dinámicamente por GTK/GIO
├── share/                   # esquemas, temas, traducciones y documentación
├── BUILDINFO.txt
└── SHA256SUMS
```

Copie el directorio completo a otra ubicación o equipo Windows x64. Inicie la
aplicación mediante `xnec2c.cmd`; el lanzador fija el directorio de trabajo y
las rutas relocatables de módulos GTK. No es necesario instalar MSYS2 en el
equipo de destino.

## 5. GitHub Actions

`.github/workflows/build-windows.yml` usa `windows-2022`, UCRT64 y las mismas
dependencias. Las acciones externas están fijadas por SHA. Cada artefacto
incluye `BUILDINFO.txt` con:

- commit upstream fijado;
- commit exacto del repositorio de integración;
- versión del compilador;
- lista y versión de todos los paquetes MSYS2 instalados.

Los repositorios binarios de MSYS2 son móviles; por eso `BUILDINFO.txt` es parte
del contrato de reproducibilidad. Para una reproducción histórica exacta deben
usarse las versiones allí registradas o un mirror/snapshot equivalente.

## Limitaciones de la primera etapa

- `fork()` no existe en MinGW. Windows fuerza el camino serial upstream de
  `-j0`; un `-j` mayor se reduce de forma segura a un único worker.
- Los pthreads continúan activos. `--no-pthreads` queda disponible para
  diagnóstico, no es necesario para el uso normal.
- El observador externo de archivos requiere inotify y queda deshabilitado;
  los optimizadores internos con pthread siguen compilándose.
- OpenGL está fuera de alcance y se configura siempre con
  `--disable-opengl`.
- El solver NEC2 incorporado es el fallback portable. Incluir y validar
  OpenBLAS/MKL nativos será una etapa posterior.
- El paquete aún no está firmado y no incluye instalador.

## Diagnóstico rápido

- Si `pkg-config` encuentra rutas `/usr` en vez de `/ucrt64`, se abrió la
  terminal equivocada.
- Si `autoreconf` informa que falta `autopoint`, instale `gettext-devel`.
- Si faltan iconos SVG en el paquete, confirme que `librsvg` estaba instalado
  al ejecutar `package-portable.sh`.
- Si Windows bloquea el paquete descargado, extraiga primero el artefacto y
  revise sus hashes con `SHA256SUMS` antes de ejecutarlo.
