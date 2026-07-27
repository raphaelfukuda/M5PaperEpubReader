# Desempenho

O firmware registra tempo de montagem do SD, instante de submissão do boot,
heap interno, PSRAM, flash e CPU. A leitura usa descompressão XHTML em blocos de
1 KiB, cache textual limitado a 96 KiB e checkpoints leves para reconstrução.

O buffer 1-bit full-screen tem custo teórico de `540 × 960 / 8 = 64.800` bytes, desconsiderando metadados e alinhamento. Medidas reais dependem do hardware.

Há uma medição física inicial de boot. Ainda não existem séries comparáveis para
abertura de EPUB, troca de página, reflow, reconstrução de cache ou autonomia.

## Build baseline

- Baseline repetido em 2026-07-26 com PlatformIO Core 6.1.19.
- `python -m pip install -r requirements-dev.txt`: sucesso; dependências já
  satisfeitas, sem alteração de versão.
- `pio test -e native`: 18/18 casos aprovados em 3,305 s com GCC/G++ 16.1.0
  WinLibs/MinGW64. A ativação do código de `src` no teste e includes C++
  explícitos foram necessários; o baseline também corrigiu uma fronteira de
  chunk do tokenizer e um fixture UTF-8 incorreto.
- `pio run -e m5stack-paper`: sucesso, sem warning do compilador.
- RAM estática reportada: 100.028 bytes (2,2% do limite do manifesto, que inclui PSRAM).
- Flash reportada: 2.291.341 bytes (35,0% da partição de aplicação de 6.553.600 bytes).
- Versões resolvidas: Espressif32 7.0.1, Arduino-ESP32
  3.20017.241212 (`2.0.17`), M5Unified 0.2.19, M5GFX 0.2.26,
  UnzipLIB 1.0.0 e Xtensa toolchain 8.4.0+2021r2-patch5.
- O build não mede heap/PSRAM em runtime; esses valores serão obtidos pelo log no aparelho.

## Primeira medição física

- Boot até a submissão da tela: 3.061 ms.
- Montagem do SD de 15.980.298.240 bytes: 114 ms.
- PSRAM total/livre após inicialização: 4.192.059/4.100.075 bytes.
- Heap interno livre/maior bloco: 212.348/110.580 bytes.
- A PSRAM detectada é aproximadamente 4 MB, divergindo da expectativa inicial de 8 MB e exigindo investigação antes de dimensionar caches.

## Métricas ainda obrigatórias

- mediana e pior caso de abertura para EPUB pequeno, médio e capítulo de 64 MiB;
- latência de página em cache, página nova, mudança de capítulo e reconstrução;
- duração do reflow nos cinco níveis de zoom;
- heap/maior bloco e PSRAM após 10, 100 e 500 páginas;
- contagem de refresh rápido/textual/qualidade e ocorrência de ghosting;
- consumo em leitura, ocioso e suspensão, além da autonomia real.

Não inferir autonomia ou estabilidade apenas a partir do uso estático de flash/RAM.

## Validação física de 2026-07-27

O firmware foi compilado, enviado e exercitado no M5Paper original. O usuário
validou navegação, retomada, sleep/wake, biblioteca e responsividade percebida.
A melhoria de refresh combina `epd_fast` na leitura normal, `epd_fastest` em
bursts, double buffering, submissão parcial e limpeza `epd_quality`. Após
ghosting visível, os limites foram reduzidos para 5 Fast, 2 Fastest, 6
atualizações de leitura ou 5 minutos.

O build final desta rodada usa 101.860 bytes de RAM estática (2,3%) e 2.359.125
bytes de flash (36,0%). A biblioteca extrai apenas metadados/capa das quatro
entradas visíveis e faz um único refresh de qualidade ao concluir. Qualidade e
autonomia ainda podem variar entre painéis, cartões e baterias.

## Cache incremental da biblioteca

As capas são convertidas uma vez para miniaturas 4-bit de até 204×323 pixels
(aproximadamente 33 KiB), gravadas atomicamente em `/.m5epub-cache` e validadas
por caminho, tamanho e data de modificação. Um LRU mantém até 20 miniaturas na
memória e é esvaziado ao abrir um livro. A preparação completa do cartão usa um
arquivo temporário como fila, evitando manter todos os caminhos em RAM, e faz
refresh de progresso a cada quatro segundos. O teste físico do usuário confirmou
carregamento, navegação por pastas e restauração da página anterior.

## Fase 1 — instrumentação de refresh

Concluída em 2026-07-26. O firmware agora separa reconhecimento do evento,
preparação da página, espera do refresh anterior, upload do sprite, comando ao
display, período de busy físico, trabalho de CPU durante busy e tempos até
submissão e ociosidade. As linhas começam com `M5EPUB_METRIC` e não contêm texto
livre, permitindo análise automática. Os cálculos de duração usam subtração
unsigned de `micros()`, inclusive através do overflow do contador.

O script `tools/analyze_refresh_metrics.py` agrupa por modo, origem da página e
região e calcula contagem, mínimo, média, mediana, P90, P95 e máximo. Exemplo:

```bash
python tools/analyze_refresh_metrics.py serial.log --format markdown -o refresh.md
```

Validação desta fase: 3/3 testes Python do analisador e 18/18 testes C++ nativos
aprovados. O firmware compilou sem warnings, usando 100.164 bytes de RAM estática
(2,2%) e 2.292.477 bytes de flash (35,0%). `canvas_render_us` permanecerá zero
nos caminhos que ainda não delimitam a renderização do canvas; isso será
preenchido quando as etapas de layout/render forem separadas. Não há medição
física nova nem conclusão de ganho de velocidade nesta fase.

## Fases 2–4 — política, ghosting e aceitação de input

Foram adicionados `RefreshPolicy` e `GhostingBudget` puros, com seleção Fast para
virada normal, Fastest para burst e Quality por limites/timeout. O mapeamento foi
integrado às APIs verificadas do M5GFX 0.2.26 e gera logs `M5EPUB_REFRESH`.

Uma fila limitada aceita ações do leitor durante `displayBusy()` ou prefetch. Ela
prioriza menu/anterior/fonte sobre avanço, acumula no máximo três avanços e
registra enfileiramento, coalescência e atraso até execução. O cancelamento
transacional do prefetch ainda pertence à próxima fase; até lá, a ação é mantida
e executada quando o prefetch atual alcançar seu término seguro.

Validação: 24/24 testes C++ nativos e 3/3 testes Python aprovados; firmware sem
warnings, 100.244 bytes de RAM estática (2,2%) e 2.295.041 bytes de flash (35,0%).
Não houve teste físico nesta etapa.

## Fases 5–7 — cancelamento, pipeline e trabalho durante busy

O prefetch passou a ter estados explícitos e cancelamento diferido em limite
seguro. Menu, página anterior e alteração de fonte solicitam cancelamento do
trabalho incompleto; uma página já pronta permanece em cache. A restauração fecha
a entrada ZIP, reabre o capítulo e usa o `PageAnchor` anterior ao prefetch, sem
reutilizar tokenizer ou layout parcial.

O caminho incremental agora alterna entre `readInputChunk()`,
`processBufferedInput()` e `continueLayout()`. O buffer é limitado a 1.024 bytes.
HTML e layout são executados após a liberação do `SpiBusGuard` e podem avançar
durante `displayBusy()` com orçamento de 2.000 us e no máximo duas transições por
tick. EOF/CRC e leitura do cartão continuam fora desse caminho CPU-only.

A UnzipLIB 1.0.0 executa inflate dentro de `readCurrentFile()`, acionado através
dos callbacks do arquivo no SD. Assim, leitura física e inflate ZIP não puderam
ser separados sem substituir a biblioteca; tokenizer e layout, que dominam o
trabalho posterior, já estão desacoplados. Essa limitação deve permanecer visível
nas métricas e na arquitetura.

Validação: 26/26 testes nativos aprovados e build embarcado sem warnings. Uso
estático: 101.300 bytes de RAM (2,2%) e 2.296.693 bytes de flash (35,0%). A
continuidade após cancelamento ainda requer teste físico antes do double buffering.

## Fases 8–9 — double buffering e submissão sem redraw

Os dois canvases existentes agora trocam papéis por ponteiros lógicos. Ao consumir
uma página prefetched, o leitor incrementa a página atual e submete diretamente o
back buffer pronto; não chama `layout_.processText()` e não copia o framebuffer.
O log `M5EPUB_BUFFER,swap=1,presentation_source=ready_back_buffer` identifica o
caminho rápido. Cache textual, reconstrução e single buffer permanecem como
fallbacks.

Validação de build: 26/26 testes nativos, sem warnings, 101.316 bytes de RAM
estática (2,2%) e 2.297.221 bytes de flash (35,1%). A melhora de latência e a
correção visual da alternância de canvases dependem do teste físico.

## Limpeza manual e geometria de regiões

O botão central (`M5.BtnB`, API confirmada nos exemplos instalados do M5Unified
0.2.19 para M5Paper) solicita refresh completo `epd_quality`. A solicitação é
coalescida e aguarda assincronamente o painel ficar livre; o loop não espera pelo
busy físico. Os logs `M5EPUB_REFRESH_INPUT` distinguem fila e submissão.

`DisplayRegion` foi criado como componente puro com clamp, união, interseção,
expansão, alinhamento horizontal e cálculo de área/razão protegidos contra
overflow. Ainda não altera a área enviada ao painel. Validação: 28/28 testes
nativos e build sem warnings; 101.324 bytes de RAM estática e 2.297.713 bytes de
flash (35,1%).

## Retomada de paginação

O estado persistido passou à versão 2 e inclui número absoluto da página e até 32
âncoras anteriores. Os textos dessas páginas não são persistidos; ao voltar, o
leitor os reconstrói pelo checkpoint, mantendo o arquivo pequeno e o uso de RAM
limitado. Estados versão 1 são migrados com fallback para página 1 porque não há
informação suficiente para recuperar o índice antigo.

Validação: round-trip e migração do codec cobertos na suíte de 28 testes nativos;
build sem warnings, 101.332 bytes de RAM estática (2,2%) e 2.303.833 bytes de
flash (35,2%). A navegação para trás após reboot depende de validação física.

## Fases 10–12 — região parcial e detecção de alterações

O `DisplayManager` agora compara os canvases monocromáticos front/back quando
ambos são válidos, calcula a bounding box dos pixels alterados, expande e alinha
a região e seleciona submissão parcial quando ela ocupa no máximo 85% da tela.
Atualizações de qualidade continuam obrigatoriamente em tela cheia. O envio usa
o clip do destino, `pushSprite()` e a sobrecarga regional de `display()`
confirmada no M5GFX 0.2.26; o alinhamento interno adicional permanece sob
responsabilidade do `Panel_IT8951`.

Os logs `M5EPUB_DIRTY` registram custo do diff, pixels alterados, bounding box
selecionada e razões de área. `M5EPUB_METRIC` passa a informar a região efetiva e
o número de pixels submetidos. O diff é omitido quando não existe framebuffer
anterior independente e válido, preservando o fallback de tela inteira.

Validação em ambiente de desenvolvimento: 30/30 testes nativos aprovados e build
embarcado sem warnings. Uso estático: 101.340 bytes de RAM (2,2%) e 2.306.033
bytes de flash (35,2%). A redução física de latência, a correção do recorte e o
impacto sobre ghosting ainda precisam ser validados no M5Paper original.

## Fases 13–16 — chrome, persistência, memória e segundo núcleo

As regiões alteradas agora são classificadas em corpo, cabeçalho, rodapé e
bateria (`dirty_flags` em `M5EPUB_DIRTY`). A bateria continua sendo amostrada
somente quando já existe uma mudança visual; pixels idênticos não ampliam a
região física selecionada pelo diff.

A persistência usa debounce cooperativo: 15 segundos de inatividade, cinco
páginas ou intervalo máximo de 60 segundos. Saída para biblioteca, sleep e
mudança de fonte mantêm persistência obrigatória. Escritas nunca começam durante
busy do display, prefetch ou ação pendente e geram `M5EPUB_PERSIST` com motivo,
duração, página e contador da sessão.

A política de canvas mede heap interno livre, maior bloco e PSRAM. O primeiro
teste físico mostrou que o front buffer em SRAM travou no primeiro envio ao
IT8951, apesar de haver espaço contíguo. Por isso o modo Auto mantém PSRAM como
default. O override interno só tenta SRAM quando o canvas cabe e ainda preserva
64 KiB de margem, exclusivamente para diagnóstico controlado:

```text
M5EPUB_CANVAS_MEMORY_INTERNAL=1
M5EPUB_CANVAS_MEMORY_PSRAM=1
```

O worker no segundo núcleo não foi ativado. As medições disponíveis comprovam
trabalho cooperativo durante busy, mas ainda não demonstram que parsing/layout é
o gargalo físico dominante. `M5EPUB_ENABLE_LAYOUT_WORKER` permanece 0 para evitar
ownership concorrente de canvas e estado do parser sem benefício medido.

Validação local final desta etapa: 33/33 testes nativos e build embarcado sem
warnings. RAM estática: 101.380 bytes (2,2%); flash: 2.307.861 bytes (35,2%).
Resultados de latência, estabilidade e ghosting continuam dependentes do teste
físico no M5Paper original.
