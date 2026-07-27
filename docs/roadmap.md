# Roadmap

This document lists the next planned improvements. It describes intent rather
than a release commitment; contributions should follow `CONTRIBUTING.md` and
remain compatible with the original ESP32 M5Paper.

## Implemented roadmap features

### Cover image in the reading menu

- Extract the cover relationship from EPUB 2/3 metadata and manifest data.
- Decode and render a bounded raster thumbnail without loading the full image
  into internal RAM.
- Keep menu layout usable when a book has no valid cover.

Implemented with EPUB 2/3 cover discovery, a 2 MiB safety limit, proportional
JPEG/PNG rendering and text-only fallback.

### Book cover on the sleep screen

- Reuse the validated cover pipeline from the reading menu.
- Show the current book cover when sleep starts from reading.
- Use the largest proportional cover area and provide a text-only fallback.
- Perform no periodic E-Ink refresh while sleeping.

Implemented by reusing the in-memory validated cover in an 8-bit grayscale
canvas. The cover is shown without overlay text, and full-quality refreshes are
forced both before entering and after leaving light sleep. Touch and both lever
directions can wake the ESP32 without changing the sleep type.

### Kindle-style library

The library presents two books per row, with proportionally resized covers and
UTF-8 titles below them. Metadata extraction stops as soon as title and cover
are available, and all visible cards are collected before a single quality
refresh. The auxiliary parser lives in PSRAM and falls back to text cards when
memory, metadata or cover limits prevent image loading.

### Font-family selection in the reading menu

- Offer a small set of fonts suitable for the M5Paper and Portuguese text.
- Provide every option in English and Brazilian Portuguese UI flows.
- Preserve the current reading anchor during reflow and persist the selection.
- Document flash, PSRAM and rendering-performance impact for each bundled font.

Implemented with Book (FreeSerif), Sans (FreeSans) and the prior Unicode Compact
font. Selection preserves the anchor through reflow and is stored in reading
state version 3; older state files migrate to Compact to preserve appearance.

## Resolved bugs

### Incorrect page number after returning to a book

The visible reading position can be restored while the displayed page number
starts again from the beginning of the in-memory session. Page numbering must
remain consistent after leaving and reopening a book, restarting the device or
restoring a saved reading position.

Resolved in the reading-state version 2 format. The reader persists the absolute
session page number and a bounded window of up to 32 previous `PageAnchor` values.
This preserves numbering and allows backward navigation after reopening without
repaginating the whole book. Version 1 files are accepted, but cannot recover a
page number or history that they never stored; the next save upgrades the file.
# Estado da otimização de refresh

As etapas de arquitetura e firmware da otimização do M5Paper original estão
concluídas. Permanecem como validação física: matriz comparativa de waveforms,
fotografias/notas de ghosting, estabilidade após 500 páginas e experimentos
controlados de SRAM. O segundo núcleo só será reconsiderado se essas medições
mostrarem parsing/layout no caminho crítico.

Os testes físicos conduzidos durante o desenvolvimento confirmaram navegação,
retomada, sleep/wake, capas e a política adaptativa no aparelho usado. Para
reduzir ghosting observado, a limpeza Quality foi ajustada para 2 refreshes
Fastest, 5 Fast, limite geral de 6 refreshes de leitura ou 5 minutos.
