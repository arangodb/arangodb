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

#include "utils/assert.hpp"

#include <type_traits>
#include <utility>

#include "source_location.hpp"

namespace irs::assert {

static Callback kCallback = nullptr;

Callback SetCallback(Callback callback) noexcept {
  return std::exchange(kCallback, callback);
}

static_assert(std::is_same_v<Callback, decltype(&Message)>,
              "We want tail call optimization for this call");

void Message(SourceLocation&& location, std::string_view message) {
  if (IRS_LIKELY(kCallback != nullptr)) {
    kCallback(std::move(location), message);
  }
}

}  // namespace irs::assert
