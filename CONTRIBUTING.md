# Contributing

Thank you for helping improve M5Paper EPUB Reader.

1. Open an issue before a large architectural change.
2. Keep the target limited to the original ESP32 M5Paper.
3. Preserve the cooperative loop and exclusive display/microSD SPI access.
4. Keep code and comments in English. User-facing strings must be added to both
   English and Brazilian Portuguese tables; English remains the default.
5. Do not add network services, M5EPD or LVGL.
6. Run `pio test -e native` and `pio run -e m5stack-paper`.
7. Describe hardware validation, memory impact and current limitations in the PR.

Use focused commits and do not include EPUB books unless they are purpose-built,
redistributable test fixtures with documented provenance.
