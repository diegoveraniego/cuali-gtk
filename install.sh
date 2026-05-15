#!/bin/bash
set -e

# Install icon
mkdir -p ~/.local/share/icons/hicolor/scalable/apps
cp icons/hicolor/scalable/apps/org.cuali.CualiGTK.svg ~/.local/share/icons/hicolor/scalable/apps/
gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor 2>/dev/null || true

# Install desktop file (uses full path to binary)
DESKTOP_DIR=~/.local/share/applications
mkdir -p "$DESKTOP_DIR"
sed "s|Exec=cuali-gtk|Exec=$PWD/cuali-gtk|g" org.cuali.CualiGTK.desktop > "$DESKTOP_DIR/org.cuali.CualiGTK.desktop"

echo "Instalado. La app debería aparecer en el menú tras cerrar sesión o reiniciar."
echo "Para usarla ya, ejecuta: ./cuali-gtk"
