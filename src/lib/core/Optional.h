/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include <optional>

namespace chip {

template <class T>
using Optional = std::optional<T>;

// TODO: all the items below should be replaced with std::optional replacements
inline constexpr std::nullopt_t NullOptional = std::nullopt;

template <class... Args>
auto MakeOptional = std::make_optional<Args...>;

} // namespace chip
