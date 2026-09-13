#!/bin/sh
#
# debian/install-tree.sh - copia los datos que CMake no instala en el
# árbol del paquete (debian/<paquete>). Invocado desde debian/rules.
#
# Uso: install-tree.sh <dir-del-paquete>
#
set -eu

PKGDIR="${1:?Uso: install-tree.sh <dir-del-paquete>}"

# configuración por defecto -> /etc/xdg/stellarbox
mkdir -p "$PKGDIR/etc/xdg/stellarbox"
cp data/rc.xml data/menu.xml data/environment "$PKGDIR/etc/xdg/stellarbox/"
sed -e 's|@libexecdir@|/usr/lib/stellarbox|g' data/autostart/autostart.in \
    > "$PKGDIR/etc/xdg/stellarbox/autostart"
chmod 0755 "$PKGDIR/etc/xdg/stellarbox/autostart"

# temas -> /usr/share/stellarbox/themes (árbol propio, ajeno a openbox)
mkdir -p "$PKGDIR/usr/share/stellarbox/themes"
for th in themes/*/; do
    [ -d "$th" ] || continue
    cp -a "$th" "$PKGDIR/usr/share/stellarbox/themes/"
done

# icono -> /usr/share/pixmaps
mkdir -p "$PKGDIR/usr/share/pixmaps"
cp data/stellarbox.png "$PKGDIR/usr/share/pixmaps/stellarbox.png"

# entrada para el Display Manager -> /usr/share/xsessions
mkdir -p "$PKGDIR/usr/share/xsessions"
cp debian/stellarbox.desktop "$PKGDIR/usr/share/xsessions/stellarbox.desktop"

# script de arranque de sesión -> /usr/bin
mkdir -p "$PKGDIR/usr/bin"
cp debian/stellarbox-session "$PKGDIR/usr/bin/stellarbox-session"
chmod 0755 "$PKGDIR/usr/bin/stellarbox-session"

# generador del menú de aplicaciones (pipe menu) -> /usr/bin
cp data/stellarbox-apps-menu.py "$PKGDIR/usr/bin/stellarbox-apps-menu"
chmod 0755 "$PKGDIR/usr/bin/stellarbox-apps-menu"

# helpers de autostart -> /usr/lib/stellarbox (libexec)
mkdir -p "$PKGDIR/usr/lib/stellarbox"
sed -e 's|@rcdir@|/etc/xdg/stellarbox|g' \
    -e 's|@libexecdir@|/usr/lib/stellarbox|g' \
    data/autostart/stellarbox-autostart.in \
    > "$PKGDIR/usr/lib/stellarbox/stellarbox-autostart"
chmod 0755 "$PKGDIR/usr/lib/stellarbox/stellarbox-autostart"
cp data/autostart/stellarbox-xdg-autostart \
   "$PKGDIR/usr/lib/stellarbox/stellarbox-xdg-autostart"
chmod 0755 "$PKGDIR/usr/lib/stellarbox/stellarbox-xdg-autostart"

# sesiones GNOME -> /usr/share/gnome-session/sessions
mkdir -p "$PKGDIR/usr/share/gnome-session/sessions"
cp data/gnome-session/*.session "$PKGDIR/usr/share/gnome-session/sessions/"

# soporte GNOME -> /usr/share/gnome/wm-properties
mkdir -p "$PKGDIR/usr/share/gnome/wm-properties"
cp data/stellarbox.desktop "$PKGDIR/usr/share/gnome/wm-properties/stellarbox.desktop"