////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Properties/CollectionDescriptor.h"
#include "VocBase/voc-types.h"

#include <string>
#include <utility>

namespace arangodb::tests {

/// @brief the descriptor most tests need: a collection that exists under a
/// known name, and an id when something else refers to the collection by id.
/// Pass DataSourceId::none() to let the id be generated.
inline CollectionDescriptor testCollectionDescriptor(
    std::string name, DataSourceId id = DataSourceId::none(),
    TRI_col_type_e type = TRI_COL_TYPE_DOCUMENT) {
  CollectionDescriptor d;
  d.mutableProps.name = std::move(name);
  d.identity.id = id;
  d.constant.type = type;
  return d;
}

}  // namespace arangodb::tests
