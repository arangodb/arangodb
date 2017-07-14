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

#ifndef ARANGOD_AQL_DOCUMENT_PRODUCING_NODE_H
#define ARANGOD_AQL_DOCUMENT_PRODUCING_NODE_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {
class ExecutionPlan;
struct Variable;

class DocumentProducingNode {
 public:
  explicit DocumentProducingNode(Variable const* outVariable);
  DocumentProducingNode(ExecutionPlan* plan, arangodb::velocypack::Slice slice);

  virtual ~DocumentProducingNode() = default;

 public:
  /// @brief return the out variable
  Variable const* outVariable() const { return _outVariable; }

  std::vector<std::string> const& projection() const {
    return _projection;
  }
  
  void setProjection(std::vector<std::string> const& attributeNames) {
    _projection = attributeNames;
  }

  void setProjection(std::vector<std::string>&& attributeNames) {
    _projection = std::move(attributeNames);
  }
  
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

 protected:
  Variable const* _outVariable;

  /// @brief produce only the following attribute (with possible subattributes)
  std::vector<std::string> _projection;
};

}
}

#endif
