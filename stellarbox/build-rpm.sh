#!/usr/bin/env bash
#
# build-rpm.sh - Compila Stellarbox (build CMake) y empaqueta un .rpm
#
# Requisitos: cmake, make, rpmbuild, tar
# Salida:      dist/stellarbox-<version>-<release>.<arch>.rpm
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

BUILD_DIR="$ROOT/build"
DIST_DIR="$ROOT/dist"
STAGE_DIR="$DIST_DIR/rpm/stage"
RPMROOT="$DIST_DIR/rpm/rpmbuild"

PREFIX="/usr"
SYSCONFDIR="/etc"
DATADIR="/usr/share"
LIBEXECDIR="/usr/lib/stellarbox"
PKGNAME="stellarbox"

# version desde CMakeLists.txt (project(... VERSION x.y.z))
VERSION="$(grep -m1 '^    VERSION ' CMakeLists.txt | awk '{print $2}')"
RELEASE="1"
if command -v rpm >/dev/null 2>&1; then
    ARCH_PATH="$(rpm --eval '%{_arch}')"
else
    ARCH_PATH="$(uname -m)"
fi
PACKAGE_NAME="${PKGNAME}-${VERSION}-${RELEASE}.${ARCH_PATH}.rpm"

echo "==> Compilando stellarbox ($VERSION, arch $ARCH_PATH)"

# configurar con los prefijos de empaquetado (/usr, /etc)
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release

# compilar
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Preparando el árbol de instalación"

# árbol de instalación limpio
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

# binario (stellarbox) instalado por CMake
DESTDIR="$STAGE_DIR" cmake --install "$BUILD_DIR"

# --- datos que CMake no instala -------------------------------------
# config por defecto del sistema -> /etc/xdg/stellarbox
XDG_DIR="$STAGE_DIR${SYSCONFDIR}/xdg/$PKGNAME"
mkdir -p "$XDG_DIR"
cp data/rc.xml data/menu.xml data/environment "$XDG_DIR/"
sed -e 's|@libexecdir@|/usr/lib|g' data/autostart/autostart.in \
    > "$XDG_DIR/autostart"
chmod +x "$XDG_DIR/autostart"

# temas -> /usr/share/stellarbox/themes (árbol propio, ajeno a openbox)
THEMES_DIR="$STAGE_DIR${DATADIR}/stellarbox/themes"
rm -rf "$THEMES_DIR"
mkdir -p "$THEMES_DIR"
for th in themes/*/; do
    cp -a "$th" "$THEMES_DIR/"
done

# icono y sessiones -> /usr/share
mkdir -p "$STAGE_DIR${DATADIR}/pixmaps"
cp data/stellarbox.png "$STAGE_DIR${DATADIR}/pixmaps/$PKGNAME.png"

XSESSIONS_DIR="$STAGE_DIR${DATADIR}/xsessions"
mkdir -p "$XSESSIONS_DIR"
for f in stellarbox stellarbox-gnome stellarbox-kde; do
    sed -e 's|@bindir@|/usr/bin|g' "data/xsession/${f}.desktop.in" \
        > "$XSESSIONS_DIR/${f}.desktop"
done
sed -e 's|@configdir@|/etc/xdg|g' -e 's|@bindir@|/usr/bin|g' -e 's|@libexecdir@|/usr/lib/stellarbox|g' \
    data/xsession/stellarbox-session.in > "$STAGE_DIR/usr/bin/stellarbox-session"
sed -e 's|@bindir@|/usr/bin|g' \
    data/xsession/stellarbox-gnome-session.in > "$STAGE_DIR/usr/bin/stellarbox-gnome-session"
sed -e 's|@bindir@|/usr/bin|g' \
    data/xsession/stellarbox-kde-session.in > "$STAGE_DIR/usr/bin/stellarbox-kde-session"
cp data/stellarbox-apps-menu.py "$STAGE_DIR/usr/bin/stellarbox-apps-menu"
chmod +x "$STAGE_DIR/usr/bin/stellarbox-apps-menu"
chmod +x "$STAGE_DIR/usr/bin/stellarbox-session" \
        "$STAGE_DIR/usr/bin/stellarbox-gnome-session" \
        "$STAGE_DIR/usr/bin/stellarbox-kde-session"

# helper de autostart -> /usr/lib/stellarbox
mkdir -p "$STAGE_DIR$LIBEXECDIR"
sed -e 's|@rcdir@|/etc/xdg/stellarbox|g' \
    -e 's|@libexecdir@|/usr/lib/stellarbox|g' \
    data/autostart/stellarbox-autostart.in \
    > "$STAGE_DIR$LIBEXECDIR/stellarbox-autostart"
cp data/autostart/stellarbox-xdg-autostart \
   "$STAGE_DIR$LIBEXECDIR/stellarbox-xdg-autostart"
chmod +x "$STAGE_DIR$LIBEXECDIR/stellarbox-autostart" \
        "$STAGE_DIR$LIBEXECDIR/stellarbox-xdg-autostart"

# sesiones GNOME -> /usr/share/gnome-session
GNSESS_DIR="$STAGE_DIR${DATADIR}/gnome-session/sessions"
mkdir -p "$GNSESS_DIR"
cp data/gnome-session/*.session "$GNSESS_DIR/"

# soporte GNOME -> /usr/share/gnome/wm-properties
WMPROP_DIR="$STAGE_DIR${DATADIR}/gnome/wm-properties"
mkdir -p "$WMPROP_DIR"
cp "data/stellarbox.desktop" "$WMPROP_DIR/stellarbox.desktop"

echo "==> Preparando el árbol rpmbuild"

# árbol de trabajo de rpmbuild en dist/rpm/rpmbuild
rm -rf "$RPMROOT"
mkdir -p "$RPMROOT"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# "payload" = contenido completo del árbol de instalación
( cd "$STAGE_DIR" && tar -czf "$RPMROOT/SOURCES/stellarbox-payload.tar.gz" . )

cat > "$RPMROOT/SPECS/stellarbox.spec" <<EOF
Name: stellarbox
Version: ${VERSION}
Release: ${RELEASE}
Summary: A minimalistic, highly configurable X11 window manager (CMake fork)
License: GPL-2.0-or-later
Vendor: Stellarbox CMake fork
URL: http://openbox.org/
Source0: stellarbox-payload.tar.gz
Group: User Interface/Desktops

%description
Stellarbox (formerly Openbox) is a lightweight, highly configurable X11
window manager with extensive standards support (EWMH/ICCCM). This package
is built from the CMake-customized fork with vector (Cairo) rendering,
rounded corners, HiDPI support and _NET_WM_OPAQUE_REGION.

%prep
tar -xzf %{SOURCE0}

%build
# sin recompila: instalamos el árbol ya compilado por CMake

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a . %{buildroot}/

%clean
rm -rf %{buildroot}

%files
/

%post
/usr/bin/update-menus 2>/dev/null || true

%postun
/usr/bin/update-menus 2>/dev/null || true

%changelog
* $(date '+%a %b %d %Y') Stellarbox CMake fork
- Initial RPM packaging of the CMake fork.
EOF

echo "==> Generando $PACKAGE_NAME"

mkdir -p "$DIST_DIR"
rm -f "$DIST_DIR/$PACKAGE_NAME"

rpmbuild -bb \
    --define "_topdir $RPMROOT" \
    --define "_binary_payload w9.xzdio" \
    "$RPMROOT/SPECS/stellarbox.spec"

# buscar el rpm generado y copiarlo a dist/
RPMS_FILE="$(find "$RPMROOT/RPMS" -name 'stellarbox-*.rpm' | head -1)"
if [ -z "$RPMS_FILE" ]; then
    echo "ERROR: no se generó ningún rpm" >&2
    exit 1
fi
cp "$RPMS_FILE" "$DIST_DIR/$PACKAGE_NAME"

echo "==> Hecho: $DIST_DIR/$PACKAGE_NAME"