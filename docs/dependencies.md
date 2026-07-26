# Dependências

## Primeira resolução

Conforme o requisito, a primeira compilação foi tentada sem versões explícitas. Ela reutilizou a plataforma global Espressif32 3.3.2 e falhou com `UnknownBoard: Unknown board ID 'm5stack-paper'` antes da compilação.

O Registry do PlatformIO informou `platformio/espressif32` 7.0.1 como versão atual em 24/07/2026. Essa versão foi então fixada para preservar o identificador obrigatório da placa, sem configuração manual:

- plataforma `platformio/espressif32@7.0.1`;
- placa `m5stack-paper`;
- framework Arduino;
- `m5stack/M5Unified`;
- `m5stack/M5GFX`.

O PlatformIO Core é 6.1.19. A plataforma global antiga permanece intacta.

Mesmo a plataforma 7.0.1 não contém `m5stack-paper`. Após essa segunda falha documentada, foi criado `boards/m5stack-paper.json`, derivado dos manifestos locais oficiais do PlatformIO para ESP32/M5Stack: MCU `esp32`, 240 MHz, flash DIO de 16 MB, `BOARD_HAS_PSRAM`, partição `default_16MB.csv` e macro `ARDUINO_M5STACK_PAPER`. Não houve substituição por outra placa. O variant genérico é usado apenas para core/toolchain; o firmware configura explicitamente o SPI compartilhado do M5Paper em SCK 14, MISO 13 e MOSI 12, conforme M5GFX.

## Combinação funcional fixada

| Componente | Versão resolvida |
|---|---|
| PlatformIO Core | 6.1.19 |
| platformio/espressif32 | 7.0.1 |
| Arduino-ESP32 | 2.0.17 (`3.20017.241212`, commit `dcc1105b`) |
| M5Unified | 0.2.19 |
| M5GFX | 0.2.26 |
| toolchain Xtensa ESP32 | 8.4.0+2021r2-patch5 |
| esptool.py | 4.11.0 |

M5Unified e M5GFX foram fixadas após o primeiro build bem-sucedido. A compilação limpa subsequente é o baseline reproduzível desta fase.

Nenhuma biblioteca ZIP foi selecionada; essa análise pertence à Fase 3.

## ZIP — Fase 3

Foi fixada `bitbank2/UnzipLIB@1.0.0`, commit de referência `594a95216d70cb16404c767922cd8aa55e382f66`.

- licença Apache-2.0; núcleo histórico zlib-compatible;
- API sem exceções C++;
- estrutura fixa documentada de aproximadamente 41 KB, confirmada pelo aumento de RAM estática no build;
- callbacks `open/read/seek/close` compatíveis com `fs::File` no SD;
- leitura incremental da entrada por blocos de 1 KB;
- métodos Stored e Deflate aceitos; outros são rejeitados;
- não carrega o arquivo EPUB inteiro;
- callbacks são globais na biblioteca, portanto o projeto permite apenas um arquivo EPUB aberto e mantém acesso single-threaded.

Nesta fase, `container.xml` é limitado a 64 KB e OPF a 512 KB. Capítulos não são carregados.
