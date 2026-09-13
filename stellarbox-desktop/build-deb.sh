#!/usr/bin/env bash
#
# build-deb.sh - Compila stellarbox-desktop (CMake) y empaqueta un .deb.
#
# Requisitos: cmake, gcc, pkg-config, libgtk-3-dev, debhelper (13).
# Salida:      dist/stellarbox-desktop_<version>.deb
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

echo "==> Compilando stellarbox-desktop"

cmake -S "$ROOT" -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

VERSION="$(grep -m1 'stellarbox-desktop (' debian/changelog | sed -n 's/.*(\([^)]*\)).*/\1/p')"

echo "==> Empaquetando .deb ($VERSION)"

dpkg-buildpackage -b --no-sign -us -uc -d 2>&1 | tail -5

mkdir -p dist
cp ../stellarbox-desktop_${VERSION}_amd64.deb dist/ 2>/dev/null || \
    cp ../stellarbox-desktop_${VERSION}_*.deb dist/

# limpiar artefactos transient de debhelper
rm -rf debian/.debhelper debian/files debian/*.debhelper.log debian/*.postinst.debhelper debian/*.postrm.debhelper debian/*.preinst.debhelper debian/*.prerm.debhelper

echo "==> Hecho: dist/$(ls dist | grep stellarbox-desktop | tail -1)"