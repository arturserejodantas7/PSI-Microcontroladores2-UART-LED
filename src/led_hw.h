/*
 * led_hw.h - Saida no LED RGB onboard (PWM por TPM, ou GPIO) e pisca por
 * k_timer.
 */
#ifndef LED_HW_H_
#define LED_HW_H_

#include <stdint.h>

int led_hw_init(void);
void led_hw_executar(uint8_t acao, uint8_t r, uint8_t g, uint8_t b);
const char *led_hw_modo(void); /* "PWM" ou "GPIO" */

#endif
