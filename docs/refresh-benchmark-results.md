# Benchmark de refresh

## Metodologia

Capture a saída serial a 115200 baud durante sequências identificadas de leitura.
Cada amostra estruturada começa com `M5EPUB_METRIC`. Analise o arquivo com:

```bash
python tools/analyze_refresh_metrics.py serial.log --format markdown -o summary.md
python tools/analyze_refresh_metrics.py serial.log --format csv -o summary.csv
```

Os grupos são formados por `mode`, `kind` e `region`. Compare mediana, P95 e
máximo; a média isolada não é critério de aprovação.

## Campos

- `event_to_handling_us`: reconhecimento até início do tratamento.
- `event_to_page_ready_us`: reconhecimento até página pronta em memória.
- `canvas_render_us`: renderização isolada, quando delimitada pelo caminho.
- `display_wait_us`: espera por uma atualização anterior.
- `sprite_upload_us`: transferência do canvas ao framebuffer do display.
- `display_command_us`: duração da chamada que inicia o refresh.
- `panel_busy_us`: busy físico observado após o comando.
- `cpu_work_during_busy_us`: trabalho útil de CPU contabilizado durante busy.
- `event_to_submit_us` e `event_to_idle_us`: latência até submissão e até painel livre.
- `updated_pixels`, `total_pixels` e `total_us`: área e duração total.

## Resultados físicos

Ainda não coletados. As tabelas abaixo devem ser preenchidas somente com logs do
M5Paper original e acompanhadas da avaliação visual de ghosting.

| Cenário | Modo | Região | Amostras | Mediana (us) | P95 (us) | Ghosting 0–5 |
|---|---|---|---:|---:|---:|---:|
| Página em cache | pendente | pendente | 0 | — | — | — |
| Página prefetched | pendente | pendente | 0 | — | — | — |
| Página gerada | pendente | pendente | 0 | — | — | — |
| Menu abrir/fechar | pendente | pendente | 0 | — | — | — |
# Estado da implementação

Arquitetura, instrumentação, refresh adaptativo, double buffering, regiões,
dirty diff, persistência com debounce e política de memória estão implementados.
Nenhum valor de melhoria física é declarado sem coleta no M5Paper original. O
worker de segundo núcleo foi avaliado e mantido desativado por falta de evidência
de gargalo de CPU após a sobreposição cooperativa.
