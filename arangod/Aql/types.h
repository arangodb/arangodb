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

#include "Basics/Common.h"
#include "Basics/HashSet.h"

namespace arangodb {
namespace aql {

/// @brief type for variable ids
typedef uint32_t VariableId;

/// @brief type for register numbers/ids
typedef unsigned int RegisterId;
typedef unsigned int RegisterCount;

/// @brief type of a query id
typedef uint64_t QueryId;

// Map RemoteID->ServerID->[SnippetId]
typedef std::unordered_map<size_t, std::unordered_map<std::string, std::vector<std::string>>> MapRemoteToSnippet;
}  // namespace aql
}  // namespace arangodb

#endif
