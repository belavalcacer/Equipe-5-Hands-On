# Firmware e protocolo IR — resumo

[English version](ir_quickstart_en.md) · [Documentação completa](ir_protocol.md)

## Introdução: do botão do controle aos símbolos RMT

### IR, portadora, MARK e SPACE

**IR** significa infravermelho: o controle envia informação por uma luz invisível
emitida por um LED. Um protocolo IR define como representar os bits com tempos,
como organizar endereço/comando e como indicar início, fim e repetições da mensagem.
O aparelho precisa reconhecer essas regras para executar o comando.

Durante a emissão, o LED liga e desliga rapidamente. Essa modulação é a
**portadora**, normalmente 38 kHz neste projeto: aproximadamente 38 mil ciclos
por segundo, com período de 26,3 µs. Essa é a frequência de acionamento do LED,
não a frequência óptica da luz infravermelha.

| Conceito | Significado no sinal |
|---|---|
| Portadora | Oscilação rápida usada enquanto há emissão IR. |
| Duty cycle | Fração de cada ciclo em que o LED é acionado; a configuração atual usa 33%. |
| MARK (marca) | Intervalo com portadora: um conjunto de ciclos rápidos. |
| SPACE (espaço) | Intervalo sem portadora: uma pausa na emissão. |
| Envelope | Sequência de MARKs e SPACEs, desconsiderando os ciclos rápidos dentro de cada MARK. |

Por exemplo, um MARK de 9000 µs com portadora de 38 kHz contém aproximadamente
342 ciclos. Depois dele pode existir um SPACE de 4500 µs. Os tempos de MARK/SPACE
carregam a informação; frequência da portadora e duty cycle descrevem como a luz
é emitida durante os MARKs.

Desenho de alguns ciclos da portadora (esquemático, sem escala):

```text
LED ligado      +---+        +---+        +---+
                |   |        |   |        |   |
LED desligado --+   +--------+   +--------+   +--------
                <----------->
                 um ciclo: ~26,3 us (38 kHz)
                 ligado ~8,7 us; desligado ~17,6 us (33%)
tempo ------------------------------------------------->
```

Agora afastando o zoom: cada MARK contém muitos desses ciclos. Os desenhos
abaixo compartilham os limites de MARK/SPACE, mas não estão em escala:

```text
                    MARK                SPACE          MARK
                <-------------><--------------------><------->
LED IR          |_|_|_|_|_|_|_|______________________|_|_|_|_

Envelope (1)    +-------------+                      +-------+
         (0) ---+             +----------------------+       +---

GPIO RX  (1) ---+             +----------------------+       +---
         (0)    +-------------+                      +-------+
tempo ---------------------------------------------------------->
```

O envelope indica **presença de portadora**, mesmo quando o LED desliga por
um instante dentro de cada ciclo. O receptor demodulado entrega esse envelope
invertido no GPIO; o firmware normaliza de volta para MARK=1 e SPACE=0.

### NEC: um exemplo de protocolo IR

No NEC clássico, uma mensagem começa com MARK de 9 ms e SPACE de 4,5 ms, seguidos
de 32 bits: endereço, endereço invertido, comando e comando invertido, cada campo
com 8 bits. Os bytes são enviados com o bit menos significativo primeiro, e há
um MARK final. Existem variantes e sequências próprias de repetição.

| Elemento NEC | MARK nominal | SPACE nominal |
|---|---:|---:|
| Início | 9000 µs | 4500 µs |
| Bit 0 | 562,5 µs | 562,5 µs |
| Bit 1 | 562,5 µs | 1687,5 µs |

A duração da pausa distingue 0 de 1. **Um bit 0 também contém um MARK**; portanto,
MARK não significa "bit de dados 1", nem SPACE significa "bit de dados 0".
Esses tempos e a estrutura são descritos no
[exemplo NEC da Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html#customize-rmt-encoder-for-nec-protocol).

NEC serve aqui para explicar a codificação. Este firmware captura RAW sem
interpretar endereço, comando ou bits NEC. Um sinal que começa perto de 9 ms /
4,5 ms não basta para identificar o protocolo, e a captura do Gree não deve ser
tratada automaticamente como NEC.

### O que o receptor entrega ao ESP32

O receptor utilizado é **demodulado**: ele detecta a portadora e entrega ao GPIO
o envelope, sem reproduzir cada ciclo de 38 kHz. Como sua saída é active-low:

| Estado óptico | Nível elétrico no GPIO RX | Nível normalizado pelo firmware |
|---|---:|---:|
| MARK: portadora presente | 0 | 1 |
| SPACE: portadora ausente | 1 | 0 |

Por isso, o RMT mede os tempos dos intervalos em nível baixo/alto. Ele não mede a
frequência original da portadora nesse receptor; o payload usa `carrier_hz=0`.
No replay, o TX recria a portadora, usando 38 kHz quando o campo é zero.

### Como o envelope vira uma tabela de símbolos

O RMT registra dois intervalos consecutivos por símbolo:
`level0/duration0` e `level1/duration1`. A resolução de 1 MHz faz cada tick valer
1 µs. O firmware remove o repouso inicial e converte os níveis elétricos para
MARK=1 e SPACE=0.

Este trecho didático representa início NEC, um bit 0, um bit 1 e o MARK final;
os demais bits foram omitidos. Para facilitar a leitura, os tempos dos bits
foram arredondados para 560 e 1690 µs. Capturas reais apresentam variações.

| Símbolo | Interpretação didática | RX: nível / duração 0 | RX: nível / duração 1 | Após normalização |
|---|---|---|---|---|
| 1 | Início | 0 / 9000 µs | 1 / 4500 µs | MARK 9000, SPACE 4500 |
| 2 | Bit 0 | 0 / 560 µs | 1 / 560 µs | MARK 560, SPACE 560 |
| 3 | Bit 1 | 0 / 560 µs | 1 / 1690 µs | MARK 560, SPACE 1690 |
| … | Demais bits | … | … | … |
| Final | MARK de encerramento | 0 / 560 µs | Terminador, se não houver SPACE útil | MARK 560, SPACE 0 |

O SPACE zero da última linha é a representação usada pelo firmware para uma
fase final ausente. O nível elétrico de um terminador de duração zero não tem
significado útil. Conforme o que o hardware entregar, pode existir também um
SPACE final não nulo. Um símbolo RMT pode conter o início, um bit ou outro trecho
do sinal: sua unidade é um par de intervalos, sem significado de comando embutido.

### Exemplo real: tabela de símbolos capturados

Na captura de **70 símbolos / 292 bytes** enviada durante os testes, o arquivo
`captura.ir` começa com este cabeçalho de payload RAW:

```text
46 00 | 00 00 | 40 42 0F 00 | 00 00 00 00
  70    flags    1.000.000 Hz   portadora desconhecida
```

Depois desses 12 bytes vêm os símbolos, com 4 bytes por linha. A tabela abaixo
mostra os cinco primeiros e o último, **já normalizados pelo firmware**:
`level0=1` representa MARK e `level1=0` representa SPACE. A resolução é de
1 µs por tick, então os valores de duração também são os tempos em µs.

| Símbolo (contando de 1) | Bytes no arquivo | `level0` | `duration0` (µs) | `level1` | `duration1` (µs) |
|---|---|---:|---:|---:|---:|
| 1 | `26 A3 6A 11` | 1 | 8998 | 0 | 4458 |
| 2 | `9C 82 66 06` | 1 | 668 | 0 | 1638 |
| 3 | `92 82 1B 02` | 1 | 658 | 0 | 539 |
| 4 | `96 82 1B 02` | 1 | 662 | 0 | 539 |
| 5 | `96 82 67 06` | 1 | 662 | 0 | 1639 |
| … | Símbolos 6 a 69 omitidos | … | … | … | … |
| 70 | `99 82 00 00` | 1 | 665 | 0 | 0 |

Lendo a primeira linha: houve emissão IR por **8998 µs**, seguida de uma pausa
de **4458 µs**. Depois começa o MARK de 668 µs da segunda linha, e assim por
diante. No GPIO do receptor active-low, esses mesmos intervalos úteis têm
níveis invertidos: MARK=0 e SPACE=1. A tabela representa a sequência após a
normalização, não um dump direto da memória do RX.

```text
Sequencia: [MARK 8998][SPACE 4458][MARK 668][SPACE 1638] ... [MARK 665]
Simbolos:  |----- simbolo 1 -----|----- simbolo 2 -----| ... |-- 70 --|
```

A última linha tem SPACE de duração zero, indicando que não há uma segunda
fase útil. O tamanho confere: `12 + 70 * 4 = 292 bytes`. Esse arquivo contém
somente o payload RAW; o cabeçalho de transporte UART não é salvo nele.
Os 70 símbolos não significam necessariamente 70 bits de dados: também há
intervalos de início, pausas e encerramento.

### Da tabela ao pacote UART

O firmware codifica cada fase normalizada como `(nivel << 15) | duracao`.

**Por que deslocar 15 posições?** Cada fase ocupa 16 bits: o bit de índice 15
(o mais alto, contando a partir de zero) guarda o nível; os outros 15 guardam a
duração em ticks. O operador `<< 15` coloca o nível nessa posição e o `|` junta
os dois campos:

```text
bit:       15 | 14 ............ 0
         nivel| duracao (15 bits)
         +----+-----------------+
MARK 560 | 1  | 000001000110000 | = 0x8230
         +----+-----------------+
          1 bit     15 bits

Um simbolo RMT = duas fases = 32 bits (4 bytes):
         +-----------------------+-----------------------+
         | fase 0: nivel + tempo | fase 1: nivel + tempo |
         +-----------------------+-----------------------+
                  16 bits                16 bits
```

Exemplo com nível `1` (MARK normalizado) e duração de `560` ticks:

```c
(1 << 15) | 560
// 0x8000 | 0x0230 = 0x8230

// Para separar os campos novamente:
nivel   = (valor >> 15) & 1;
duracao = valor & 0x7FFF;
```

No payload little-endian, `0x8230` vira os bytes `30 82`. Os 15 bits permitem
duração máxima de `32767` ticks: **32,767 ms a 1 MHz**, por fase. Esse limite do
campo é diferente do limiar de inatividade de 40 ms que encerra a captura.

O primeiro símbolo da tabela didática NEC (MARK 9000 + SPACE 4500) vira:

```text
MARK 9000  → 0x8000 | 0x2328 = 0xA328 → bytes 28 A3
SPACE 4500 → 0x0000 | 0x1194 = 0x1194 → bytes 94 11
Símbolo no payload: 28 A3 94 11
```

Esses bytes fazem parte do payload RAW enviado ao Linux por READ. São duas
camadas de protocolo distintas:

| Camada | O que define |
|---|---|
| Protocolo IR do controle, por exemplo NEC | Como tempos representam bits e como esses bits formam comandos para o aparelho. |
| Protocolo UART deste projeto | Como transportar os tempos RAW entre Linux e ESP32, com READ/WRITE/PING, tamanho, sequência e CRC. |

No WRITE, o caminho se inverte: bytes → fases MARK/SPACE → RMT TX → portadora no
LED IR. O firmware consegue repetir os tempos sem precisar saber qual botão eles
representam. A ordem dos bits do NEC no ar e o little-endian dos inteiros UART
são convenções de camadas diferentes.

## O que o firmware faz

O ESP32 captura os tempos dos pulsos infravermelhos (RAW) **somente quando o
Linux envia READ**. Ao iniciar, RX fica desabilitado. Cada READ abre uma janela
nova de **5 segundos**: o primeiro frame completo e válido é devolvido; sem um
frame válido no prazo, a resposta é `NO_DATA`. RX é desabilitado antes da resposta.
Não há reaproveitamento de capturas anteriores nem decodificação de controles.

```text
Linux → READ → habilita RX → aguarda até 5 s → frame novo ou NO_DATA → Linux
Linux → WRITE → validação → RMT TX → transmissor IR
```

O callback RX avisa uma fila. A task `ir_rx` normaliza os níveis e encerra a
recepção após o primeiro frame válido ou o timeout. Frames inválidos são
ignorados, sem renovar o prazo. A task `ir_protocol` lê a UART, valida os pacotes,
executa os comandos e responde. Durante READ, ela espera a captura; durante WRITE,
espera a transmissão terminar. RX permanece desabilitado em WRITE e PING.

## Arquivos e inicialização

| Arquivo ou módulo | Responsabilidade |
|---|---|
| `main.c` | Inicializa a aplicação. |
| `ir_rmt` | Cria e configura os canais RMT. |
| `ir_rx` | Captura um frame novo sob demanda, normaliza e aplica o timeout. |
| `ir_tx` | Transmite RAW usando encoder de cópia e portadora. |
| `ir_protocol` | Monta/valida pacotes, calcula CRC e processa comandos. |
| `ir_transport_uart` | Transporta bytes pela UART. |
| `ir_types.h` | Define fases, símbolos, códigos IR e seus limites. |

`app_main()` executa esta ordem:

```text
ir_rmt_init → ir_rx_init → ir_tx_init → ir_transport_init
           → ir_protocol_init
```

Os `init` preparam os recursos; `ir_rx_init` cria a task RX e
`ir_protocol_init` cria a task de comunicação. Nenhum `init` inicia a captura.
Depois, `app_main` retorna e as tasks aguardam solicitações. READ usa
`ir_rx_capture(code, timeout_ms)`; WRITE usa `ir_tx_send()`.

## Pacote entre Linux e firmware

```text
[Cabeçalho: 16 bytes] [Payload: 0 a 1036 bytes]
```

Todos os inteiros multibyte são **little-endian**. Os campos aparecem nesta ordem:

| Campo | Bytes | Função |
|---|---:|---|
| `magic` | 2 | Bytes `49 52` (`IR`): identificam um possível início de pacote. |
| `version` | 1 | Versão do protocolo: `1`. |
| `command` | 1 | READ=`1`, WRITE=`2`, PING=`3`. |
| `flags` | 1 | Requisição=`0`, resposta=`1`. |
| `status` | 1 | Requisição=`0`; resposta indica sucesso ou erro. |
| `sequence_id` | 2 | O firmware repete o identificador na resposta. |
| `payload_length` | 4 | Tamanho do payload em bytes. |
| `crc32` | 4 | Detecta corrupção dos dados. |

O **CRC-32/ISO-HDLC** cobre os primeiros 12 bytes do cabeçalho seguidos pelo
payload, omitindo o próprio campo CRC. Equivalente em Python:
`zlib.crc32(header[:12] + payload)`. O parser procura novamente o magic quando
perde a sincronização e descarta recepções parciais após 250 ms sem novos bytes.

| Comando | Linux → firmware | Firmware → Linux |
|---|---|---|
| READ (`0x01`) | Sem payload; inicia uma captura nova. | Primeiro frame válido no prazo, ou `NO_DATA` (`0x05`) após o timeout. |
| WRITE (`0x02`) | Código IR no payload. | `OK` (`0x00`) após transmitir, ou erro; sem payload. |
| PING (`0x03`) | Sem payload. | `OK`, sem payload. |

Cada READ exige uma nova captura. Envie **uma requisição por vez** e aguarde a
resposta. Para READ, o timeout serial deve superar o prazo de captura (por exemplo,
7 s para a janela padrão de 5 s). Para WRITE, use pelo menos 20 s. O cliente usa
20 s por padrão; ajuste `--timeout` se aumentar a janela RX. Reenviar WRITE pode
transmitir de novo.
Os demais códigos de erro estão na documentação completa.

## O que vai no payload IR

```text
symbol_count:u16 | flags:u16 | resolution_hz:u32 | carrier_hz:u32
symbols: symbol_count × 4 bytes
```

São 1–256 símbolos, flags=`0`, resolução de 1 MHz e tamanho total de
`12 + symbol_count × 4` bytes. Cada símbolo tem duas fases de 16 bits:
MARK e SPACE. O bit 15 indica o nível; os outros 15 bits guardam a duração em µs.
**MARK=1** significa portadora presente; **SPACE=0**, portadora ausente.
O RX já corrige a polaridade active-low: o Linux não precisa inverter nada.

As durações vão de 1 a 32767 µs; somente o último SPACE pode ser zero.
Exemplo: MARK 3320 µs + SPACE 9936 µs → `F8 8C D0 26`.
Capturas usam `carrier_hz=0` (desconhecida); TX usa 38 kHz nesse caso.
Portadoras explícitas aceitas: 20–60 kHz.

O Linux pode reutilizar diretamente o **payload de READ em WRITE**, montando um
novo cabeçalho e recalculando o CRC.

## Símbolos RMT: de pulsos a binário e hexadecimal

Um `rmt_symbol_word_t` representa **dois intervalos consecutivos** do sinal:
`level0/duration0` e `level1/duration1`. Cada nível ocupa 1 bit e cada duração,
15 bits. Com resolução de 1 MHz, 1 tick = 1 µs. Um símbolo não representa
necessariamente um bit de dados do controle remoto: ele descreve tempos e níveis.

No RX, os níveis são elétricos. Como o receptor é active-low, o firmware usa
`nivel_normalizado = !nivel_rx`. Por exemplo:

| Intervalo | Nível elétrico RX | Duração | Representação normalizada |
|---|---:|---:|---|
| Primeiro | 0 | 3320 ticks | MARK: nível 1, 3320 µs |
| Segundo | 1 | 9936 ticks | SPACE: nível 0, 9936 µs |

O RMT pode começar com um SPACE ocioso; o firmware o remove e reorganiza as fases
para que cada símbolo do protocolo tenha MARK primeiro e SPACE depois.
O formato wire é serializado explicitamente, sem enviar a struct do ESP-IDF.

Para converter cada fase normalizada:

```text
fase = (nivel << 15) | duracao

MARK:  0x8000 | 3320 = 0x8000 | 0x0CF8 = 0x8CF8
SPACE: 0x0000 | 9936 = 0x0000 | 0x26D0 = 0x26D0
```

| Fase | Binário de 16 bits (bit 15 = nível) | Hexadecimal | Bytes little-endian |
|---|---|---|---|
| MARK | `1000 1100 1111 1000` | `8CF8` | `F8 8C` |
| SPACE | `0010 0110 1101 0000` | `26D0` | `D0 26` |

O símbolo completo no payload é **`F8 8C D0 26`**. Em binário, esses bytes são
`11111000 10001100 11010000 00100110`. Little-endian muda a ordem dos bytes de cada
inteiro; não inverte os bits dentro do byte.

Para fazer o caminho inverso, reconstrua cada inteiro de 16 bits:

```text
fase = byte_baixo | (byte_alto << 8)
nivel = (fase >> 15) & 1
duracao = fase & 0x7FFF

F8 8C → 0x8CF8 → nivel 1, duracao 3320 → MARK 3320 µs
D0 26 → 0x26D0 → nivel 0, duracao 9936 → SPACE 9936 µs
```

No TX, esses níveis normalizados vão diretamente para o RMT: nível 1 habilita a
portadora e nível 0 produz uma pausa. O MARK de 3320 µs contém vários ciclos da
portadora de 38 kHz; não é um único pulso de 38 kHz. Essa conversão recupera a
sequência RAW, sem interpretar os bits de um protocolo como NEC ou Samsung.

## Exemplos de RX, TX e PING

RX/TX são operações físicas do RMT. Na UART, os comandos correspondentes são
READ para iniciar uma captura e WRITE para transmitir. Os pacotes abaixo usam
sequências fixas para facilitar a leitura e incluem CRC válido; o cliente Python
gera sua própria sequência e CRC. O frame de um símbolo é didático: uma captura
real normalmente contém vários símbolos e pode incluir uma pausa final.

### RX: capturar e consultar com READ

Execute READ primeiro. Quando o cliente indicar que enviou a requisição, aponte
o controle ao receptor no GPIO18 e pressione um botão dentro de 5 segundos.
Um comando emitido antes da requisição não fica armazenado para leitura posterior.

```sh
python tools/ir_client.py --port /dev/ttyUSB1 read capture.ir
```

Linux → firmware, READ com sequência 1:

```text
49 52 01 01 00 00 01 00 00 00 00 00 33 83 28 C5
```

Firmware → Linux, supondo uma captura com MARK 3320 µs e SPACE 9936 µs:

```text
49 52 01 01 01 00 01 00 10 00 00 00 49 CE 6B ED
01 00 00 00 40 42 0F 00 00 00 00 00 F8 8C D0 26
```

A primeira linha é o cabeçalho; a segunda é o payload de 16 bytes salvo em
`capture.ir`: um símbolo, flags zero, resolução de 1 MHz e portadora desconhecida.
Se nenhum frame válido terminar dentro do prazo, a resposta será `NO_DATA`, sem payload.

### TX: retransmitir com WRITE

Envie o payload salvo ou gere o exemplo de um símbolo:

```sh
python tools/ir_client.py --port /dev/ttyUSB1 write capture.ir
python tools/ir_client.py example example.ir
python tools/ir_client.py --port /dev/ttyUSB1 write example.ir
```

Linux → firmware, WRITE do exemplo com sequência 2:

```text
49 52 01 02 00 00 02 00 10 00 00 00 7D B0 DB 60
01 00 00 00 40 42 0F 00 00 00 00 00 F8 8C D0 26
```

O firmware valida, emite MARK por 3320 µs com portadora de 38 kHz e depois SPACE
por 9936 µs no GPIO19. Ao concluir, responde `OK`, sem payload:

```text
49 52 01 02 01 00 02 00 00 00 00 00 C6 CD 9B B6
```

### PING: verificar a comunicação

```sh
python tools/ir_client.py --port /dev/ttyUSB1 ping
```

Linux → firmware, PING com sequência 3:

```text
49 52 01 03 00 00 03 00 00 00 00 00 BE 0A 16 A6
```

Firmware → Linux, `OK` com a mesma sequência:

```text
49 52 01 03 01 00 03 00 00 00 00 00 20 0A BC 6A
```

PING não captura nem transmite IR; o cliente imprime `OK` quando recebe a resposta.

## Configuração padrão e teste rápido

RX no **GPIO18**, TX no **GPIO19**, portadora de **38 kHz / 33%**, sem DMA.
Uma captura termina após 40 ms de nível constante; pausas maiores separam frames. O limite
de fim de frame do ESP32 clássico usa 16 bits; as fases RAW continuam limitadas a
32.767 µs cada. Para testar o novo intervalo, grave o firmware e capture novamente;
um arquivo antigo pode conter apenas parte do comando.
A UART binária é **UART1, TX GPIO25, RX GPIO26, 115200 8N1**.
UART0 fica para logs. Em `idf.py menuconfig` → `RAW IR device`, configure a UART,
o limite de captura `CONFIG_IR_RX_CAPTURE_TIMEOUT_MS` (padrão 5000 ms) e o intervalo
de fim de frame `CONFIG_IR_RX_IDLE_US` (padrão 40000 µs). São tempos diferentes:
5 s para obter um frame completo; 40 ms de nível constante para delimitar o frame.
O timeout de 250 ms do parser se aplica a bytes UART incompletos, não à captura IR.

Com o ESP-IDF 6.1 ativo, compile com `idf.py build`. Para testar com o
[cliente serial](../tools/ir_client.py), substitua a porta pela UART binária real;
envie READ e só então pressione o botão do controle:

```sh
python tools/ir_client.py --port /dev/ttyUSB1 ping
python tools/ir_client.py --port /dev/ttyUSB1 read capture.ir
python tools/ir_client.py --port /dev/ttyUSB1 write capture.ir
```
