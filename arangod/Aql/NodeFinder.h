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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Basics/Common.h"
#include "Containers/SmallVector.h"

namespace arangodb {
namespace aql {

template<typename T, WalkerUniqueness U>
class NodeFinder final : public WalkerWorker<ExecutionNode, U> {
  containers::SmallVector<ExecutionNode*, 8>& _out;

  T _lookingFor;

  bool _enterSubqueries;

 public:
  NodeFinder(T const&, containers::SmallVector<ExecutionNode*, 8>&,
             bool enterSubqueries);

  bool before(ExecutionNode*) override final;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return _enterSubqueries;
  }
};

class EndNodeFinder final
    : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  containers::SmallVector<ExecutionNode*, 8>& _out;

  std::vector<bool> _found;

  bool _enterSubqueries;

 public:
  EndNodeFinder(containers::SmallVector<ExecutionNode*, 8>&,
                bool enterSubqueries);

  bool before(ExecutionNode*) override final;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    _found.push_back(false);
    return _enterSubqueries;
  }

  void leaveSubquery(ExecutionNode*, ExecutionNode*) override final {
    TRI_ASSERT(!_found.empty());
    _found.pop_back();
  }
};
}  // namespace aql
}  // namespace arangodb
