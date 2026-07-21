#!/usr/bin/env bash
# Analyse un coredump brut (core_NNN.bin exporté sur la SD) sans vérif SHA256.
# Usage: scripts/coredump.sh <core.bin> [elf] [info|dbg]
#   elf    : défaut build/esp32_mp3_player.elf
#   mode   : info (défaut, résumé + backtraces) ou dbg (gdb interactif)
# Le check SHA de l'app est contourné (monkeypatch exe_name=None) ; les symboles
# restent valides tant que l'ELF vient des mêmes sources que le firmware flashé.
set -euo pipefail

CORE="${1:?usage: coredump.sh <core.bin> [elf] [info|dbg]}"
ELF="${2:-build/esp32_mp3_player.elf}"
MODE="${3:-info}"

python - "$CORE" "$ELF" "$MODE" <<'PY'
import sys
from esp_coredump.corefile import loader
# Neutralise la vérif SHA256 : force exe_name=None dans _extract_elf_corefile
_orig = loader.EspCoreDumpLoader._extract_elf_corefile
def _patched(self, exe_name=None, e_machine=loader.ESPCoreDumpElfFile.EM_XTENSA):
    return _orig(self, None, e_machine)
loader.EspCoreDumpLoader._extract_elf_corefile = _patched

from esp_coredump import CoreDump
core, elf, mode = sys.argv[1], sys.argv[2], sys.argv[3]
import glob
gdbs = glob.glob('/Users/yann/.espressif/tools/xtensa-esp-elf-gdb/*/xtensa-esp-elf-gdb/bin/xtensa-esp32-elf-gdb')
kw = {'gdb': gdbs[0]} if gdbs else {}
cd = CoreDump(chip='esp32', core=core, core_format='raw', prog=elf, **kw)
if mode == 'dbg':
    cd.dbg_corefile()
else:
    cd.info_corefile()
PY
