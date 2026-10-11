#!/usr/bin/env bash
# Construit le .cia à partir de l'ELF.
# Usage : tools/make_cia.sh [GeminiChat.elf] [GeminiChat.cia]
# Variables : MAKEROM et BANNERTOOL = chemins des exécutables (sinon cherchés dans le PATH)
set -euo pipefail

ELF="${1:-GeminiChat.elf}"
OUT="${2:-GeminiChat.cia}"
MAKEROM="${MAKEROM:-makerom}"
BANNERTOOL="${BANNERTOOL:-bannertool}"
WORK="build/cia"

mkdir -p "$WORK"

"$BANNERTOOL" makebanner \
  -i assets/banner.png \
  -a assets/audio.wav \
  -o "$WORK/banner.bnr"

"$BANNERTOOL" makesmdh \
  -s "Gemini Chat" \
  -l "Chat avec l'API Gemini" \
  -p "Non officiel" \
  -i assets/icon.png \
  -o "$WORK/icon.icn"

"$MAKEROM" -f cia -o "$OUT" \
  -elf "$ELF" \
  -rsf app.rsf \
  -target t \
  -exefslogo \
  -icon "$WORK/icon.icn" \
  -banner "$WORK/banner.bnr"

ls -l "$OUT"
