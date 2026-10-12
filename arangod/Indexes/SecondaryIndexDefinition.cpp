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

#include "SecondaryIndexDefinition.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

Result SecondaryIndexDefinition::normalize(velocypack::Builder& normalized,
                                           velocypack::Slice definition,
                                           bool isCreation,
                                           Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());
  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  if (isCreation) {
    bool est = basics::VelocyPackHelper::getBooleanValue(
        definition, StaticStrings::IndexEstimates, true);
    normalized.add(StaticStrings::IndexEstimates, velocypack::Value(est));
  }

  return IndexFactory::enhanceJsonIndexGeneric(definition, normalized,
                                               isCreation);
}

std::shared_ptr<Index> SecondaryIndexDefinition::create(
    IIndexFactory const& factory, LogicalCollection& collection,
    velocypack::Slice definition, IndexId id, bool isClusterConstructor) const {
  // this class backs three IndexType values that share one set of
  // normalize()/equal() rules but each need a different concrete Index class
  switch (_type) {
    case IndexType::Hash:
      return factory.createHash(collection, definition, id,
                                isClusterConstructor);
    case IndexType::Persistent:
      return factory.createPersistent(collection, definition, id,
                                      isClusterConstructor);
    case IndexType::Skiplist:
      return factory.createSkiplist(collection, definition, id,
                                    isClusterConstructor);
    default:
      TRI_ASSERT(false) << "SecondaryIndexDefinition used for " << int(_type);
      return nullptr;
  }
}
