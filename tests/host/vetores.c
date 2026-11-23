/*
 * Gera vetores (dados de 5 bytes -> XOR e CRC-8) com a implementacao C do
 * firmware, para comparacao com a referencia em Python (valida_crc8.py).
 * Saida: uma linha por vetor "b0 b1 b2 b3 b4 xor crc" em hexadecimal.
 */
#include <stdint.h>
#include <stdio.h>

#include "protocolo.h"

int main(void)
{
	uint32_t s = 2024U;
	uint8_t d[5];

	for (int i = 0; i < 2000; i++) {
		for (int j = 0; j < 5; j++) {
			s = s * 1664525U + 1013904223U; /* LCG */
			d[j] = (uint8_t)(s >> 24);
		}
		printf("%02x %02x %02x %02x %02x %02x %02x\n", d[0], d[1], d[2], d[3], d[4],
		       proto_xor(d, 5), proto_crc8(d, 5));
	}
	printf("check %02x\n", proto_crc8((const uint8_t *)"123456789", 9));
	return 0;
}
