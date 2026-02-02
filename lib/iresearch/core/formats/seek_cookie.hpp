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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "utils/attribute_provider.hpp"

namespace irs {

// Implementation defined term value state
struct seek_cookie : attribute_provider {
  using ptr = std::unique_ptr<seek_cookie>;

  // Return `true` is cookie denoted by `rhs` is equal to the given one,
  // false - otherwise.
  // Caller must provide correct cookie type for comparison
  virtual bool IsEqual(const seek_cookie& rhs) const = 0;

  // Return cookie hash value.
  virtual size_t Hash() const = 0;
};

}  // namespace irs
