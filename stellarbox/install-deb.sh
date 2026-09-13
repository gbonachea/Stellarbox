#!/usr/bin/env bash
#
# install-deb.sh - Instala los paquetes .deb generados (dist/stellarbox_*.deb)
#                  y resuelve/instala sus dependencias.
#
# Requisitos: apt-get (Debian/Ubuntu) o dpkg; derechos de sudo si no es root.
#
set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# todos los .deb generados por build-deb.sh (stellarbox y stellarbox-obconf)
mapfile -t DEBS < <(ls dist/stellarbox_*.deb dist/stellarbox-obconf_*.deb 2>/dev/null)

if [ "${#DEBS[@]}" -eq 0 ]; then
    echo "ERROR: no hay paquetes .deb en dist/. Ejecuta primero ./build-deb.sh" >&2
    exit 1
fi

# limpiar restos de paquetes anteriores (1.0-1): los temas ahora viven en
# /usr/share/stellarbox/themes para no colisionar con los de openbox
STALE_THEME_DIRS=(Artwiz-boxed Bear2 Clearlooks Clearlooks-3.4 Clearlooks-Olive
                  Mikachu Natura Onyx Onyx-Citrus Orang Syscrash)
if [ "$(id -u)" -eq 0 ]; then
    SUDO_ROOT=""
else
    SUDO_ROOT="sudo"
fi
echo "==> Limpiando temas antiguos de /usr/share/themes"
for th in "${STALE_THEME_DIRS[@]}"; do
    $SUDO_ROOT rm -rf "/usr/share/themes/$th"
done
$SUDO_ROOT rm -f "/usr/share/themes/Makefile"

# usar sudo si no somos root
if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    if ! command -v sudo >/dev/null 2>&1; then
        echo "ERROR: se necesita ser root o tener sudo instalado" >&2
        exit 1
    fi
    SUDO="sudo"
fi

echo "==> Paquetes a instalar:"
printf '    %s\n' "${DEBS[@]}"

if command -v apt-get >/dev/null 2>&1; then
    # apt resuelve e instala las dependencias automáticamente
    echo "==> Instalando con apt-get (dependencias incluidas)"
    $SUDO apt-get install -y "${DEBS[@]}"
    RC=$?
elif command -v dpkg >/dev/null 2>&1; then
    # sin apt-get: instalar los paquetes y reparar dependencias con apt si existe
    echo "==> Instalando con dpkg"
    $SUDO dpkg -i "${DEBS[@]}"
    RC=$?
    if command -v apt-get >/dev/null 2>&1; then
        echo "==> Reparando dependencias con 'apt-get install -f'"
        $SUDO apt-get install -y -f
        RC=$?
    fi
else
    echo "ERROR: no se encontró dpkg ni apt-get (¿sistema Debian?)" >&2
    exit 1
fi

# stack de escritorio del paquete (los Depends de debian/control los
# instala apt de todos modos; aquí como respaldo para el camino dpkg -i)
echo "==> Instalando dependencias adicionales"
if command -v apt-get >/dev/null 2>&1; then
    $SUDO apt-get install -y \
        picom polybar rofi feh dunst x11-xserver-utils dbus-x11 mate-polkit \
        || true
fi

if [ $RC -eq 0 ]; then
    echo "==> Stellarbox instalado."
    echo "    Sálectalo sesión en el gestor de pantallas o ejecuta 'stellarbox-session'."
fi
exit $RC