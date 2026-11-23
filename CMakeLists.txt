# PSI - Microcontroladores 2 - Controle de LED RGB pela UART (FRDM-KL25Z)
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(uart_led)

target_sources(app PRIVATE
  src/main.c
  src/uart_rx.c
  src/led_hw.c
  src/protocolo.c
  src/led_logica.c
)

# O MKL25Z4_features.h do hal_nxp (Zephyr 4.2) nao define a macro
# FSL_FEATURE_TPM_HAS_32BIT_COUNTERn, usada pelo driver fsl_tpm e pelo
# pwm_mcux_tpm. Sem ela o driver PWM (TPM) nao compila para o KL25Z.
# Macro do tipo funcao nao pode ir por -D no CMake, entao forcamos a
# inclusao de um cabecalho que a define (o TPM do KL25Z e de 16 bits).
if(CONFIG_SOC_MKL25Z4 AND CONFIG_PWM_MCUX_TPM)
  zephyr_compile_options(-include ${CMAKE_CURRENT_SOURCE_DIR}/src/kl25z_tpm_fixup.h)
endif()
