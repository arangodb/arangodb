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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Identifiers/IndexId.h"

#include "Basics/StringUtils.h"
#include "Basics/voc-errors.h"

namespace arangodb {

ResultT<IndexId> IndexId::fromString(std::string_view str) {
  if (auto const pos = str.find('/'); pos != std::string_view::npos) {
    str = str.substr(pos + 1);
  }
  auto const parsed = basics::StringUtils::try_uint64(str);
  if (parsed.fail()) {
    return ResultT<IndexId>::error(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
  }
  return IndexId{parsed.get()};
}

/// @brief whether or not the id is set (not none())
bool IndexId::isSet() const noexcept {
  return id() != std::numeric_limits<BaseType>::max();
}

/// @brief whether or not the identifier is unset (equal to none())
bool IndexId::empty() const noexcept { return !isSet(); }

bool IndexId::isPrimary() const { return id() == 0; }

bool IndexId::isEdge() const { return id() == 1 || id() == 2; }

}  // namespace arangodb
