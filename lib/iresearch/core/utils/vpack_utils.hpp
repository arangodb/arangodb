////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "string.hpp"
#include "velocypack/Options.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace irs {

// return slice as string
inline std::string slice_to_string(
  const VPackSlice slice,
  const VPackOptions* options = &VPackOptions::Defaults) noexcept {
  std::string str;
  try {
    str = slice.toString(options);
  } catch (...) {
    try {
      str = "<non-representable type>";
    } catch (...) {
    }
  }

  return str;
}

}  // namespace irs
