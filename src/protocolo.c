/*
 * protocolo.c - Receptor do protocolo de 6 bytes (logica pura, sem Zephyr).
 *
 * Enquadramento:
 *  - Os bytes sao acumulados em buf[] ate completar 6.
 *  - Com 6 bytes o quadro e validado na ordem: ID -> CHECK -> ACAO.
 *  - Quadro valido (ou integro com acao desconhecida): buf e esvaziado.
 *  - ID ou CHECK invalido: ressincronizacao por janela deslizante. Procura-se
 *    o proximo byte igual ao ID dentro de buf[1..5]; se existir, os bytes a
 *    partir dele viram o inicio do proximo quadro (nao se perde um pacote bom
 *    que veio "colado" depois de lixo). Se nao existir, buf e esvaziado.
 *  - Timeout entre bytes (chamado pela aplicacao): se havia bytes parciais,
 *    eles sao descartados e reportados como pacote truncado.
 */
#include "protocolo.h"

#include <stdio.h>
#include <string.h>

uint8_t proto_xor(const uint8_t *dados, size_t n)
{
	uint8_t x = 0U;

	for (size_t i = 0; i < n; i++) {
		x ^= dados[i];
	}
	return x;
}

/*
 * CRC-8 "classico": polinomio x^8 + x^2 + x + 1 (0x07), valor inicial 0x00,
 * sem reflexao e sem XOR final (catalogado como CRC-8/SMBUS, tambem chamado
 * CRC-8/ATM). Valor de verificacao: CRC("123456789") = 0xF4.
 * Implementacao bit a bit (sem tabela) para economizar flash.
 */
uint8_t proto_crc8(const uint8_t *dados, size_t n)
{
	uint8_t crc = 0x00U;

	for (size_t i = 0; i < n; i++) {
		crc ^= dados[i];
		for (int bit = 0; bit < 8; bit++) {
			if ((crc & 0x80U) != 0U) {
				crc = (uint8_t)((crc << 1) ^ 0x07U);
			} else {
				crc = (uint8_t)(crc << 1);
			}
		}
	}
	return crc;
}

uint8_t proto_calcular_check(enum proto_modo_check modo, const uint8_t *dados5)
{
	return (modo == PROTO_CHECK_CRC8) ? proto_crc8(dados5, PROTO_TAM_DADOS)
					  : proto_xor(dados5, PROTO_TAM_DADOS);
}

bool proto_acao_valida(uint8_t acao)
{
	return (acao >= ACAO_LIGAR) && (acao <= ACAO_PISCA_2S);
}

const char *proto_nome_acao(uint8_t acao)
{
	switch (acao) {
	case ACAO_LIGAR:
		return "LIGAR";
	case ACAO_DESLIGAR:
		return "DESLIGAR";
	case ACAO_TOGGLE:
		return "TOGGLE";
	case ACAO_PISCA_1S:
		return "PISCA_1S";
	case ACAO_PISCA_2S:
		return "PISCA_2S";
	default:
		return "?";
	}
}

void proto_montar(uint8_t out[PROTO_TAM_PACOTE], uint8_t id, uint8_t r, uint8_t g,
		  uint8_t b, uint8_t acao, enum proto_modo_check modo)
{
	out[0] = id;
	out[1] = r;
	out[2] = g;
	out[3] = b;
	out[4] = acao;
	out[5] = proto_calcular_check(modo, out);
}

void proto_init(struct proto_receptor *rx, uint8_t id, enum proto_modo_check modo)
{
	memset(rx, 0, sizeof(*rx));
	rx->id_esperado = id;
	rx->modo = modo;
}

static void preencher_evento(const struct proto_receptor *rx, struct proto_evento *ev,
			     enum proto_resultado res)
{
	ev->res = res;
	ev->pac.id = rx->buf[0];
	ev->pac.r = rx->buf[1];
	ev->pac.g = rx->buf[2];
	ev->pac.b = rx->buf[3];
	ev->pac.acao = rx->buf[4];
	ev->pac.check = rx->buf[5];
	ev->id_esperado = rx->id_esperado;
	ev->check_calc = proto_calcular_check(rx->modo, rx->buf);
	ev->n_bytes = rx->len;
}

/* Descarta o quadro atual, mantendo os bytes a partir do proximo ID. */
static void ressincronizar(struct proto_receptor *rx)
{
	uint8_t k;

	for (k = 1U; k < rx->len; k++) {
		if (rx->buf[k] == rx->id_esperado) {
			break;
		}
	}
	rx->len = (uint8_t)(rx->len - k);
	memmove(rx->buf, &rx->buf[k], rx->len);
}

enum proto_resultado proto_alimentar(struct proto_receptor *rx, uint8_t byte,
				     struct proto_evento *ev)
{
	enum proto_resultado res;

	rx->st.bytes++;
	rx->buf[rx->len++] = byte;

	if (rx->len < PROTO_TAM_PACOTE) {
		return PROTO_INCOMPLETO;
	}

	if (rx->buf[0] != rx->id_esperado) {
		res = PROTO_ERR_ID;
	} else if (rx->buf[5] != proto_calcular_check(rx->modo, rx->buf)) {
		res = PROTO_ERR_CHECK;
	} else if (!proto_acao_valida(rx->buf[4])) {
		res = PROTO_ERR_ACAO;
	} else {
		res = PROTO_OK;
	}

	preencher_evento(rx, ev, res);

	switch (res) {
	case PROTO_OK:
		rx->st.ok++;
		rx->len = 0U;
		break;
	case PROTO_ERR_ACAO:
		/* quadro integro (ID e CHECK conferem): nao ha o que ressincronizar */
		rx->st.err_acao++;
		rx->len = 0U;
		break;
	case PROTO_ERR_ID:
		rx->st.err_id++;
		ressincronizar(rx);
		break;
	default: /* PROTO_ERR_CHECK */
		rx->st.err_check++;
		ressincronizar(rx);
		break;
	}
	return res;
}

enum proto_resultado proto_timeout(struct proto_receptor *rx, struct proto_evento *ev)
{
	if (rx->len == 0U) {
		return PROTO_INCOMPLETO;
	}
	/* zera o restante do buffer so para o evento ficar limpo */
	memset(&rx->buf[rx->len], 0, PROTO_TAM_PACOTE - rx->len);
	preencher_evento(rx, ev, PROTO_ERR_TRUNCADO);
	rx->st.err_truncado++;
	rx->len = 0U;
	return PROTO_ERR_TRUNCADO;
}

int proto_descrever(const struct proto_evento *ev, char *out, size_t tam)
{
	switch (ev->res) {
	case PROTO_OK:
		if (ev->pac.acao == ACAO_DESLIGAR) {
			return snprintf(out, tam, "OK DESLIGAR");
		}
		return snprintf(out, tam, "OK %s R=%u G=%u B=%u", proto_nome_acao(ev->pac.acao),
				ev->pac.r, ev->pac.g, ev->pac.b);
	case PROTO_ERR_ID:
		return snprintf(out, tam, "ERR ID recebido=0x%02X esperado=0x%02X", ev->pac.id,
				ev->id_esperado);
	case PROTO_ERR_CHECK:
		return snprintf(out, tam, "ERR CHECK recebido=0x%02X calculado=0x%02X",
				ev->pac.check, ev->check_calc);
	case PROTO_ERR_ACAO:
		return snprintf(out, tam, "ERR ACAO 0x%02X desconhecida", ev->pac.acao);
	case PROTO_ERR_TRUNCADO:
		return snprintf(out, tam, "ERR TRUNCADO %u byte(s) descartado(s)", ev->n_bytes);
	default:
		return snprintf(out, tam, "?");
	}
}
