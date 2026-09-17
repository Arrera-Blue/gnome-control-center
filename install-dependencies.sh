#!/usr/bin/env bash
set -e

# ==============================================================================
# Script d'installation des dépendances pour GNOME Control Center (Fedora Linux)
# ==============================================================================

echo "=========================================================="
echo "📦 Installation des dépendances (Fedora Linux)"
echo "=========================================================="

# Vérifier si l'utilisateur a les droits sudo / root
if [ "$EUID" -ne 0 ]; then
    SUDO="sudo"
else
    SUDO=""
fi

# Détecter le gestionnaire de paquets (dnf5 ou dnf)
if command -v dnf5 >/dev/null 2>&1; then
    DNF="dnf5"
elif command -v dnf >/dev/null 2>&1; then
    DNF="dnf"
else
    echo "❌ Erreur : 'dnf' ou 'dnf5' introuvable. Ce script est destiné à Fedora Linux."
    exit 1
fi

echo "🔍 [1/3] Gestionnaire de paquets détecté : $DNF"

# 1. Installer les outils de build et plugins
echo "⚙️ [2/3] Installation des outils de compilation et des dépendances de base..."
$SUDO $DNF install -y \
    meson \
    ninja-build \
    gcc \
    gcc-c++ \
    git \
    pkgconf-pkg-config \
    blueprint-compiler \
    desktop-file-utils \
    gettext \
    docbook-style-xsl \
    libxslt \
    tecla

# Tenter d'utiliser builddep si disponible pour récupérer automatiquement les dépendances GNOME standard
echo "📚 [3/3] Installation des bibliothèques de développement GNOME..."
if $DNF --help | grep -q "builddep"; then
    echo "→ Utilisation de '$DNF builddep' pour gnome-control-center..."
    $SUDO $DNF builddep -y gnome-control-center || true
fi

# Installer explicitement toutes les dépendances de développement (y compris pour le panneau Dock)
$SUDO $DNF install -y \
    accountsservice-devel \
    colord-devel \
    colord-gtk-devel \
    cups-devel \
    gcr-devel \
    gdk-pixbuf2-devel \
    glib2-devel \
    gnome-desktop4-devel \
    gnome-settings-daemon-devel \
    gnome-online-accounts-devel \
    gsettings-desktop-schemas-devel \
    gsound-devel \
    gtk4-devel \
    libadwaita-devel \
    libgudev-devel \
    ibus-devel \
    krb5-devel \
    libgtop2-devel \
    NetworkManager-libnm-devel \
    libnma-gtk4-devel \
    pulseaudio-libs-devel \
    libsecret-devel \
    libsoup3-devel \
    libxml2-devel \
    ModemManager-glib-devel \
    polkit-devel \
    libpwquality-devel \
    libsmbclient-devel \
    tecla-devel \
    udisks2-devel \
    upower-devel \
    libX11-devel \
    libXi-devel \
    gnome-bluetooth-devel \
    libwacom-devel \
    malcontent-devel

echo ""
echo "=========================================================="
echo "✅ Toutes les dépendances sont installées avec succès !"
echo "👉 Vous pouvez maintenant exécuter './run.sh' pour compiler et lancer GNOME Settings."
echo "=========================================================="
