# Fase 3 — estrutura EPUB

## Entregas

- integração UnzipLIB auditada;
- `EpubArchive`, `EpubParser`, modelos de book/manifest/spine;
- tokenizer XML e decoder básico de entidades;
- proteção de caminhos ZIP e limites de entrada;
- tela de informações do livro e tela de erro;
- gerador de EPUB sintético e seis fixtures inválidas.

## Build

- RAM estática: 93.472 bytes (2,1% do manifesto com PSRAM);
- flash: 1.152.113 bytes (17,6% da partição);
- incremento de RAM coerente com os ~41 KB fixos da UnzipLIB.

## Limitações

- metadata é processada em passos cooperativos, mas cada descompressão de `container.xml`/OPF mantém o SPI até concluir;
- somente um arquivo ZIP pode estar aberto devido ao modelo de callback global;
- XML suporta o subconjunto necessário ao container/OPF, não DTD geral;
- validação física com um EPUB ainda é necessária.
