/*
 * Testes unitarios (ztest) da logica pura: checksums, receptor de pacotes
 * e maquina de estados do LED. Rodam em native_sim, na placa ou no PC
 * (tests/host, com um shim minimo da API do ztest).
 */
#include <string.h>
#include <zephyr/ztest.h>

#include "led_logica.h"
#include "protocolo.h"

#define ID 0xA5

static struct proto_receptor rx;
static struct proto_evento ev;

/* Alimenta n bytes; devolve quantos eventos (resultado != INCOMPLETO) ocorreram
 * e guarda os resultados em res[] (ate max). */
static int alimentar(const uint8_t *d, size_t n, enum proto_resultado *res, int max)
{
	int k = 0;

	for (size_t i = 0; i < n; i++) {
		enum proto_resultado r = proto_alimentar(&rx, d[i], &ev);

		if (r != PROTO_INCOMPLETO) {
			if (k < max) {
				res[k] = r;
			}
			k++;
		}
	}
	return k;
}

static void *preparar(void)
{
	return NULL;
}

static void antes(void *f)
{
	(void)f;
	proto_init(&rx, ID, PROTO_CHECK_CRC8);
	memset(&ev, 0, sizeof(ev));
}

ZTEST_SUITE(protocolo, NULL, preparar, antes, NULL, NULL);

/* ---------- checksums ---------- */

ZTEST(protocolo, test_crc8_valor_de_verificacao)
{
	const uint8_t s[] = "123456789";

	/* CRC-8 poly 0x07 init 0x00 (CRC-8/SMBUS): check = 0xF4 */
	zassert_equal(proto_crc8(s, 9), 0xF4, "CRC-8 de '123456789' deveria ser 0xF4");
	zassert_equal(proto_crc8(s, 0), 0x00, "CRC de 0 bytes = init");
}

ZTEST(protocolo, test_crc8_vetores_do_protocolo)
{
	/* valores conferidos com implementacao de referencia em Python */
	const uint8_t p1[] = {0xA5, 0xFF, 0x00, 0x00, 0x01};
	const uint8_t p2[] = {0xA5, 0x00, 0x00, 0x00, 0x02};
	const uint8_t p3[] = {0xA5, 0x80, 0x80, 0x80, 0x01};
	const uint8_t p4[] = {0xA5, 0x00, 0xFF, 0xFF, 0x05};

	zassert_equal(proto_crc8(p1, 5), 0xC8);
	zassert_equal(proto_crc8(p2, 5), 0x10);
	zassert_equal(proto_crc8(p3, 5), 0x95);
	zassert_equal(proto_crc8(p4, 5), 0xF9);
}

ZTEST(protocolo, test_xor_vetores_do_protocolo)
{
	const uint8_t p1[] = {0xA5, 0xFF, 0x00, 0x00, 0x01};
	const uint8_t p3[] = {0xA5, 0x80, 0x80, 0x80, 0x01};

	zassert_equal(proto_xor(p1, 5), 0x5B);
	zassert_equal(proto_xor(p3, 5), 0x24);
}

ZTEST(protocolo, test_montar_pacote)
{
	uint8_t p[6];

	proto_montar(p, ID, 255, 0, 0, ACAO_LIGAR, PROTO_CHECK_CRC8);
	zassert_equal(p[5], 0xC8);
	proto_montar(p, ID, 255, 0, 0, ACAO_LIGAR, PROTO_CHECK_XOR);
	zassert_equal(p[5], 0x5B);
}

/* ---------- receptor: casos validos ---------- */

ZTEST(protocolo, test_pacote_valido_crc8)
{
	const uint8_t p[] = {0xA5, 0xFF, 0x00, 0x00, 0x01, 0xC8};
	enum proto_resultado r[4];

	zassert_equal(alimentar(p, 5, r, 4), 0, "nao pode decidir antes de 6 bytes");
	zassert_equal(proto_alimentar(&rx, p[5], &ev), PROTO_OK);
	zassert_equal(ev.pac.r, 255);
	zassert_equal(ev.pac.acao, ACAO_LIGAR);
	zassert_equal(rx.len, 0);
	zassert_equal(rx.st.ok, 1);
}

ZTEST(protocolo, test_pacote_valido_xor)
{
	const uint8_t p[] = {0xA5, 0xFF, 0x00, 0x00, 0x01, 0x5B};
	enum proto_resultado r[2];

	proto_init(&rx, ID, PROTO_CHECK_XOR);
	zassert_equal(alimentar(p, 6, r, 2), 1);
	zassert_equal(r[0], PROTO_OK);
}

ZTEST(protocolo, test_dois_pacotes_colados)
{
	uint8_t d[12];
	enum proto_resultado r[4];

	proto_montar(&d[0], ID, 1, 2, 3, ACAO_LIGAR, PROTO_CHECK_CRC8);
	proto_montar(&d[6], ID, 0, 0, 0, ACAO_DESLIGAR, PROTO_CHECK_CRC8);
	zassert_equal(alimentar(d, 12, r, 4), 2);
	zassert_equal(r[0], PROTO_OK);
	zassert_equal(r[1], PROTO_OK);
	zassert_equal(ev.pac.acao, ACAO_DESLIGAR);
}

ZTEST(protocolo, test_todas_as_acoes_validas)
{
	uint8_t p[6];
	enum proto_resultado r[2];

	for (uint8_t a = ACAO_LIGAR; a <= ACAO_PISCA_2S; a++) {
		proto_montar(p, ID, 10, 20, 30, a, PROTO_CHECK_CRC8);
		zassert_equal(alimentar(p, 6, r, 2), 1);
		zassert_equal(r[0], PROTO_OK, "acao 0x%02X deveria ser aceita", a);
	}
}

/* ---------- receptor: pacotes invalidos ---------- */

ZTEST(protocolo, test_id_errado)
{
	uint8_t p[6];
	enum proto_resultado r[2];

	proto_montar(p, 0x11, 255, 0, 0, ACAO_LIGAR, PROTO_CHECK_CRC8);
	zassert_equal(alimentar(p, 6, r, 2), 1);
	zassert_equal(r[0], PROTO_ERR_ID);
	zassert_equal(ev.pac.id, 0x11);
	zassert_equal(rx.len, 0, "sem ID no quadro: buffer deve ser esvaziado");
	zassert_equal(rx.st.err_id, 1);
}

ZTEST(protocolo, test_check_errado)
{
	const uint8_t p[] = {0xA5, 0xFF, 0x00, 0x00, 0x01, 0xC9};
	enum proto_resultado r[2];

	zassert_equal(alimentar(p, 6, r, 2), 1);
	zassert_equal(r[0], PROTO_ERR_CHECK);
	zassert_equal(ev.check_calc, 0xC8);
	zassert_equal(rx.len, 0);
}

ZTEST(protocolo, test_modo_trocado_xor_em_receptor_crc)
{
	uint8_t p[6];
	enum proto_resultado r[2];

	proto_montar(p, ID, 255, 0, 0, ACAO_LIGAR, PROTO_CHECK_XOR);
	zassert_equal(alimentar(p, 6, r, 2), 1);
	zassert_equal(r[0], PROTO_ERR_CHECK);
}

ZTEST(protocolo, test_acao_desconhecida)
{
	uint8_t p[6];
	enum proto_resultado r[2];

	proto_montar(p, ID, 0, 0, 0, 0x09, PROTO_CHECK_CRC8);
	zassert_equal(alimentar(p, 6, r, 2), 1);
	zassert_equal(r[0], PROTO_ERR_ACAO);
	zassert_equal(rx.len, 0);

	proto_montar(p, ID, 0, 0, 0, 0x00, PROTO_CHECK_CRC8);
	zassert_equal(alimentar(p, 6, r, 2), 1);
	zassert_equal(r[0], PROTO_ERR_ACAO);
}

ZTEST(protocolo, test_truncado_por_timeout)
{
	const uint8_t p[] = {0xA5, 0xFF, 0x00};
	enum proto_resultado r[2];

	zassert_equal(proto_timeout(&rx, &ev), PROTO_INCOMPLETO, "sem bytes: nada a fazer");
	zassert_equal(alimentar(p, 3, r, 2), 0);
	zassert_equal(proto_timeout(&rx, &ev), PROTO_ERR_TRUNCADO);
	zassert_equal(ev.n_bytes, 3);
	zassert_equal(rx.len, 0);
	zassert_equal(rx.st.err_truncado, 1);
}

ZTEST(protocolo, test_recupera_apos_truncado)
{
	uint8_t p[6];
	enum proto_resultado r[2];

	proto_montar(p, ID, 0, 255, 0, ACAO_LIGAR, PROTO_CHECK_CRC8);
	alimentar(p, 4, r, 2);
	zassert_equal(proto_timeout(&rx, &ev), PROTO_ERR_TRUNCADO);
	zassert_equal(alimentar(p, 6, r, 2), 1);
	zassert_equal(r[0], PROTO_OK);
}

ZTEST(protocolo, test_lixo_antes_de_pacote_valido_ressincroniza)
{
	/* 3 bytes de lixo + pacote valido, sem pausa entre eles */
	uint8_t d[9] = {0x00, 0x13, 0x37};
	enum proto_resultado r[4];

	proto_montar(&d[3], ID, 0, 0, 255, ACAO_LIGAR, PROTO_CHECK_CRC8);
	zassert_equal(alimentar(d, 9, r, 4), 2);
	zassert_equal(r[0], PROTO_ERR_ID);
	zassert_equal(r[1], PROTO_OK, "pacote valido apos lixo deve ser recuperado");
	zassert_equal(ev.pac.b, 255);
}

ZTEST(protocolo, test_check_errado_com_id_no_meio_ressincroniza)
{
	/* quadro corrompido que contem 0xA5 no meio, seguido de pacote valido */
	uint8_t d[12] = {0xA5, 0x10, 0xA5, 0x20, 0x01, 0x00};
	enum proto_resultado r[4];
	int n;

	proto_montar(&d[6], ID, 7, 8, 9, ACAO_TOGGLE, PROTO_CHECK_CRC8);
	n = alimentar(d, 12, r, 4);
	zassert_true(n >= 2, "deve haver erro(s) e depois o OK");
	zassert_equal(r[0], PROTO_ERR_CHECK);
	zassert_equal(r[n - 1], PROTO_OK, "ultimo evento deve ser o pacote valido");
	zassert_equal(ev.pac.acao, ACAO_TOGGLE);
}

ZTEST(protocolo, test_lixo_longo_nao_trava)
{
	/* gerador pseudoaleatorio simples (LCG), deterministico */
	uint32_t s = 12345U;
	enum proto_resultado r[1];
	uint8_t p[6];

	for (int i = 0; i < 5000; i++) {
		s = s * 1103515245U + 12345U;
		uint8_t c = (uint8_t)(s >> 16);

		(void)proto_alimentar(&rx, c, &ev);
		zassert_true(rx.len < PROTO_TAM_PACOTE, "buffer nunca pode estourar");
	}
	(void)proto_timeout(&rx, &ev);
	zassert_equal(rx.len, 0);
	proto_montar(p, ID, 1, 1, 1, ACAO_LIGAR, PROTO_CHECK_CRC8);
	zassert_equal(alimentar(p, 6, r, 1), 1);
	zassert_equal(r[0], PROTO_OK, "apos lixo + timeout o receptor volta a aceitar");
}

ZTEST(protocolo, test_descrever_respostas)
{
	char s[64];
	const uint8_t p[] = {0xA5, 0xFF, 0x00, 0x00, 0x01, 0xC8};
	enum proto_resultado r[1];

	alimentar(p, 6, r, 1);
	proto_descrever(&ev, s, sizeof(s));
	zassert_equal(strcmp(s, "OK LIGAR R=255 G=0 B=0"), 0, "%s", s);

	alimentar((const uint8_t *)"\xA5\x01", 2, r, 1);
	proto_timeout(&rx, &ev);
	proto_descrever(&ev, s, sizeof(s));
	zassert_equal(strcmp(s, "ERR TRUNCADO 2 byte(s) descartado(s)"), 0, "%s", s);
}

/* ---------- logica do LED ---------- */

ZTEST(protocolo, test_led_ligar_desligar)
{
	struct led_estado e;
	uint8_t o[3];

	led_logica_init(&e);
	led_logica_saida(&e, o);
	zassert_equal(o[0] | o[1] | o[2], 0, "inicia apagado");

	zassert_equal(led_logica_aplicar(&e, ACAO_LIGAR, 255, 128, 0), 0);
	led_logica_saida(&e, o);
	zassert_equal(o[0], 255);
	zassert_equal(o[1], 128);
	zassert_equal(o[2], 0);

	led_logica_aplicar(&e, ACAO_DESLIGAR, 0, 0, 0);
	led_logica_saida(&e, o);
	zassert_equal(o[0] | o[1] | o[2], 0);
}

ZTEST(protocolo, test_led_toggle)
{
	struct led_estado e;
	uint8_t o[3];

	led_logica_init(&e);
	led_logica_aplicar(&e, ACAO_TOGGLE, 0, 0, 200);
	led_logica_saida(&e, o);
	zassert_equal(o[2], 200, "toggle de apagado acende com a cor do pacote");
	led_logica_aplicar(&e, ACAO_TOGGLE, 0, 0, 0);
	led_logica_saida(&e, o);
	zassert_equal(o[2], 0, "toggle de aceso apaga");
	led_logica_aplicar(&e, ACAO_TOGGLE, 0, 0, 0);
	led_logica_saida(&e, o);
	zassert_equal(o[2], 200, "RGB=0 reaproveita a ultima cor");
}

ZTEST(protocolo, test_led_pisca)
{
	struct led_estado e;
	uint8_t o[3];

	led_logica_init(&e);
	zassert_equal(led_logica_aplicar(&e, ACAO_PISCA_1S, 255, 255, 0),
		      LED_MEIO_PERIODO_PISCA_1S_MS);
	led_logica_saida(&e, o);
	zassert_equal(o[0], 255, "pisca comeca aceso");
	led_logica_tick(&e);
	led_logica_saida(&e, o);
	zassert_equal(o[0], 0);
	led_logica_tick(&e);
	led_logica_saida(&e, o);
	zassert_equal(o[0], 255);

	zassert_equal(led_logica_aplicar(&e, ACAO_PISCA_2S, 0, 0, 0),
		      LED_MEIO_PERIODO_PISCA_2S_MS);
	zassert_equal(led_logica_aplicar(&e, ACAO_LIGAR, 0, 0, 0), 0, "LIGAR para o pisca");
	led_logica_tick(&e);
	led_logica_saida(&e, o);
	zassert_equal(o[0], 255, "tick sem pisca nao muda nada");
}

ZTEST(protocolo, test_led_toggle_durante_pisca_apaga)
{
	struct led_estado e;
	uint8_t o[3];

	led_logica_init(&e);
	led_logica_aplicar(&e, ACAO_PISCA_2S, 9, 9, 9);
	zassert_equal(led_logica_aplicar(&e, ACAO_TOGGLE, 0, 0, 0), 0);
	led_logica_tick(&e);
	led_logica_saida(&e, o);
	zassert_equal(o[0] | o[1] | o[2], 0);
}
