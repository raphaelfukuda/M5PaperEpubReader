# Referências oficiais consultadas

Clones locais rasos, mantidos fora do projeto e sem modificações:

| Repositório | Commit | Tag descrita |
|---|---|---|
| M5GFX | `729297d6e3d657ddc1ec5189bac2f2ea68828085` | `0.2.26` |
| M5Unified | `4fb444784c85791e0b0207701392b42be234b2e7` | `0.2.19` |
| UnzipLIB | `594a95216d70cb16404c767922cd8aa55e382f66` | sem tag no clone raso; Registry 1.0.0 |

## Arquivos consultados

- `M5GFX/src/M5GFX.cpp`: autodetecção do M5Paper original, IT8951, GT911, dimensões, rotação, pinos SPI/I²C e PSRAM do display.
- `M5GFX/src/lgfx/v1/panel/Panel_IT8951.cpp`: `waitDisplay()`, refresh por região e mapeamento IT8951 de `epd_fastest`, `epd_fast`, `epd_text` e qualidade.
- `M5GFX/src/lgfx/v1/LGFXBase.hpp`: declarações de `display(...)`, `waitDisplay()`, `displayBusy()` e `setEpdMode(...)`.
- `M5GFX/src/M5GFX.h` e `lgfx/v1/LGFX_Sprite.hpp`: `M5Canvas`, PSRAM, canvas 1-bit.
- `M5GFX/examples/Basic/TextLogScroll/TextLogScroll.ino`: canvas monocromático.
- `M5Unified/examples/Test/build_test/main/main.cpp`: `M5.config()`, `M5.begin(config)`, rotação, touch e board enum.
- `M5Unified/examples/Basic/Touch/DragDrop/DragDrop.ino`: detalhes de touch, `waitDisplay()` e `display()`.
- `M5Unified/examples/Basic/HowToUse/HowToUse.ino`: semântica dos modos E-Ink e identificação `board_M5Paper`.
- `M5Unified/examples/Basic/LogOutput/LogOutput.ino`: `SD.begin(GPIO_NUM_4, SPI, 25000000)`.
- `UnzipLIB/src/unzipLIB.h`, `src/unzip.h` e `examples/unzip_sdcard/unzip_sdcard.ino`: callbacks, memória fixa e leitura incremental.

## Decisões derivadas

- O primeiro teste físico mostrou que rotação 1 resulta em 960×540. A configuração foi corrigida para rotação 0, que deve produzir 540×960; o resultado e o alinhamento do GT911 devem ser reconfirmados após o novo upload.
- SD usa CS 4 no objeto Arduino `SPI`; o display usa o mesmo host SPI com CS 15, confirmado na configuração original.
- Como o manifesto PlatformIO não existe nem na Espressif32 7.0.1, o manifesto local preserva o ESP32 original e o firmware configura explicitamente SCK 14/MISO 13/MOSI 12 antes de `SD.begin`.
- `epd_quality` é usado no diagnóstico inicial, `epd_fast` em regiões pequenas e `epd_text` fica reservado às páginas.
- `waitDisplay()` antecede troca de proprietário do barramento; não há acesso concorrente.
