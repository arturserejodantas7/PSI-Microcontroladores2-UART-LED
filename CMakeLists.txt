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
