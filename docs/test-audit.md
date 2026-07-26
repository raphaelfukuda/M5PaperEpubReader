# Auditoria de testes sem hardware

## Cobertura adicionada

O ambiente `native` agora inclui os componentes puros `HtmlTokenizer` e
`HtmlEntityDecoder`, além de `PathUtils`. A suíte cobre:

- entidades latinas usadas em português, entidades numéricas decimais e
  hexadecimais;
- preservação de entidades desconhecidas ou inválidas;
- remoção de `head`, normalização do espaço do código-fonte XHTML e manutenção
  de quebras estruturais;
- conversão de `&nbsp;` em oportunidade de quebra de linha;
- entidades, tags e sequências UTF-8 divididas entre blocos de entrada;
- ausência de espaços e quebras duplicadas nas fronteiras dos blocos;
- valores padrão e capacidade de representar offsets acima de 4 GiB em
  `PageAnchor`.

O caso de fronteira entre blocos revelou que o tokenizer não mantinha a última
separação emitida. O estado mínimo dessa fronteira passou a ser preservado, de
forma que o resultado não dependa do tamanho do bloco usado para ler o XHTML.

## Como executar

```text
pio test -e native
pio run -e m5stack-paper
```

Nesta estação, o primeiro comando está bloqueado pela ausência de `gcc` e
`g++`. O compilador Xtensa aceitou os componentes alterados durante o build do
firmware; o build completo, porém, deve ser executado sem outro processo
PlatformIO concorrente, pois todos compartilham `.pio/build`.

## Riscos e lacunas priorizados

1. **Paginação não é testável sem M5GFX.** `TextLayoutEngine` depende diretamente
   de `M5Canvas`. Extrair a decisão de quebra para um componente puro, recebendo
   uma função de medição de texto, permitiria testar perda, repetição e progresso
   monotônico sem display.
2. **A âncora ainda não tem semântica validável isoladamente.** O teste atual
   garante apenas armazenamento e largura dos campos. Faltam invariantes entre
   `uncompressedOffset`, texto normalizado e `parserCheckpoint`, inclusive após
   zoom e mudança de capítulo.
3. **Elementos XHTML ocultos não usam profundidade.** O tokenizer mantém apenas
   um booleano para `head`, `script` e `style`; marcação malformada ou aninhada
   pode reativar texto antes do fechamento externo.
4. **Entidades Unicode suplementares não são decodificadas.** Valores acima de
   U+FFFF permanecem literais. Isso evita UTF-8 inválido, mas não oferece suporte
   completo a EPUBs com caracteres suplementares.
5. **Palavras maiores que a largura útil são cortadas sem hífen.** É uma escolha
   atual de robustez do layout; precisa de testes com medição desacoplada e de
   uma política tipográfica explícita.
6. **Faltam testes de propriedades.** O tokenizer deve produzir o mesmo texto
   para qualquer particionamento dos mesmos bytes. Uma futura suíte deve variar
   automaticamente todas as posições de corte, incluindo UTF-8, tags e
   entidades.

