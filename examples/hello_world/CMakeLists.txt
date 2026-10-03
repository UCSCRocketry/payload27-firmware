# SPDX-License-Identifier: Apache-2.0

set(BOARD nucleo_n657x0_q)

cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)
