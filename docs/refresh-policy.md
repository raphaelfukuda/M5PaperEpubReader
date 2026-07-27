# Política de refresh

O `RefreshPolicy` é independente de Arduino, M5GFX, display, SD e relógio. O
chamador fornece motivo, área alterada, ritmo de navegação e timestamp. Isso
permite validar a decisão no ambiente nativo.

## Perfis e mapeamento confirmado

Na versão fixada M5GFX 0.2.26 e no `Panel_IT8951`:

| Perfil | M5GFX | Uso inicial |
|---|---|---|
| Quality | `epd_quality` | boot, wake e limpeza |
| Text | `epd_text` | abertura, reflow e TOC |
| Fast | `epd_fast` | virada normal, anterior e menus |
| Fastest | `epd_fastest` | burst de viradas |

O modo adaptativo escolhe `Fastest` a partir da segunda virada dentro de 900 ms.
Uma página anterior isolada permanece em `Fast`. Os defaults configuráveis estão
em `AppConfig.h` e têm equivalentes na configuração pura usada pelos testes.

## Orçamento de ghosting

O orçamento global conta refreshes Fast, Fastest, parciais e de leitura. Uma
limpeza Quality é forçada ao atingir 2 Fastest, 5 Fast, 6 refreshes de leitura
ou 5 minutos desde a última limpeza. Esses limites foram reduzidos depois de
ghosting visível no teste físico. Os contadores usam incremento saturado e
são zerados após Quality; o histórico de limpezas é preservado.

Cada submissão gera uma linha `M5EPUB_REFRESH` com modo solicitado, modo efetivo,
motivo, causa da limpeza e contadores. Os thresholds são conservadores e precisam
de validação visual no M5Paper original.

Entrar em sleep, sair de sleep e pressionar o centro da alavanca sempre força
Quality em tela inteira. Essas operações também reiniciam o orçamento global.

## Fallback e limitações atuais

`Adaptive` nunca é enviado diretamente ao painel. Caso apareça sem decisão, o
fallback é `epd_text`. A política por tiles foi deliberadamente adiada porque
ainda não existe refresh parcial consolidado. A abertura inicial do livro ainda
precisa receber um motivo explícito no caminho de apresentação; não se deve
inferir sua qualidade visual apenas pelo build.
