# Plano de teste no M5Paper original

## Preparação

1. Confirme visualmente que o aparelho é o M5Paper original, não PaperS3.
2. Use um microSD FAT32 conhecido e insira-o com o aparelho desligado.
3. Conecte USB, identifique a porta com `pio device list` e feche outros monitores seriais.
4. Na raiz do projeto, execute `pio run --target upload --upload-port COMx`.
5. Execute `pio device monitor --port COMx --baud 115200` e reinicie o aparelho.

## Evidências esperadas

1. O serial deve informar `Board enum` igual ao valor esperado, display `540 x 960`, touch `yes`, flash, PSRAM próxima de 8 MB, heap interno, SD e CPU.
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

Sumário, links/notas, imagens e CSS ainda não têm fluxo visual completo e não
devem ser aprovados apenas porque seus parsers puros existem.
