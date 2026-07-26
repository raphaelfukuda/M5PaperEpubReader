# Desempenho

O firmware registra tempo de montagem do SD, instante de submissão do boot,
heap interno, PSRAM, flash e CPU. A leitura usa descompressão XHTML em blocos de
1 KiB, cache textual limitado a 96 KiB e checkpoints leves para reconstrução.

O buffer 1-bit full-screen tem custo teórico de `540 × 960 / 8 = 64.800` bytes, desconsiderando metadados e alinhamento. Medidas reais dependem do hardware.

Há uma medição física inicial de boot. Ainda não existem séries comparáveis para
abertura de EPUB, troca de página, reflow, reconstrução de cache ou autonomia.

## Build baseline

- RAM estática reportada: 51.472 bytes (1,1% do limite do manifesto, que inclui PSRAM).
- Flash reportada: 1.101.097 bytes (16,8% da partição de aplicação de 6.553.600 bytes).
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
