#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
source "$script_dir/upstream.env"

exe="${1:-$repo_root/src/xnec2c.exe}"
output="${2:-$repo_root/dist/xnec2c-windows-x64-ucrt64}"

if [[ "${MSYSTEM:-}" != "UCRT64" || "${MINGW_PREFIX:-}" != "/ucrt64" ]]; then
  echo "error: run this script from an MSYS2 UCRT64 shell" >&2
  exit 1
fi

if [[ ! -f "$exe" ]]; then
  echo "error: executable not found: $exe" >&2
  exit 1
fi

if [[ -e "$output" ]]; then
  case "$output" in
    "$repo_root"/dist/*) rm -rf -- "$output" ;;
    *)
      echo "error: refusing to replace output outside $repo_root/dist" >&2
      exit 1
      ;;
  esac
fi

mkdir -p "$output/bin" "$output/lib" "$output/share/doc/xnec2c-windows"
cp -- "$exe" "$output/bin/xnec2c.exe"
cp -- "$script_dir/xnec2c.cmd" "$output/xnec2c.cmd"

copy_tree() {
  local source_path="$1"
  local destination_path="$2"

  if [[ -d "$source_path" ]]; then
    mkdir -p -- "$(dirname -- "$destination_path")"
    cp -a -- "$source_path" "$destination_path"
  fi
}

# Modules loaded at runtime do not appear in xnec2c.exe's import table. Keep
# their native directory layout so GLib's Windows prefix relocation works.
module_sources=()
module_relpaths=(
  "lib/gdk-pixbuf-2.0"
  "lib/gio/modules"
  "lib/gtk-3.0/3.0.0/immodules"
  "lib/gtk-3.0/3.0.0/printbackends"
)

for relpath in "${module_relpaths[@]}"; do
  source_path="$MINGW_PREFIX/$relpath"
  if [[ -d "$source_path" ]]; then
    copy_tree "$source_path" "$output/$relpath"
    while IFS= read -r -d '' module; do
      module_sources+=("$module")
    done < <(find "$source_path" -type f -name '*.dll' -print0)
  fi
done

copy_tree "$MINGW_PREFIX/share/glib-2.0/schemas" \
  "$output/share/glib-2.0/schemas"
copy_tree "$MINGW_PREFIX/share/icons/Adwaita" "$output/share/icons/Adwaita"
copy_tree "$MINGW_PREFIX/share/icons/hicolor" "$output/share/icons/hicolor"
copy_tree "$MINGW_PREFIX/share/themes" "$output/share/themes"

# Install the translations produced by `make` using gettext's conventional
# relocatable layout.
while IFS= read -r language; do
  language="${language%$'\r'}"
  [[ -n "$language" && "$language" != \#* ]] || continue
  catalog="$repo_root/po/$language.gmo"
  if [[ -f "$catalog" ]]; then
    mkdir -p "$output/share/locale/$language/LC_MESSAGES"
    cp -- "$catalog" \
      "$output/share/locale/$language/LC_MESSAGES/xnec2c.mo"
  fi
done < "$repo_root/po/LINGUAS"

declare -A runtime_dlls=()

collect_deps() {
  local binary="$1"
  local line token candidate

  while IFS= read -r line; do
    for token in $line; do
      candidate="${token%$'\r'}"
      candidate="${candidate//\\//}"
      [[ "$candidate" == *.dll ]] || continue

      if [[ "$candidate" =~ ^[A-Za-z]:/ ]]; then
        candidate="$(cygpath -u "$candidate")"
      fi

      if [[ -f "$candidate" && "$candidate" == */ucrt64/bin/*.dll ]]; then
        runtime_dlls["$candidate"]=1
      fi
    done
  done < <(ldd "$binary")
}

collect_deps "$exe"
for module in "${module_sources[@]}"; do
  collect_deps "$module"
done

for dll in "${!runtime_dlls[@]}"; do
  cp -- "$dll" "$output/bin/"
done

if (( ${#runtime_dlls[@]} == 0 )); then
  echo "error: no UCRT64 runtime DLLs were discovered" >&2
  exit 1
fi

# Generate relocatable module caches. Their entries are relative to the package
# root; xnec2c.cmd changes to that root before starting the executable.
pixbuf_dir="$output/lib/gdk-pixbuf-2.0/2.10.0/loaders"
if compgen -G "$pixbuf_dir/*.dll" >/dev/null && \
   command -v gdk-pixbuf-query-loaders >/dev/null; then
  output_mixed="$(cygpath -m "$output")"
  gdk-pixbuf-query-loaders "$pixbuf_dir"/*.dll \
    | sed -e "s|$output/||g" -e "s|$output_mixed/||g" \
    > "$output/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
fi

immodules_dir="$output/lib/gtk-3.0/3.0.0/immodules"
if compgen -G "$immodules_dir/*.dll" >/dev/null && \
   command -v gtk-query-immodules-3.0 >/dev/null; then
  output_mixed="$(cygpath -m "$output")"
  gtk-query-immodules-3.0 "$immodules_dir"/*.dll \
    | sed -e "s|$output/||g" -e "s|$output_mixed/||g" \
    > "$output/lib/gtk-3.0/3.0.0/immodules.cache"
fi

if [[ -d "$output/lib/gio/modules" ]] && command -v gio-querymodules >/dev/null; then
  gio-querymodules "$output/lib/gio/modules"
fi

for document in COPYING README.md README-WINDOWS.md CHANGELOG.md UPSTREAM.md; do
  if [[ -f "$repo_root/$document" ]]; then
    cp -- "$repo_root/$document" "$output/share/doc/xnec2c-windows/"
  fi
done
cp -- "$script_dir/upstream.env" "$output/share/doc/xnec2c-windows/"

{
  echo "Xnec2c portable Windows build"
  echo "upstream_url=$XNEC2C_UPSTREAM_URL"
  echo "upstream_ref=$XNEC2C_UPSTREAM_REF"
  echo "upstream_commit=$XNEC2C_UPSTREAM_COMMIT"
  echo "upstream_version=$XNEC2C_UPSTREAM_VERSION"
  echo "integration_commit=$(git -C "$repo_root" rev-parse HEAD)"
  echo "configure_flags=--disable-opengl --disable-silent-rules"
  echo "msystem=$MSYSTEM"
  echo
  gcc --version
  echo
  echo "MSYS2 packages:"
  pacman -Q | LC_ALL=C sort
} > "$output/BUILDINFO.txt"

(
  cd "$output"
  find . -type f ! -name SHA256SUMS -print0 \
    | LC_ALL=C sort -z \
    | xargs -0 sha256sum > SHA256SUMS
)

echo "Portable package created at: $output"
