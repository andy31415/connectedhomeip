/*
 *
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

#include "tracing/enabled_features.h"

#include <tracing/json/json_tracing.h>

#if ENABLE_PERFETTO_TRACING
#include <tracing/perfetto/file_output.h>      // nogncheck
#include <tracing/perfetto/perfetto_tracing.h> // nogncheck

#define _PERFETTO_CMDLINE_ARGS ", perfetto, perfetto:<path>"
#else
#define _PERFETTO_CMDLINE_ARGS ""
#endif

#if ENABLE_PWTRACE_TRACING
#include <tracing/pwtrace/pwtrace_tracing.h> // nogncheck

#define _PWTRACE_CMDLINE_ARGS ", pwtrace"
#else
#define _PWTRACE_CMDLINE_ARGS ""
#endif

/// A string with supported command line tracing targets
/// to be pretty-printed in help strings if needed
#define SUPPORTED_COMMAND_LINE_TRACING_TARGETS "json:log, json:<path>" _PERFETTO_CMDLINE_ARGS _PWTRACE_CMDLINE_ARGS

namespace chip {
namespace CommandLineApp {

class TracingSetup
{
public:
    TracingSetup() = default;
    ~TracingSetup() { StopTracing(); }

    /// Enable tracing based on the given command line argument
    /// like "json:log" or "json:/tmp/foo.txt,perfetto" or similar
    ///
    /// Single arguments as well as comma separated ones are accepted.
    ///
    /// Calling this method multiple times is ok and will enable each of
    /// the given tracing modules if not already enabled.
    void EnableTracingFor(const char * cliArg);

    /// If EnableTracingFor is called, this MUST be called as well
    /// to unregister tracing backends
    void StopTracing();

private:
    ::chip::Tracing::Json::JsonBackend mJsonBackend;

#if ENABLE_PERFETTO_TRACING
    chip::Tracing::Perfetto::FileTraceOutput mPerfettoFileOutput;
    chip::Tracing::Perfetto::PerfettoBackend mPerfettoBackend;
#endif

#if ENABLE_PWTRACE_TRACING
    chip::Tracing::PwTrace::PwTraceBackend mPwTraceBackend;
#endif
};

} // namespace CommandLineApp
} // namespace chip
