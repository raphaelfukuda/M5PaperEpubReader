# Regras do repositório

- O alvo é exclusivamente o M5Stack M5Paper original (`board_M5Paper`, ESP32), nunca PaperS3.
- Não adicionar M5EPD, LVGL ou serviços de rede permanentes. O portal HTTP
  opcional existente é a única exceção: deve permanecer desligado por padrão,
  cooperativo e protegido por `M5EPUB_ENABLE_WEB_PORTAL`.
- Manter `references/` fora deste projeto e somente para leitura.
- Confirmar APIs nas referências oficiais antes de usá-las.
- Preservar o loop cooperativo e serializar SD/display no mesmo barramento SPI.
- Nomes de código e comentários em inglês. Documentação pública prioriza inglês
  e pode incluir português do Brasil. Toda UI deve existir nos dois idiomas,
  com inglês como padrão do dispositivo.
- Cada fase deve encerrar com build, testes, limitações, métricas e próximo passo documentados.
