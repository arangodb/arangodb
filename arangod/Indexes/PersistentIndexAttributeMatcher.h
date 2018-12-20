////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VPACK_INDEX_MATCHER_H
#define ARANGOD_VPACK_INDEX_MATCHER_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace aql {
class Ast;
struct AstNode;
class SortCondition;
struct Variable;
}

class Index;

/// Contains code for persistent sorted indexes such as the RocksDBVPackIndex
/// and the MMFilesPersistentIndex. Weights used are mostly intended for
/// rocksdb based storage
namespace PersistentIndexAttributeMatcher {

bool supportsSortCondition(arangodb::Index const*,
                           arangodb::aql::SortCondition const* sortCondition,
                           arangodb::aql::Variable const* reference,
                           size_t itemsInIndex, double& estimatedCost,
                           size_t& coveredAttributes);

};
}

#endif
