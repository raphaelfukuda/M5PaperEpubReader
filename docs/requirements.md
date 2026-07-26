# Requisitos e rastreabilidade

O produto final será um leitor textual EPUB 2/3 incremental para M5Paper original. As prioridades são responsividade percebida, memória estável, atualizações E-Ink deliberadas e acesso serializado ao SPI compartilhado.

## Escopo desta entrega

| Requisito | Fase | Estado |
|---|---:|---|
| PlatformIO `m5stack-paper`, Arduino | 0 | Implementado; build deve validar disponibilidade |
| M5Unified/M5GFX sem M5EPD | 0 | Implementado |
| Máquina de estados explícita | 0 | Estrutura implementada |
| Detecção estrita do M5Paper original | 1 | Implementada e validada no aparelho |
| Display 540×960 em retrato | 1 | Implementado e validado |
| Touch GT911 | 1 | Implementado; regressão física final pendente |
| PSRAM e diagnóstico de memória | 1 | Implementado |
| microSD no SPI compartilhado | 1 | Implementado em fluxo cooperativo |
| Navegador incremental do microSD | 2 | Implementado |
| Container, OPF, manifest e spine seguros | 3 | Implementado |
| Texto XHTML e entidades portuguesas | 4 | Implementado |
| Paginação contínua e cache limitado | 5 | Implementado |
| Zoom com preservação da âncora | 5 | Implementado e validado no aparelho |
| Persistência e retomada | 6 | Implementado; teste de corte de energia pendente |
| Menu e progresso aproximado | 7 | Implementado; validação física pendente |
| Sumário EPUB 2/3 | 8 | Parser e navegação por capítulo integrados; fragmentos exatos pendentes |
| UI inglês/português | transversal | Implementada; inglês padrão e seleção persistida em NVS |
| Links e notas de rodapé | 10 | Indexação pura implementada; UI pendente |
| Imagens raster e CSS básico | 9–10 | Descoberta/parsing isolados; renderização pendente |
| Atualização E-Ink deliberada | transversal | Modos por intenção implementados; política adaptativa pendente |
| Recuperação de EPUB/SD inválido | transversal | Limites e erros principais implementados; remoção em uso pendente |

“Implementado” descreve o código integrado. Ensaios que exigem observação do
painel, touch, consumo ou interrupção de energia permanecem identificados como
pendentes até registro no hardware.
