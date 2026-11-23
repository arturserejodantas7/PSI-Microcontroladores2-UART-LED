/*
 * led_logica.h - Estado do LED RGB e efeito de cada acao (logica pura).
 *
 * Regras:
 *  - LIGAR: acende com a cor (R,G,B) e para de piscar.
 *  - DESLIGAR: apaga e para de piscar (a cor e lembrada).
 *  - TOGGLE: se aceso (ou piscando) apaga; se apagado acende com (R,G,B).
 *  - PISCA_1S / PISCA_2S: pisca com a cor (R,G,B); o LED troca de estado a
 *    cada 1 s / 2 s (1 s aceso, 1 s apagado; ou 2 s aceso, 2 s apagado).
 *  - Se R=G=B=0 numa acao que acende, usa-se a ultima cor (inicial: branco).
 */
#ifndef LED_LOGICA_H_
#define LED_LOGICA_H_

#include <stdbool.h>
#include <stdint.h>

#define LED_MEIO_PERIODO_PISCA_1S_MS 1000U
#define LED_MEIO_PERIODO_PISCA_2S_MS 2000U

struct led_estado {
	uint8_t cor[3];          /* R, G, B (0..255) */
	bool ligado;             /* aceso fixo ou piscando */
	uint16_t meio_periodo_ms; /* 0 = nao pisca */
	bool fase_acesa;         /* fase atual do pisca */
};

void led_logica_init(struct led_estado *e);
/* Aplica a acao; retorna o meio periodo do pisca em ms (0 = parar timer). */
uint16_t led_logica_aplicar(struct led_estado *e, uint8_t acao, uint8_t r, uint8_t g,
			    uint8_t b);
/* Chamado a cada meio periodo pelo timer: alterna a fase se estiver piscando. */
void led_logica_tick(struct led_estado *e);
/* Intensidade que deve sair em cada canal agora (0 = apagado). */
void led_logica_saida(const struct led_estado *e, uint8_t rgb[3]);

#endif /* LED_LOGICA_H_ */
