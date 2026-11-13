#!/bin/sh
# Usage: docs/build-manual.sh slopebox-1024-manuale   (name of docs/<name>.md, without extension)
# Needs pandoc and Node with Playwright (Chromium).
set -e
cd "$(dirname "$0")"
pandoc "$1.md" -s --toc --toc-depth=2 -M toc-title="Indice" --css manual-style.css -o "$1.html"
NODE_PATH="${NODE_PATH:-$(npm root -g)}" node tools/print-pdf.js "$PWD/$1.html" "$PWD/$1.pdf"
rm -f "$1.html"
echo "wrote docs/$1.pdf"
