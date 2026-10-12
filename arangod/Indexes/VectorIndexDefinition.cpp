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

#include "VectorIndexDefinition.h"

#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "VectorIndex/IVectorIndexProvider.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

Result VectorIndexDefinition::normalize(velocypack::Builder& normalized,
                                        velocypack::Slice definition,
                                        bool isCreation,
                                        Database const& /*vocbase*/) const {
  TRI_ASSERT(normalized.isOpenObject());

  if (!_vectorIndexProvider.isVectorIndexEnabled()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "vector index feature is not enabled. Run ArangoDB with "
            "`--vector-index` flag turned on."};
  }

  normalized.add(StaticStrings::IndexType,
                 velocypack::Value(Index::oldtypeName(_type)));

  // a vector index never uses index estimates
  normalized.add(StaticStrings::IndexEstimates, velocypack::Value(false));

  return IndexFactory::enhanceJsonIndexVector(definition, normalized,
                                              isCreation);
}
