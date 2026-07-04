# Plan — Correctifs pré-release DubInstante (v0.12.0)

## Contexte

La revue de code pré-release a identifié 4 bugs critiques (inversion de la plage temporelle d'export, data race UB dans le worker FFmpeg, crash sur fichier sans flux vidéo, sauvegarde `.dbi` non atomique = perte de données sur disque plein), 6 bugs majeurs et ~10 problèmes de robustesse. Objectif : tout corriger sans régression, par petits lots compilés et testés indépendamment, chaque lot = 1 commit conventionnel.

**Décisions actées** : version unifiée **0.12.0** (la v1.0.0 Stable sortira après la campagne de tests humains) ; autosave = compléter la sérialisation (pas de flux de restauration au démarrage) ; correctifs Android inclus en batch final.

**Principe anti-casse** : ordre des batches du plus isolé au plus couplé ; compile + smoke test après chaque batch avant de passer au suivant ; aucune modification de format de fichier incompatible (les nouveaux champs JSON `.dbi` sont optionnels, pas de bump de la version binaire `m_version = 1`).

---

## Étape 0 — Préparation

1. Créer la branche `$GIT_BRANCH_PREFIX/fix/pre-release-hardening` depuis `master` (jamais de travail direct sur master).
2. Commiter le travail en cours non commité sur `src/core/ExportService.{cpp,h}` (accumulateur d'erreurs borné, garde anti-double-émission, pré-check disque) : `fix(export): report ffmpeg errors and pre-check disk space`. C'est la base sur laquelle les batches 4 s'appuient.
3. Laisser `docs/dbi_validation_report.md` et `README.md` modifiés de côté (commit `docs:` séparé en fin de chantier).

---

## Batch 1 — Correctifs isolés, risque quasi nul — `fix(core): ...` / `fix(gui): ...`

| # | Fichier | Fix |
|---|---------|-----|
| 1.1 | `src/gui/ExportDialog.cpp:835` | Plage temporelle inversée : tester `m_rangeCombo->currentData().toString() == "last"` au lieu de `currentIndex() == 0`. |
| 1.2 | `src/core/ExportService.cpp:32-38` | `isFFmpegAvailable()` : `waitForStarted(3000) && waitForFinished(3000) && exitStatus()==NormalExit && exitCode()==0`. |
| 1.3 | `src/core/AudioRecorder.cpp:146` | Format non supporté : `m_audioBuffer.clear()` avant le `return` (croissance mémoire non bornée). |
| 1.4 | `src/gui/RythmoWidget.{h,cpp}` | Remplacer `QDateTime::currentMSecsSinceEpoch()` (3 sites : lignes 111, 125, 147) par un membre `QElapsedTimer m_syncClock` démarré au constructeur (horloge monotone). |
| 1.5 | `src/core/RythmoManager.{h,cpp}` | `m_insertOffset`/`m_lastInsertPosition` par piste : `QMap<int,int> m_insertOffsets` + `QMap<int,qint64> m_lastInsertPositions` (utilisés dans `insertCharacter` et `deleteCharacter`). |

Vérif : compile + lancement, édition rythmo sur 2 pistes.

## Batch 2 — FFmpegWorker : thread-safety + états invalides — `fix(core): ...`

Fichiers : `src/core/FFmpegFrameExtractor.{h,cpp}`.

1. `m_abort` et `m_hasNewRequest` → `std::atomic<bool>` (le `.cpp` ne change pas : écritures déjà sous mutex avant `wakeOne()`). `m_fileChanged` reste un bool (toujours accédé sous mutex).
2. `process()` : si `openFile()` échoue → `closeFile()` pour purger l'état partiel ; renforcer le garde : `if (!m_formatContext || !m_codecContext || m_videoStreamIndex < 0 || !m_swsContext || !m_rgbBuffer || targetMs < 0) continue;`.
3. `openFile()` : vérifier `av_image_get_buffer_size() > 0`, `av_malloc != nullptr`, `sws_getContext != nullptr` — chaque échec émet `errorOccurred` et retourne false.
4. Forward des erreurs : `FFmpegFrameExtractor` expose `errorOccurred(QString)` connecté au worker ; `PlaybackEngine` (constructeur, `src/core/PlaybackEngine.cpp:42`) le relaie vers son signal `errorOccurred` existant → dialogue via `MainWindow::onError` déjà branché.

Vérif : ouvrir un fichier audio-only ou corrompu puis scrubber → dialogue d'erreur, zéro crash ; fermeture de l'app propre pendant un scrub.

## Batch 3 — SaveManager : atomicité + durcissement + police — `fix(core): ...`

Fichier : `src/core/SaveManager.cpp` (+ include `<QSaveFile>`).

1. `save()` : remplacer `QFile` par `QSaveFile`, vérifier chaque `write()`, `cancelWriting()` en cas d'échec, `return file.commit()`. Protège save manuel ET autosave contre le disque plein.
2. `load()` : après lecture de `payloadSize`, rejeter si `payloadSize == 0 || payloadSize > 64 Mo || payloadSize > file.size()` ; appliquer `data = sanitize(data);` juste avant `return true`.
3. Persistance police : `save()` écrit `font_family` + `font_bold` dans `styleObj` ; `load()` restaure via `setFamily()` (si champ non vide) et `setBold()` (défaut true). Anciens `.dbi` → défauts « Classic », aucun bump de version.

Ajouter un test minimal assert-based `tests/test_savemanager.cpp` (cible CMake optionnelle `test_savemanager`, non liée à la GUI) : roundtrip complet avec police custom, fichier tronqué, checksum invalide, payloadSize géant. C'est l'unique check exécutable laissé derrière ce chantier.

Vérif : `test_savemanager` vert + roundtrip manuel dans l'app.

## Batch 4 — Pipeline d'export — `fix(export): ...`

Fichiers : `src/core/ExportService.{h,cpp}`, `src/gui/MainWindow.cpp`, `src/gui/ExportDialog.cpp`.

1. `buildFFmpegArgs` : `amix=inputs=N:duration=longest:normalize=0` (sinon chaque piste est atténuée par N). Contrainte FFmpeg ≥ 4.4 à documenter dans le README.
2. `showExportDialog` (MainWindow:1551) : refuser si `!m_hasRecording.value(0)` ; pour les extras, passer `QString()` quand `!m_hasRecording.value(i)` (une piste jamais enregistrée ne bloque plus l'export).
3. Nettoyage du fichier partiel : membre `m_currentOutputPath` mémorisé dans `startExport`, `QFile::remove` dans la branche échec de `handleProcessFinished`, dans `handleProcessError` et dans `cancelExport`.
4. Bouton « Annuler » à côté de `m_exportProgressBar` (visible pendant l'export uniquement) → `m_exportService->cancelExport()`.
5. `startExport` déjà en cours : remplacer l'`emit exportFinished(false, ...)` par un `qWarning` + return (l'émission actuelle masque la barre de progression de l'export en cours).
6. Garde copy+scale : dans `buildFFmpegArgs`, ne pas ajouter `-vf` si `videoCodec == "copy"` ; dans `ExportDialog::validateSettings`, warning explicite.
7. Vidéo sans flux audio : étendre le ffprobe existant de `ExportDialog::populateFields` (`stream=width,height` → ajouter une requête `a:0`) ; si pas d'audio, forcer `m_defaultOriginalVolume = 0` et désactiver le slider « Son original ».

Vérif : export réel 2 pistes (volumes conformes, mesure à l'oreille + ffprobe sur les durées « dernier enregistrement » vs « tout le projet ») ; annulation → fichier supprimé ; piste 2 vierge → export passe.

## Batch 5 — MainWindow : courses et captures — `fix(gui): ...`

Fichier : `src/gui/MainWindow.cpp`.

1. Course WAV non finalisé : dans `setTrackCount`, connecter `AudioRecorder::recorderStateChanged` → si `StoppedState && !m_isRecording` alors `refreshPreviewSources()` ; retirer l'appel direct dans `toggleRecording` (branche stop, ligne 1428).
2. Lambda `QtConcurrent::run` (ligne 1131) : capturer une copie `paths = m_tempAudioPaths` au lieu de lire le membre via `this` depuis le thread worker (et passer `paths` à `saveWithMedia`).
3. `connectTrack` : recalculer le fps dans le lambda `navigationRequested` (au lieu de capturer `frameStep` figé avant chargement des métadonnées).
4. Autosave (`onAutoSaveTriggered`) : sérialiser aussi `hasRecording`, `audioFilePath`, `recordStartMs`, `recordDurationMs` — aligner la construction de `TrackAudioSaveData` sur celle de `onSaveProject` (factoriser la construction de `SaveData` dans un helper privé `collectSaveData()` utilisé par les deux — helper utilisé 2+ fois).

Vérif : record → stop → preview lit immédiatement (répéter 5×) ; autosave puis ouverture manuelle du backup → prises présentes.

## Batch 6 — GlobalSettingsDialog : profils de sortie — `fix(gui): ...`

Fichier : `src/gui/GlobalSettingsDialog.cpp`.

Stocker `"label|device"` dans `Qt::UserRole` de chaque `QListWidgetItem` (`addPreferredOutput` et `loadSettings`) ; `saveSettings` lit `data(Qt::UserRole)` au lieu de parser le texte affiché (cassé dès que le nom de périphérique contient des parenthèses — cas quasi systématique sous Windows).

Vérif : créer un profil dont le device contient `(...)`, sauvegarder, rouvrir → profil intact et sélectionnable dans le menu Audio.

## Batch 7 — saveWithMedia : gros fichiers — `fix(core): ...`

Fichier : `src/core/SaveManager.cpp:125-247`.

1. `QTemporaryDir` sur le volume de destination : `QTemporaryDir tempDir(QFileInfo(zipPath).absolutePath() + "/.dbi_tmp_XXXXXX");` (jamais `/tmp`, souvent un tmpfs limité par la RAM sous Linux).
2. Pré-check `QStorageInfo` : espace libre ≥ 2 × taille vidéo + 64 Mo, sinon message d'erreur explicite.
3. Unix : `zip -0 -r` (stockage sans recompression d'un MP4 déjà compressé).

Vérif : archive zip d'une grosse vidéo → temps raisonnable ; simuler l'espace insuffisant (petite partition ou quota) → message clair, pas de fichier résiduel.

## Batch 8 — ExportDialog : ffprobe non bloquant — `fix(gui): ...`

Fichier : `src/gui/ExportDialog.cpp:554-582`.

Passer le probe en asynchrone : `QProcess` membre, `connect(&probe, finished, ...)` remplit `m_videoOriginalWidth/Height/AspectRatio` + spinbox custom à l'arrivée ; défauts 1920×1080 immédiats. Supprime le blocage GUI de 1,5 s max à l'ouverture du dialog (sensible sur stockage réseau).

## Batch 9 — Version unique 0.12.0 — `build: ...`

1. `CMakeLists.txt` : `project(DubInstante VERSION 0.12.0 ...)` + `target_compile_definitions(... APP_VERSION="${PROJECT_VERSION}")`.
2. `main.cpp:40` : `app.setApplicationVersion(APP_VERSION);` (remplace le « 1.4.0 » codé en dur — CMake devient la source unique de version).
3. Vérifier que `CHANGELOG.md` contient une entrée `## [0.12.0]` (la CI `main.yml` en extrait le numéro pour nommer les artefacts).

## Batch 10 — Android — `fix(android): ...`

Fichier : `src/phonegui/app/src/main/java/com/dubinstante/app/AndroidExportService.kt`.

1. Supprimer `tempSourceFile` après l'export (succès ET échec — dans `onFinish`/`onError`/`onCancel`).
2. `openInputStream` null → `onComplete(false, "Impossible de lire la vidéo source", null)` au lieu d'un fichier vide passé à FFmpeg.
3. Pré-check espace : `StatFs(cacheDir.path).availableBytes` ≥ 2 × taille estimée de la source avant la copie.

Vérif : build Gradle (`./gradlew assembleDebug`) ; test device si disponible, sinon revue + compilation suffisent pour ce batch.

---

## Hors périmètre (explicitement exclus)

- Prompt de restauration autosave au démarrage (décision utilisateur : non).
- Refonte du zip sans copie intermédiaire (`Compress-Archive -Path a,b,c`) — amélioration future, le batch 7 rend le chemin actuel sûr.
- Protection double-`release()` du handle JNI (acceptable tant que `NativeBridge` est l'unique appelant).

## Compatibilité & garde-fous

- **`.dbi`** : champs JSON ajoutés optionnels, `m_version` inchangé — anciens fichiers lisibles (défauts Classic), nouveaux fichiers lisibles par l'ancien parseur (champs ignorés).
- **FFmpeg ≥ 4.4** requis pour `normalize=0` (2021 ; couvert par les cibles CI) — à mentionner dans le README.
- **1 batch = 1 commit** conventionnel (`fix(scope): ...`, ≤72 chars) ; compile + smoke test avant chaque commit ; `git diff` avant commit (pas de secrets/fichiers parasites).

## Vérification finale (avant merge)

1. `test_savemanager` vert.
2. Scénarios manuels : scrub intensif sur gros fichier ; fichier corrompu/audio-only → dialogue propre ; record 2 pistes → export « dernier enregistrement » et « tout le projet » (durées contrôlées via `ffprobe -show_format`) ; annulation d'export → aucun fichier résiduel ; save/load `.dbi` avec police et couleurs custom ; profil audio avec parenthèses ; save `.zip` avec vidéo.
3. Push → CI GitHub Actions verte sur Linux/Windows/macOS.
4. Artefacts nommés `*_0.12.0` — cette build part en tests humains ; la v1.0.0 Stable sera taguée ensuite sans nouveau changement de code.
