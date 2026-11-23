#!/usr/bin/env python3
"""
envia_pacote.py - Envia pacotes do protocolo UART-LED para a FRDM-KL25Z.

Pacote (6 bytes):  ID | R | G | B | ACAO | CHECK
ACAO: 0x01 LIGAR, 0x02 DESLIGAR, 0x03 TOGGLE, 0x04 PISCA 1s, 0x05 PISCA 2s
CHECK: CRC-8 (poly 0x07, init 0x00) [padrao do firmware] ou XOR dos 5 bytes.

Exemplos (Windows: a porta e algo como COM5; Linux: /dev/ttyACM0):
  python envia_pacote.py --listar
  python envia_pacote.py -p COM5 ligar 255 0 0
  python envia_pacote.py -p COM5 pisca1 0 0 255
  python envia_pacote.py -p COM5 desligar
  python envia_pacote.py -p COM5 bruto A5 FF 00 00 01 C8
  python envia_pacote.py -p COM5 bateria
  python envia_pacote.py -p COM5 --modo xor ligar 0 255 0   (firmware em modo XOR)

Requer: pip install pyserial
"""
import argparse
import random
import re
import sys
import time
from functools import reduce

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover
    print("Instale o pyserial:  pip install pyserial")
    sys.exit(2)

ID_PADRAO = 0xA5
ACOES = {
    "ligar": 0x01,
    "desligar": 0x02,
    "toggle": 0x03,
    "pisca1": 0x04,
    "pisca2": 0x05,
}


# ----------------------------------------------------------------- checksums
def _tabela_crc8(poly=0x07):
    tab = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFF if c & 0x80 else (c << 1) & 0xFF
        tab.append(c)
    return tab


_TAB = _tabela_crc8()


def crc8(dados):
    """CRC-8 poly 0x07, init 0x00, sem reflexao, sem XOR final."""
    c = 0
    for b in dados:
        c = _TAB[c ^ b]
    return c


def xor(dados):
    return reduce(lambda a, b: a ^ b, dados, 0)


def montar(id_, r, g, b, acao, modo="crc8"):
    dados = bytes([id_ & 0xFF, r & 0xFF, g & 0xFF, b & 0xFF, acao & 0xFF])
    check = crc8(dados) if modo == "crc8" else xor(dados)
    return dados + bytes([check])


def hexs(b):
    return " ".join(f"{x:02X}" for x in b)


# ------------------------------------------------------------------- serial
def abrir(porta, baud):
    s = serial.Serial(porta, baud, bytesize=8, parity="N", stopbits=1, timeout=0.05)
    time.sleep(0.3)
    s.reset_input_buffer()
    return s


def ler_respostas(s, espera):
    """Le linhas de texto durante 'espera' segundos."""
    fim = time.time() + espera
    dados = b""
    while time.time() < fim:
        dados += s.read(256)
    linhas = [l.strip() for l in dados.decode("ascii", "replace").splitlines()]
    return [l for l in linhas if l]


def enviar(s, pacote, espera=0.3, mostrar=True):
    s.write(pacote)
    s.flush()
    resp = ler_respostas(s, espera)
    if mostrar:
        print(f"TX: {hexs(pacote)}")
        for l in resp:
            print(f"RX: {l}")
        if not resp:
            print("RX: (sem resposta)")
    return resp


# ---------------------------------------------------------- bateria de testes
def casos_bateria(id_, modo):
    outro = "xor" if modo == "crc8" else "crc8"
    m = lambda r, g, b, a: montar(id_, r, g, b, a, modo)  # noqa: E731
    rnd = random.Random(42)
    lixo = bytes(v for v in (rnd.randrange(256) for _ in range(40)) if v != id_)[:7]
    id_errado = 0x11 if id_ != 0x11 else 0x22
    pac_check_errado = bytearray(m(255, 0, 0, 0x01))
    pac_check_errado[5] ^= 0xFF
    # (descricao, bytes, respostas esperadas [prefixos], tempo de observacao s)
    return [
        ("LIGAR vermelho", m(255, 0, 0, 0x01), ["OK LIGAR"], 1.0),
        ("LIGAR verde", m(0, 255, 0, 0x01), ["OK LIGAR"], 1.0),
        ("LIGAR azul", m(0, 0, 255, 0x01), ["OK LIGAR"], 1.0),
        ("LIGAR branco 50% (PWM)", m(128, 128, 128, 0x01), ["OK LIGAR"], 1.0),
        ("LIGAR laranja (PWM)", m(255, 64, 0, 0x01), ["OK LIGAR"], 1.0),
        ("TOGGLE (apaga)", m(0, 0, 0, 0x03), ["OK TOGGLE"], 1.0),
        ("TOGGLE (acende roxo)", m(128, 0, 255, 0x03), ["OK TOGGLE"], 1.0),
        ("PISCA 1s amarelo", m(255, 255, 0, 0x04), ["OK PISCA_1S"], 4.0),
        ("PISCA 2s ciano", m(0, 255, 255, 0x05), ["OK PISCA_2S"], 6.0),
        ("DESLIGAR", m(0, 0, 0, 0x02), ["OK DESLIGAR"], 1.0),
        ("ID errado", montar(id_errado, 255, 0, 0, 0x01, modo), ["ERR ID"], 0.3),
        ("CHECK errado", bytes(pac_check_errado), ["ERR CHECK"], 0.3),
        ("CHECK no modo errado (" + outro + ")", montar(id_, 255, 0, 0, 0x01, outro),
         ["ERR CHECK"], 0.3),
        ("ACAO desconhecida 0x09", m(0, 0, 0, 0x09), ["ERR ACAO"], 0.3),
        ("Pacote truncado (3 bytes)", m(255, 0, 0, 0x01)[:3], ["ERR TRUNCADO"], 0.3),
        ("Lixo aleatorio (7 bytes)", lixo, ["ERR ID", "ERR TRUNCADO"], 0.3),
        ("Lixo + pacote valido colado", bytes([0x00, 0x13, 0x37]) + m(0, 0, 255, 0x01),
         ["ERR ID", "OK LIGAR"], 1.0),
        ("Dois pacotes colados", m(255, 0, 0, 0x01) + m(0, 255, 0, 0x01),
         ["OK LIGAR", "OK LIGAR"], 1.0),
        ("Sistema segue vivo: DESLIGAR", m(0, 0, 0, 0x02), ["OK DESLIGAR"], 0.3),
    ]


def bateria(s, id_, modo):
    casos = casos_bateria(id_, modo)
    aprovados = 0
    print(f"Bateria de testes: {len(casos)} casos | ID=0x{id_:02X} | CHECK={modo}\n")
    for i, (desc, pac, esperado, obs) in enumerate(casos, 1):
        resp = enviar(s, pac, espera=obs, mostrar=False)
        resp = [l for l in resp if re.match(r"^(OK|ERR)\b", l)]
        ok = len(resp) == len(esperado) and all(
            r.startswith(e) for r, e in zip(resp, esperado))
        aprovados += ok
        print(f"[{i:02d}] {'PASSOU' if ok else 'FALHOU'}  {desc}")
        print(f"      TX: {hexs(pac)}")
        for l in resp or ["(sem resposta)"]:
            print(f"      RX: {l}")
        if not ok:
            print(f"      esperado: {esperado}")
    print(f"\nResultado: {aprovados}/{len(casos)} casos passaram")
    return 0 if aprovados == len(casos) else 1


# ---------------------------------------------------------------------- main
def num(txt):
    return int(txt, 0)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Envia pacotes UART-LED (6 bytes).")
    ap.add_argument("-p", "--porta", help="porta serial (ex.: COM5, /dev/ttyACM0)")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("--id", type=num, default=ID_PADRAO, help="ID do receptor (padrao 0xA5)")
    ap.add_argument("--modo", choices=["crc8", "xor"], default="crc8",
                    help="calculo do CHECK (deve ser igual ao do firmware)")
    ap.add_argument("--listar", action="store_true", help="lista as portas seriais")
    ap.add_argument("comando", nargs="?",
                    help="ligar | desligar | toggle | pisca1 | pisca2 | bruto | bateria")
    ap.add_argument("valores", nargs="*", help="R G B (0-255) ou bytes em hex para 'bruto'")
    a = ap.parse_args(argv)

    if a.listar:
        for p in serial.tools.list_ports.comports():
            print(f"{p.device:12s} {p.description}")
        return 0
    if not a.porta or not a.comando:
        ap.print_help()
        return 2

    s = abrir(a.porta, a.baud)
    try:
        if a.comando == "bateria":
            return bateria(s, a.id, a.modo)
        if a.comando == "bruto":
            pacote = bytes(int(v, 16) for v in a.valores)
        elif a.comando in ACOES:
            rgb = [num(v) for v in a.valores] + [0, 0, 0]
            pacote = montar(a.id, rgb[0], rgb[1], rgb[2], ACOES[a.comando], a.modo)
        else:
            ap.error(f"comando desconhecido: {a.comando}")
        enviar(s, pacote)
        return 0
    finally:
        s.close()


if __name__ == "__main__":
    sys.exit(main())
