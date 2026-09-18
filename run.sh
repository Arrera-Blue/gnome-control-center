#!/usr/bin/env bash
set -e

# Se placer à la racine du projet
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="_build"

echo "🔨 [1/3] Vérification de la configuration Meson..."
if [ ! -d "$BUILD_DIR" ]; then
    meson setup "$BUILD_DIR"
fi

echo "⚙️ [2/3] Compilation du projet (Ninja)..."
ninja -C "$BUILD_DIR"

echo "📦 [3/3] Enregistrement du panneau et de l'icône dans l'environnement local..."
mkdir -p "$HOME/.local/share/applications" 2>/dev/null || true
if [ -f "$BUILD_DIR/panels/dock/gnome-dock-panel.desktop" ]; then
    cp -u "$BUILD_DIR/panels/dock/gnome-dock-panel.desktop" "$HOME/.local/share/applications/" 2>/dev/null || true
fi

mkdir -p "$HOME/.local/share/icons/hicolor/scalable/apps" 2>/dev/null || true
if [ -f "panels/dock/icons/scalable/org.gnome.Settings-dock-symbolic.svg" ]; then
    cp -u "panels/dock/icons/scalable/org.gnome.Settings-dock-symbolic.svg" "$HOME/.local/share/icons/hicolor/scalable/apps/" 2>/dev/null || true
fi

# Synchronisation et compilation du schéma GSettings pour Arrera Dock
EXT_SCHEMA=""
if [ -f "$HOME/.local/share/gnome-shell/extensions/dock@linux.arrera-software.fr/schemas/org.gnome.shell.extensions.dock.gschema.xml" ]; then
    EXT_SCHEMA="$HOME/.local/share/gnome-shell/extensions/dock@linux.arrera-software.fr/schemas/org.gnome.shell.extensions.dock.gschema.xml"
elif [ -f "$HOME/.local/share/gnome-shell/extensions/arrera-dock/schemas/org.gnome.shell.extensions.dock.gschema.xml" ]; then
    EXT_SCHEMA="$HOME/.local/share/gnome-shell/extensions/arrera-dock/schemas/org.gnome.shell.extensions.dock.gschema.xml"
fi

if [ -n "$EXT_SCHEMA" ]; then
    mkdir -p "$HOME/.local/share/glib-2.0/schemas"
    cp -u "$EXT_SCHEMA" "$HOME/.local/share/glib-2.0/schemas/"
    glib-compile-schemas "$HOME/.local/share/glib-2.0/schemas"
fi

echo "🚀 Lancement de GNOME Settings..."
if [ -n "$1" ]; then
    echo "→ Paramètre : $1"
    exec "$BUILD_DIR/shell/gnome-control-center" "$@"
else
    exec "$BUILD_DIR/shell/gnome-control-center"
fi
