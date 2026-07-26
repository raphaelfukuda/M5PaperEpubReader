# Links internos e notas de rodapé

`epub/EpubLinks` indexa um documento XHTML já extraído do EPUB sem acessar SD, display ou rede. A saída contém fragmentos (`id`), links e notas de rodapé, sempre associada ao caminho interno do documento.

`EpubLinkParser::resolveInternal` separa caminho e fragmento, resolve caminhos relativos com `PathUtils` e rejeita travessia, caminhos absolutos e esquemas URI. Links externos são apenas classificados e preservados como texto; este componente nunca os abre.

O parser reconhece:

- links XHTML comuns;
- referências de nota por `epub:type="noteref"` ou `role="doc-noteref"`;
- backlinks por `epub:type="backlink"` ou `role="doc-backlink"`;
- notas por `epub:type="footnote"` ou `role="doc-footnote"` em elementos com `id`;
- destinos de fragmento definidos por qualquer atributo `id`.

Os limites padrão são 512 links, 1.024 fragmentos, 128 notas, 512 bytes por rótulo, 2 KiB por referência e 16 KiB por texto de nota. Exceder um limite encerra o parsing com erro, evitando crescimento de memória não controlado.

Limitações: links externos não são navegáveis; não há resolução automática para spine/página; notas sem marcação semântica não são inferidas; o texto é plano e não preserva estilo ou imagens. A integração futura deve carregar o documento de destino, localizar seu spine e reconstruir a página até o fragmento escolhido.
