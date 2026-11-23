/*
 * PSI - Microcontroladores 2 - Controle do LED RGB pela UART (FRDM-KL25Z)
 *
 * Fluxo:
 *   UART0 RX --ISR--> k_msgq (bytes) --> thread main: receptor do protocolo
 *   (timeout entre bytes) --> acao no LED (led_hw) + resposta em texto.
 *
 * Pacote: ID | R | G | B | ACAO | CHECK   (115200 8N1)
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "led_hw.h"
#include "protocolo.h"
#include "uart_rx.h"

#if defined(CONFIG_APP_CHECK_CRC8)
#define MODO_CHECK PROTO_CHECK_CRC8
#define NOME_CHECK "CRC-8"
#else
#define MODO_CHECK PROTO_CHECK_XOR
#define NOME_CHECK "XOR"
#endif

int main(void)
{
	static struct proto_receptor rx;
	struct proto_evento ev;
	char linha[64];
	int ret;

	ret = led_hw_init();
	if (ret < 0) {
		printk("ERRO: LED nao inicializou (%d)\n", ret);
		return 0;
	}
	ret = uart_rx_init();
	if (ret < 0) {
		printk("ERRO: UART RX nao inicializou (%d)\n", ret);
		return 0;
	}
	proto_init(&rx, CONFIG_APP_ID_DISPOSITIVO, MODO_CHECK);

	printk("UART-LED pronto | ID=0x%02X | CHECK=%s | timeout=%d ms | saida=%s\n",
	       CONFIG_APP_ID_DISPOSITIVO, NOME_CHECK, CONFIG_APP_TIMEOUT_BYTE_MS, led_hw_modo());

	for (;;) {
		enum proto_resultado res;
		uint8_t c;

		/* espera o proximo byte por no maximo TIMEOUT ms; se nao vier
		 * nada e havia um pacote pela metade, ele e descartado */
		if (uart_rx_ler(&c, K_MSEC(CONFIG_APP_TIMEOUT_BYTE_MS)) == 0) {
			res = proto_alimentar(&rx, c, &ev);
		} else {
			res = proto_timeout(&rx, &ev);
		}

		if (res == PROTO_INCOMPLETO) {
			continue;
		}
		if (res == PROTO_OK) {
			led_hw_executar(ev.pac.acao, ev.pac.r, ev.pac.g, ev.pac.b);
		}
		proto_descrever(&ev, linha, sizeof(linha));
		printk("%s\n", linha);
	}
	return 0;
}
