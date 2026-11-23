#!/usr/bin/env python3
"""Teste ponta a ponta no PC (Linux/macOS), sem a placa.

Cria uma pseudo-serial (pty), liga o simulador do firmware (sim_receptor,
que usa o mesmo protocolo.c/led_logica.c) a um lado e roda a bateria do
tools/envia_pacote.py no outro lado, nos modos CRC-8 e XOR.
"""
import os
import subprocess
import sys
import tty

AQUI = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(AQUI, "..", "..", "tools", "envia_pacote.py")


def rodar(modo):
    mestre, escravo = os.openpty()
    tty.setraw(mestre)
    tty.setraw(escravo)
    nome = os.ttyname(escravo)
    args = [os.path.join(AQUI, "sim_receptor"), str(mestre)]
    if modo == "xor":
        args.insert(1, "--xor")
    sim = subprocess.Popen(args, pass_fds=[mestre], stderr=subprocess.DEVNULL)
    try:
        r = subprocess.run([sys.executable, SCRIPT, "-p", nome, "--modo", modo, "bateria"])
        return r.returncode
    finally:
        sim.terminate()
        sim.wait()
        os.close(mestre)
        os.close(escravo)


if __name__ == "__main__":
    falhas = 0
    for modo in ("crc8", "xor"):
        print(f"===== simulador em modo {modo.upper()} =====")
        falhas += rodar(modo) != 0
    print("E2E:", "OK" if falhas == 0 else f"{falhas} modo(s) com falha")
    sys.exit(1 if falhas else 0)
