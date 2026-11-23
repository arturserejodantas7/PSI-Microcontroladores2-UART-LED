/*
 * sim_receptor.c - Simulador do firmware no PC (Linux/macOS).
 *
 * Usa EXATAMENTE os mesmos protocolo.c e led_logica.c do firmware e imprime
 * as mesmas respostas, lendo bytes de um descritor (pty) com o mesmo timeout
 * entre bytes. Serve para testar o script tools/envia_pacote.py sem a placa.
 *
 * Uso: sim_receptor [--xor] [--id 0xA5] [--timeout-ms 50] <fd>
 */
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "led_logica.h"
#include "protocolo.h"

static int fd;

static void enviar(const char *s)
{
	char linha[96];
	int n = snprintf(linha, sizeof(linha), "%s\r\n", s);

	if (write(fd, linha, (size_t)n) < 0) {
		exit(1);
	}
}

int main(int argc, char **argv)
{
	enum proto_modo_check modo = PROTO_CHECK_CRC8;
	unsigned id = 0xA5;
	int timeout_ms = 50;
	struct proto_receptor rx;
	struct proto_evento ev;
	struct led_estado led;
	char s[96];

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--xor") == 0) {
			modo = PROTO_CHECK_XOR;
		} else if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
			id = (unsigned)strtoul(argv[++i], NULL, 0);
		} else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
			timeout_ms = atoi(argv[++i]);
		} else {
			fd = atoi(argv[i]);
		}
	}

	proto_init(&rx, (uint8_t)id, modo);
	led_logica_init(&led);
	snprintf(s, sizeof(s), "UART-LED pronto | ID=0x%02X | CHECK=%s | timeout=%d ms | saida=SIM",
		 id, modo == PROTO_CHECK_CRC8 ? "CRC-8" : "XOR", timeout_ms);
	enviar(s);

	for (;;) {
		struct pollfd p = {.fd = fd, .events = POLLIN};
		enum proto_resultado r;
		uint8_t c;
		int n = poll(&p, 1, timeout_ms);

		if (n < 0) {
			return 1;
		}
		if (n == 0) {
			r = proto_timeout(&rx, &ev);
		} else {
			if (read(fd, &c, 1) != 1) {
				return 0; /* outro lado fechou */
			}
			r = proto_alimentar(&rx, c, &ev);
		}
		if (r == PROTO_INCOMPLETO) {
			continue;
		}
		if (r == PROTO_OK) {
			uint8_t o[3];

			led_logica_aplicar(&led, ev.pac.acao, ev.pac.r, ev.pac.g, ev.pac.b);
			led_logica_saida(&led, o);
			fprintf(stderr, "[sim] LED agora R=%u G=%u B=%u (pisca %u ms)\n", o[0], o[1],
				o[2], led.meio_periodo_ms);
		}
		proto_descrever(&ev, s, sizeof(s));
		enviar(s);
	}
}
