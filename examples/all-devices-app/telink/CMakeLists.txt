#
#    Copyright (c) 2026 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

cmake_minimum_required(VERSION 3.20)

get_filename_component(CHIP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../.. REALPATH)
get_filename_component(TELINK_COMMON ${CHIP_ROOT}/examples/platform/telink REALPATH)
get_filename_component(ALL_DEVICES_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../all-devices-common REALPATH)

include(${TELINK_COMMON}/common.cmake)
include(${CHIP_ROOT}/config/telink/app/enable-gnu-std.cmake)

project(chip-telink-all-devices-example)

include(${ALL_DEVICES_COMMON_DIR}/devices/enabled_devices.cmake)

target_include_directories(app PRIVATE
                           include
                           ${ALL_DEVICES_COMMON_DIR}
                           ${CHIP_ROOT}/examples/providers
                           ${CHIP_ROOT}/src/data-model-providers/codedriven
                           ${CMAKE_CURRENT_BINARY_DIR}  # for generated app_config/enabled_devices.h
                           ${TELINK_COMMON}/common/include
                           ${TELINK_COMMON}/util/include
                           ${TELINK_COMMON}/zephyr_ext)

target_sources(app PRIVATE
               src/main.cpp
               ${TELINK_COMMON}/util/src/LEDManager.cpp
               ${TELINK_COMMON}/util/src/ButtonManager.cpp
               ${TELINK_COMMON}/util/src/ThreadUtil.cpp
               ${TELINK_COMMON}/util/src/PWMManager.cpp
               ${TELINK_COMMON}/zephyr_ext/zephyr_key_matrix.c
               ${TELINK_COMMON}/zephyr_ext/zephyr_key_pool.c
               ${TELINK_COMMON}/zephyr_ext/zephyr_led_pool.c
               ${TELINK_COMMON}/zephyr_ext/zephyr_pwm_pool.c
               ${TELINK_COMMON}/zephyr_ext/zephyr_ws2812.c
               ${CHIP_ROOT}/examples/providers/DeviceInfoProviderImpl.cpp
               ${CHIP_ROOT}/src/data-model-providers/codedriven/CodeDrivenDataModelProvider.cpp
               ${CHIP_ROOT}/src/data-model-providers/codedriven/endpoint/EndpointInterfaceRegistry.cpp
               ${CHIP_ROOT}/src/data-model-providers/codedriven/endpoint/SpanEndpoint.cpp
               ${CHIP_ROOT}/src/app/server-cluster/ServerClusterInterfaceRegistry.cpp
               ${CHIP_ROOT}/src/app/clusters/network-commissioning/NetworkCommissioningCluster.cpp
               ${CHIP_ROOT}/src/app/clusters/network-commissioning/ThreadScanResponse.cpp
               ${ALL_DEVICES_DEVICE_SOURCES}
)

if(CONFIG_BOOTLOADER_MCUBOOT)
  target_sources(app PRIVATE ${TELINK_COMMON}/util/src/OTAUtil.cpp)
endif()
