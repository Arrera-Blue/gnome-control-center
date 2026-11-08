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

# Synchronisation et compilation des schémas GSettings pour les extensions Arrera
EXT_NAMES=("dock@linux.arrera-software.fr" "top-bar@linux.arrera-software.fr" "app-menu@linux.arrera-software.fr" "arrera-dock")
SCHEMA_COPIED=0

for ext in "${EXT_NAMES[@]}"; do
    SCHEMA_PATH="$HOME/.local/share/gnome-shell/extensions/$ext/schemas"
    if [ -d "$SCHEMA_PATH" ]; then
        for xml in "$SCHEMA_PATH"/*.gschema.xml; do
            if [ -f "$xml" ]; then
                mkdir -p "$HOME/.local/share/glib-2.0/schemas"
                cp -u "$xml" "$HOME/.local/share/glib-2.0/schemas/"
                SCHEMA_COPIED=1
            fi
        done
    fi
done

if [ "$SCHEMA_COPIED" -eq 1 ]; then
    glib-compile-schemas "$HOME/.local/share/glib-2.0/schemas"
fi

# Correction de sécurité pour le bouton d'activités dans topBar.js (restauration du conteneur parent)
USER_TOPBAR="$HOME/.local/share/gnome-shell/extensions/top-bar@linux.arrera-software.fr"
if [ ! -d "$USER_TOPBAR" ] && [ -d "/usr/share/gnome-shell/extensions/top-bar@linux.arrera-software.fr" ]; then
    mkdir -p "$HOME/.local/share/gnome-shell/extensions" 2>/dev/null || true
    cp -r "/usr/share/gnome-shell/extensions/top-bar@linux.arrera-software.fr" "$USER_TOPBAR" 2>/dev/null || true
fi
if [ -f "$USER_TOPBAR/topBar.js" ]; then
    sed -i 's/if (this._activitiesButton)\s*this._activitiesButton.visible = true;/if (this._activitiesButton) { this._activitiesButton.visible = true; if (this._activitiesButton.container) this._activitiesButton.container.visible = true; }/' "$USER_TOPBAR/topBar.js" 2>/dev/null || true
fi

echo "🚀 Lancement de GNOME Settings..."
if [ -n "$1" ]; then
    echo "→ Paramètre : $1"
    exec "$BUILD_DIR/shell/gnome-control-center" "$@"
else
    exec "$BUILD_DIR/shell/gnome-control-center"
fi
