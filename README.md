# DUBSync Phase 1 - Socle Lecteur Vidéo

Ce projet constitue le socle technologique pour un logiciel de doublage vidéo. Il implémente un lecteur vidéo modulaire utilisant Qt 6 et OpenGL pour un rendu performant.

## Architecture

- **VideoWidget** : Gère l'affichage des frames vidéo via un `QVideoSink` et un rendu OpenGL. Totalement indépendant de la source.
- **PlayerController** : Encapsule la logique de `QMediaPlayer`. Communique avec les autres composants via Signals/Slots.
- **MainWindow** : Gère l'interface utilisateur, le layout (marge de 20%) et les interactions.

## Prérequis

- **Qt 6** (Modules : `Widgets`, `Multimedia`, `OpenGL`, `OpenGLWidgets`)
  - Sur Ubuntu (24.04+) :

    ```bash
    sudo apt install qt6-base-dev qt6-multimedia-dev libqt6opengl6-dev build-essential
    sudo apt install libgl1-mesa-dev libxkbcommon-dev
    ```

- **Codecs (GStreamer)** : Pour la lecture des fichiers MP4 (H.264/AAC).

    ```bash
    sudo apt install gstreamer1.0-libav gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly
    ```

- **CMake** (3.16+)
- **Compilateur C++17** (GCC/Clang/MSVC)

## Compilation et Lancement

1. Créer un répertoire de build :

   ```bash
   mkdir build && cd build
   ```

2. Configurer le projet avec CMake :

   ```bash
   cmake ..
   ```

3. Compiler :

   ```bash
   make -j$(nproc)
   ```

4. Lancer l'application :

   ```bash
   ./DUBSync
   ```

## Fonctionnalités (Phase 1)

- Chargement de fichier MP4.
- Contrôles de lecture (Play, Pause, Stop).
- Barre de progression navigable et synchronisée.
- Layout moderne avec fond blanc et marges respectées.
