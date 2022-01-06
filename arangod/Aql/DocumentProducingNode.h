////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Aql/Projections.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
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

  arangodb::aql::Projections const& projections() const noexcept;

  arangodb::aql::Projections& projections() noexcept;

  void setProjections(arangodb::aql::Projections projections);

  /// @brief remember the condition to execute for early filtering
  void setFilter(std::unique_ptr<Expression> filter);

  /// @brief return the early pruning condition for the node
  Expression* filter() const { return _filter.get(); }

  /// @brief whether or not the node has an early pruning filter condition
  bool hasFilter() const { return _filter != nullptr; }

  void toVelocyPack(arangodb::velocypack::Builder& builder,
                    unsigned flags) const;

  void setCountFlag() { _count = true; }

  void copyCountFlag(DocumentProducingNode const* other) {
    _count = other->_count;
  }

  /// @brief wheter or not the node can be used for counting
  bool doCount() const;

  ReadOwnWrites canReadOwnWrites() const noexcept { return _readOwnWrites; }

  void setCanReadOwnWrites(ReadOwnWrites v) noexcept { _readOwnWrites = v; }

  size_t maxProjections() const noexcept { return _maxProjections; }

  void setMaxProjections(size_t value) noexcept { _maxProjections = value; }

  // arbitrary default value for the maximum number of projected attributes
  static constexpr size_t kMaxProjections = 5;

 protected:
  Variable const* _outVariable;

  /// @brief produce only the following attributes
  arangodb::aql::Projections _projections;

  /// @brief early filtering condition
  std::unique_ptr<Expression> _filter;

  bool _count;

  /// @brief Whether we should read our own writes performed by the current
  /// query. ATM this is only necessary for UPSERTS.
  ReadOwnWrites _readOwnWrites{ReadOwnWrites::no};

  size_t _maxProjections{kMaxProjections};
};

}  // namespace aql
}  // namespace arangodb
