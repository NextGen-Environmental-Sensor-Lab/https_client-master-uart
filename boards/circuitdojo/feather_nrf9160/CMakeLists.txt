#
# Copyright (c) 2024 Circuit Dojo LLC
#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_MCUBOOT_SERIAL startup.c)

if(CONFIG_CIRCUITDOJO_FEATHER_NRF9160_STATIC_PARTITIONS_FACTORY)
    set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_LIST_DIR}/circuitdojo_feather_nrf9160_pm_static.yml CACHE INTERNAL "")
endif()