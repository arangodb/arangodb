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

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#include "Aql/Projections.h"
#include "Aql/types.h"
#include "Utils/OperationOptions.h"

namespace arangodb {
namespace velocypack {

class Builder;
class Slice;

}  // namespace velocypack
namespace aql {

class ExecutionNode;
class ExecutionPlan;
class Expression;
struct Variable;

enum class AsyncPrefetchEligibility;

class DocumentProducingNode {
 public:
  explicit DocumentProducingNode(Variable const* outVariable);
  DocumentProducingNode(ExecutionPlan* plan, velocypack::Slice slice);

  virtual ~DocumentProducingNode() = default;

 public:
  void cloneInto(ExecutionPlan* plan, DocumentProducingNode& c) const;

  /// @brief replaces variables in the internals of the execution node
  /// replacements are { old variable id => new variable }
  void replaceVariables(
      std::unordered_map<VariableId, Variable const*> const& replacements);

  void replaceAttributeAccess(ExecutionNode const* self,
                              Variable const* searchVariable,
                              std::span<std::string_view> attribute,
                              Variable const* replaceVariable);

  /// @brief return the out variable
  Variable const* outVariable() const;

  std::vector<Variable const*> getVariablesSetHere() const;

  Projections const& projections() const noexcept;

  Projections& projections() noexcept;

  virtual void setProjections(Projections projections);

  /// @brief remember the condition to execute for early filtering
  virtual void setFilter(std::unique_ptr<Expression> filter);

  /// @brief return the early pruning condition for the node
  Expression* filter() const noexcept { return _filter.get(); }

  void setFilterProjections(Projections projections);

  Projections const& filterProjections() const noexcept;

  /// @brief whether or not the node has an early pruning filter condition
  bool hasFilter() const noexcept { return _filter != nullptr; }

  void toVelocyPack(velocypack::Builder& builder, unsigned flags) const;

  void setCountFlag() noexcept { _count = true; }

  void copyCountFlag(DocumentProducingNode const* other) noexcept {
    _count = other->_count;
  }

  /// @brief whether or not the node can be used for counting
  bool doCount() const noexcept;

  [[nodiscard]] bool useCache() const noexcept { return _useCache; }

  void setUseCache(bool value) noexcept { _useCache = value; }

  ReadOwnWrites canReadOwnWrites() const noexcept { return _readOwnWrites; }

  void setCanReadOwnWrites(ReadOwnWrites v) noexcept { _readOwnWrites = v; }

  size_t maxProjections() const noexcept { return _maxProjections; }

  void setMaxProjections(size_t value) noexcept { _maxProjections = value; }

  AsyncPrefetchEligibility canUseAsyncPrefetching() const noexcept;

  // arbitrary default value for the maximum number of projected attributes
  static constexpr size_t kMaxProjections = 5;

  // returns true if projections have been updated
  virtual bool recalculateProjections(ExecutionPlan* plan);

  virtual bool isProduceResult() const = 0;

 protected:
  Variable const* _outVariable;

  /// @brief produce only the following attributes
  Projections _projections;

  Projections _filterProjections;

  /// @brief early filtering condition
  std::unique_ptr<Expression> _filter;

  bool _count;

  bool _useCache = true;

  /// @brief Whether we should read our own writes performed by the current
  /// query. ATM this is only necessary for UPSERTS.
  ReadOwnWrites _readOwnWrites{ReadOwnWrites::no};

  size_t _maxProjections{kMaxProjections};
};

}  // namespace aql
}  // namespace arangodb
