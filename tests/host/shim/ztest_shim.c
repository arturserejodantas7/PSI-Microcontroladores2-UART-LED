/* Executor do shim do ztest: roda os testes registrados e imprime no
 * mesmo estilo do ztest (PASS/FAIL por teste e resumo da suite). */
#include <stdlib.h>

#include "zephyr/ztest.h"

jmp_buf ztest_shim_jmp;
int ztest_shim_falhou;

static const char *suite_nome = "?";
static ztest_shim_setup_t suite_setup;
static ztest_shim_before_t suite_before;
static struct {
	const char *nome;
	void (*f)(void *);
} testes[128];
static int n_testes;

void ztest_shim_reg_suite(const char *nome, ztest_shim_setup_t setup, ztest_shim_before_t before)
{
	suite_nome = nome;
	suite_setup = setup;
	suite_before = before;
}

void ztest_shim_reg_teste(const char *nome, void (*f)(void *))
{
	testes[n_testes].nome = nome;
	testes[n_testes].f = f;
	n_testes++;
}

static int cmp(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int main(void)
{
	volatile int ok = 0, falhas = 0;
	void *volatile fixture = NULL;

	setvbuf(stdout, NULL, _IOLBF, 0);
	qsort(testes, n_testes, sizeof(testes[0]), cmp);
	if (suite_setup) {
		fixture = suite_setup();
	}
	printf("Running TESTSUITE %s\n", suite_nome);
	for (int i = 0; i < n_testes; i++) {
		ztest_shim_falhou = 0;
		if (suite_before) {
			suite_before(fixture);
		}
		if (!setjmp(ztest_shim_jmp)) {
			testes[i].f(fixture);
		}
		printf(" %s - %s\n", ztest_shim_falhou ? "FAIL" : "PASS", testes[i].nome);
		ztest_shim_falhou ? falhas++ : ok++;
	}
	printf("SUITE %s - %s: %d PASS, %d FAIL (total %d)\n", falhas ? "FAIL" : "PASS",
	       suite_nome, ok, falhas, n_testes);
	return falhas ? 1 : 0;
}
