# Arquitetura

`AppController` possui a máquina de estados e executa um loop cooperativo: `M5.update()`, coleta de evento, pequeno trabalho pendente, renderização suja e `yield()`. Não há tasks auxiliares nem `delay()` no fluxo normal.

## Responsabilidades

- `hal/DisplayManager`: canvas monocromático em PSRAM, intenções de refresh e submissão E-Ink.
- `hal/TouchController`: converte estado do M5Unified em eventos básicos.
- `hal/SdCardService`: montagem do SD e métricas.
- `hal/SpiBusGuard`: propriedade lógica exclusiva entre SD e display.
- `hal/PowerManager`: mantém Wi-Fi e Bluetooth desligados.
- `diagnostics`: logs e estruturas de métricas.
- `ui`: contrato de views; telas funcionais começam na Fase 2.

O display e o SD são usados somente pela task Arduino principal. Antes do SD, o display é aguardado com `waitDisplay()`. Se futuramente houver worker, ele não possuirá SD, display ou touch.

## Renderização

O smoke test usa um canvas full-screen de 1 bit: aproximadamente 64.800 bytes para 540×960, alocado preferencialmente em PSRAM e reutilizado. Feedback de toque submete apenas a região do marcador e usa modo `epd_fast`; a tela inicial usa `epd_quality`. Páginas textuais usarão `epd_text`.

