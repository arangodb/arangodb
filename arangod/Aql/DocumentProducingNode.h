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

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace aql {
class ExecutionPlan;
class Expression;
struct Variable;

class DocumentProducingNode {
 public:
  explicit DocumentProducingNode(Variable const* outVariable);
  DocumentProducingNode(ExecutionPlan* plan, arangodb::velocypack::Slice slice);

  virtual ~DocumentProducingNode() = default;

 public:
  void cloneInto(ExecutionPlan* plan, DocumentProducingNode& c) const;

  /// @brief return the out variable
  Variable const* outVariable() const;

  std::vector<std::string> const& projections() const noexcept;

  void projections(std::vector<std::string> const& projections);

  void projections(std::vector<std::string>&& projections) noexcept;

  void projections(std::unordered_set<std::string>&& projections);

  std::vector<size_t> const& coveringIndexAttributePositions() const noexcept;
  
  /// @brief remember the condition to execute for early filtering
  void setFilter(std::unique_ptr<Expression> filter);

  /// @brief return the early pruning condition for the node
  Expression* filter() const { return _filter.get(); }
  
  /// @brief whether or not the node has an early pruning filter condition
  bool hasFilter() const { return _filter != nullptr; }

  void toVelocyPack(arangodb::velocypack::Builder& builder, unsigned flags) const;

  void setCountFlag() { _count = true; }

  void copyCountFlag(DocumentProducingNode const* other) {
    _count = other->_count;
  }
  
  /// @brief wheter or not the node can be used for counting
  bool doCount() const;

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
  
  /// @brief early filtering condition
  std::unique_ptr<Expression> _filter;

  bool _count;
};

}  // namespace aql
}  // namespace arangodb

#endif
