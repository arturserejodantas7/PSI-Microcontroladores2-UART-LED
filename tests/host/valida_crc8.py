#!/usr/bin/env python3
"""Confere o XOR e o CRC-8 do firmware (C) contra uma referencia em Python.

A referencia usa um algoritmo diferente (por tabela) do usado no C (bit a bit).
Uso: ./vetores | python3 valida_crc8.py
"""
import sys
from functools import reduce


def tabela_crc8(poly=0x07):
    tab = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFF if c & 0x80 else (c << 1) & 0xFF
        tab.append(c)
    return tab


TAB = tabela_crc8()


def crc8(dados, init=0x00):
    c = init
    for b in dados:
        c = TAB[c ^ b]
    return c


def xor(dados):
    return reduce(lambda a, b: a ^ b, dados, 0)


def main():
    n = erros = 0
    for linha in sys.stdin:
        partes = linha.split()
        if partes[0] == "check":
            ref = crc8(b"123456789")
            ok = int(partes[1], 16) == ref == 0xF4
            print(f"CRC-8('123456789'): C=0x{partes[1].upper()} Python=0x{ref:02X} "
                  f"(esperado 0xF4) -> {'OK' if ok else 'ERRO'}")
            erros += 0 if ok else 1
            continue
        v = [int(p, 16) for p in partes]
        dados, x_c, crc_c = v[:5], v[5], v[6]
        n += 1
        if xor(dados) != x_c or crc8(dados) != crc_c:
            erros += 1
            print("DIVERGENCIA:", linha.strip())
    print(f"{n} vetores comparados, {erros} divergencia(s)")
    return 1 if erros else 0


if __name__ == "__main__":
    sys.exit(main())
