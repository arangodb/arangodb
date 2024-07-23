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

#pragma once

#include "Maskings/AttributeMasking.h"
#include "Maskings/CollectionFilter.h"
#include "Maskings/CollectionSelection.h"
#include "Maskings/ParseResult.h"

#include <string_view>
#include <vector>

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace maskings {
class Collection {
 public:
  static ParseResult<Collection> parse(Maskings* maskings,
                                       velocypack::Slice def);

 public:
  Collection() {}

  Collection(CollectionSelection selection,
             std::vector<AttributeMasking> const& maskings)
      : _selection(selection), _maskings(maskings) {}

  CollectionSelection selection() const noexcept { return _selection; }

  MaskingFunction* masking(std::vector<std::string_view> const& path) const;

 private:
  CollectionSelection _selection;
  // LATER: CollectionFilter _filter;
  std::vector<AttributeMasking> _maskings;
};
}  // namespace maskings
}  // namespace arangodb
