////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <unordered_set>

#include "Basics/Common.h"
#include "Basics/StaticStrings.h"

#include "Pregel/Worker/Messages.h"
#include "Pregel/DatabaseTypes.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/GraphStore/Graph.h"
#include "Pregel/GraphStore/GraphSerdeConfig.h"

struct TRI_vocbase_t;

namespace arangodb::pregel {
class PregelFeature;

template<typename V, typename E, typename M>
class Worker;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry common parameters
////////////////////////////////////////////////////////////////////////////////
class WorkerConfig : std::enable_shared_from_this<WorkerConfig> {
  template<typename V, typename E, typename M>
  friend class Worker;

 public:
  explicit WorkerConfig(TRI_vocbase_t* vocbase);
  void updateConfig(worker::message::CreateWorker const& updated);

  ExecutionNumber executionNumber() const { return _executionNumber; }

  uint64_t globalSuperstep() const { return _globalSuperstep; }

  uint64_t localSuperstep() const { return _localSuperstep; }

  uint64_t parallelism() const { return _parallelism; }

  std::string const& coordinatorId() const { return _coordinatorId; }

  TRI_vocbase_t* vocbase() const { return _vocbase; }
  std::string const& database() const;

  GraphSerdeConfig const& graphSerdeConfig() const { return _graphSerdeConfig; }

  // convert an arangodb document id to a pregel id
  VertexID documentIdToPregel(std::string_view documentID) const;

  uint64_t _globalSuperstep = 0;
  uint64_t _localSuperstep = 0;

 private:
  ExecutionNumber _executionNumber{};

  std::string _coordinatorId;
  TRI_vocbase_t* _vocbase;

  // parallelism. will be updated by config later
  size_t _parallelism = 1;

  GraphSerdeConfig _graphSerdeConfig;
};

}  // namespace arangodb::pregel
