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

#include "DocumentProducingNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Variable.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

DocumentProducingNode::DocumentProducingNode(Variable const* outVariable)  
    : _outVariable(outVariable) {
  TRI_ASSERT(_outVariable != nullptr);
}

DocumentProducingNode::DocumentProducingNode(ExecutionPlan* plan, 
                                             arangodb::velocypack::Slice slice)
    : _outVariable(Variable::varFromVPack(plan->getAst(), slice, "outVariable")) {
  TRI_ASSERT(_outVariable != nullptr);

  if (slice.hasKey("projection")) {
    VPackSlice p = slice.get("projection");
    if (p.isArray()) {
      for (auto const& it : VPackArrayIterator(p)) {
        _projection.emplace_back(it.copyString());
      }
    }
  }
}
  
void DocumentProducingNode::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  builder.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(builder);
  
  if (!_projection.empty()) {
    builder.add("projection", VPackValue(VPackValueType::Array));
    for (auto const& it : _projection) {
      builder.add(VPackValue(it));
    }
    builder.close();
  }
}
