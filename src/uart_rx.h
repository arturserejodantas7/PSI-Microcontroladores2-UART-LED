/*
 * uart_rx.h - Recepcao de bytes da UART por interrupcao.
 * A ISR coloca cada byte recebido numa fila (k_msgq); a thread de protocolo
 * le da fila com timeout curto.
 */
#ifndef UART_RX_H_
#define UART_RX_H_

#include <stdint.h>
#include <zephyr/kernel.h>

int uart_rx_init(void);
/* 0 = byte lido; -EAGAIN = timeout sem bytes */
int uart_rx_ler(uint8_t *byte, k_timeout_t timeout);
/* contadores de diagnostico (fila cheia / erros de linha: overrun, framing) */
uint32_t uart_rx_bytes_perdidos(void);
uint32_t uart_rx_erros_linha(void);

#endif
