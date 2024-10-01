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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNodeId.h"
#include "Aql/RegisterId.h"
#include "Aql/RegIdFlatSet.h"
#include "Basics/RebootId.h"
#include "Containers/HashSetFwd.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <absl/container/flat_hash_map.h>

namespace arangodb {

namespace aql {
struct Collection;

/// @brief type for variable ids
using VariableId = uint32_t;

/// @brief type of a query id
using QueryId = uint64_t;
using EngineId = uint64_t;

// Map RemoteID->ServerID->[SnippetId]
using MapRemoteToSnippet = std::unordered_map<
    ExecutionNodeId, std::unordered_map<std::string, std::vector<std::string>>>;

// Enable/Disable block passthrough in fetchers
enum class BlockPassthrough { Disable, Enable };

class ExecutionEngine;

// list of snippets on coordinators
using SnippetList = std::vector<std::unique_ptr<ExecutionEngine>>;

struct ServerQueryIdEntry {
  std::string server;
  QueryId queryId;
  RebootId rebootId;
};

using ServerQueryIdList = std::vector<ServerQueryIdEntry>;

using AqlCollectionMap = std::map<std::string, aql::Collection*, std::less<>>;

struct Variable;
// Note: #include <Containers/HashSet.h> to use the following types
using VarSet = containers::HashSet<Variable const*>;
using VarIdSet = containers::HashSet<VariableId>;
using VarSetStack = std::vector<VarSet>;
using RegIdSet = containers::HashSet<RegisterId>;
using RegIdSetStack = std::vector<RegIdSet>;
using RegIdOrderedSet = std::set<RegisterId>;
using RegIdOrderedSetStack = std::vector<RegIdOrderedSet>;
using RegIdFlatSetStack = std::vector<containers::flat_set<RegisterId>>;

using BindParameterVariableMapping =
    absl::flat_hash_map<std::string, Variable const*>;

}  // namespace aql

namespace traverser {
class BaseEngine;
// list of graph engines on coordinators
using GraphEngineList = std::vector<std::unique_ptr<BaseEngine>>;
}  // namespace traverser

enum class ExplainRegisterPlan : uint8_t { No = 0, Yes };

}  // namespace arangodb
