/*
 * led_hw.c - Aciona o LED RGB da FRDM-KL25Z conforme led_logica.
 *
 *  - Modo PWM (bonus): se o devicetree tem os aliases red/green/blue-pwm-led
 *    (boards/frdm_kl25z.overlay), a intensidade 0..255 vira largura de pulso.
 *  - Modo GPIO: sem os aliases, usa os nos red_led/green_led/blue_led da
 *    placa; canal aceso se intensidade > 0.
 *
 * Pisca: um k_timer periodico (meio periodo = 1 s ou 2 s) submete um k_work;
 * o work alterna a fase e atualiza os pinos em contexto de thread. O estado
 * e protegido por mutex (a thread de protocolo tambem o altera).
 */
#include "led_hw.h"

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include "led_logica.h"

#if DT_HAS_ALIAS(red_pwm_led) && DT_HAS_ALIAS(green_pwm_led) && DT_HAS_ALIAS(blue_pwm_led)
#define USA_PWM 1
#include <zephyr/drivers/pwm.h>

static const struct pwm_dt_spec canais[3] = {
	PWM_DT_SPEC_GET(DT_ALIAS(red_pwm_led)),
	PWM_DT_SPEC_GET(DT_ALIAS(green_pwm_led)),
	PWM_DT_SPEC_GET(DT_ALIAS(blue_pwm_led)),
};

#if defined(CONFIG_SOC_MKL25Z4)
#include <fsl_clock.h>
/*
 * Fonte do contador dos TPM: SIM_SOPT2[TPMSRC] = 1 -> MCGFLLCLK ou
 * MCGPLLCLK/2 (a placa seleciona PLL: 96 MHz / 2 = 48 MHz). Sem isso o
 * TPMSRC fica 0 (clock desligado) e o PWM nao gira. Roda antes da
 * inicializacao do driver PWM (POST_KERNEL).
 */
static int kl25z_tpm_clock_init(void)
{
	CLOCK_SetTpmClock(1U);
	return 0;
}
SYS_INIT(kl25z_tpm_clock_init, PRE_KERNEL_1, 0);
#endif

#else
#define USA_PWM 0
#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec canais[3] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(red_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(green_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led), gpios),
};
#endif

static struct led_estado estado;
K_MUTEX_DEFINE(mtx_led);

static void escrever_saida(void)
{
	uint8_t v[3];

	led_logica_saida(&estado, v);
	for (int i = 0; i < 3; i++) {
#if USA_PWM
		uint32_t pulso = (uint32_t)(((uint64_t)canais[i].period * v[i]) / 255U);

		(void)pwm_set_pulse_dt(&canais[i], pulso);
#else
		(void)gpio_pin_set_dt(&canais[i], v[i] > 0U);
#endif
	}
}

static void pisca_work_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	k_mutex_lock(&mtx_led, K_FOREVER);
	led_logica_tick(&estado);
	escrever_saida();
	k_mutex_unlock(&mtx_led);
}

K_WORK_DEFINE(pisca_work, pisca_work_fn);

/* roda em contexto de interrupcao: so agenda o trabalho */
static void pisca_timer_fn(struct k_timer *t)
{
	ARG_UNUSED(t);
	k_work_submit(&pisca_work);
}

K_TIMER_DEFINE(pisca_timer, pisca_timer_fn, NULL);

int led_hw_init(void)
{
	led_logica_init(&estado);
	for (int i = 0; i < 3; i++) {
#if USA_PWM
		if (!pwm_is_ready_dt(&canais[i])) {
			return -ENODEV;
		}
#else
		if (!gpio_is_ready_dt(&canais[i])) {
			return -ENODEV;
		}
		int ret = gpio_pin_configure_dt(&canais[i], GPIO_OUTPUT_INACTIVE);

		if (ret < 0) {
			return ret;
		}
#endif
	}
	escrever_saida(); /* comeca apagado */
	return 0;
}

void led_hw_executar(uint8_t acao, uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t meio_ms;

	k_mutex_lock(&mtx_led, K_FOREVER);
	meio_ms = led_logica_aplicar(&estado, acao, r, g, b);
	/* reinicia o pisca do zero e descarta um tick antigo ainda pendente */
	k_timer_stop(&pisca_timer);
	(void)k_work_cancel(&pisca_work);
	if (meio_ms != 0U) {
		k_timer_start(&pisca_timer, K_MSEC(meio_ms), K_MSEC(meio_ms));
	}
	escrever_saida();
	k_mutex_unlock(&mtx_led);
}

const char *led_hw_modo(void)
{
	return USA_PWM ? "PWM" : "GPIO";
}
