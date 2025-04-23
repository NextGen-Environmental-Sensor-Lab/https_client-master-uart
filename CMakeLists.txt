#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(https_client)

# Generate hex files from pem files
set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/certs)
zephyr_include_directories(${gen_dir})
generate_inc_file_for_target(
    app
    cert/ngens.pem
    ${gen_dir}/ngens.pem.inc   #loaded with google cert
    # cert/webhook_RootCA.pem
    # ${gen_dir}/webhook_RootCA.pem.inc   #loaded with google cert
    )

# NORDIC SDK APP START
# target_sources(app PRIVATE 
#                 src/main.c
#                 src/https_handler.h
#                 src/https_handler.c
#                 # src/rg15.h
#                 # src/rg15.c
#                 src/uart_handler.h
#                 src/uart_handler.c
#                 src/data_acq.h
#                 src/data_acq.c)


FILE(GLOB app_sources src/*.c src/*.h)
target_sources(app PRIVATE ${app_sources})
