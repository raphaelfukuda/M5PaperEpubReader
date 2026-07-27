# Arquitetura

`AppController` possui a máquina de estados e executa um loop cooperativo: `M5.update()`, coleta de evento, pequeno trabalho pendente, renderização suja e `yield()`. Não há tasks auxiliares nem `delay()` no fluxo normal.

## Responsabilidades

- `hal/DisplayManager`: canvas monocromático em PSRAM, intenções de refresh,
  indicador de bateria amostrado durante a submissão e envio E-Ink.
- `hal/TouchController`: converte estado do M5Unified em eventos básicos.
- `hal/SdCardService`: montagem do SD e métricas.
- `hal/SpiBusGuard`: propriedade lógica exclusiva entre SD e display.
- `hal/PowerManager`: mantém Bluetooth desligado, controla a exceção temporária
  do rádio Wi-Fi e configura light sleep com wake-up pelos GPIOs 37/39 da
  alavanca e GPIO 36 do touch.
- `net/WifiService`: scan e associação assíncronos, redes conhecidas em um blob
  NVS e desligamento explícito do rádio.
- `net/WebPortalService`: `WebServer` síncrono, API REST e uploads transacionais
  `.part`; callbacks rodam somente por `handleClient()` no loop principal.
- `net/PortalPath`: normalização de caminhos e nomes FAT testável no ambiente
  nativo, sem dependência de Arduino.
- `storage/LibraryThumbnailCache`: catálogo persistente e miniaturas 4-bit
  validadas por caminho, tamanho e data do EPUB.
- `diagnostics`: logs e estruturas de métricas.
- `ui`: contrato de views; telas funcionais começam na Fase 2.

O display e o SD são usados somente pela task Arduino principal. Antes do SD, o display é aguardado com `waitDisplay()`. Se futuramente houver worker, ele não possuirá SD, display ou touch.

## Portal local de upload

O portal é compilado apenas com `M5EPUB_ENABLE_WEB_PORTAL=1`. O rádio inicia
desligado. Ao ativar o toggle da biblioteca, o scan e a associação avançam por
`poll()`. O `WebServer` síncrono é atendido no mesmo loop; seus hooks aguardam o
display e tomam `SpiBusGuard` como `SdCard`. Uploads mantêm o guard entre
`UPLOAD_FILE_START` e o encerramento, escrevem em `.part` e renomeiam somente
após tamanho e escrita completos. A UI nunca desenha dentro de `onActivity`.
Enquanto o portal roda, o sleep automático fica inibido; oito minutos sem
requisição encerram HTTP, mDNS, associação e rádio.

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
Uma capa ampliada é submetida com refresh completo antes de o painel dormir.
Touch ou qualquer extremo da alavanca acorda o ESP32. O painel sai do power-save,
executa outro refresh completo e a página é redesenhada diretamente do estado
mantido em RAM, sem aplicar o
acionamento de wake como zoom. A posição persistida no microSD continua sendo a
proteção contra perda de energia durante a espera.

O ESP32 original não oferece wake EXT1 em “qualquer GPIO baixo” durante deep
sleep. Como os dois extremos da alavanca são ativos em nível baixo, o firmware
usa light sleep para aceitar separadamente GPIO 37 ou GPIO 39. Deep sleep só
permitiria um único extremo via EXT0 ou exigiria ambos simultaneamente via
EXT1 `ALL_LOW`.

## Biblioteca e cache de capas

A biblioteca carrega primeiro a página visual com `Carregando...` e só então
processa os livros visíveis. Miniaturas proporcionais de 204×323 no máximo usam
4 bits por pixel. Até 20 permanecem em um LRU na memória; o LRU é esvaziado ao
abrir um livro. O cache persistente é incremental e a preparação completa do
cartão percorre pastas cooperativamente, ignora entradas válidas e atualiza a
barra de progresso a cada quatro segundos. O prefetch da página seguinte não
provoca refresh isolado.

## Renderização

Páginas textuais usam canvases front/back de 1 bit em PSRAM. Capas e menus com
imagem usam um canvas de processamento de 8 bits, quantizado para os 16 níveis
reais do painel. A política escolhe `epd_fast`/`epd_fastest`, regiões parciais e
limpezas `epd_quality` conforme ritmo e orçamento de ghosting.
