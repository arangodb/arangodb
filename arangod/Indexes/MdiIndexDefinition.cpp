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

#include "MdiIndexDefinition.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

Result MdiIndexDefinition::normalize(velocypack::Builder& normalized,
                                     velocypack::Slice definition,
                                     bool isCreation,
                                     Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  if (definition.hasKey(StaticStrings::IndexPrefixFields)) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  "`mdi` index does not support prefixed fields. use "
                  "`mdi-prefixed` as type instead.");
  }
  // a mdi never uses index estimates
  normalized.add(StaticStrings::IndexEstimates, velocypack::Value(false));

  return IndexFactory::enhanceJsonIndexMdi(definition, normalized, isCreation);
}

std::shared_ptr<Index> MdiIndexDefinition::create(
    IIndexFactory const& factory, LogicalCollection& collection,
    velocypack::Slice definition, IndexId id, bool isClusterConstructor) const {
  // this class backs both Zkd (legacy name) and MDI
  if (_type == IndexType::Zkd) {
    return factory.createZkd(collection, definition, id, isClusterConstructor);
  }
  TRI_ASSERT(_type == IndexType::MDI);
  return factory.createMdi(collection, definition, id, isClusterConstructor);
}
