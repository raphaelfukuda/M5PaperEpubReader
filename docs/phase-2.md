# Fase 2 — navegador de arquivos

## Entregas

- scan não recursivo do diretório atual em lotes de oito entradas;
- início em `/Books` quando existe, senão `/`;
- filtro de ocultos e extensões EPUB sem diferença de caixa;
- diretórios antes de EPUBs, ordenados alfabeticamente;
- paginação, swipe e toque direto;
- retorno ao diretório pai;
- truncamento apenas visual de nomes;
- seleção de EPUB exibida na tela e no serial;
- exclusão cooperativa entre SD e display.

## Limitações

- no máximo 256 entradas relevantes são retidas; a UI avisa quando truncada;
- o caminho selecionado ainda não é aberto, pois ZIP/OPF pertencem à Fase 3;
- remoção/hot-plug do SD durante o scan ainda não possui recuperação completa;
- acentos são preservados em UTF-8, mas a fonte baseline deve ser validada visualmente;
- o feedback de linha é um contorno parcial rápido; abrir diretório provoca depois refresh completo.

## Build

- RAM estática: 51.688 bytes;
- flash: 1.113.073 bytes;
- teste físico inicial: SD montado, raiz escaneada com uma entrada.

## Testes

- quatro suítes para extensão EPUB, arquivos ocultos, join/parent/fileName e comparação case-insensitive foram adicionadas em `test/test_native`;
- o PlatformIO descobriu a suíte, mas não pôde compilá-la porque este Windows não possui `gcc/g++` no PATH;
- o firmware alvo compilou normalmente com o toolchain Xtensa;
- a execução dos testes nativos permanece pendente até instalar um compilador host, sem mascarar o resultado como sucesso.

## Próximo passo

Fase 3: avaliar biblioteca ZIP incremental, gerar fixtures e interpretar `container.xml` e OPF.
