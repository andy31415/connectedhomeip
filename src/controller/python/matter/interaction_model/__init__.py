#
#    Copyright (c) 2021 Project CHIP Authors
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

#
#    @file
#      Provides Python APIs for CHIP.
#

"""Provides Python APIs for CHIP."""

import enum

from ..exceptions import ChipStackException
from .delegate import (
    AttributePath,
    AttributePathIBstruct,
    DataVersionFilterIBstruct,
    EventPath,
    EventPathIBstruct,
    PyInvokeRequestData,
    PyWriteAttributeData,
    SessionParameters,
    SessionParametersStruct,
    TestOnlyPyBatchCommandsOverrides,
    TestOnlyPyOnDoneInfo,
)

__all__ = [
    "AttributePath",
    "AttributePathIBstruct",
    "DataVersionFilterIBstruct",
    "EventPath",
    "EventPathIBstruct",
    "InteractionModelError",
    "PyInvokeRequestData",
    "PyWriteAttributeData",
    "SessionParameters",
    "SessionParametersStruct",
    "Status",
    "TestOnlyPyBatchCommandsOverrides",
    "TestOnlyPyOnDoneInfo",
]


# defined src/controller/python/matter/interaction_model/Delegate.h
kUndefinedClusterStatus: int = 0xFF


class Status(enum.IntEnum):
    Success = 0x0
    Failure = 0x01
    InvalidSubscription = 0x7D
    UnsupportedAccess = 0x7E
    UnsupportedEndpoint = 0x7F
    InvalidAction = 0x80
    UnsupportedCommand = 0x81
    Deprecated82 = 0x82
    Deprecated83 = 0x83
    Deprecated84 = 0x84
    InvalidCommand = 0x85
    UnsupportedAttribute = 0x86
    ConstraintError = 0x87
    UnsupportedWrite = 0x88
    ResourceExhausted = 0x89
    Deprecated8a = 0x8A
    NotFound = 0x8B
    UnreportableAttribute = 0x8C
    InvalidDataType = 0x8D
    Deprecated8e = 0x8E
    UnsupportedRead = 0x8F
    Deprecated90 = 0x90
    Deprecated91 = 0x91
    DataVersionMismatch = 0x92
    Deprecated93 = 0x93
    Timeout = 0x94
    Reserved95 = 0x95
    Reserved96 = 0x96
    Reserved97 = 0x97
    Reserved98 = 0x98
    Reserved99 = 0x99
    Reserved9a = 0x9A
    Busy = 0x9C
    AccessRestricted = 0x9D
    Deprecatedc0 = 0xC0
    Deprecatedc1 = 0xC1
    Deprecatedc2 = 0xC2
    UnsupportedCluster = 0xC3
    Deprecatedc4 = 0xC4
    NoUpstreamSubscription = 0xC5
    NeedsTimedInteraction = 0xC6
    UnsupportedEvent = 0xC7
    PathsExhausted = 0xC8
    TimedRequestMismatch = 0xC9
    FailsafeRequired = 0xCA
    InvalidInState = 0xCB
    NoCommandResponse = 0xCC
    DynamicConstraintError = 0xCF
    AlreadyExists = 0xD0
    InvalidTransportType = 0xD1
    WriteIgnored = 0xF0


class InteractionModelError(ChipStackException):
    def __init__(self, status: Status, clusterStatus: int = kUndefinedClusterStatus):
        self._status = status
        self._clusterStatus = clusterStatus

    def __str__(self):
        if self.hasClusterStatus:
            return f"InteractionModelError: {self._status.name} (0x{self._status.value:x}, clusterStatus: {self._clusterStatus})"
        return f"InteractionModelError: {self._status.name} (0x{self._status.value:x})"

    @property
    def hasClusterStatus(self) -> bool:
        return self._clusterStatus != kUndefinedClusterStatus

    @property
    def status(self) -> Status:
        return self._status

    @property
    def clusterStatus(self) -> int:
        return self._clusterStatus
