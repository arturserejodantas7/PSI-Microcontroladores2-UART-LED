# Testes unitarios (ztest) da logica pura do protocolo e do LED
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_protocolo)

target_include_directories(app PRIVATE ../../src)
target_sources(app PRIVATE
  src/main.c
  ../../src/protocolo.c
  ../../src/led_logica.c
)
