////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

#include "shared.hpp"
#include "utils/source_location.hpp"

namespace irs::assert {

using Callback = void (*)(SourceLocation&& location, std::string_view message);
// not thread-safe
Callback SetCallback(Callback callback) noexcept;
void Message(SourceLocation&& location, std::string_view message);

}  // namespace irs::assert

#ifdef IRESEARCH_DEBUG

// https://stackoverflow.com/questions/9183993/msvc-variadic-macro-expansion
#define IRS_GLUE(x, y) x y
#define IRS_RETURN_ARG_COUNT(_1_, _2_, _3_, _4_, _5_, count, ...) count
#define IRS_EXPAND_ARGS(args) IRS_RETURN_ARG_COUNT args
#define IRS_COUNT_ARGS_MAX5(...) \
  IRS_EXPAND_ARGS((__VA_ARGS__, 5, 4, 3, 2, 1, 0))

#define IRS_OVERLOAD_MACRO2(name, count) name##count
#define IRS_OVERLOAD_MACRO1(name, count) IRS_OVERLOAD_MACRO2(name, count)
#define IRS_OVERLOAD_MACRO(name, count) IRS_OVERLOAD_MACRO1(name, count)

#define IRS_CALL_OVERLOAD(name, ...)                                   \
  IRS_GLUE(IRS_OVERLOAD_MACRO(name, IRS_COUNT_ARGS_MAX5(__VA_ARGS__)), \
           (__VA_ARGS__))

#define IRS_ASSERT2(condition, message)                     \
  do {                                                      \
    if (IRS_UNLIKELY(!(condition))) {                       \
      ::irs::assert::Message(IRS_SOURCE_LOCATION, message); \
    }                                                       \
  } while (false)

#define IRS_ASSERT1(condition)                                 \
  do {                                                         \
    if (IRS_UNLIKELY(!(condition))) {                          \
      ::irs::assert::Message(IRS_SOURCE_LOCATION, #condition); \
    }                                                          \
  } while (false)

#define IRS_ASSERT(...) IRS_CALL_OVERLOAD(IRS_ASSERT, __VA_ARGS__)

#else

#define IRS_ASSERT(...) ((void)1)

#endif
