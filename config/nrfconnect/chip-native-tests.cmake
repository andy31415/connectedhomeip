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

# Enable shared libraries: unit test libraries will be dynamically loaded
# and run.
macro(enable_dynamic_libraries)
 SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS true) 
 SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
endmacro()

function(chip_add_dynamic_unit_test TARGET_NAME)
    cmake_parse_arguments(CHIP
                          ""
                          "TEST_LIBRARY"
                          "HELPER_LIBRARIES"
                          ${ARGN})

    add_library(${TARGET_NAME} SHARED empty.cpp)

    target_link_directories(${TARGET_NAME} PUBLIC ${CHIP_OUTPUT_DIR}/lib)

    if (BOARD STREQUAL "native_posix")
        target_link_options(${TARGET_NAME} PUBLIC -m32)
        target_compile_options(${TARGET_NAME} PUBLIC -m32)
    elseif (BOARD STREQUAL "native_posix_64")
        target_link_options(${TARGET_NAME} PUBLIC -m64)
        target_compile_options(${TARGET_NAME} PUBLIC -m64)
    endif()

    target_link_libraries(${TARGET_NAME} PUBLIC 
       ${CHIP_TEST_LIBRARY}
       -Wl,--whole-archive ${CHIP_HELPER_LIBRARIES} -Wl,--no-whole-archive
    )

endfunction()