#!/usr/bin/env bash
#
# install-rpm.sh - Instala los paquetes .rpm generados (dist/stellarbox-*.rpm)
#                  y resuelve/instala sus dependencias.
#
# Requisitos: dnf (Fedora) / yum (RHEL, CentOS) / zypper (openSUSE),
#             o al menos rpm; derechos de sudo si no es root.
#
set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# todos los .rpm generados por build-rpm.sh
mapfile -t RPMS < <(ls dist/stellarbox-*.rpm 2>/dev/null)

if [ "${#RPMS[@]}" -eq 0 ]; then
    echo "ERROR: no hay paquetes .rpm en dist/. Ejecuta primero ./build-rpm.sh" >&2
    exit 1
fi

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
printf '    %s\n' "${RPMS[@]}"

if command -v dnf >/dev/null 2>&1; then
    # dnf resuelve e instala las dependencias automáticamente
    echo "==> Instalando con dnf (dependencias incluidas)"
    $SUDO dnf install -y "${RPMS[@]}"
    RC=$?
elif command -v yum >/dev/null 2>&1; then
    echo "==> Instalando con yum (dependencias incluidas)"
    $SUDO yum install -y "${RPMS[@]}"
    RC=$?
elif command -v zypper >/dev/null 2>&1; then
    echo "==> Instalando con zypper (dependencias incluidas)"
    $SUDO zypper install -y -l "${RPMS[@]}"
    RC=$?
elif command -v rpm >/dev/null 2>&1; then
    # último recurso: rpm NO resuelve dependencias automáticamente
    echo "==> Instalando con rpm (sin resolución automática de dependencias)"
    $SUDO rpm -Uvh "${RPMS[@]}"
    RC=$?
    if [ $RC -ne 0 ]; then
        echo "AVISO: rpm no resuelve dependencias." >&2
        echo "       Instala las dependencias indicadas arriba o usa dnf/yum/zypper." >&2
    fi
else
    echo "ERROR: no se encontró dnf, yum, zypper ni rpm (¿sistema RPM?)" >&2
    exit 1
fi

# dependencias extra (configurador, herramientas y libs) - con nombres RPM
echo "==> Instalando dependencias adicionales"
if command -v dnf >/dev/null 2>&1; then
    $SUDO dnf install -y obconf-qt scrot imlib2 libid3tag libXext libXrandr \
        || true
elif command -v yum >/dev/null 2>&1; then
    $SUDO yum install -y obconf-qt scrot imlib2 libid3tag libXext libXrandr \
        || true
elif command -v zypper >/dev/null 2>&1; then
    $SUDO zypper install -y obconf-qt scrot imlib2 libid3tag libXext libXrandr \
        || true
fi

if [ $RC -eq 0 ]; then
    echo "==> Stellarbox instalado."
    echo "    Sálectalo sesión en el gestor de pantallas o ejecuta 'stellarbox'."
fi
exit $RC