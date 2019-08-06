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

#include <unordered_set>

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

  std::vector<std::string> const& projections() const { return _projections; }

  void projections(std::vector<std::string> const& projections) {
    _projections = projections;
  }

  void projections(std::vector<std::string>&& projections) {
    _projections = std::move(projections);
  }

  void projections(std::unordered_set<std::string>&& projections) {
    _projections.clear();
    _projections.reserve(projections.size());
    for (auto& it : projections) {
      _projections.push_back(std::move(it));
    }
  }

  std::vector<size_t> const& coveringIndexAttributePositions() const {
    return _coveringIndexAttributePositions;
  }

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

 protected:
  Variable const* _outVariable;

  /// @brief produce only the following attributes
  std::vector<std::string> _projections;

  /// @brief vector (built up in order of projection attributes) that contains
  /// the position of the index attribute value in the data returned by the
  /// index example, if the index is on ["a", "b", "c"], and the projections are
  /// ["b", "a"], then this vector will contain [1, 0] the vector will only be
  /// populated by IndexNodes, and will be left empty by
  /// EnumerateCollectionNodes
  std::vector<std::size_t> mutable _coveringIndexAttributePositions;
};

}  // namespace aql
}  // namespace arangodb

#endif
