# scripts/

Utilitaires de développement et de diagnostic pour le firmware TB2026-YDS.

---

## `campagne.sh` — flash + capture d'une campagne de diagnostic

Flashe la carte puis capture toute la sortie série (logs `ESP_LOGI`) dans un fichier
horodaté `campagne_MMJJ_HHMM.log` à la racine du projet. La capture est mise en place
**avant** le run car rien n'est stocké sur la carte : les logs partent sur l'UART et
disparaissent si on ne les enregistre pas en direct.

### Prérequis

- Module diag compilé : `#define DIAG_ENABLED 1` dans
  [components/services/diag/diag.h](../components/services/diag/diag.h)
  (le script refuse de tourner sinon).
- Carte branchée en USB.

### Lancement

```sh
zsh scripts/campagne.sh [port]
```

- `port` optionnel : ex. `/dev/cu.usbserial-XXXX`. Omis → autodétection par idf.py.
- **`zsh`, pas `sh` ni `bash`** : le script `activate` d'Espressif détecte mal le
  sourcing sous POSIX-sh et refuse de s'exécuter.
- Le flash a lieu **une seule fois, au début** (il remet à zéro les high-water marks et
  compteurs de heap) — jamais au milieu d'une campagne.
- `Ctrl-]` pour arrêter le monitor et clôturer le log.

Le Mac est maintenu éveillé (`caffeinate`) pendant la capture : s'il s'endort, le port
USB tombe et le log est tronqué.

---

## `coredump.sh` — analyser un coredump `.bin`

Décode un coredump brut (`core_NNN.bin` exporté sur la SD après un crash) en texte
lisible : registres, backtraces de toutes les tâches, régions mémoire.

Contourne la vérif SHA256 de l'app (un ELF recompilé depuis des sources identiques
échoue au check car le timestamp de build est embarqué). Les symboles restent valides
tant que l'ELF vient bien des **mêmes sources que le firmware flashé**.

### Prérequis

Activer l'environnement Python de l'IDF **une fois** dans le shell :

```sh
. $HOME/.espressif/python_env/idf6.0_py3.14_env/bin/activate
```

(le `gdb` xtensa est localisé automatiquement, pas besoin de l'ajouter au PATH).

### Lancement

```sh
bash scripts/coredump.sh <core.bin> [elf] [info|dbg]
```

- `core.bin` : le coredump à analyser (obligatoire).
- `elf` : ELF correspondant au firmware qui a planté. Défaut
  `build/esp32_mp3_player.elf`. Pour un crash d'une version antérieure, utiliser l'ELF
  archivé, ex. `build/elf_at_crash.elf`.
- mode :
  - `info` (défaut) — résumé : tâche crashée, `exccause`, registres, backtraces, mémoire.
  - `dbg` — ouvre gdb en interactif (`bt`, `info threads`, `thread N`, `p var`, …).

### Exemples

```sh
# Résumé à l'écran
bash scripts/coredump.sh ~/Desktop/core_001.bin build/elf_at_crash.elf

# Sauvegarder le dump texte complet
bash scripts/coredump.sh ~/Desktop/core_001.bin build/elf_at_crash.elf info > ~/Desktop/core_001.txt 2>&1

# Session gdb interactive
bash scripts/coredump.sh ~/Desktop/core_001.bin build/elf_at_crash.elf dbg
```

> Si l'ELF ne correspond pas aux sources flashées, l'analyse sort quand même mais les
> symboles/offsets seront faux — vérifier que l'ELF est le bon avant de conclure.
