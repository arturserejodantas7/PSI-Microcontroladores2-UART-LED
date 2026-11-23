# Testes no PC (sem Zephyr): compila a logica pura do firmware com gcc.
#   make        -> compila e roda os testes unitarios + validacao do CRC-8
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

testes: unit_protocolo
	./unit_protocolo

crc8: vetores
	./vetores | $(PY) valida_crc8.py

clean:
	rm -f unit_protocolo vetores

.PHONY: all testes crc8 clean
