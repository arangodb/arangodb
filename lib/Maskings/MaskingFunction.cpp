////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Maskings/MaskingFunction.h"

#include <velocypack/Builder.h>

namespace arangodb::maskings {

// default implementations for masking functions.
// these will only add the original value.
// the benefit of having default implementation is that
// derived classes only need to specialize the functions
// they need.

void MaskingFunction::mask(bool value, velocypack::Builder& out,
                           std::string& /*buffer*/) const {
  out.add(VPackValue(value));
}

void MaskingFunction::mask(std::string_view value, velocypack::Builder& out,
                           std::string& /*buffer*/) const {
  out.add(VPackValue(value));
}

void MaskingFunction::mask(int64_t value, velocypack::Builder& out,
                           std::string& /*buffer*/) const {
  out.add(VPackValue(value));
}

void MaskingFunction::mask(double value, velocypack::Builder& out,
                           std::string& /*buffer*/) const {
  out.add(VPackValue(value));
}

}  // namespace arangodb::maskings
