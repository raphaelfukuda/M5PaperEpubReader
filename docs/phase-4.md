# Fase 4 — primeira página textual

## Pipeline

1. Selecionar o primeiro item linear do spine.
2. Abrir a entrada XHTML sem carregar o capítulo.
3. Ler/descompactar até 1 KB por iteração.
4. Preservar estado de tags e entidades entre blocos.
5. Gerar texto e quebras estruturais.
6. Medir palavras com `textWidth()` e desenhar no canvas 1-bit.
7. Interromper quando a página estiver cheia e submeter com `epd_text`.

## Build

- RAM estática: 93.704 bytes;
- flash: 1.476.597 bytes;
- aumento principal de flash: fonte Unicode `efontCN_16`;
- capítulo limitado a tamanho plausível de 64 MB, mas nunca alocado integralmente.

## Limitações

- somente a primeira página do primeiro capítulo linear;
- negrito/itálico ainda não trocam a fonte;
- estilos CSS são ignorados;
- âncora exata e continuação da página pertencem à Fase 5;
- cobertura de português e aparência devem ser validadas no display físico.

## Interação adicionada

- toque na faixa superior de 110 px: voltar à biblioteca (futuro menu);
- toque na metade esquerda restante: página anterior;
- toque na metade direita restante: próxima página;
- swipe direita/esquerda: anterior/próxima;
- stream XHTML permanece aberto entre páginas;
- até 96 KB de texto de páginas visitadas são mantidos para retorno rápido, com descarte das mais antigas.
- zoom do texto em cinco níveis: 16, 24, 32, 36 e 40 px.
- espaços não separáveis (`U+00A0`/`&nbsp;`) são convertidos em oportunidades
  de quebra no texto corrido para evitar cortes arbitrários com zoom alto.
- whitespace usado apenas para formatar o XHTML é condensado; somente tags de
  bloco geram quebras estruturais no texto apresentado.
