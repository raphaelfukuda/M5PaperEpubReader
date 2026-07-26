# Roadmap

This document lists the next planned improvements. It describes intent rather
than a release commitment; contributions should follow `CONTRIBUTING.md` and
remain compatible with the original ESP32 M5Paper.

## Planned features

### Cover image in the reading menu

- Extract the cover relationship from EPUB 2/3 metadata and manifest data.
- Decode and render a bounded raster thumbnail without loading the full image
  into internal RAM.
- Keep menu layout usable when a book has no valid cover.

### Book cover on the sleep screen

- Reuse the validated cover pipeline from the reading menu.
- Show the current book cover when sleep starts from reading.
- Retain the localized sleep/wake instructions and provide a text-only fallback.
- Perform no periodic E-Ink refresh while sleeping.

### Font-family selection in the reading menu

- Offer a small set of fonts suitable for the M5Paper and Portuguese text.
- Provide every option in English and Brazilian Portuguese UI flows.
- Preserve the current reading anchor during reflow and persist the selection.
- Document flash, PSRAM and rendering-performance impact for each bundled font.

## Known bug

### Incorrect page number after returning to a book

The visible reading position can be restored while the displayed page number
starts again from the beginning of the in-memory session. Page numbering must
remain consistent after leaving and reopening a book, restarting the device or
restoring a saved reading position.

The fix must define whether numbering is global or chapter-relative, persist or
reconstruct the required state with bounded memory, and include regression tests
for reopen, sleep/wake, zoom reflow and chapter transitions.
