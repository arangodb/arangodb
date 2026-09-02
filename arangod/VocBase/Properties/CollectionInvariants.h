////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VocBase/voc-types.h"

#include <optional>
#include <string>

namespace arangodb {

/// @brief The collection properties with no runtime owner. Everything else a
/// CollectionDescriptor carries belongs to ShardingInfo, PhysicalCollection,
/// KeyGenerator, LogicalDataSource or a LogicalCollection member.
/// Never parsed or serialized, hence the plain types.
struct CollectionInvariants {
  TRI_col_type_e type{TRI_col_type_e::TRI_COL_TYPE_DOCUMENT};

  bool isSmart{false};
  bool isDisjoint{false};
  bool isSmartChild{false};

  std::optional<std::string> smartJoinAttribute;

  bool operator==(CollectionInvariants const&) const = default;
};

}  // namespace arangodb
