/*
 * led_logica.c - Maquina de estados do LED RGB (logica pura, sem Zephyr).
 */
#include "led_logica.h"

#include "protocolo.h"

void led_logica_init(struct led_estado *e)
{
	e->cor[0] = 255U;
	e->cor[1] = 255U;
	e->cor[2] = 255U;
	e->ligado = false;
	e->meio_periodo_ms = 0U;
	e->fase_acesa = false;
}

static void definir_cor(struct led_estado *e, uint8_t r, uint8_t g, uint8_t b)
{
	if ((r | g | b) != 0U) {
		e->cor[0] = r;
		e->cor[1] = g;
		e->cor[2] = b;
	}
}

uint16_t led_logica_aplicar(struct led_estado *e, uint8_t acao, uint8_t r, uint8_t g,
			    uint8_t b)
{
	switch (acao) {
	case ACAO_LIGAR:
		definir_cor(e, r, g, b);
		e->ligado = true;
		e->meio_periodo_ms = 0U;
		break;
	case ACAO_DESLIGAR:
		e->ligado = false;
		e->meio_periodo_ms = 0U;
		break;
	case ACAO_TOGGLE:
		e->meio_periodo_ms = 0U;
		if (e->ligado) {
			e->ligado = false;
		} else {
			definir_cor(e, r, g, b);
			e->ligado = true;
		}
		break;
	case ACAO_PISCA_1S:
	case ACAO_PISCA_2S:
		definir_cor(e, r, g, b);
		e->ligado = true;
		e->meio_periodo_ms = (acao == ACAO_PISCA_1S) ? LED_MEIO_PERIODO_PISCA_1S_MS
							     : LED_MEIO_PERIODO_PISCA_2S_MS;
		break;
	default:
		/* acao desconhecida: estado inalterado (o protocolo ja filtra) */
		break;
	}
	e->fase_acesa = e->ligado; /* pisca sempre comeca aceso */
	return e->meio_periodo_ms;
}

void led_logica_tick(struct led_estado *e)
{
	if (e->ligado && (e->meio_periodo_ms != 0U)) {
		e->fase_acesa = !e->fase_acesa;
	}
}

void led_logica_saida(const struct led_estado *e, uint8_t rgb[3])
{
	bool aceso = e->ligado && ((e->meio_periodo_ms == 0U) || e->fase_acesa);

	for (int i = 0; i < 3; i++) {
		rgb[i] = aceso ? e->cor[i] : 0U;
	}
}
