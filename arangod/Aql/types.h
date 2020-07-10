////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_TYPES_H
#define ARANGOD_AQL_TYPES_H 1

#include "Aql/ExecutionNodeId.h"
#include "Basics/debugging.h"

#include <Basics/debugging.h>
#include <Containers/HashSetFwd.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace boost {
namespace container {
template<class T>
class new_allocator;
template <class Key, class Compare, class AllocatorOrContainer>
class flat_set;
}
}  // namespace boost

namespace arangodb {

namespace containers {
template <class Key, class Compare = std::less<Key>, class AllocatorOrContainer = boost::container::new_allocator<Key> >
using flat_set = boost::container::flat_set<Key, Compare, AllocatorOrContainer>;
}

namespace aql {
struct Collection;

/// @brief type for variable ids
typedef uint32_t VariableId;

/// @brief type for register numbers/ids
typedef unsigned RegisterId;
typedef size_t RegisterCount;

/// @brief type of a query id
typedef uint64_t QueryId;
typedef uint64_t EngineId;

// Map RemoteID->ServerID->[SnippetId]
using MapRemoteToSnippet =
    std::unordered_map<ExecutionNodeId, std::unordered_map<std::string, std::vector<std::string>>>;

// Enable/Disable block passthrough in fetchers
enum class BlockPassthrough { Disable, Enable };

class ExecutionEngine;
// list of snippets on coordinators
using SnippetList = std::vector<std::pair<EngineId, std::unique_ptr<ExecutionEngine>>>;

using AqlCollectionMap = std::map<std::string, aql::Collection*, std::less<>>;

struct Variable;
// Note: #include <Containers/HashSet.h> to use the following types
using VarSet = containers::HashSet<Variable const*>;
using VarSetStack = std::vector<VarSet>;
using RegIdSet = containers::HashSet<RegisterId>;
using RegIdSetStack = std::vector<RegIdSet>;
using RegIdOrderedSet = std::set<RegisterId>;
using RegIdOrderedSetStack = std::vector<RegIdOrderedSet>;
// Note: #include <boost/container/flat_set.hpp> to use the following types
using RegIdFlatSet = containers::flat_set<RegisterId>;
using RegIdFlatSetStack = std::vector<containers::flat_set<RegisterId>>;

}  // namespace aql

namespace traverser {
class BaseEngine;
// list of graph engines on coordinators
using GraphEngineList =
    std::vector<std::pair<arangodb::aql::EngineId, std::unique_ptr<BaseEngine>>>;
}  // namespace traverser

}  // namespace arangodb

#endif
