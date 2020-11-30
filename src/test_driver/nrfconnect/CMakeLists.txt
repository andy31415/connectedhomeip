#
#   Copyright (c) 2020 Project CHIP Authors
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

#
#   @file
#         CMake project for building and running all CHIP unit tests
#

# TODO:
# As long as we use autotools under the hood we build all CHIP libraries and tests in this project
# but ideally we would have a subdirectory for each CHIP test directory so that it be possible to
# build a test driver for a single test directory and required dependencies, e.g.
#
# src/test_driver/zephyr/:
# - CMakeLists.txt          <- build test driver and run all tests
# - system/CMakeLists.txt   <- build and run only system tests
# - inet/CMakeLists.txt     <- build and run only inet tests
# - ...

cmake_minimum_required(VERSION 3.13.1)

set(CHIP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/connectedhomeip)
get_filename_component(CHIP_ROOT ${CHIP_ROOT} REALPATH)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CHIP_ROOT}/config/nrfconnect/)

# Make sure all components can see our mbedtls config
include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR}/main/include)

# ==================================================
# Load NCS/Zephyr build system
# ==================================================
find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})

# ==================================================
# General settings
# ==================================================

include(chip-lib)
include(chip-native-tests)

set(CHIP_COMMON_FLAGS
    -DCHIP_SYSTEM_CONFIG_USE_POSIX_TIME_FUNCTS=1
    -DCHIP_SYSTEM_CONFIG_USE_ZEPHYR_SOCKET_EXTENSIONS=0
    -DCHIP_SYSTEM_CONFIG_USE_ZEPHYR_NET_IF=0
    -DCHIP_SYSTEM_CONFIG_USE_BSD_IFADDRS=1
    -I${CMAKE_CURRENT_SOURCE_DIR}/main/include
    -isystem${ZEPHYR_BASE}/../modules/crypto/mbedtls/configs
    -isystem${ZEPHYR_BASE}/../mbedtls/include
)

set(CHIP_LIBRARIES
    -lCHIP
    -lInetLayerTests
    -lnlunit-test
)

set(CHIP_TESTS
    -lSupportTests
    -lCoreTests
    -lSystemLayerTests
    -lTransportLayerTests
    -lChipCryptoTests
    -lPlatformTests
)

# ==================================================
# Setup CHIP build (including unit tests)
# ==================================================
project(AllChipTests)
enable_testing()

chip_configure(ChipConfig
    BUILD_TESTS
    FORCE_PIC
    PROJECT_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/main/include/CHIPProjectConfig.h
    CFLAGS ${CHIP_COMMON_FLAGS}
    CXXFLAGS ${CHIP_COMMON_FLAGS}
)

chip_build(ChipLib ChipConfig
    BUILD_COMMAND ninja
    BUILD_ARTIFACTS ${CHIP_LIBRARIES} ${CHIP_TESTS}
)

# ==================================================
# Build 'all tests' runner
# ==================================================
target_sources(app PRIVATE main/runner.cpp)
target_link_options(app PUBLIC -Wl,--whole-archive ${CHIP_TESTS} -Wl,--no-whole-archive)
target_link_libraries(app PUBLIC ChipLib $<TARGET_FILE:kernel>)
target_compile_definitions(app PUBLIC CHIP_HAVE_CONFIG_H)


set(SHARED_CHIP_LIBS
    -lCHIP
    -lInetLayerTests # helpers should be here!
)

enable_dynamic_libraries()

foreach(TEST_LIB ${CHIP_TESTS})
    STRING(REPLACE "-l" "TEST" TEST_LIB_NAME ${TEST_LIB})

    chip_add_dynamic_unit_test(
        ${TEST_LIB_NAME}    
        HELPER_LIBRARIES ${SHARED_CHIP_LIBS} 
        TEST_LIBRARY ${TEST_LIB}
    )

    add_dependencies(${TEST_LIB_NAME} ChipLib)
endforeach()

add_test(AllChipTests zephyr/zephyr.exe)
