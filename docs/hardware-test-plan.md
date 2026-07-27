# Plano de teste no M5Paper original

## Preparação

1. Confirme visualmente que o aparelho é o M5Paper original, não PaperS3.
2. Use um microSD FAT32 conhecido e insira-o com o aparelho desligado.
3. Conecte USB, identifique a porta com `pio device list` e feche outros monitores seriais.
4. Na raiz do projeto, execute `pio run --target upload --upload-port COMx`.
5. Execute `pio device monitor --port COMx --baud 115200` e reinicie o aparelho.

## Evidências esperadas

1. O serial deve informar `Board enum` igual ao valor esperado, display `540 x 960`, touch `yes`, flash, PSRAM próxima de 4 MB no aparelho atualmente medido, heap interno, SD e CPU.
2. A tela deve ficar em retrato, com título horizontal e cinco alvos circulares: dois superiores, centro e dois inferiores.
3. Toque uma vez no centro de cada alvo. O círculo deve ser preenchido próximo ao dedo e o serial deve imprimir coordenadas coerentes.
4. Toque sequencialmente nos quatro cantos, sem alcançar a borda extrema. Verifique que X cresce da esquerda para a direita e Y cresce de cima para baixo.
5. Arraste lentamente em diagonal. O smoke test marca apenas o início de cada contato; confirme que não há coordenadas espelhadas ou trocadas.
6. Reinicie sem microSD. A UI deve mostrar falha legível e o loop/touch deve continuar funcional.
7. Reinsira o cartão somente com o aparelho desligado, reinicie e confirme tamanho não zero e tempo de montagem no serial.
8. Faça dez reinicializações. Registre qualquer boot travado, tela incompleta ou falha intermitente do SD.

## Registro obrigatório

Anote modelo/versão da placa, tamanho e formato do SD, tensão/bateria, log serial completo, resultado de cada alvo e fotos. Não aprove a rotação apenas pela aparência: compare posições físicas e coordenadas. Se houver inversão, não ajuste empiricamente sem registrar os valores observados.

## Critério da Fase 1

Passa quando identifica `board_M5Paper`, mostra 540×960, encontra PSRAM/touch, monta um SD válido, mantém touch responsivo sem SD e os cinco alvos coincidem com as coordenadas físicas. O build sozinho significa apenas “pronto para teste”.

## Regressão funcional do leitor

1. Abra EPUB 2 e EPUB 3 com acentos, entidades e capítulos múltiplos.
2. Avance e volte cruzando capítulos; não pode haver perda, repetição ou página vazia.
3. Em um trecho reconhecível, percorra 16, 24, 32, 36 e 40 px. A primeira frase
   deve permanecer como âncora e o texto não deve herdar quebras do código XHTML.
4. Volte a páginas antigas após exceder 96 KiB de texto; valide reconstrução pelo
   checkpoint e continuidade da página seguinte.
5. Abra o menu pela faixa superior, confira zoom/capítulo/progresso, feche e
   confirme que a página é restaurada. Depois volte à biblioteca pelo menu.
6. Reinicie e reabra o mesmo EPUB; confirme retomada do trecho e zoom. Repita
   desligando durante uma troca de página para exercitar arquivo temporário/backup.
7. Teste EPUB truncado, OPF inválido, spine ausente, traversal e capítulo grande;
   o firmware deve informar erro e continuar responsivo.
8. Leia por pelo menos 500 trocas de página, registrando heap, PSRAM, travamentos,
   ghosting e refresh completo necessário.
9. Em um dispositivo limpo, confirme que a UI inicia em inglês. Troque para
   português no menu, reinicie e confirme que a escolha persiste.
10. Em EPUB 2 e EPUB 3, abra o sumário, percorra suas páginas e selecione
    capítulos próximos do início, meio e fim.

## Espera e bateria

1. Abra um trecho reconhecível de um EPUB e anote conteúdo, zoom e percentual
   de bateria. Abra o menu, escolha a espera e confirme a capa ampliada, sem
   texto sobreposto e apresentada por um refresh completo.
2. Acione apenas o lado de aumentar fonte. O aparelho deve acordar, redesenhar
   o mesmo trecho e manter o zoom anterior.
3. Repita usando apenas o lado de diminuir fonte. O acionamento de wake-up não
   pode mudar o zoom.
4. Repita acordando somente com um novo toque. O toque usado para selecionar
   sleep não pode causar wake imediato, e a página restaurada deve receber um
   refresh completo.
5. Deixe o aparelho sem toque nem alavanca por dez minutos. Confirme entrada
   automática na espera e repetição correta do passo 2 ou 3.
6. Durante os dez minutos, interaja perto do limite primeiro por toque e depois
   pela alavanca. Cada interação deve reiniciar integralmente o temporizador.
7. Entre em espera a partir da biblioteca. Ao acordar, deve retornar à
   biblioteca, sem abrir indevidamente o último livro.
8. Confira o indicador no canto superior direito na biblioteca, leitura, menu e
   sumário. Ele não pode encobrir o título e deve ficar entre 0 e 100%.
9. Deixe a tela parada e varie alimentação/carga: não deve ocorrer refresh só
   por mudança da bateria. Troque de tela e confirme então a nova amostragem.
10. Meça a corrente antes e durante a espera, com USB desconectado, e registre
   placa, tensão, cartão e instrumento. Light sleep é usado porque deep sleep
   do ESP32 original não acorda por qualquer um de dois GPIOs ativos em baixo.

Sumário, links/notas, imagens e CSS ainda não têm fluxo visual completo e não
devem ser aprovados apenas porque seus parsers puros existem.

## Teste da política adaptativa e input pendente

Use o firmware de 2026-07-26 ou posterior e mantenha o monitor serial aberto. O
upload validado nesta máquina usa a porta `COM8`.

1. Abra um EPUB textual e avance uma página por vez, esperando mais de um segundo.
   Confirme no serial `M5EPUB_REFRESH` com `effective=fast`.
2. Faça cinco viradas com intervalo aproximado de 300–800 ms. A partir do burst,
   confirme `effective=fastest`; observe se texto permanece confortável.
3. Durante uma atualização física, toque novamente para avançar. Deve aparecer
   `M5EPUB_INPUT` com `status=queued` e depois `status=executed`; a ação não pode
   desaparecer nem executar mais de três avanços acumulados.
4. Durante prefetch, abra o menu. O pedido deve ser registrado e o menu deve abrir
   quando o prefetch chegar ao término seguro. Nesta versão o prefetch ainda não
   é cancelado no meio do chunk.
5. Alterne próxima/anterior dez vezes. Anterior deve prevalecer sobre avanços
   ainda pendentes e não pode pular ou repetir texto.
6. Continue até ocorrer limpeza automática. Confirme uma linha
   `effective=quality,reason=ghosting_cleanup` e verifique visualmente a remoção
   de resíduos. Registre o campo `cleanup` que causou a limpeza.
7. Capture pelo menos 30 viradas normais e 30 rápidas em arquivo. Analise com:

   ```powershell
   pio device monitor --port COM8 --baud 115200 | Tee-Object serial-refresh.log
   python tools/analyze_refresh_metrics.py serial-refresh.log -o refresh-summary.md
   ```

8. Inspecione mediana, P95 e máximo de `event_to_submit_us` e `event_to_idle_us`.
   Não conclua ganho apenas pela média. Fotografe a tela após cada burst e atribua
   nota de ghosting de 0 a 5.

Interrompa o teste se houver página vazia, texto pulado/repetido, reinicialização,
watchdog ou toque executado no sentido errado. Preserve o log completo nesse caso.

## Checkpoint físico antes do double buffering

1. Abra uma página com a última frase facilmente reconhecível.
2. Assim que a página aparecer, abra o menu enquanto o próximo prefetch está em
   andamento. Confirme `M5EPUB_PREFETCH,status=cancel_requested` seguido de
   `status=cancelled` e retorno correto ao fechar o menu.
3. Repita pedindo página anterior e depois alterando a fonte durante prefetch.
4. Em cada caso, avance novamente e confira palavra por palavra a fronteira entre
   páginas: não pode haver texto duplicado, omitido ou deslocado.
5. Faça dez ciclos de abrir menu/cancelar/fechar e depois vinte viradas contínuas.
6. Confirme que `cpu_work_during_busy_us` aparece maior que zero em pelo menos
   algumas linhas `M5EPUB_METRIC`; isso prova sobreposição de CPU, não ganho visual.

## Checkpoint físico do double buffering

1. Faça vinte viradas para a frente, esperando o prefetch terminar antes de cada
   toque. Cada caminho rápido deve registrar `M5EPUB_BUFFER,swap=1`.
2. Confirme que título, bateria, número de página e corpo pertencem à mesma página.
3. Compare a última linha de cada página com a primeira da seguinte; não pode haver
   repetição ou omissão.
4. A cada cinco viradas, abra e feche o menu. A página restaurada deve ser a atual,
   nunca o menu nem a página seguinte prefetched.
5. Faça anterior, próxima, anterior, próxima. O back buffer obsoleto deve ser
   invalidado e o conteúdo permanecer correto.
6. Altere a fonte e avance. A primeira página com a nova fonte não pode reutilizar
   pixels ou paginação do tamanho anterior.

## Botão central — limpeza manual assíncrona

1. Com uma página aberta e o painel livre, pressione o centro da alavanca uma vez.
   A página deve receber um refresh completo de qualidade sem mudar posição ou fonte.
2. Pressione novamente enquanto o painel estiver ocupado. O log deve mostrar
   `status=queued` imediatamente e `status=submitted` quando a submissão for segura.
3. Repita na biblioteca e no menu; o conteúdo atual deve permanecer o mesmo.
4. Confirme `M5EPUB_REFRESH` com `effective=quality,reason=manual_cleanup`.
5. Segurar ou pressionar repetidamente durante busy deve coalescer em uma única
   solicitação pendente, sem bloquear touch ou as extremidades da alavanca.

## Retomada com número e histórico

1. Com este firmware, abra um livro, avance pelo menos dez páginas e volte à
   biblioteca pelo menu para forçar a gravação do estado versão 2.
2. Reabra o livro. Conteúdo e número devem ser os mesmos do momento da saída.
3. Volte cinco páginas. Cada página deve aparecer corretamente e o número deve
   diminuir sem retornar artificialmente para 1.
4. Avance novamente e confira continuidade do texto nas duas direções.
5. Reinicie o aparelho e repita. Depois teste sleep/wake e mudança de fonte.
6. Para validar o limite, avance mais de 32 páginas, reabra e confirme pelo menos
   as 32 páginas anteriores. Páginas mais antigas podem exigir reconstrução futura.

Um estado antigo versão 1 não contém número nem histórico. Faça ao menos uma
virada e saia pelo menu para que ele seja regravado como versão 2 antes do teste.

## Checkpoint físico de refresh parcial e dirty region

1. Abra um livro e faça vinte viradas prefetched. Verifique que não restam faixas
   da página anterior fora da região atualizada.
2. No serial, confira `M5EPUB_DIRTY` e compare `changed_ratio` com
   `selected_ratio`. A região selecionada deve conter toda a alteração.
3. Abra e feche o menu, volte uma página, mude a fonte e atravesse um capítulo.
   Nenhuma dessas operações pode deixar pixels antigos, cortar texto ou misturar
   cabeçalho, corpo e rodapé.
4. Pressione o botão central. A limpeza manual deve registrar tela cheia e
   `effective=quality`, eliminando resíduos visíveis.
5. Faça cinco bursts de cinco páginas e atribua nota de ghosting de 0 a 5 após
   cada grupo. Guarde o log e fotografias; não conclua melhora apenas pelo build.
6. Se aparecer corrupção, registre a última linha `M5EPUB_DIRTY`, a linha
   `M5EPUB_REFRESH` correspondente e a orientação/região visível antes de
   reiniciar o aparelho.

## Checkpoint final de persistência e memória

1. No boot, confirme dois logs `M5EPUB_MEMORY` e que ambos os canvases foram
   alocados; em Auto, anote se cada um ficou em PSRAM ou SRAM.
2. Vire quatro páginas e aguarde menos de 15 segundos: não deve haver escrita por
   página. Depois aguarde 15 segundos e confirme `reason=idle_timeout`.
3. Vire cinco páginas continuamente e confirme uma única escrita com
   `reason=page_threshold`, somente depois do painel ficar livre.
4. Teste voltar à biblioteca, mudar fonte e entrar em sleep; o log deve mostrar a
   persistência obrigatória correspondente e a retomada deve manter a posição.
5. Faça 100 viradas e observe reinicializações, watchdog, falhas de alocação e
   redução contínua do maior bloco. O segundo núcleo deve permanecer desativado.

## Capas e famílias de fonte

1. Abra um EPUB 2 com `meta name="cover"` e um EPUB 3 com `cover-image`.
2. Confirme a miniatura no canto superior do menu e a capa ampliada no sleep.
3. Teste também livro sem capa, PNG/JPEG inválido e capa acima de 2 MiB; o leitor
   deve manter fallback textual e continuar navegável.
4. Toque no botão Fonte/Font e percorra Book, Sans e Compact. Cada troca deve
   manter a posição textual, repaginar e ser preservada após reiniciar.
5. Confira acentos, cedilha, aspas, travessões e tamanhos 16–40 em todas as
   famílias antes de aprovar a aparência tipográfica.
6. Na biblioteca, confirme duas capas por linha e o título UTF-8 abaixo de cada
   uma, especialmente nomes com á, é, í, ó, ú, ã, õ e ç.
7. Cronometre quatro livros visíveis. Deve ocorrer apenas um refresh Quality
   depois da coleta das capas, e não um refresh completo por livro.
8. Troque de página e confirme que a grade aparece primeiro com `Carregando...`.
   Depois da preparação, as quatro capas devem surgir em um único refresh.
9. Entre em uma subpasta a partir de uma página diferente da primeira. Toque em
   `.. Voltar` e confirme retorno à mesma página da pasta anterior.
10. No topo da biblioteca, abra o menu e escolha preparar as capas. Confira o
    aviso, a fase de procura, o contador exato de capas, refresh periódico e
    cancelamento. Execute novamente: livros válidos devem ser ignorados.
11. Adicione um EPUB ao cartão e repita. Somente o livro novo ou modificado deve
    precisar de preparação completa.

## Portal web de upload

1. Reinicie e confirme no roteador ou consumo que o Wi-Fi começa desligado.
2. No menu superior da biblioteca, ative o servidor, selecione uma rede WPA2 e
   digite a senha. Confirme IP e `m5paper.local` na tela.
3. Em celular e computador, abra a página, navegue por pastas, crie uma pasta e
   confira pastas antes de arquivos, ordenação numérica e espaço livre.
4. Envie sequencialmente dois EPUBs, incluindo um de aproximadamente 5 MiB e
   `Memórias Póstumas.epub`. O segundo nome deve aparecer como
   `Memorias_Postumas.epub`; nenhum reset ou watchdog pode ocorrer.
5. Interrompa um upload no meio. Não pode existir `.epub` truncado; o `.part`
   temporário deve ser removido quando o callback de aborto for recebido.
6. Tente `/api/list?path=../` com `curl`; espere HTTP 400 e nenhum acesso fora
   da raiz. Tente apagar pasta não vazia; espere HTTP 409.
7. Desligue o toggle e confirme servidor, mDNS e rádio desligados. Repita sem
   requisições e aguarde oito minutos para validar o timeout automático.
8. Reabra a pasta afetada e confirme que o novo livro aparece e sua capa pode
   ser preparada normalmente. Teste leitura e sleep depois do portal.
