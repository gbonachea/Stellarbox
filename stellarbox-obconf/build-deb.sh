#!/usr/bin/env bash
#
# build-deb.sh - Compila stellarbox-obconf (build CMake) y empaqueta un .deb
#
# Requisitos: qt6-base-dev, qt6-tools-dev, cmake, debhelper (13) y
#             stellarbox instalado (pagoseducativos .pc de obt/obrender).
# Salida:      dist/stellarbox-obconf_<version>.deb
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# sanity: pkg-config debe encontrar obt-3.5 / obrender-3.5 (instalados por stellarbox)
pkg-config --exists obt-3.5 obrender-3.5 || {
    echo "ERROR: no se encuentran obt-3.5/obrender-3.5. Instala primero stellarbox." >&2
    exit 1
}

echo "==> Compilando stellarbox-obconf"

cmake -S "$ROOT" -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

VERSION="$(grep -m1 'stellarbox-obconf (' debian/changelog | sed -n 's/.*(\([^)]*\)).*/\1/p')"

echo "==> Empaquetando .deb ($VERSION)"

dpkg-buildpackage -b --no-sign -us -uc -d 2>&1 | tail -5

mkdir -p dist
cp ../stellarbox-obconf_${VERSION}_amd64.deb dist/ 2>/dev/null || \
    cp ../stellarbox-obconf_${VERSION}_*.deb dist/

# limpiar artefactos transient de debhelper
rm -rf debian/.debhelper debian/files debian/*.debhelper.log debian/*.postinst.debhelper debian/*.postrm.debhelper debian/*.preinst.debhelper debian/*.prerm.debhelper

echo "==> Hecho: dist/$(ls dist | grep stellarbox-obconf | tail -1)"