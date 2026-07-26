# Fase 5 — âncora de leitura e reflow

## Entrega inicial

- cada página em cache registra o item do spine e o deslocamento textual inicial;
- a mudança de zoom preserva a âncora da primeira linha visível;
- páginas posteriores e suas âncoras são invalidadas e repaginadas juntas;
- descarte do cache mantém textos e âncoras sincronizados;
- mudança de capítulo inicia uma nova âncora no item correto do spine.
- o limite do cache descarta somente o texto; checkpoints leves continuam disponíveis;
- ao voltar a uma página descartada, o capítulo é reaberto e avançado de forma
  cooperativa até o checkpoint, sem bloquear o loop principal;
- a continuação posterior à página reconstruída é invalidada e repaginada no
  tamanho de fonte atual.

## Limitações

- a âncora usa deslocamento no texto normalizado, não byte bruto do XHTML;
- reconstruir uma página distante exige reler o XHTML desde o início do capítulo;
- persistência da posição após reinicialização pertence à Fase 6.

## Validação física

Alterar o zoom em uma página textual deve manter a primeira frase visível como
início da página repaginada. Em seguida, avançar deve continuar sem repetição ou
perda de texto.
