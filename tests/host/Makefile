# Testes no PC (sem Zephyr): compila a logica pura do firmware com gcc.
#   make        -> compila e roda os testes unitarios + validacao do CRC-8
#   make e2e    -> roda o script de teste do PC contra o simulador (pty)
#   make clean
CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Werror -D_DEFAULT_SOURCE
SRC     := ../../src
CFLAGS  += -I$(SRC) -Ishim
PY      ?= python3

all: testes crc8

unit_protocolo: ../unit_protocolo/src/main.c shim/ztest_shim.c $(SRC)/protocolo.c $(SRC)/led_logica.c
	$(CC) $(CFLAGS) -o $@ $^

vetores: vetores.c $(SRC)/protocolo.c
	$(CC) $(CFLAGS) -o $@ $^

sim_receptor: sim_receptor.c $(SRC)/protocolo.c $(SRC)/led_logica.c
	$(CC) $(CFLAGS) -o $@ $^

testes: unit_protocolo
	./unit_protocolo

crc8: vetores
	./vetores | $(PY) valida_crc8.py

e2e: sim_receptor
	$(PY) e2e_pty.py

clean:
	rm -f unit_protocolo vetores sim_receptor

.PHONY: all testes crc8 e2e clean
