#!/usr/bin/env bash
#
# build-rpm.sh - Compila stellarbox-desktop y empaqueta un .rpm.
#
# Requisitos: cmake, gcc, rpmbuild, tar.
# Salida:      dist/stellarbox-desktop-<version>.<arch>.rpm
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

VERSION="$(grep -m1 'stellarbox-desktop (' debian/changelog | sed -n 's/.*(\([^)]*\)).*/\1/p')"
RELEASE="1"
if command -v rpm >/dev/null 2>&1; then
    ARCH_PATH="$(rpm --eval '%{_arch}')"
else
    ARCH_PATH="$(uname -m)"
fi
PACKAGE_NAME="stellarbox-desktop-${VERSION}-${RELEASE}.${ARCH_PATH}.rpm"

STAGE_DIR="$ROOT/dist/rpm/stage"
RPMROOT="$ROOT/dist/rpm/rpmbuild"

echo "==> Compilando stellarbox-desktop"

cmake -S "$ROOT" -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo "==> Preparando el arbol de instalacion"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
DESTDIR="$STAGE_DIR" cmake --install build

echo "==> Generando $PACKAGE_NAME"

mkdir -p dist
rm -rf "$RPMROOT"
mkdir -p "$RPMROOT"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

( cd "$STAGE_DIR" && tar -czf "$RPMROOT/SOURCES/stellarbox-desktop-payload.tar.gz" . )

cat > "$RPMROOT/SPECS/stellarbox-desktop.spec" <<EOF
Name: stellarbox-desktop
Version: ${VERSION}
Release: ${RELEASE}
Summary: Independent desktop (wallpaper + ~/Desktop icons) for Stellarbox
License: MIT
Vendor: Stellarbox
Group: User Interface/Desktops
Source0: stellarbox-desktop-payload.tar.gz

%description
Wallpaper and desktop icons for Stellarbox. Independent of any specific
file manager; works with stellarbox or any window manager.

%prep
tar -xzf %{SOURCE0}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a . %{buildroot}/

%files
/

%changelog
* $(date '+%a %b %d %Y') Stellarbox
- Initial RPM packaging.
EOF

rm -f "$PACKAGE_NAME"
rpmbuild -bb \
    --define "_topdir $RPMROOT" \
    --define "_binary_payload w9.xzdio" \
    "$RPMROOT/SPECS/stellarbox-desktop.spec"

RPMS_FILE="$(find "$RPMROOT/RPMS" -name 'stellarbox-desktop-*.rpm' | head -1)"
cp "$RPMS_FILE" "dist/$PACKAGE_NAME"

echo "==> Hecho: dist/$PACKAGE_NAME"