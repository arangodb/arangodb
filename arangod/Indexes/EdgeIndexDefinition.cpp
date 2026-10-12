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

#include "EdgeIndexDefinition.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

Result EdgeIndexDefinition::normalize(velocypack::Builder& normalized,
                                      velocypack::Slice /*definition*/,
                                      bool isCreation,
                                      Database const& /*vocbase*/) const {
  if (isCreation) {
    // creating these indexes yourself is forbidden
    return TRI_ERROR_FORBIDDEN;
  }

  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(IndexType::Edge)));

  return TRI_ERROR_INTERNAL;
}

std::shared_ptr<Index> EdgeIndexDefinition::create(
    IIndexFactory const& factory, LogicalCollection& collection,
    velocypack::Slice definition, IndexId id, bool isClusterConstructor) const {
  if (!isClusterConstructor) {
    // this index type cannot be created directly
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot create edge index");
  }
  return factory.createEdge(collection, definition, id, isClusterConstructor);
}
