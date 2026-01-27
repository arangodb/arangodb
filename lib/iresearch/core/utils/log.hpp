////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>

#include "shared.hpp"
#include "utils/source_location.hpp"

namespace irs::log {

enum class Level : uint8_t {
  kFatal,
  kError,
  kWarn,
  kInfo,
  kDebug,
  kTrace,
};

using Callback = void (*)(SourceLocation&& location, std::string_view message);

// thread-safe
Callback GetCallback(Level level) noexcept;
Callback SetCallback(Level level, Callback callback) noexcept;

}  // namespace irs::log

// We want to disable log for coverage run
#ifdef IRS_DISABLE_LOG

#define IRS_LOG(...) ((void)1)

#else

// unlikely because trace/debug/info is commonly disabled,
// but warn/error/fatal is rare situation
#define IRS_LOG(level, message)                      \
  do {                                               \
    auto* callback = ::irs::log::GetCallback(level); \
    if (IRS_UNLIKELY(callback != nullptr)) {         \
      callback(IRS_SOURCE_LOCATION, message);        \
    }                                                \
  } while (false)

#endif

#define IRS_LOG_FATAL(message) IRS_LOG(::irs::log::Level::kFatal, message)
#define IRS_LOG_ERROR(message) IRS_LOG(::irs::log::Level::kError, message)
#define IRS_LOG_WARN(message) IRS_LOG(::irs::log::Level::kWarn, message)
#define IRS_LOG_INFO(message) IRS_LOG(::irs::log::Level::kInfo, message)
#define IRS_LOG_DEBUG(message) IRS_LOG(::irs::log::Level::kDebug, message)
#define IRS_LOG_TRACE(message) IRS_LOG(::irs::log::Level::kTrace, message)
