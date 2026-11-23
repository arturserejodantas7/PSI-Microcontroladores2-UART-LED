/*
 * uart_rx.c - UART0 (console, OpenSDA/USB) recebida por interrupcao.
 *
 * A mesma UART0 do console e usada para os pacotes (entrada) e para as
 * respostas em texto (saida via printk). Assim basta o cabo USB da placa.
 * O console do Zephyr so escreve (uart_poll_out); a recepcao e nossa.
 */
#include "uart_rx.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/atomic.h>

#define TAM_FILA 128 /* ~11 ms de dados a 115200 bps */

K_MSGQ_DEFINE(fila_rx, sizeof(uint8_t), TAM_FILA, 1);

static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static atomic_t perdidos;
static atomic_t erros_linha;

static void uart_isr(const struct device *dev, void *user_data)
{
	uint8_t c;

	ARG_UNUSED(user_data);

	if (!uart_irq_update(dev)) {
		return;
	}
	/* le e limpa overrun/framing/ruido: com overrun pendente a UART0 do
	 * KL25Z para de receber, entao isso evita travar a recepcao */
	if (uart_err_check(dev) != 0) {
		atomic_inc(&erros_linha);
	}
	if (!uart_irq_rx_ready(dev)) {
		return;
	}
	while (uart_fifo_read(dev, &c, 1) == 1) {
		/* ISR nunca bloqueia: se a fila estiver cheia, descarta e conta */
		if (k_msgq_put(&fila_rx, &c, K_NO_WAIT) != 0) {
			atomic_inc(&perdidos);
		}
	}
}

int uart_rx_init(void)
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		return -ENODEV;
	}
	ret = uart_irq_callback_user_data_set(uart_dev, uart_isr, NULL);
	if (ret < 0) {
		return ret; /* -ENOTSUP / -ENOSYS: driver sem modo por interrupcao */
	}
	uart_irq_rx_enable(uart_dev);
	return 0;
}

int uart_rx_ler(uint8_t *byte, k_timeout_t timeout)
{
	return k_msgq_get(&fila_rx, byte, timeout);
}

uint32_t uart_rx_bytes_perdidos(void)
{
	return (uint32_t)atomic_get(&perdidos);
}

uint32_t uart_rx_erros_linha(void)
{
	return (uint32_t)atomic_get(&erros_linha);
}
