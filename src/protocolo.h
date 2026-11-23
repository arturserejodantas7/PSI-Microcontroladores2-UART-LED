/*
 * protocolo.h - Protocolo fixo de 6 bytes para controle do LED RGB.
 *
 *   | ID | R | G | B | ACAO | CHECK |
 *
 * Este modulo e logica PURA (C99, sem Zephyr): pode ser compilado e testado
 * no PC. O receptor e alimentado byte a byte e informa quando um pacote foi
 * aceito ou descartado (ID errado, CHECK errado, acao desconhecida ou
 * pacote truncado por timeout).
 */
#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTO_TAM_PACOTE 6U
#define PROTO_TAM_DADOS  5U /* bytes cobertos pelo CHECK: ID R G B ACAO */

/* Codigos do campo ACAO */
enum proto_acao {
	ACAO_LIGAR = 0x01,
	ACAO_DESLIGAR = 0x02,
	ACAO_TOGGLE = 0x03,
	ACAO_PISCA_1S = 0x04,
	ACAO_PISCA_2S = 0x05,
};

/* Como o campo CHECK e calculado */
enum proto_modo_check {
	PROTO_CHECK_XOR = 0,  /* XOR dos 5 primeiros bytes (enunciado) */
	PROTO_CHECK_CRC8 = 1, /* CRC-8 poly 0x07, init 0x00 (bonus) */
};

/* Resultado de cada chamada do receptor */
enum proto_resultado {
	PROTO_INCOMPLETO = 0, /* nada a fazer ainda */
	PROTO_OK,             /* pacote valido: executar a acao */
	PROTO_ERR_ID,         /* byte 0 diferente do ID deste receptor */
	PROTO_ERR_CHECK,      /* CHECK recebido != CHECK calculado */
	PROTO_ERR_ACAO,       /* pacote integro, mas acao desconhecida */
	PROTO_ERR_TRUNCADO,   /* timeout com pacote incompleto */
};

struct proto_pacote {
	uint8_t id;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t acao;
	uint8_t check;
};

/* Descreve o que aconteceu (para executar a acao e montar a resposta) */
struct proto_evento {
	enum proto_resultado res;
	struct proto_pacote pac; /* bytes do quadro avaliado */
	uint8_t id_esperado;
	uint8_t check_calc;      /* CHECK calculado localmente */
	uint8_t n_bytes;         /* bytes descartados (truncado) */
};

struct proto_stats {
	uint32_t bytes;
	uint32_t ok;
	uint32_t err_id;
	uint32_t err_check;
	uint32_t err_acao;
	uint32_t err_truncado;
};

struct proto_receptor {
	uint8_t id_esperado;
	enum proto_modo_check modo;
	uint8_t buf[PROTO_TAM_PACOTE];
	uint8_t len;
	struct proto_stats st;
};

/* Checksums */
uint8_t proto_xor(const uint8_t *dados, size_t n);
uint8_t proto_crc8(const uint8_t *dados, size_t n);
uint8_t proto_calcular_check(enum proto_modo_check modo, const uint8_t *dados5);

/* Receptor */
void proto_init(struct proto_receptor *rx, uint8_t id, enum proto_modo_check modo);
enum proto_resultado proto_alimentar(struct proto_receptor *rx, uint8_t byte,
				     struct proto_evento *ev);
enum proto_resultado proto_timeout(struct proto_receptor *rx, struct proto_evento *ev);

/* Utilitarios */
bool proto_acao_valida(uint8_t acao);
const char *proto_nome_acao(uint8_t acao); /* "?" se invalida */
void proto_montar(uint8_t out[PROTO_TAM_PACOTE], uint8_t id, uint8_t r, uint8_t g,
		  uint8_t b, uint8_t acao, enum proto_modo_check modo);
int proto_descrever(const struct proto_evento *ev, char *out, size_t tam);

#endif /* PROTOCOLO_H_ */
