#!/usr/bin/env bash
#
# build-deb.sh - Compila Stellarbox y genera el .deb usando la estructura
#                debian/ (debhelper compat 13, build CMake)
#
# Requisitos: dpkg-buildpackage, debhelper, cmake, make y las librerías -dev
#             indicadas en debian/control.
# Salida:      dist/stellarbox_<version>-<release>_<arch>.deb
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

DIST_DIR="$ROOT/dist"

# version desde CMakeLists.txt (project(... VERSION x.y.z))
VERSION="$(grep -m1 '^    VERSION ' CMakeLists.txt | awk '{print $2}')"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
RELEASE="$(dpkg-parsechangelog -SVersion 2>/dev/null | awk -F- '{print $NF}' || echo 1)"
PACKAGE_NAME="stellarbox_${VERSION}-${RELEASE}_${ARCH}.deb"

echo "==> Compilando y empaquetando Stellarbox ($VERSION, arch $ARCH)"
echo "    con dpkg-buildpackage (debian/rules -> dh --buildsystem=cmake)"

# limpiar árboles de build previos de debhelper (obj-*)
find . -maxdepth 1 -type d -name 'obj-*' -exec rm -rf {} + 2>/dev/null || true

# -b        : solo paquete binario
# --no-sign : sin firmar (sin GPG)
# -d        : no comprobar Build-Depends (libxshape-dev no existe en Debian;
#             XShape está en libxext-dev, y librsvg2-dev es opcional)
# los .deb quedan en el directorio padre del proyecto
dpkg-buildpackage -b --no-sign -us -uc -d

echo "==> Copiando el .deb generado a dist/"

mkdir -p "$DIST_DIR"
rm -f "$DIST_DIR/$PACKAGE_NAME"
if [ -f "../$PACKAGE_NAME" ]; then
    cp "../$PACKAGE_NAME" "$DIST_DIR/$PACKAGE_NAME"
    echo "==> Hecho: $DIST_DIR/$PACKAGE_NAME"
else
    echo "ERROR: no se encontró ../$PACKAGE_NAME" >&2
    exit 1
fi