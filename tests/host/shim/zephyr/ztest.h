/*
 * Shim minimo da API do ztest para rodar os mesmos testes no PC (gcc).
 * Suporta: ZTEST_SUITE, ZTEST, zassert_true/false/equal/not_equal.
 */
#ifndef ZTEST_SHIM_H_
#define ZTEST_SHIM_H_

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

extern jmp_buf ztest_shim_jmp;
extern int ztest_shim_falhou;

typedef void *(*ztest_shim_setup_t)(void);
typedef void (*ztest_shim_before_t)(void *);

void ztest_shim_reg_suite(const char *nome, ztest_shim_setup_t setup, ztest_shim_before_t before);
void ztest_shim_reg_teste(const char *nome, void (*f)(void *));

#define ZTEST_SHIM_FALHA(...)                                                                     \
	do {                                                                                       \
		printf("    FALHOU %s:%d: ", __FILE__, __LINE__);                                    \
		printf("" __VA_ARGS__);                                                            \
		printf("\n");                                                                      \
		ztest_shim_falhou = 1;                                                             \
		longjmp(ztest_shim_jmp, 1);                                                        \
	} while (0)

#define zassert_true(c, ...)                                                                      \
	do {                                                                                       \
		if (!(c)) {                                                                        \
			ZTEST_SHIM_FALHA("(" #c ") " __VA_ARGS__);                                \
		}                                                                                  \
	} while (0)
#define zassert_false(c, ...)        zassert_true(!(c), ##__VA_ARGS__)
#define zassert_equal(a, b, ...)     zassert_true((a) == (b), ##__VA_ARGS__)
#define zassert_not_equal(a, b, ...) zassert_true((a) != (b), ##__VA_ARGS__)

#define ZTEST_SUITE(nome, pred, setup, before, after, teardown)                                   \
	__attribute__((constructor)) static void ztest_shim_suite_##nome(void)                     \
	{                                                                                          \
		ztest_shim_reg_suite(#nome, setup, before);                                        \
	}

#define ZTEST(suite, teste)                                                                       \
	static void teste(void *fixture);                                                          \
	__attribute__((constructor)) static void ztest_shim_reg_##teste(void)                      \
	{                                                                                          \
		ztest_shim_reg_teste(#teste, teste);                                               \
	}                                                                                          \
	static void teste(__attribute__((unused)) void *fixture)

#endif
