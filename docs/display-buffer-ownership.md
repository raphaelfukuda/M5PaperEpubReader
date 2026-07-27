# Ownership dos buffers de página

O leitor usa dois slots lógicos sem mover nem aplicar `std::swap` aos objetos
`M5Canvas`:

- `pageCanvas_`: página apresentada ou que será apresentada;
- `freeCanvas_`: buffer disponível para o prefetch seguinte.

Os ponteiros trocam de papel somente quando o prefetch está `Ready`. O canvas
pronto é então submetido diretamente por `DisplayManager::submitCanvas()`. O
texto não passa novamente por `layout_.processText()`, o ZIP não é reaberto e os
pixels não são copiados entre canvases. O chrome e a bateria são acrescentados ao
canvas pronto imediatamente antes da submissão.

Estados previstos: `Free`, `Rendering`, `Ready`, `Submitting` e `Displayed`. A
transferência por `pushSprite()` termina antes de o slot antigo voltar a ser usado;
o busy físico posterior pertence ao framebuffer do IT8951, não ao sprite fonte.

Menu, página anterior e reflow invalidam um back buffer pronto quando sua relação
com a página atual deixa de ser válida. O texto e o anchor continuam no cache e
permitem o fallback por redraw. Se o segundo sprite não puder ser alocado, o
firmware registra `M5EPUB_BUFFER,double=0` e continua em single buffer, sem
prefetch gráfico.
