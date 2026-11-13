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
EXT_NAMES=("arrera-desktop@linux.arrera-software.fr" "arrera-desktop" "dock@linux.arrera-software.fr" "top-bar@linux.arrera-software.fr" "app-menu@linux.arrera-software.fr" "arrera-dock")
SCHEMA_COPIED=0

for ext in "${EXT_NAMES[@]}"; do
    for base in "$HOME/.local/share/gnome-shell/extensions/$ext/schemas" "/usr/share/gnome-shell/extensions/$ext/schemas"; do
        if [ -d "$base" ]; then
            for xml in "$base"/*.gschema.xml; do
                if [ -f "$xml" ]; then
                    mkdir -p "$HOME/.local/share/glib-2.0/schemas"
                    cp -u "$xml" "$HOME/.local/share/glib-2.0/schemas/"
                    SCHEMA_COPIED=1
                fi
            done
        fi
    done
done

if [ "$SCHEMA_COPIED" -eq 1 ]; then
    glib-compile-schemas "$HOME/.local/share/glib-2.0/schemas"
fi

# Correction de sécurité optionnelle pour le bouton d'activités dans topBar.js si présent
for user_ext in "$HOME/.local/share/gnome-shell/extensions/arrera-desktop@"* "$HOME/.local/share/gnome-shell/extensions/arrera-desktop" "$HOME/.local/share/gnome-shell/extensions/top-bar@linux.arrera-software.fr"; do
    if [ -f "$user_ext/topBar.js" ]; then
        sed -i 's/if (this._activitiesButton)\s*this._activitiesButton.visible = true;/if (this._activitiesButton) { this._activitiesButton.visible = true; if (this._activitiesButton.container) this._activitiesButton.container.visible = true; }/' "$user_ext/topBar.js" 2>/dev/null || true
    fi
done

# Vérification et activation de l'extension et du service GPaste pour le presse-papiers
systemctl --user start org.gnome.GPaste.service 2>/dev/null || true
gnome-extensions enable GPaste@gnome-shell-extensions.gnome.org 2>/dev/null || true
gnome-extensions enable appindicatorsupport@rgcjonas.gmail.com 2>/dev/null || true

echo "🚀 Lancement de GNOME Settings..."
if [ -n "$1" ]; then
    echo "→ Paramètre : $1"
    exec "$BUILD_DIR/shell/gnome-control-center" "$@"
else
    exec "$BUILD_DIR/shell/gnome-control-center"
fi
