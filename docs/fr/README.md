# DubInstante - Studio de Doublage Vidéo Professionnel

DubInstante est un logiciel de doublage vidéo professionnel conçu pour être puissant, intuitif et visuellement raffiné. Il permet de lire des vidéos, d'écrire des textes de doublage sur une bande rythmo, d'enregistrer des pistes vocales synchronisées et d'exporter le résultat final.

## ✨ Fonctionnalités Principales

### 🎬 Lecture Vidéo
- **Lecteur Haute Performance**: Rendu accéléré par OpenGL via Qt 6 Multimedia
- **Navigation Précise**: Défilement image par image avec timeline visuelle
- **Synchronisation Temps Réel**: Audio et vidéo parfaitement synchronisés avec les bandes rythmo
- **Contrôle de Vitesse**: Vitesse de lecture ajustable (1% à 400%) pour la pratique et la révision

### 📝 Système de Bande Rythmo
- **Double Bande Rythmo**: Deux bandes de texte défilant indépendantes pour les workflows complexes
- **Édition Interactive**: Saisie de texte directe sur la bande rythmo avec aperçu en temps réel
- **Animation Ultra-Fluide**: Boucle d'interpolation 60 FPS dédiée garantissant un défilement fluide indépendant des saccades du moteur vidéo
- **Snap-to-Grid de Précision**: Alignement intelligent au caractère le plus proche lors de la pause pour une édition intuitive et centrée
- **Indicateurs Unifiés**: Alignement parfait de la ligne de temps et du guide de lecture pour un feedback visuel sans décalage
- **Rendu Virtualisé**: Ne dessine que le texte visible, permettant des enregistrements infinis sans lag
- **Réaction Instantanée**: Découplage de l'interface et du moteur vidéo pour une frappe fluide sur gros fichiers
- **Seek Debouncing**: Regroupement intelligent des recherches pour éviter la saturation disque (50GB+)
- **Contraste du Texte**: Bouton "Texte Blanc" pour switcher la couleur selon le fond vidéo
- **Styles Visuels**: Plusieurs modes d'affichage (Classique, Gradient moderne, Minimaliste, Contouré)
- **Synchronisation Temporelle**: Défilement automatique en synchronisation avec la vidéo
- **Navigation par Clic**: Cliquez n'importe où sur la bande rythmo pour sauter à cet instant

### 🎙️ Enregistrement Multipiste
- **Support Dual Track**: Enregistrement simultané de deux pistes vocales séparées
- **Sélection de Périphérique**: Sélection de microphone indépendante pour chaque piste
- **Monitoring Temps Réel**: Contrôle de gain en direct avec sliders visuels
- **Contrôle de Volume**: Ajustement du volume par piste (0-100%)
- **Enregistrement Professionnel**: Capture WAV haute qualité avec gain configurable

### 🎨 Interface Moderne
- **Interface Raffinée**: Thème clair professionnel avec contrôles soignés
- **Sliders Réactifs**: Sliders élégants avec remplissage dégradé
- **Spinboxes Compactes**: Entrées numériques optimisées pour un contrôle précis
- **Layout Intuitif**: Contrôles bien organisés avec hiérarchie visuelle claire
- **Style Personnalisé**: Feuille de style Qt moderne avec attention aux détails

### 📤 Export & Intégration
- **Intégration FFmpeg**: Fusion vidéo/audio professionnelle
- **Export Multipiste**: Combine la vidéo originale avec les deux pistes vocales
- **Préservation de Qualité**: Maintient la qualité vidéo originale tout en ajoutant l'audio doublé
- **Suivi de Progression**: Barre de progression visuelle pendant l'export

## 🏗️ Architecture

- **MainWindow**: Hub central coordonnant tous les composants UI et workflows
- **RythmoWidget**: Bande de texte défilant synchronisée avec la lecture vidéo
- **RythmoOverlay**: Système d'overlay transparent pour l'affichage double rythmo
- **AudioRecorderManager**: Capture audio multipiste avec gestion des périphériques
- **PlayerController**: Moteur de lecture multimédia (Qt 6 Multimedia)
- **VideoWidget**: Rendu vidéo accéléré matériellement (OpenGL)
- **Exporter**: Fusionneur vidéo/audio basé sur FFmpeg

## 📋 Prérequis

- **Qt 6.5+** (Modules: `Widgets`, `Multimedia`, `OpenGLWidgets`)
- **FFmpeg**: Requis pour l'export final (`sudo apt install ffmpeg` sur Linux)
- **Codecs (GStreamer)**: Pour la lecture MP4 sur Linux
    ```bash
    sudo apt install gstreamer1.0-libav gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly
    ```

## 🚀 Installation & Compilation

### Windows
Le projet est automatiquement compilé pour Windows via GitHub Actions.
1. Allez sur l'onglet **Actions** de ce dépôt
2. Téléchargez le dernier artefact **DubInstante-Windows**

### Linux (Compilation Manuelle)
1. Installez les dépendances:
   ```bash
   sudo apt install qt6-multimedia-dev libqt6multimediawidgets6 libqt6opengl6-dev ffmpeg
   ```
2. Compilation:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ./DubInstante
   ```

### 📦 Créer une AppImage
Pour une distribution standalone sur Linux:
```bash
./deploy/build_appimage.sh
```

## 🎹 Raccourcis Clavier & Utilisation

### Contrôles de Lecture
- **Espace**: Lecture / Pause
- **Échap**: Insère un espace sur la bande rythmo et démarre la lecture
- **Flèches Gauche/Droite**: Navigation image par image

### Workflow d'Enregistrement
1. **Charger Vidéo**: Cliquez sur "Ouvrir Vidéo" pour sélectionner votre fichier vidéo
2. **Configurer les Pistes**:
   - Sélectionnez le microphone pour la Piste 1 (et Piste 2 si activée)
   - Ajustez les niveaux de gain avec les sliders (0-100%)
   - Définissez les niveaux de volume pour le monitoring
3. **Éditer le Rythmo**:
   - Tapez directement sur la bande rythmo pour ajouter du texte
   - Le texte défile automatiquement avec la lecture vidéo
   - Cliquez pour sauter à des timestamps spécifiques
4. **Enregistrer**:
   - Cliquez sur le bouton **REC** pour démarrer l'enregistrement
   - Parlez vos lignes en synchronisation avec le rythmo
   - Cliquez à nouveau sur **REC** pour arrêter
5. **Exporter**:
   - Revoyez votre enregistrement
   - Exportez la vidéo finale avec les pistes audio fusionnées

### Fonctionnalités Avancées
- **Mode Double Piste**: Activez "Activer Piste 2" pour l'enregistrement simultané sur deux pistes
- **Ajustement de Vitesse**: Utilisez la spinbox "Vitesse Défilement" pour ralentir ou accélérer la lecture
- **Styles Visuels**: Configurez l'apparence de la bande rythmo dans le code (RythmoWidget::VisualStyle)

## 🔧 Configuration

### Styles Visuels du Rythmo
Éditez `RythmoWidget.cpp` pour personnaliser l'apparence du rythmo:
- **ClassicBox**: Affichage traditionnel en boîte
- **ModernGradient**: Look moderne avec remplissage dégradé
- **MinimalText**: Affichage texte seul épuré
- **Outlined**: Texte avec contour pour un meilleur contraste

## 🎨 Philosophie de Design UI

DubInstante présente une interface utilisateur soigneusement conçue avec:
- **Thème Professionnel Épuré**: Schéma de couleurs claires avec profondeur subtile
- **Contrôles Raffinés**: Spinboxes, sliders et boutons soignés
- **Hiérarchie Visuelle**: Organisation claire des contrôles par fonction
- **Design Réactif**: Effets de survol et interactions fluides
- **Accessibilité**: Texte à fort contraste et étiquetage clair

## 📜 Licence

Ce projet est open-source. N'hésitez pas à contribuer, fork ou l'utiliser pour vos projets de doublage!

## 🤝 Contribuer

Les contributions sont les bienvenues! Que ce soit:
- Rapports de bugs
- Demandes de fonctionnalités
- Améliorations du code
- Améliorations UI/UX
- Mises à jour de la documentation

Veuillez ouvrir une issue ou soumettre une pull request.

---

**DubInstante** - Rendre le doublage vidéo professionnel accessible à tous.

## 🗺️ Roadmap
- **v1.4.0 - Mise à jour Personnalisation**
    - [ ] Personnalisation avancée des bandes rythmo (Couleurs de fond et de texte)
    - [ ] Sous-menu dédié pour le réglage indépendant de chaque bande
    - [ ] Ajustements visuels sans impact sur la position/mise en page
- **v1.5.0 - Gestion de Projet**
    - [ ] Système de Sauvegarde/Chargement d'état (Format de fichier projet dédié)
    - [ ] Persistance de tout le texte rythmo et des paramètres modifiés
- **v1.6.0 - Expérience Pro**
    - [ ] Mode d'enregistrement plein écran
    - [ ] Raccourcis clavier complets pour le contrôle (pause et autres actions)
- **Et plus encore...**
    - [ ] À l'écoute des utilisateurs ! Vos idées et suggestions sont toujours les bienvenues 💡

