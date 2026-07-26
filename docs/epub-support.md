# Suporte EPUB

## Implementado na Fase 3

- ZIP lido diretamente do SD por callbacks, Stored e Deflate;
- `META-INF/container.xml` e descoberta segura do OPF;
- tokenizer XML sem regex e tolerância a prefixos de namespace;
- título, autor, idioma, manifest e spine;
- resolução de caminhos relativos com bloqueio de traversal;
- validação de spine vazio e referências ausentes;
- limites de tamanho, manifest e spine;
- retorno à biblioteca após erro sem reiniciar.

Naquele marco ainda faltavam XHTML e layout; ambos foram implementados nas fases
seguintes. DRM, JavaScript, mídia, MathML, layout fixo e fontes incorporadas
continuam fora do escopo.

## Fase 4

- primeiro item linear do spine aberto incrementalmente;
- descompressão de XHTML em blocos de 1 KB;
- extração textual incremental de elementos de bloco, listas e entidades;
- `script`, `style` e `head` ignorados;
- layout por métricas reais do M5GFX com `efontCN_16`;
- palavras maiores que a linha divididas em fronteiras UTF-8;
- primeira página renderizada em canvas 1-bit com `epd_text`.

Paginação seguinte/anterior, mudança de capítulo, cache limitado e reconstrução
por checkpoint foram implementados na Fase 5. A persistência versionada foi
integrada depois desse marco.

## Base para imagens e CSS

- referências raster em `img`, `image` e `object` podem ser descobertas sem
  descompactar ou renderizar a imagem;
- somente caminhos internos seguros que correspondam ao manifest são aceitos;
- referências externas, absolutas, com traversal ou sem tipo raster conhecido
  são ignoradas;
- quantidade de referências é limitada pelo chamador antes de qualquer futura
  alocação de imagem;
- JPEG, PNG, GIF, BMP e WebP são classificados como raster para descoberta.
  Essa classificação não afirma que o decoder ou a renderização já existam;
- o parser CSS básico aceita apenas declarações isoladas de `text-align`,
  `font-weight`, `font-style`, `text-indent` em pixels e `line-height` em
  porcentagem, com limites numéricos conservadores;
- seletores, cascata, folhas externas, cores, fundos, fontes incorporadas e
  URLs CSS permanecem deliberadamente sem suporte.

Os componentes apenas produzem metadados. Eles ainda não estão conectados ao
layout ou ao display e não criam canvas adicional.

## Sumário, links e notas

Há parsers puros para NCX do EPUB 2, documento `nav` do EPUB 3, links internos,
fragmentos, referências/backlinks e notas semânticas. Caminhos relativos são
resolvidos dentro do ZIP e limites evitam crescimento irrestrito. O parser EPUB
carrega o sumário descoberto, mas seleção de capítulo, abertura de links e exibição
de notas ainda não estão disponíveis no menu.
