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

#ifndef ARANGOD_AQL_NODE_FINDER_H
#define ARANGOD_AQL_NODE_FINDER_H 1

#include "Basics/Common.h"
#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Basics/SmallVector.h"

namespace arangodb {
namespace aql {

template <typename T>
class NodeFinder final : public WalkerWorker<ExecutionNode> {
  T _lookingFor;

  SmallVector<ExecutionNode*>& _out;

  bool _enterSubqueries;

 public:
  NodeFinder(T, SmallVector<ExecutionNode*>&, bool);

  bool before(ExecutionNode*) override final;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return _enterSubqueries;
  }
};

class EndNodeFinder final : public WalkerWorker<ExecutionNode> {
  SmallVector<ExecutionNode*>& _out;

  std::vector<bool> _found;

  bool _enterSubqueries;

 public:
  EndNodeFinder(SmallVector<ExecutionNode*>&, bool);

  bool before(ExecutionNode*) override final;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    _found.push_back(false);
    return _enterSubqueries;
  }

  void leaveSubquery(ExecutionNode*, ExecutionNode*) override final {
    _found.pop_back();
  }
};
}
}

#endif
