# DubInstante 🎙️🎬

**FR :** Le studio de doublage visuel open-source haute performance, fièrement sans IA et 100% local.  
**EN :** The open-source, high-performance visual dubbing studio, proudly AI-free and 100% local.

---

### [FR] 🇫🇷

**DubInstante** est un outil de doublage professionnel conçu pour les comédiens, fandubbers, monteurs vidéo et étudiants en cinéma. Grâce à sa navigation image par image, sa bande rythmo dynamique et son moteur audio/vidéo ultra-léger, il remplace les logiciels hors de prix ou vieillissants par une alternative moderne, rapide et gratuite.

### [EN] 🇬🇧

**DubInstante** is a professional-grade dubbing workbench built for voice actors, fandubbers, video editors, and cinema students. Combining frame-by-frame navigation, a dynamic Rythmo Band, and an ultra-lightweight audio/video engine, it replaces overpriced or legacy proprietary software with a modern, fast, and free alternative.

---

## 📺 Démonstration Visuelle / Visual Demo

<!-- 
Remplacez cette section par un GIF de 5 secondes montrant la bande rythmo en action.
Seeing the rythmo band scrolling in sync with the video beats reading any text description.
-->

```
                     [ Aperçu de la Bande Rythmo en Action ]
+--------------------------------------------------------------------------+
|                                                                          |
|                              [ VIDÉO HD ]                                |
|                                                                          |
+--------------------------------------------------------------------------+
|  <- Défilant <-   Je ne t'attendais pas si tôt...    |  BARRE DE SYNCHRO |
+--------------------------------------------------------------------------+
```

*(Placez votre GIF de démonstration `demo.gif` ici)*

---

## ✊ Nos Valeurs Fondatrices / Our Core Values

### 🎙️ Fièrement Sans IA / Proudly AI-Free (#TouchePasMaVF)
* **FR :** Nous défendons le métier d'acteur de doublage. DubInstante refuse d'intégrer des technologies de génération ou de remplacement de voix par IA. C'est un outil conçu exclusivement pour magnifier le talent des artisans de la voix humaine.
* **EN :** We stand firmly with voice actors. DubInstante will never integrate generative AI voice replacement. This is a dedicated workbench built to empower and respect human voice artisans.

### 🔒 100% Local & Privé / 100% Local & Privacy-First
* **FR :** Aucun cloud forcé, aucune télémétrie masquée, aucune création de compte obligatoire. Vos gros fichiers vidéo HD et vos enregistrements audio PCM restent à 100% sur votre disque dur. Vous êtes propriétaire exclusif de vos créations.
* **EN :** Zero forced cloud services, zero tracking, zero mandatory accounts. Your massive HD video files and lossless PCM recordings stay entirely on your local machine. Your work remains yours.

### ⚡ Ingénierie Sans Concession / Extreme Engineering
* **FR :** Fini les applications Electron lourdes et gourmandes. DubInstante est écrit en C++ natif avec Qt 6 pour offrir des performances maximales et une réactivité instantanée, même sur des configurations modestes.
* **EN :** No heavy Electron wrappers or bloated frameworks. DubInstante is written in native C++ using Qt 6, delivering peak performance and immediate response times.

---

## ⌨️ Raccourcis Clavier / Studio Shortcuts

Conçu pour s'intégrer instantanément dans vos réflexes de studio :

| Action (FR) | Action (EN) | Raccourci / Shortcut |
|:---|:---|:---|
| **Lecture / Pause** | Play / Pause | `Espace` / `Space` |
| **Démarrer l'enregistrement** | Start recording | `Ctrl+R` |
| **Arrêt de l'enregistrement** | Stop recording | `Échap` / `Esc` |
| **Insérer espace & lecture** (hors enregistrement) | Insert space & play (when not recording) | `Échap` / `Esc` |
| **Sauvegarder le projet** | Save project | `Ctrl+S` |
| **Navigation image par image** | Frame-by-frame navigation | `←` / `→` |
| **Retour arrière rapide (-5s)** | Quick seek back (-5s) | `Shift + ←` |
| **Avance rapide (+5s)** | Quick seek forward (+5s) | `Shift + →` |

---

## 🛠️ Piliers de Fonctionnalités / Feature Pillars

### 🚀 Performance & Robustesse
* **FR : Moteur N-Pistes & Fichiers 50 Go+** : Conçu pour lire directement les fichiers vidéo HD non compressés de plus de 50 Go sans latence ni saccade.
* **EN : N-Track Engine & 50GB+ Files** : Designed to smoothly play and record alongside massive uncompressed HD video files directly without lagging.

### 🎨 Bande Rythmo Dynamique
* **FR : Jusqu'à 4 Pistes Synchros** : Affichez et éditez jusqu'à 4 pistes de texte simultanées, défilant avec la vidéo à la vitesse que vous réglez.
* **EN : Up to 4 Synced Tracks** : Display and edit up to 4 simultaneous text tracks, scrolling with the video at the speed you set.

### 🎚️ Moteur Audio & FFmpeg
* **FR : Export Sans Perte** : Ajustez les gains de chaque micro en temps réel et exportez le mix final proprement avec FFmpeg. Le moteur intercepte proprement les erreurs système (comme le manque d'espace disque) sans bloquer l'application.
* **EN : Lossless Audio & FFmpeg** : Dynamically adjust gain per track and export clean, high-quality audio merges via FFmpeg. System errors (like running out of disk space) are intercepted gracefully to keep the application responsive.
* **FR : FFmpeg embarqué** : les paquets publiés (AppImage, Windows, macOS) contiennent `ffmpeg` et `ffprobe` — rien à installer. Une compilation depuis les sources demande FFmpeg ≥ 4.4 (mixage `amix` avec `normalize=0`).
* **EN : FFmpeg included** : the published packages (AppImage, Windows, macOS) ship `ffmpeg` and `ffprobe` — nothing to install. Building from source requires FFmpeg ≥ 4.4 (`amix` mixing with `normalize=0`).

### 📦 Multiplateforme Natif
* **FR :** Windows (Installer), macOS (DMG), Linux (AppImage) et Android Native (Beta).
* **EN :** Windows (Installer), macOS (DMG), Linux (AppImage), and Android Native (Beta).

---

## 🚀 Historique & Feuille de Route / Development Roadmap

* ✅ **v0.4.0 - v0.9.0 :** Gestion de projet `.dbi`, mode plein écran de secours, beta Android et personnalisation des styles.
* ✅ **v0.10.0 :** Support multi-pistes rythmo (jusqu'à 4 pistes indépendantes).
* ✅ **v0.11.0 :** Refonte de l'interface utilisateur (aesthetics wow factor).
* 🎯 **v1.0.0 (En cours) :** Finalisation des profils audio avancés, stabilité générale pour la production.

---

## 🤝 Participer au Projet / Feedback & Contributions

* **FR :** DubInstante grandit grâce à vos retours ! Si vous êtes comédien de doublage, ingénieur du son ou passionné, venez proposer vos idées de fonctionnalités ou signaler des bugs sur nos [GitHub Issues](https://github.com/loimathos/DubInstante/issues).
* **EN :** DubInstante is driven by the community. Whether you are a professional voice actor, a sound engineer, or a hobbyist, please share your ideas or report issues on our [GitHub Issues](https://github.com/loimathos/DubInstante/issues).