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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_INDEX_NODE_H
#define ARANGOD_AQL_INDEX_NODE_H 1

#include "Basics/Common.h"
#include "Aql/Ast.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/types.h"
#include "Aql/Variable.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "Transaction/Methods.h"

#include <velocypack/Slice.h>

namespace arangodb {

namespace aql {
struct Collection;
class Condition;
class ExecutionBlock;
class ExecutionPlan;
struct Index;

/// @brief class IndexNode
class IndexNode : public ExecutionNode, public DocumentProducingNode {
  friend class ExecutionBlock;
  friend class IndexBlock;

 public:
  IndexNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
            Collection const* collection, Variable const* outVariable,
            std::vector<transaction::Methods::IndexHandle> const& indexes,
            Condition* condition, bool reverse);

  IndexNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  ~IndexNode();

  /// @brief return the type of the node
  NodeType getType() const override final { return INDEX; }

  /// @brief return the database
  TRI_vocbase_t* vocbase() const { return _vocbase; }

  /// @brief return the collection
  Collection const* collection() const { return _collection; }

  /// @brief return the condition for the node
  Condition* condition() const { return _condition; }

  /// @brief whether or not all indexes are accessed in reverse order
  bool reverse() const { return _reverse; }
 
  /// @brief set reverse mode  
  void reverse(bool value) { _reverse = value; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&,
                          bool) const override final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(
      std::unordered_set<Variable const*>& vars) const override final;

  /// @brief estimateCost
  double estimateCost(size_t&) const override final;

  /// @brief getIndexes, hand out the indexes used
  std::vector<transaction::Methods::IndexHandle> const& getIndexes() const { return _indexes; }

 private:
  /// @brief the database
  TRI_vocbase_t* _vocbase;

  /// @brief collection
  Collection const* _collection;

  /// @brief the index
  std::vector<transaction::Methods::IndexHandle> _indexes;

  /// @brief the index(es) condition
  Condition* _condition;

  /// @brief the index sort order - this is the same order for all indexes
  bool _reverse;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
