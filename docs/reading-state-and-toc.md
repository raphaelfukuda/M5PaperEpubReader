# Persistência de leitura e sumário

Esta base é independente da interface e dos controladores. Ela não realiza trabalho em segundo plano nem acessa rede, display ou touch.

## Retomada

`storage/ReadingState` registra o caminho do EPUB, índice no spine, âncora textual de 64 bits, checkpoint do parser e preferências essenciais de layout. `ReadingStateCodec` converte o registro para um formato textual versionado, limitado a 4 KiB na leitura. Caminhos usam escape percentual, portanto caracteres UTF-8, `=` e quebras de linha não corrompem o arquivo.

`ReadingStateStore`, construído com um `fs::FS`, oferece:

- `save(path, state)`: grava em `.tmp`, preserva a versão anterior em `.bak`, promove o temporário e remove o backup;
- `load(path, state)`: lê e valida integralmente antes de substituir o estado fornecido;
- `error()`: motivo em português para log ou UI.

A integração deve salvar após uma troca estável de página/zoom, nunca a cada frame. Na retomada, deve validar se `spineIndex` ainda existe no livro e reconstruir a página a partir de `textOffset`; `parserCheckpoint` é uma otimização, não a identidade definitiva da posição.

Limitações atuais: há um estado por arquivo indicado pelo chamador; não há índice de biblioteca, timestamp, hash do EPUB, migração de versões antigas ou sincronização. A sequência temporário/backup protege contra a maioria das interrupções no FAT, mas o cartão SD não oferece garantia transacional de sistema de arquivos.

## Sumário EPUB 2 e EPUB 3

`epub/EpubTableOfContents` é uma API em duas etapas:

1. `discover(opfXml, packagePath, document, error)` procura primeiro um item de manifest com `properties="nav"` (EPUB 3), depois o NCX indicado por `spine toc` ou pelo media type EPUB 2.
2. O chamador lê `document.path` do `EpubArchive` e passa o conteúdo a `parse(xml, document, entries, error, maximumEntries)`.

Cada `EpubTocEntry` contém título normalizado, caminho interno resolvido com segurança, fragmento e profundidade hierárquica. O limite padrão é 1.024 entradas.

O parser EPUB 3 aceita `epub:type="toc"` (o tokenizer compara o nome local `type`) e `role="doc-toc"`. O parser EPUB 2 lê `navPoint`, `navLabel/text` e `content src`. Entidades HTML/XML suportadas pelo decodificador existente são convertidas nos títulos.

Limitações atuais: a API não lê o ZIP por conta própria; não associa destinos ao índice do spine; ignora `page-list`, `landmarks`, `playOrder`, elementos sem link e estilos CSS. XML com entidades definidas em DTD e marcação XHTML gravemente inválida continua fora do escopo.

## Navegação integrada

O menu pagina as entradas do sumário dentro de limites fixos e mapeia cada
`documentPath` pelo manifest até o item correspondente do spine. A seleção
reconstrói a leitura desde o início desse documento. O fragmento do destino é
preservado pelo parser, mas ainda não posiciona exatamente dentro do capítulo.
