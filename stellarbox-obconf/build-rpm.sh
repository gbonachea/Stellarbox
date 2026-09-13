#!/usr/bin/env bash
#
# build-rpm.sh - Compila stellarbox-obconf y empaqueta un .rpm
#
# Requisitos: cmake, make, rpmbuild, tar y stellarbox instalado (pc de obt/obrender).
# Salida:      dist/stellarbox-obconf-<version>.<arch>.rpm
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

pkg-config --exists obt-3.5 obrender-3.5 || {
    echo "ERROR: no se encuentran obt-3.5/obrender-3.5. Instala primero stellarbox." >&2
    exit 1
}

VERSION="$(grep -m1 'stellarbox-obconf (' debian/changelog | sed -n 's/.*(\([^)]*\)).*/\1/p')"
RELEASE="1"
if command -v rpm >/dev/null 2>&1; then
    ARCH_PATH="$(rpm --eval '%{_arch}')"
else
    ARCH_PATH="$(uname -m)"
fi
PACKAGE_NAME="stellarbox-obconf-${VERSION}-${RELEASE}.${ARCH_PATH}.rpm"

STAGE_DIR="$ROOT/dist/rpm/stage"
RPMROOT="$ROOT/dist/rpm/rpmbuild"

echo "==> Compilando stellarbox-obconf"

cmake -S "$ROOT" -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo "==> Preparando el árbol de instalación"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
DESTDIR="$STAGE_DIR" cmake --install build

echo "==> Generando $PACKAGE_NAME"

mkdir -p dist
rm -rf "$RPMROOT"
mkdir -p "$RPMROOT"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

( cd "$STAGE_DIR" && tar -czf "$RPMROOT/SOURCES/stellarbox-obconf-payload.tar.gz" . )

cat > "$RPMROOT/SPECS/stellarbox-obconf.spec" <<EOF
Name: stellarbox-obconf
Version: ${VERSION}
Release: ${RELEASE}
Summary: A Qt6 preferences manager for Stellarbox (fork of ObConf-Qt)
License: GPL-2.0-or-later
Vendor: Stellarbox CMake fork
URL: https://github.com/lxqt/obconf-qt
Source0: stellarbox-obconf-payload.tar.gz
Group: User Interface/Desktops

%description
Stellarbox Configuration is a Qt6 port of ObConf, a configuration editor for
the Stellarbox window manager (a fork of Openbox). It edits appearance,
window, mouse, desktop, dock and keyboard settings in the stellarbox rc.xml.

Requires: stellarbox
Requires: qt6-qtbase

%prep
tar -xzf %{SOURCE0}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a . %{buildroot}/

%files
/

%post
/usr/bin/update-menus 2>/dev/null || true

%postun
/usr/bin/update-menus 2>/dev/null || true

%changelog
* $(date '+%a %b %d %Y') Stellarbox CMake fork
- Initial RPM packaging of the Qt6 fork.
EOF

rm -f "$PACKAGE_NAME"
rpmbuild -bb \
    --define "_topdir $RPMROOT" \
    --define "_binary_payload w9.xzdio" \
    "$RPMROOT/SPECS/stellarbox-obconf.spec"

RPMS_FILE="$(find "$RPMROOT/RPMS" -name 'stellarbox-obconf-*.rpm' | head -1)"
cp "$RPMS_FILE" "dist/$PACKAGE_NAME"

echo "==> Hecho: dist/$PACKAGE_NAME"