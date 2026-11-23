# PSI – Microcontroladores 2 · Controle de LED RGB pela UART

**Repositório:** <https://github.com/arturserejodantas7/PSI-Microcontroladores2-UART-LED>

**Autor:** Artur Serejo Dantas · Nº USP 15441730 · GitHub `arturserejodantas7`\
**Plataforma:** NXP FRDM-KL25Z · Zephyr RTOS 4.2 · Windows + VS Code + west

## 1. Objetivo

Receptor UART que controla o LED RGB da placa com um **protocolo fixo de 6 bytes**.
O receptor lê os bytes com **timeout curto**, valida **ID** e **CHECK**, executa a ação e
**descarta pacotes inválidos sem travar**. Os dois bônus foram implementados:
**CRC-8** no lugar do XOR e **intensidade por PWM**.

## 2. Protocolo

Linha serial: **UART0, 115200 bps, 8N1** (sem paridade na linha; a verificação é feita no
campo CHECK, em nível de protocolo).

| Byte | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| Campo | ID | R | G | B | AÇÃO | CHECK |
| Valor | `0xA5` | 0–255 | 0–255 | 0–255 | 0x01–0x05 | XOR ou CRC-8 dos bytes 0–4 |

| AÇÃO | Nome | Efeito no LED |
|---|---|---|
| `0x01` | LIGAR | acende com a cor (R,G,B) |
| `0x02` | DESLIGAR | apaga (R,G,B ignorados) |
| `0x03` | TOGGLE | aceso/piscando → apaga; apagado → acende com (R,G,B) |
| `0x04` | PISCA 1s | pisca com (R,G,B): 1 s aceso, 1 s apagado |
| `0x05` | PISCA 2s | pisca com (R,G,B): 2 s aceso, 2 s apagado |

- Se R = G = B = 0 numa ação que acende, é usada a última cor (no início: branco).
- O **ID** (`0xA5`), o tipo de CHECK e o timeout podem ser alterados em `prj.conf`
  (`CONFIG_APP_ID_DISPOSITIVO`, `CONFIG_APP_CHECK_CRC8`, `CONFIG_APP_TIMEOUT_BYTE_MS`).

### 2.1 Cálculo do CHECK – exemplo real (LIGAR vermelho)

Bytes 0–4: `A5 FF 00 00 01`

**XOR** (enunciado): `A5 ⊕ FF ⊕ 00 ⊕ 00 ⊕ 01 = 5B` → pacote `A5 FF 00 00 01 5B`

**CRC-8** (bônus, padrão do firmware): polinômio x⁸+x²+x+1 (`0x07`), valor inicial `0x00`,
sem reflexão e sem XOR final (catalogado como CRC-8/SMBUS; verificação: CRC("123456789") = `0xF4`).
Para cada byte: `crc ^= byte`; depois 8 vezes: se o bit 7 vale 1, `crc = (crc << 1) ^ 0x07`,
senão `crc = crc << 1`.

| Byte | `crc ^ byte` | CRC após 8 deslocamentos |
|---|---|---|
| A5 | A5 | 72 |
| FF | 8D | AA |
| 00 | AA | 5F |
| 00 | 5F | 9A |
| 01 | 9B | **C8** |

Pacote final: **`A5 FF 00 00 01 C8`**.

Por que o CRC é melhor: com XOR, LIGAR vermelho (`A5 FF 00 00 01`), verde (`A5 00 FF 00 01`)
e azul (`A5 00 00 FF 01`) têm o **mesmo** CHECK `5B`, então uma troca de bytes passa
despercebida. Com CRC-8 eles valem `C8`, `32` e `CE`.

## 3. Arquitetura

```
 PC (tools/envia_pacote.py)          FRDM-KL25Z (Zephyr 4.2)
 ==========================          ==================================================

 pacote de 6 bytes --USB/OpenSDA--> UART0 RX
                                      | interrupção
                                      v
                                    uart_isr(): uart_fifo_read() -> k_msgq_put()
                                      | fila de 128 bytes
                                      v
                                    thread main: k_msgq_get(timeout 50 ms)
                                      | chegou byte    -> proto_alimentar()
                                      | 50 ms sem byte -> proto_timeout()
                                      v
                                    valida ID -> CHECK -> AÇÃO
                                      |                          |
                                      | OK                       | ERR (ID/CHECK/AÇÃO/TRUNCADO)
                                      v                          |
                                    led_hw_executar()            |
                                    (k_mutex + led_logica)       |
                                      |-> PWM/GPIO -> LED RGB    |
                                      |-> k_timer 1 s / 2 s      |
                                      |     -> k_work: pisca     |
                                      v                          v
                                    printk("OK ...")           printk("ERR ...")
                                      |                          |
                                      +------------+-------------+
                                                   v
 respostas em texto <--USB/OpenSDA------------ UART0 TX
```

| Arquivo | Papel |
|---|---|
| `src/protocolo.c/.h` | **lógica pura**: receptor byte a byte, XOR, CRC-8, validação, respostas |
| `src/led_logica.c/.h` | **lógica pura**: estado do LED e efeito de cada ação (inclui fases do pisca) |
| `src/uart_rx.c/.h` | ISR da UART0 → `k_msgq`; limpa overrun/framing (`uart_err_check`) |
| `src/led_hw.c/.h` | saída PWM (TPM) ou GPIO; pisca com `k_timer` + `k_work`; `k_mutex` |
| `src/main.c` | laço de recepção com timeout, executa ação, imprime resposta |
| `boards/frdm_kl25z.overlay` | bônus PWM: nós TPM0/TPM2, pinmux e `pwm-leds` |
| `tests/unit_protocolo/` | testes **ztest** (frdm_kl25z, native_sim) |
| `tests/host/` | os mesmos testes no PC (gcc), validação do CRC-8 e simulador |
| `tools/envia_pacote.py` | script do PC: comandos, pacotes brutos e bateria de testes |

**Escolha da UART:** os pacotes chegam pela **UART0**, a mesma do console, que a OpenSDA
expõe como porta COM pelo **cabo USB**. Não é preciso adaptador USB-serial. O console do
Zephyr só transmite (`uart_poll_out`); a recepção é feita pela nossa ISR. As respostas em
texto terminam em fim de linha, e o script considera apenas as linhas que começam com
`OK` ou `ERR`.

**Por que a lógica é separada:** `protocolo.c` e `led_logica.c` não usam Zephyr. Com isso
os mesmos arquivos rodam no firmware, nos testes ztest e no PC (gcc), onde foram testados.

## 4. Tratamento de erros (sem travar)

| Situação | Detecção | Reação | Resposta |
|---|---|---|---|
| ID errado | byte 0 ≠ `0xA5` | descarta e ressincroniza | `ERR ID recebido=0x11 esperado=0xA5` |
| CHECK errado | CHECK ≠ calculado | descarta e ressincroniza | `ERR CHECK recebido=.. calculado=..` |
| Ação desconhecida | ID e CHECK ok, ação ∉ 1..5 | ignora o pacote e mantém o LED | `ERR ACAO 0x09 desconhecida` |
| Pacote truncado | nenhum byte por 50 ms com pacote incompleto | descarta os bytes parciais | `ERR TRUNCADO 3 byte(s) descartado(s)` |
| Lixo na linha | quadros inválidos | ressincroniza e descarta | `ERR ID ...` / `ERR TRUNCADO ...` |
| Fila cheia / overrun | `k_msgq_put` falha / `uart_err_check` | descarta e conta; a ISR nunca bloqueia | – |

**Ressincronização por janela deslizante:** quando um quadro de 6 bytes falha no ID ou no
CHECK, o receptor não joga tudo fora. Ele procura o próximo byte igual ao ID dentro do
próprio quadro, e esse byte vira o início do próximo pacote. Por exemplo,
`00 13 37 | A5 00 00 FF 01 CE` gera `ERR ID` e, em seguida, `OK LIGAR` (o pacote bom que
veio colado ao lixo é recuperado). Se não houver ID no quadro, o buffer é esvaziado. O
timeout de 50 ms entre bytes zera qualquer resto. O buffer nunca passa de 6 bytes, e a
ISR só enfileira (nunca bloqueia), então nenhuma entrada consegue travar o sistema.

## 5. Bônus

- **CRC-8** (seção 2.1), ativo por padrão. Para usar o XOR do enunciado, coloque
  `CONFIG_APP_CHECK_CRC8=n` no `prj.conf` e rode o script com `--modo xor`.
- **PWM:** intensidade 0–255 por canal → `pulso = período × valor / 255` com
  `pwm_set_pulse_dt()` (mesma API do sample `samples/basic/rgb_led` do Zephyr 4.2).
  Período de 1 ms (1 kHz, sem cintilação). Os LEDs são ativos em nível baixo, por isso os
  canais usam `PWM_POLARITY_INVERTED`.

**Detalhes do PWM na KL25Z (conferidos no código-fonte do Zephyr v4.2.0 e do hal_nxp):**

1. O `nxp_kl25z.dtsi` **não tem nós de TPM**. Eles foram declarados no overlay
   (`TPM0 @0x40038000`, IRQ 17; `TPM2 @0x4003A000`, IRQ 19; driver `nxp,kinetis-tpm`).
2. O `MKL25Z4_features.h` não define `FSL_FEATURE_TPM_HAS_32BIT_COUNTERn`, que é usada pelo
   driver TPM. Sem ela o driver não compila para esta placa. O `CMakeLists.txt` inclui
   `src/kl25z_tpm_fixup.h`, que define a macro como 0 (o TPM da KL25Z tem 16 bits).
3. O SoC não seleciona a fonte de clock do contador TPM. O `led_hw.c` faz
   `CLOCK_SetTpmClock(1)` em `SYS_INIT` (MCGPLLCLK/2 = 48 MHz). Com prescaler 16 o contador
   fica em 3 MHz e 1 ms corresponde a 3000 passos.
4. **Plano B:** se o PWM não funcionar na placa, compile com
   `-DDTC_OVERLAY_FILE=gpio_simples.overlay`. O firmware passa a usar GPIO (liga/desliga) e
   todo o resto continua igual.

## 6. Hardware

Basta o **cabo USB na porta OpenSDA** (alimentação, gravação e serial). Não há ligações
externas.

| Cor | Pino | GPIO (sem PWM) | PWM (bônus) |
|---|---|---|---|
| Vermelho | PTB18 | `red_led`, ativo baixo | TPM2_CH0 (ALT3) |
| Verde | PTB19 | `green_led`, ativo baixo | TPM2_CH1 (ALT3) |
| Azul | PTD1 | `blue_led`, ativo baixo | TPM0_CH1 (ALT4) |
| UART0 | PTA1 (RX) / PTA2 (TX) | console → OpenSDA (COM) | – |

Fontes: `boards/nxp/frdm_kl25z/frdm_kl25z.dts` (LEDs e UART0) e
`hal_nxp/dts/nxp/kinetis/MKL25Z128VLK4-pinctrl.h` (funções alternativas TPM).

## 7. Compilar e gravar (Windows, VS Code, west)

```bash
west build -p always -b frdm_kl25z .
west flash
```

Alternativa sem `west flash`: copie `build/zephyr/zephyr.bin` para o drive que a placa monta
no Windows (ex.: `FRDM-KL25Z` / `DAPLINK`), espere a cópia terminar e aperte RESET.

Variações:

```bash
west build -p always -b frdm_kl25z . -- -DCONFIG_APP_CHECK_CRC8=n      # CHECK = XOR
west build -p always -b frdm_kl25z . -- -DDTC_OVERLAY_FILE=gpio_simples.overlay  # sem PWM
west twister -T tests/unit_protocolo -p native_sim                        # ztest (Linux/WSL)
```

Ao ligar, a placa imprime:
`UART-LED pronto | ID=0xA5 | CHECK=CRC-8 | timeout=50 ms | saida=PWM`

## 8. Como testar

**Importante:** feche o monitor serial do VS Code antes de rodar o script, porque a porta COM
só pode ser aberta por um programa de cada vez.

```bash
pip install pyserial
python tools/envia_pacote.py --listar                 # descobre a COM da OpenSDA
python tools/envia_pacote.py -p COM5 ligar 255 0 0    # vermelho
python tools/envia_pacote.py -p COM5 ligar 40 0 40    # roxo fraco (PWM)
python tools/envia_pacote.py -p COM5 pisca2 0 255 255
python tools/envia_pacote.py -p COM5 toggle
python tools/envia_pacote.py -p COM5 desligar
python tools/envia_pacote.py -p COM5 bruto A5 FF 00 00 01 00   # CHECK errado
python tools/envia_pacote.py -p COM5 bateria          # 19 casos em sequência
```

Saída esperada:

```
TX: A5 FF 00 00 01 C8
RX: OK LIGAR R=255 G=0 B=0
TX: A5 FF 00 00 01 00
RX: ERR CHECK recebido=0x00 calculado=0xC8
```

A **bateria** envia, em sequência, as 5 ações (com cores e intensidades diferentes) e depois os
casos inválidos: ID errado, CHECK errado, CHECK no modo trocado, ação desconhecida, pacote
truncado, lixo aleatório, lixo seguido de pacote válido e dois pacotes colados. Termina com um
comando válido para mostrar que o sistema continua respondendo. Cada caso é marcado como
`PASSOU` ou `FALHOU` comparando a resposta recebida com a esperada.

**Testes no PC (Linux/macOS/WSL, sem placa):**

```bash
make -C tests/host          # 22 testes ztest (shim) + 2000 vetores XOR/CRC-8 vs. Python
make -C tests/host e2e      # bateria do script via pseudo-serial contra o simulador
```

O simulador (`tests/host/sim_receptor.c`) compila os **mesmos** `protocolo.c` e
`led_logica.c` do firmware e responde com as mesmas mensagens, usando o mesmo timeout.

## 9. Testes realizados

**Executados no PC** (Linux, gcc 13, Python 3.13), com os resultados abaixo:

| Teste | Onde | Resultado |
|---|---|---|
| 22 casos ztest de `tests/unit_protocolo` (checksums, ID/CHECK/ação inválidos, truncado, ressincronização, 5000 bytes de lixo, todas as ações, pisca e toggle do LED) | PC, via shim do ztest | **22/22 PASS** |
| CRC-8("123456789") = 0xF4 (valor de catálogo) | PC, C × Python | **OK** |
| 2000 vetores aleatórios: XOR e CRC-8 em C × referência Python (algoritmo por tabela) | PC | **0 divergências** |
| Bateria do `envia_pacote.py` (19 casos) contra o simulador, modo CRC-8 | PC, pseudo-serial | **19/19 PASSOU** |
| A mesma bateria, modo XOR | PC, pseudo-serial | **19/19 PASSOU** |
| Teste de mutação: com a ressincronização desligada, o teste correspondente falha | PC | **detectado** |

<div style="page-break-inside: avoid">

**Na placa FRDM-KL25Z:** os testes abaixo **ainda não foram executados**. O firmware Zephyr
**não foi compilado** neste ambiente, pois a toolchain ARM não estava disponível. Preencher
depois de gravar a placa:

| # | Teste na placa | Esperado | Resultado |
|---|---|---|---|
| P1 | `west build` + `west flash` | compila e imprime a mensagem `UART-LED pronto` | ☐ |
| P2 | `bateria` com o script (CRC-8) | 19/19 PASSOU | ☐ |
| P3 | LIGAR 255/128/40 no mesmo canal | brilho visivelmente diferente (PWM) | ☐ |
| P4 | PISCA 1s / PISCA 2s | troca de estado a cada 1 s / 2 s | ☐ |
| P5 | `ztest` na placa: `west twister -T tests/unit_protocolo -p frdm_kl25z --device-testing --device-serial COM5` | 22/22 PASS | ☐ |

</div>

## 10. Limitações e observações

- Por usar a UART0 do console, a placa também imprime o banner de boot do Zephyr e as
  respostas. O script ignora tudo o que não começa com `OK`/`ERR`.
- Um quadro com ID errado cujo conteúdo contenha `0xA5` pode gerar, além do `ERR ID`, um
  `ERR TRUNCADO` do resto ressincronizado. É o comportamento esperado da janela deslizante.
- Se houver erro de pacote ao receber rajadas grandes, aumente `TAM_FILA` em `uart_rx.c`.
