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

#include "utils/log.hpp"

#include <atomic>

namespace irs::log {

// We don't need alignas(cache_line_size) here.
// Because false sharing is ok for this data but generated code will be worse.
struct CallbackCell {
  std::atomic<Callback> callback{nullptr};
};

static CallbackCell kCallbacks[static_cast<size_t>(Level::kTrace) + 1]{};

Callback GetCallback(Level level) noexcept {
  return kCallbacks[static_cast<size_t>(level)].callback.load(
    std::memory_order_relaxed);
}

Callback SetCallback(Level level, Callback callback) noexcept {
  return kCallbacks[static_cast<size_t>(level)].callback.exchange(
    callback, std::memory_order_relaxed);
}

}  // namespace irs::log
