# Arquitetura

`AppController` possui a máquina de estados e executa um loop cooperativo: `M5.update()`, coleta de evento, pequeno trabalho pendente, renderização suja e `yield()`. Não há tasks auxiliares nem `delay()` no fluxo normal.

## Responsabilidades

- `hal/DisplayManager`: canvas monocromático em PSRAM, intenções de refresh,
  indicador de bateria amostrado durante a submissão e envio E-Ink.
- `hal/TouchController`: converte estado do M5Unified em eventos básicos.
- `hal/SdCardService`: montagem do SD e métricas.
- `hal/SpiBusGuard`: propriedade lógica exclusiva entre SD e display.
- `hal/PowerManager`: mantém Wi-Fi e Bluetooth desligados e configura light
  sleep com wake-up pelos GPIOs 37/39 da alavanca.
- `diagnostics`: logs e estruturas de métricas.
- `ui`: contrato de views; telas funcionais começam na Fase 2.

O display e o SD são usados somente pela task Arduino principal. Antes do SD, o display é aguardado com `waitDisplay()`. Se futuramente houver worker, ele não possuirá SD, display ou touch.

## Agendamento e persistência

Input e submissão têm prioridade sobre prefetch e persistência. O estado de
leitura sujo é agregado e salvo depois de inatividade, limiar de páginas ou
intervalo máximo; transições críticas forçam a gravação assim que display e SPI
estão seguros. O worker de segundo núcleo permanece desativado porque as métricas
atuais não justificam a complexidade adicional.

## Memória dos canvases

Front e back buffers usam seleção Auto conservadora em PSRAM. O experimento
físico com front em SRAM travou no primeiro envio ao IT8951; portanto SRAM fica
restrita ao override de diagnóstico, que ainda exige maior bloco suficiente e
margem de 64 KiB. A seleção é registrada por `M5EPUB_MEMORY`.

## Espera de baixo consumo

Após dez minutos sem toque ou alavanca, ou por solicitação no menu, o estado de
leitura é gravado atomicamente no microSD e o caminho do livro é marcado em NVS.
O aviso de espera é submetido e concluído antes de o painel entrar em power-save.
Qualquer extremo da alavanca acorda o ESP32. O painel sai do power-save e a
página é redesenhada diretamente do estado mantido em RAM, sem aplicar o
acionamento de wake como zoom. A posição persistida no microSD continua sendo a
proteção contra perda de energia durante a espera.

O ESP32 original não oferece wake EXT1 em “qualquer GPIO baixo” durante deep
sleep. Como os dois extremos da alavanca são ativos em nível baixo, o firmware
usa light sleep para aceitar separadamente GPIO 37 ou GPIO 39. Deep sleep só
permitiria um único extremo via EXT0 ou exigiria ambos simultaneamente via
EXT1 `ALL_LOW`.

## Renderização

O smoke test usa um canvas full-screen de 1 bit: aproximadamente 64.800 bytes para 540×960, alocado preferencialmente em PSRAM e reutilizado. Feedback de toque submete apenas a região do marcador e usa modo `epd_fast`; a tela inicial usa `epd_quality`. Páginas textuais usarão `epd_text`.
