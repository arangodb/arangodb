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

#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/AttributeNamePath.h"
#include "Containers/FlatHashSet.h"

namespace arangodb::aql::optimizer {
// find projection attributes for variable v, starting from node n
// down to the root node of the plan/subquery.
// returns true if it is safe to reduce the full document data from
// "v" to only the projections stored in "attributes". returns false
// otherwise. if false is returned, the contents of "attributes" must
// be ignored by the caller.
// note: this function will not wipe "attributes" if there is already
// some data in it.
bool findProjections(ExecutionNode* n, Variable const* v,
                     std::string_view expectedAttribute,
                     bool excludeStartNodeFilterCondition,
                     containers::FlatHashSet<AttributeNamePath>& attributes);

}  // namespace arangodb::aql::optimizer
