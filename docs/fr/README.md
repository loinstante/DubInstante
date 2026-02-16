# DubInstante — Documentation Technique

DubInstante est un studio de doublage vidéo professionnel construit avec **Qt 6 / C++17**. Il permet aux comédiens de doublage de lire une vidéo, écrire du texte sur une bande rythmo défilante, enregistrer leur voix en synchronisation, et exporter le résultat final. Ce document est le guide d'intégration — tout ce dont un nouveau contributeur a besoin pour comprendre le code et commencer à contribuer.

---

## Table des Matières

1. [Vue d'Ensemble](#vue-densemble)
2. [Structure du Projet](#structure-du-projet)
3. [Architecture](#architecture)
4. [Couche Core — Logique Métier](#couche-core--logique-métier)
5. [Couche GUI — Interface Utilisateur](#couche-gui--interface-utilisateur)
6. [Couche Utils](#couche-utils)
7. [Flux de Données & Signaux/Slots](#flux-de-données--signauxslots)
8. [Système de Sauvegarde (format `.dbi`)](#système-de-sauvegarde-format-dbi)
9. [Pipeline d'Export](#pipeline-dexport)
10. [Compilation & Exécution](#compilation--exécution)
11. [Dépendances Externes](#dépendances-externes)
12. [Raccourcis Clavier](#raccourcis-clavier)
13. [Conventions de Code](#conventions-de-code)
14. [Roadmap](#roadmap)

---

## Vue d'Ensemble

Le workflow de l'application est :

1. **Ouvrir** un fichier vidéo (MP4, MKV, etc.)
2. **Écrire** le texte de doublage sur la bande rythmo — le texte défile en synchronisation avec la vidéo
3. **Enregistrer** la voix sur la vidéo (jusqu'à 2 pistes indépendantes)
4. **Exporter** la vidéo finale avec l'audio doublé via FFmpeg

L'application est une fenêtre unique Qt desktop, sans frameworks externes en dehors de Qt 6.

---

## Structure du Projet

```
DubInstante/
├── main.cpp                      # Point d'entrée
├── CMakeLists.txt                # Système de build (CMake + Qt6)
├── resources.qrc                 # Fichier de ressources Qt (icônes, stylesheet)
├── src/
│   ├── core/                     # Logique métier — AUCUNE dépendance UI
│   │   ├── PlaybackEngine.h/cpp  # Lecture vidéo/audio (wrapper QMediaPlayer)
│   │   ├── RythmoManager.h/cpp   # Calculs de sync rythmo & gestion du texte
│   │   ├── AudioRecorder.h/cpp   # Enregistrement micro (wrapper QMediaRecorder)
│   │   ├── ExportService.h/cpp   # Gestion du processus FFmpeg
│   │   └── SaveManager.h/cpp     # Sérialisation .dbi & archivage ZIP
│   ├── gui/                      # Widgets UI passifs — AUCUNE logique métier
│   │   ├── MainWindow.h/cpp      # Fenêtre principale — câblage uniquement
│   │   ├── VideoWidget.h/cpp     # Rendu vidéo OpenGL
│   │   ├── RythmoWidget.h/cpp    # Rendu d'une seule bande rythmo
│   │   ├── RythmoOverlay.h/cpp   # Conteneur pour 1-2 RythmoWidgets
│   │   ├── TrackPanel.h/cpp      # Contrôles de piste audio (device, gain)
│   │   └── ClickableSlider.h     # Slider custom avec clic-pour-positionner
│   └── utils/                    # Utilitaires partagés
│       └── TimeFormatter.h/cpp   # ms → "MM:SS" / "HH:MM:SS.mmm"
├── deploy/
│   └── build_appimage.sh         # Script de packaging AppImage
├── docs/
│   ├── en/README.md              # Version anglaise
│   └── fr/README.md              # Ce fichier
└── .github/workflows/
    └── main.yml                  # CI : build Windows + AppImage
```

---

## Architecture

### La Règle d'Or

> **Les classes Core n'incluent JAMAIS de headers GUI. Les classes GUI ne contiennent JAMAIS de logique métier.**

Le code est divisé en trois couches strictes :

| Couche | Répertoire | Responsabilité | Dépend de |
|--------|-----------|----------------|-----------|
| **Core** | `src/core/` | Tous les calculs, I/O, encodage | Modules Qt Core uniquement |
| **GUI** | `src/gui/` | Rendu passif & saisie utilisateur | Core (signaux/slots uniquement) |
| **Utils** | `src/utils/` | Fonctions utilitaires partagées | Qt Core |

`MainWindow` est le **hub de câblage** : il crée les objets Core et GUI, puis les connecte via signaux/slots. Il ne contient aucune logique métier.

### Diagramme de Dépendances

```
┌──────────────────────────────────────────────────────┐
│                     MainWindow                        │
│                  (crée & câble)                        │
├──────────────────┬───────────────────────────────────┤
│   CORE (possède) │          GUI (possède)             │
│                  │                                    │
│  PlaybackEngine ─┼──→ VideoWidget                     │
│        │         │                                    │
│        ▼         │                                    │
│  RythmoManager ──┼──→ RythmoOverlay                   │
│                  │      ├── RythmoWidget (piste 1)     │
│                  │      └── RythmoWidget (piste 2)     │
│                  │                                    │
│  AudioRecorder ──┼──→ TrackPanel                      │
│  AudioRecorder ──┼──→ TrackPanel                      │
│                  │                                    │
│  ExportService   │    ClickableSlider (barre lecture) │
│  SaveManager     │                                    │
└──────────────────┴───────────────────────────────────┘
```

---

## Couche Core — Logique Métier

### `PlaybackEngine`
**Fichier** : `src/core/PlaybackEngine.h/cpp`

Encapsule `QMediaPlayer` + `QAudioOutput`. Fournit une API propre pour le contrôle de lecture.

| Méthode | Description |
|---------|-------------|
| `openFile(QUrl)` | Charge un fichier vidéo |
| `play()` / `pause()` / `stop()` | Contrôle de lecture |
| `seek(qint64 ms)` | Positionne à un timestamp |
| `setVolume(float)` | 0.0 – 1.0 |
| `setVideoSink(QVideoSink*)` | Connexion au VideoWidget |

**Signaux clés** : `positionChanged(qint64)`, `durationChanged(qint64)`, `playbackStateChanged(...)`, `errorOccurred(QString)`

C'est le **battement de cœur** de l'appli — tous les autres composants se synchronisent sur son signal `positionChanged`.

---

### `RythmoManager`
**Fichier** : `src/core/RythmoManager.h/cpp`

Le cerveau de la bande rythmo. Gère :
- **Stockage multi-pistes** du texte (`QVector<QString>`)
- **Calcul temps → index curseur** (combien de caractères ont défilé au temps T ?)
- **Insertion/suppression** de caractères à la position du curseur
- **Requêtes de seek** depuis l'interaction utilisateur sur la bande rythmo

| Méthode | Description |
|---------|-------------|
| `sync(qint64 positionMs)` | Point de sync principal — appelé à chaque `positionChanged` |
| `setText(int track, QString)` | Définit le texte entier d'une piste |
| `insertCharacter(int track, QString)` | Insère à la position du curseur |
| `deleteCharacter(int track, bool before)` | Backspace / Suppr |
| `cursorIndex(qint64 positionMs)` | Calcule la position du curseur |
| `charDurationMs()` | Durée d'un caractère en ms |

**Signaux clés** : `trackDataChanged(RythmoTrackData)`, `textChanged(int, QString)`, `seekRequested(qint64)`

Le struct `RythmoTrackData` est émis vers le GUI et contient tout ce dont un `RythmoWidget` a besoin pour le rendu : `trackIndex`, `text`, `cursorIndex`, `positionMs`, `speed`.

---

### `AudioRecorder`
**Fichier** : `src/core/AudioRecorder.h/cpp`

Encapsule `QMediaCaptureSession` + `QMediaRecorder` + `QAudioInput`. Une instance par piste d'enregistrement.

| Méthode | Description |
|---------|-------------|
| `availableDevices()` | Liste les microphones |
| `setDevice(QAudioDevice)` | Sélectionne un micro |
| `setVolume(float)` | Gain d'entrée 0.0 – 1.0 |
| `startRecording(QUrl)` | Enregistre vers un fichier WAV |
| `stopRecording()` | Arrête l'enregistrement |

**Signaux clés** : `errorOccurred(QString)`, `durationChanged(qint64)`, `recorderStateChanged(...)`

---

### `ExportService`
**Fichier** : `src/core/ExportService.h/cpp`

Gère le sous-processus FFmpeg pour fusionner vidéo + pistes audio.

| Méthode | Description |
|---------|-------------|
| `startExport(ExportConfig)` | Lance le processus FFmpeg |
| `cancelExport()` | Tue le processus |
| `isFFmpegAvailable()` | Vérifie si `ffmpeg` est dans le PATH |

Le struct `ExportConfig` regroupe : `videoPath`, `audioPath`, `secondAudioPath`, `outputPath`, `durationMs`, `startTimeMs`, `originalVolume`.

**Signaux clés** : `progressChanged(int)` (0–100), `exportFinished(bool, QString)`

---

### `SaveManager`
**Fichier** : `src/core/SaveManager.h/cpp`

Gère la sérialisation des projets. Voir [Système de Sauvegarde](#système-de-sauvegarde-format-dbi) pour la spécification détaillée du format.

| Méthode | Description |
|---------|-------------|
| `save(QString path, SaveData)` | Sauvegarde un fichier `.dbi` |
| `load(QString path, SaveData&)` | Charge un fichier `.dbi` |
| `saveWithMedia(QString zipPath, SaveData, QString*)` | Crée un ZIP avec `.dbi` + vidéo |
| `isZipAvailable(QString*)` | Vérifie la présence de `zip` (Unix) |
| `sanitize(SaveData)` | Clamp les valeurs, normalise les données |

---

## Couche GUI — Interface Utilisateur

Toutes les classes GUI sont **passives** — elles reçoivent des données via des slots et émettent des signaux pour les interactions utilisateur. Elles ne font jamais de calculs.

### `MainWindow`
**Fichier** : `src/gui/MainWindow.h/cpp`

Le hub de câblage. Crée tous les objets Core et GUI, les connecte avec signaux/slots, et gère le menu principal et les raccourcis clavier. Cette classe doit rester **fine** — si vous ajoutez de la logique métier, elle appartient au Core.

### `VideoWidget`
**Fichier** : `src/gui/VideoWidget.h/cpp`

Hérite de `QOpenGLWidget`. Reçoit les frames vidéo via `QVideoSink` et les affiche avec accélération GPU, en maintenant le ratio d'aspect. Usage : passer `videoWidget->videoSink()` à `PlaybackEngine::setVideoSink()`.

### `RythmoWidget`
**Fichier** : `src/gui/RythmoWidget.h/cpp`

Affiche une seule bande rythmo défilante. Supporte les styles visuels :
- `Standalone` — bordures complètes
- `UnifiedTop` / `UnifiedBottom` — pour l'affichage dual-track

Fonctionnalités :
- **Boucle d'animation 60 FPS** pour un défilement fluide (indépendant du framerate vidéo)
- **Debouncing des seeks** pour éviter la saturation disque sur les gros fichiers
- **Interaction souris** — cliquer/glisser pour scrubber, double-clic pour sauter
- **Saisie clavier** — capture la frappe pour l'édition de texte

**Slots** (reçoit les données du `RythmoManager`) :
- `updateDisplay(cursorIndex, positionMs, text, speed)`
- `updatePosition(cursorIndex, positionMs)`
- `setPlaying(bool)`

**Signaux** (interactions utilisateur → transmises au `RythmoManager`) :
- `scrubRequested(int deltaPixels)`
- `characterTyped(QString)`
- `backspacePressed()` / `deletePressed()`
- `navigationRequested(bool forward)`

### `RythmoOverlay`
**Fichier** : `src/gui/RythmoOverlay.h/cpp`

Widget conteneur gérant 1–2 instances de `RythmoWidget`. Gère la mise en page, la visibilité de la Piste 2, et transmet les méthodes proxy (`sync`, `setSpeed`, `setTextColor`) aux deux pistes.

### `TrackPanel`
**Fichier** : `src/gui/TrackPanel.h/cpp`

Panneau UI pour une piste audio. Contient : sélecteur de périphérique, slider de volume + spinbox. Délègue toutes les opérations audio à son instance `AudioRecorder` associée.

### `ClickableSlider`
**Fichier** : `src/gui/ClickableSlider.h`

Sous-classe header-only de `QSlider` qui supporte le clic-pour-positionner (cliquer sur la piste du slider saute directement à cette valeur au lieu d'avancer par pas).

---

## Couche Utils

### `TimeFormatter`
**Fichier** : `src/utils/TimeFormatter.h/cpp`

Namespace avec deux fonctions :
- `format(qint64 ms)` → `"MM:SS"` ou `"HH:MM:SS"`
- `formatWithMillis(qint64 ms)` → `"MM:SS.mmm"`

---

## Flux de Données & Signaux/Slots

Voici comment les données circulent dans l'application en usage normal :

### Synchronisation Lecture
```
PlaybackEngine::positionChanged(ms)
    ├──→ RythmoManager::sync(ms)
    │        └──→ RythmoManager::trackDataChanged(RythmoTrackData)
    │                 └──→ RythmoWidget::updateDisplay(...)
    ├──→ MainWindow : met à jour le slider de position
    └──→ MainWindow : met à jour le label de temps
```

### Édition de Texte
```
RythmoWidget::characterTyped("A")
    └──→ RythmoManager::insertCharacter(trackIndex, "A")
             └──→ RythmoManager::textChanged(trackIndex, newText)
                      └──→ RythmoWidget : reçoit le texte mis à jour via trackDataChanged
```

### Enregistrement
```
MainWindow::toggleRecording()
    ├──→ AudioRecorder1::startRecording(tempPath1)
    ├──→ AudioRecorder2::startRecording(tempPath2)
    ├──→ [si fullscreen coché] enterFullscreenRecording()
    └──→ PlaybackEngine::play()

MainWindow::toggleRecording() (second appui ou Ctrl+S)
    ├──→ AudioRecorder1::stopRecording()
    ├──→ AudioRecorder2::stopRecording()
    ├──→ [si fullscreen] exitFullscreenRecording()
    ├──→ PlaybackEngine::pause()
    └──→ ExportService::startExport(config)  [déclenché par l'utilisateur]
```

### Sauvegarde/Chargement
```
MainWindow::onSaveProject()
    ├──→ SaveManager::isZipAvailable()  [pré-vérification, thread principal]
    ├──→ SaveManager::save() ou saveWithMedia()  [ZIP en thread background]
    └──→ QProgressDialog  [affiché pendant le ZIP async]

MainWindow::onLoadProject()
    └──→ SaveManager::load(path, data)
              └──→ MainWindow : restaure tout l'état UI depuis SaveData
```

---

## Système de Sauvegarde (format `.dbi`)

### Structure Binaire

```
┌─────────────────────┬──────────┬───────┬──────────────────┬────────────────┬──────────┐
│ Header (15 octets)  │ Version  │ Flags │ Taille Payload   │ Payload JSON   │ Checksum │
│ "DubInstanteFile"   │ (1 oct.) │ (1 o) │ (4 o, LE)        │ XOR-masqué     │ SHA-256  │
│                     │          │       │                  │ (N octets)     │ (32 o)   │
└─────────────────────┴──────────┴───────┴──────────────────┴────────────────┴──────────┘
```

- **Taille du payload** stockée en **little-endian** pour la portabilité multi-plateformes
- **Masque XOR** (clé `0x5A`) appliqué au payload JSON pour une obfuscation basique
- **Checksum SHA-256** calculé sur le JSON **non masqué**, puis ajouté en fin de fichier
- Au chargement, le checksum est recalculé et comparé pour détecter la corruption

### Struct `SaveData`

```cpp
struct SaveData {
    QString videoUrl;       // Relatif au fichier .dbi
    float videoVolume;
    QString audioInput1;    // Nom du périphérique
    float audioGain1;
    QString audioInput2;
    float audioGain2;
    bool enableTrack2;
    int scrollSpeed;
    bool isTextWhite;
    QStringList tracks;     // Textes des pistes (espaces préservés)
};
```

### Archives ZIP

Lors de la sauvegarde avec vidéo, l'appli :
1. Crée un répertoire temporaire
2. Sauvegarde le `.dbi` dedans (avec chemin vidéo relatif)
3. Copie le fichier vidéo à côté
4. Crée le ZIP avec les outils natifs de l'OS :
   - **Windows** : `powershell Compress-Archive`
   - **macOS/Linux** : `zip -r`
5. Le ZIP tourne dans un **thread en arrière-plan** (`QtConcurrent::run`) avec une boîte de progression

---

## Pipeline d'Export

1. L'utilisateur enregistre sa voix → fichiers WAV sauvegardés dans un répertoire temporaire
2. L'utilisateur déclenche l'export → `ExportService` construit la commande FFmpeg :
   - Entrée : vidéo originale + 1-2 pistes audio
   - Encodage : H.264 CRF 18 (haute qualité)
   - Mixage audio avec contrôle de volume
3. FFmpeg tourne comme `QProcess`, la sortie est analysée pour le pourcentage de progression
4. `progressChanged(int)` met à jour la barre de progression UI
5. À la fin, `exportFinished(bool, QString)` notifie l'utilisateur

---

## Compilation & Exécution

### Prérequis

| Dépendance | Version | Usage |
|-----------|---------|-------|
| **Qt 6** | 6.5+ | Widgets, Multimedia, OpenGLWidgets, Concurrent |
| **CMake** | 3.16+ | Système de build |
| **Compilateur C++** | C++17 | GCC 9+, MSVC 2019+, Clang 10+ |
| **FFmpeg** | any | Export (runtime, pas compile-time) |
| **zip** | any | Archives ZIP (Linux/macOS uniquement, runtime) |
| **GStreamer** | 1.x | Support codecs vidéo sur Linux |

### Compilation Linux

```bash
# Installer Qt6 + codecs GStreamer
sudo apt install qt6-multimedia-dev libqt6multimediawidgets6 \
    libqt6opengl6-dev libqt6concurrent6 ffmpeg zip \
    gstreamer1.0-libav gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly

# Compiler
mkdir build && cd build
cmake ..
make -j$(nproc)
./DubInstante
```

### Windows

L'application au format zip est disponible depuis l'onglet **Actions** (artefact CI : `DubInstante-Windows`).

### macOS

**Installation :**
1. Téléchargez le `.dmg` depuis l'onglet **Actions** (artefact CI : `DubInstante_macos`).
2. Ouvrez le `.dmg` et glissez `DubInstante.app` dans votre dossier **Applications**.
3. Ouvrez le **Terminal** et exécutez :
   ```bash
   xattr -c /Applications/DubInstante.app
   ```
4. Lancez `DubInstante.app` normalement.

> **Pourquoi l'étape 3 ?** L'application n'est pas signée avec un certificat Apple Developer. macOS signale les apps téléchargées non signées comme « endommagées ». La commande `xattr -c` supprime ce marquage de quarantaine. Cette manipulation n'est requise qu'au premier lancement.

### Linux

L'application au format AppImage est disponible depuis l'onglet **Actions** (artefact CI : `DubInstante-Linux`).

### AppImage

```bash
./deploy/build_appimage.sh
```

---

## Dépendances Externes

Le projet n'utilise **aucune bibliothèque C++ externe** en dehors de Qt 6. Les outils externes sont invoqués au runtime :

| Outil | Utilisé par | Requis ? |
|-------|------------|----------|
| `ffmpeg` | `ExportService` | Pour l'export uniquement |
| `zip` | `SaveManager` | Pour les archives ZIP (Unix uniquement) |
| `powershell` | `SaveManager` | Pour les archives ZIP (Windows uniquement) |

---

## Raccourcis Clavier

| Touche | Action |
|--------|--------|
| **Espace** | Lecture / Pause |
| **Ctrl+S** | Arrêter l'enregistrement |
| **Échap** | Insère un espace sur le rythmo + lecture (ou arrête l'enregistrement plein écran) |
| **← / →** | Navigation image par image |
| **Toute lettre** | Tape sur la bande rythmo active |
| **Retour arrière** | Supprime le caractère avant le curseur |
| **Suppr** | Supprime le caractère après le curseur |

---

## Conventions de Code

1. **Séparation des couches stricte** : les classes Core n'incluent jamais de headers GUI
2. **GUI = rendu passif** : les widgets reçoivent des données via slots, émettent des signaux pour les actions utilisateur
3. **MainWindow = câblage uniquement** : aucun calcul, uniquement des appels `connect()`
4. **Forward declarations** dans les headers, `#include` dans les fichiers `.cpp`
5. **Commentaires Doxygen** sur toutes les méthodes publiques
6. **Nommage Qt** : préfixe `m_` pour les variables membres, méthodes en camelCase
7. **CMakeLists.txt** : les sources sont groupées par couche (`CORE_SOURCES`, `GUI_SOURCES`, `UTILS_SOURCES`)

---

## Roadmap

- **v0.4.0 — Gestion de Projet** ✅
    - [x] Système Sauvegarde/Chargement avec format `.dbi`
    - [x] Regroupement archive ZIP (projet + vidéo)
    - [x] Support compression multi-plateformes
    - [x] Persistance complète de l'état
- **v0.5.0 — Enregistrement Plein Écran & Raccourcis** ✅
    - [x] Mode d'enregistrement plein écran (case à cocher)
    - [x] Raccourci Ctrl+S pour arrêter l'enregistrement
    - [x] Menu popup des raccourcis
    - [x] Touche Échap pour quitter l'enregistrement plein écran
- **v0.6.0 — Personnalisation**
    - [ ] Couleurs personnalisées des bandes rythmo (fond + texte)
    - [ ] Réglages indépendants par bande
    - [ ] Ajustements visuels sans impact sur la mise en page
- **Et plus encore…**
    - [ ] Vos suggestions sont les bienvenues ! 💡
