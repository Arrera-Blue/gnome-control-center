# Guide Git : Synchronisation avec le dépôt officiel GNOME

Ce document répertorie l'ensemble des commandes Git pour récupérer, mettre à jour et synchroniser les sources depuis le dépôt officiel de GNOME (**upstream**).

---

## 📌 Informations sur les dépôts

- **Dépôt officiel GNOME (Upstream) :** `https://gitlab.gnome.org/GNOME/gnome-control-center.git`
- **Dépôt Arrera-Blue (Origin) :** `git@github.com:Arrera-Blue/gnome-control-center.git`

---

## 1. Cloner le projet

### Option A : Cloner depuis le dépôt Arrera-Blue et ajouter GNOME en upstream (Recommandé)
```bash
# 1. Cloner le dépôt Arrera
git clone --recurse-submodules git@github.com:Arrera-Blue/gnome-control-center.git
cd gnome-control-center

# 2. Ajouter le dépôt officiel GNOME comme source distante "upstream"
git remote add upstream https://gitlab.gnome.org/GNOME/gnome-control-center.git

# 3. Vérifier les dépôts distants configurés
git remote -v
```

### Option B : Cloner directement le dépôt officiel de GNOME
```bash
git clone --recurse-submodules https://gitlab.gnome.org/GNOME/gnome-control-center.git
cd gnome-control-center
```

---

## 2. Configurer le dépôt distant officiel (Upstream)

Si le dépôt local existe déjà mais que le dépôt officiel GNOME n'est pas encore configuré :

```bash
# Ajouter le remote upstream
git remote add upstream https://gitlab.gnome.org/GNOME/gnome-control-center.git

# Vérifier la configuration
git remote -v
```

---

## 3. Récupérer les sources officielles (Fetch)

Pour télécharger l'historique complet, les nouvelles branches et les tags du dépôt GNOME sans modifier vos fichiers locaux :

```bash
# Récupérer toutes les branches et tags d'upstream
git fetch upstream

# Récupérer explicitement tous les tags / versions
git fetch upstream --tags
```

---

## 4. Consulter les branches et tags officiels

```bash
# Lister toutes les branches officielles disponibles
git branch -r | grep upstream

# Lister les branches de versions stables (ex: gnome-50, gnome-49...)
git branch -r | grep "upstream/gnome-"

# Lister les tags de versions officielles (ex: 50.4, 50.3...)
git tag -l "50.*"
```

---

## 5. Créer une branche basée sur une version officielle

### Basée sur une branche officielle (ex: `gnome-50` ou `main`) :
```bash
# Créer et basculer sur une nouvelle branche locale basée sur la branche upstream
git checkout -b ma-nouvelle-branche upstream/gnome-50
```

### Basée sur un tag / version spécifique (ex: `50.4`) :
```bash
# Créer et basculer sur une nouvelle branche locale basée sur le tag 50.4
git checkout -b version-50.4-arrera 50.4
```

---

## 6. Mettre à jour sa branche avec les nouveautés de GNOME

Pour intégrer les dernières modifications officielles de GNOME dans votre branche de travail :

### Méthode 1 : Rebase (Recommandé pour un historique propre)
```bash
# 1. Récupérer les nouveautés
git fetch upstream

# 2. Rejouer vos commits par-dessus la branche GNOME (ex: gnome-50)
git rebase upstream/gnome-50

# 3. Mettre à jour les sous-modules si nécessaire
git submodule update --init --recursive
```

### Méthode 2 : Merge
```bash
# 1. Récupérer les nouveautés
git fetch upstream

# 2. Fusionner la branche GNOME dans votre branche actuelle
git merge upstream/gnome-50

# 3. Mettre à jour les sous-modules
git submodule update --init --recursive
```

---

## 7. Gestion des sous-modules (Subprojects)

Le projet utilise des sous-modules (comme `subprojects/gvc`). Pour s'assurer qu'ils sont bien initialisés et à jour :

```bash
# Initialiser et synchroniser les sous-modules
git submodule init
git submodule update --recursive
```

---

## 8. Aide-mémoire rapide (Cheat Sheet)

| Action | Commande |
|---|---|
| Ajouter le remote officiel GNOME | `git remote add upstream https://gitlab.gnome.org/GNOME/gnome-control-center.git` |
| Récupérer les mises à jour officielles | `git fetch upstream` |
| Voir les branches officielles | `git branch -r \| grep upstream` |
| Créer une branche depuis GNOME 50 | `git checkout -b <nom-branche> upstream/gnome-50` |
| Mettre à jour la branche courante | `git fetch upstream && git rebase upstream/gnome-50` |
| Mettre à jour les sous-modules | `git submodule update --init --recursive` |
