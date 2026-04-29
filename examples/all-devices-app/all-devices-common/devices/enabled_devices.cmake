#
#    Copyright (c) 2025 Project CHIP Authors
#    All rights reserved.
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

# Authoritative list of all device module source directories for the CMake
# (ESP32) build.  Callers must define ALL_DEVICES_COMMON_DIR before including
# this file.
#
# Future phases will extend this file with:
#   - ALL_DEVICES_ENABLED_DEVICES selection logic
#   - configure_file() to generate app_config/enabled_devices.h
#   - ALL_DEVICES_APP_NAME computation

set(ALL_DEVICES_DEVICE_SRCDIRS
    "${ALL_DEVICES_COMMON_DIR}/devices/boolean-state-sensor"
    "${ALL_DEVICES_COMMON_DIR}/devices/chime"
    "${ALL_DEVICES_COMMON_DIR}/devices/interface"
    "${ALL_DEVICES_COMMON_DIR}/devices/occupancy-sensor"
    "${ALL_DEVICES_COMMON_DIR}/devices/occupancy-sensor/impl"
    "${ALL_DEVICES_COMMON_DIR}/devices/dimmable-light"
    "${ALL_DEVICES_COMMON_DIR}/devices/dimmable-light/impl"
    "${ALL_DEVICES_COMMON_DIR}/devices/on-off-light"
    "${ALL_DEVICES_COMMON_DIR}/devices/root-node"
    "${ALL_DEVICES_COMMON_DIR}/devices/soil-sensor"
    "${ALL_DEVICES_COMMON_DIR}/devices/soil-sensor/impl"
    "${ALL_DEVICES_COMMON_DIR}/devices/speaker"
    "${ALL_DEVICES_COMMON_DIR}/devices/speaker/impl"
)
