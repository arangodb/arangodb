////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_NODE_FINDER_H
#define ARANGOD_AQL_NODE_FINDER_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Basics/Common.h"
#include "Containers/SmallVector.h"

namespace arangodb {
namespace aql {

template <typename T, WalkerUniqueness U>
class NodeFinder final : public WalkerWorker<ExecutionNode, U> {
  ::arangodb::containers::SmallVector<ExecutionNode*>& _out;
  
  T _lookingFor;

  bool _enterSubqueries;

 public:
  NodeFinder(T const&, ::arangodb::containers::SmallVector<ExecutionNode*>&, bool enterSubqueries);

  bool before(ExecutionNode*) override final;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return _enterSubqueries;
  }
};

class EndNodeFinder final : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
  ::arangodb::containers::SmallVector<ExecutionNode*>& _out;

  std::vector<bool> _found;

  bool _enterSubqueries;

 public:
  EndNodeFinder(::arangodb::containers::SmallVector<ExecutionNode*>&, bool enterSubqueries);

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

#endif
