/*
 *    Copyright (c) 2023 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#pragma once

#include <lib/support/CodeUtils.h>

namespace chip {
namespace Tracing {

// These structures are forward-declared so that tracing itself has no direct dependencies
// on actual types. This allows tracing to be used anywhere lib/support could be used.
struct MessageSendInfo;
struct MessageReceivedInfo;
struct NodeLookupInfo;
struct NodeDiscoveredInfo;
struct NodeDiscoveryFailedInfo;

/// Defines a metric for the state of a long-running operation.
///
/// This is used as an asynchronous alternative to begin/end
/// nested tracing (which require perfect nesting without
/// interleaving).
///
/// Execution states begin/end may be interleaved like:
///   BEGIN-A
///   BEGIN-B
///   END-A
///   END-B
enum class ProcessExecution
{
    kStart,     // execution started.
    kHeartbeat, // still in progress at some particular point
    kEnd,       // execution finished
};

// In particular events MUST NOT have any specific dependencies
class Metric
{
public:
    enum class Type : uint8_t
    {
        kInt32,            // int32_t
        kUInt32,           // uint32_t
        kErrorCode,        // CHIPError code (asInteger) stored as a UInt32
        kProcessExecution, // a ProcessExecution value
    };

    Metric(const Metric &)             = default;
    Metric(Metric &&)                  = default;
    Metric & operator=(const Metric &) = default;
    Metric & operator=(Metric &&)      = default;

    Type GetType() const { return mType; }

    uint32_t ValueUInt32() const
    {
        VerifyOrDie(mType == Type::kUInt32);
        return mStore.uint32_value;
    }

    int32_t ValueInt32() const
    {
        VerifyOrDie(mType == Type::kInt32);
        return mStore.int32_value;
    }

    uint32_t ValueErrorCode() const
    {
        VerifyOrDie(mType == Type::kErrorCode);
        return mStore.uint32_value;
    }

    ProcessExecution ValueProcessExecution() const
    {
        VerifyOrDie(mType == Type::kProcessExecution);
        return mStore.execution_value;
    }

    inline static Metric Int32(int32_t v) { return Metric(v); }
    inline static Metric UInt32(uint32_t v) { return Metric(v, Type::kUInt32); }
    inline static Metric ErrorCode(uint32_t v) { return Metric(v, Type::kErrorCode); }
    inline static Metric State(ProcessExecution v) { return Metric(v); }

private:
    union Store
    {
        int32_t int32_value;
        uint32_t uint32_value;
        ProcessExecution execution_value;

        Store(int32_t v) : int32_value(v) {}
        Store(uint32_t v) : uint32_value(v) {}
        Store(ProcessExecution v) : execution_value(v) {}
    };

    Store mStore;
    Type mType;

    Metric(uint32_t value, Type type) : mStore(value), mType(type) {}
    Metric(int32_t value) : mStore(value), mType(Type::kInt32) {}
    Metric(ProcessExecution value) : mStore(value), mType(Type::kProcessExecution) {}
};

} // namespace Tracing
} // namespace chip
