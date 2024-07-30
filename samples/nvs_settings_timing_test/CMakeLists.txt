#
# Copyright (c) 2024, Jamie M.
#
# All right reserved. This code is NOT apache or FOSS/copyleft licensed.
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nvs_settings_timing_test)

target_sources(app PRIVATE src/main.c src/settings.c)
