////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

// Execution plans like the one below are made of Nodes that inherit the
// ExecutionNode class as a base class.
//
// clang-format off
//
// Execution plan:
//  Id   NodeType                  Est.   Comment
//   1   SingletonNode                1   * ROOT
//   2   EnumerateCollectionNode   6400     - FOR d IN coll   /* full collection scan */
//   3   CalculationNode           6400       - LET #1 = DISTANCE(d.`lat`, d.`lon`, 0, 0)  /* simple expression */   /* collections used: d : coll */
//   4   SortNode                  6400       - SORT #1 ASC
//   5   LimitNode                    5       - LIMIT 0, 5
//   6   ReturnNode                   5       - RETURN d
//
// clang-format on
//
// Even though the Singleton Node has a label saying it is the "ROOT" node it
// is not in our definiton. Root Nodes are leaf nodes (at the bottom of the
// list).
//
// To get down (direction to root) from 4 to 5 you need to call getFirstParent
// on the SortNode(4) to receive a pointer to the LimitNode(5). If you want to
// go up from 5 to 4 (away from root) you need to call getFirstDependency at
// the LimitNode (5) to get a pointer to the SortNode(4).
//
// For most maybe all operations you will only need to operate on the
// Dependencies the parents will be updated automatically.
//
// If you wish to unlink (remove) or replace a node you should do it by using
// one of the plans operations.

#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "Aql/CostEstimate.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/IndexHint.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SortElement.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Basics/TypeTraits.h"
#include "Containers/HashSet.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class Index;

namespace aql {
class Ast;
struct Collection;
class ExecutionBlock;
class ExecutionEngine;
class ExecutionNode;
class ExecutionPlan;
class RegisterInfos;
class Expression;
template<typename T>
struct RegisterPlanWalkerT;
using RegisterPlanWalker = RegisterPlanWalkerT<ExecutionNode>;
template<typename T>
struct RegisterPlanT;
using RegisterPlan = RegisterPlanT<ExecutionNode>;
struct Variable;

size_t estimateListLength(ExecutionPlan const* plan, Variable const* var);

enum class AsyncPrefetchEligibility {
  // enable prefetching for one particular node
  kEnableForNode,
  // disable prefetching for one particular node
  kDisableForNode,
  // disable for node and its dependencies (north of ourselves)
  kDisableForNodeAndDependencies,
  // disable prefetching for entire query
  kDisableGlobally,
};

/// @brief class ExecutionNode, abstract base class of all execution Nodes
class ExecutionNode {
  /// @brief node type
  friend class ExecutionBlock;
  // Needs to inject sensitive RegisterInformation
  friend RegisterPlanWalkerT<ExecutionNode>;
  // We need this to replan the registers within the QuerySnippet.
  // otherwise the local gather node might delete the sorting register...
  friend class QuerySnippet;

 public:
  enum NodeType : int {
    SINGLETON = 1,
    ENUMERATE_COLLECTION = 2,
    // INDEX_RANGE          =  3, // not used anymore
    ENUMERATE_LIST = 4,
    FILTER = 5,
    LIMIT = 6,
    CALCULATION = 7,
    SUBQUERY = 8,
    SORT = 9,
    COLLECT = 10,
    SCATTER = 11,
    GATHER = 12,
    REMOTE = 13,
    INSERT = 14,
    REMOVE = 15,
    REPLACE = 16,
    UPDATE = 17,
    RETURN = 18,
    NORESULTS = 19,
    DISTRIBUTE = 20,
    UPSERT = 21,
    TRAVERSAL = 22,
    INDEX = 23,
    SHORTEST_PATH = 24,
    ENUMERATE_PATHS = 25,
    REMOTESINGLE = 26,
    ENUMERATE_IRESEARCH_VIEW = 27,
    DISTRIBUTE_CONSUMER = 28,
    SUBQUERY_START = 29,
    SUBQUERY_END = 30,
    MATERIALIZE = 31,
    ASYNC = 32,
    MUTEX = 33,
    WINDOW = 34,
    OFFSET_INFO_MATERIALIZE = 35,
    REMOTE_MULTIPLE = 36,
    JOIN = 37,

    MAX_NODE_TYPE_VALUE
  };

  ExecutionNode() = delete;
  ExecutionNode(ExecutionNode const&) = delete;
  ExecutionNode& operator=(ExecutionNode const&) = delete;

 protected:
  /// @brief Clone constructor, used for constructors of derived classes.
  /// Does not clone recursively, does not clone properties (`other.plan()` is
  /// expected to be the same as `plan)`, and does not register this node in the
  /// plan.
  ExecutionNode(ExecutionPlan& plan, ExecutionNode const& other);

 public:
  /// @brief constructor using an id
  ExecutionNode(ExecutionPlan* plan, ExecutionNodeId id);

  /// @brief constructor using a VPackSlice
  ExecutionNode(ExecutionPlan* plan, velocypack::Slice slice);

  /// @brief destructor, free dependencies
  virtual ~ExecutionNode() = default;

  /// @brief factory from JSON
  static ExecutionNode* fromVPackFactory(ExecutionPlan* plan,
                                         velocypack::Slice slice);

  /// @brief remove registers right of (greater than) the specified register
  /// from the internal maps
  void removeRegistersGreaterThan(RegisterId maxRegister);

  /// @brief cast an ExecutionNode to a specific sub-type
  /// in maintainer mode, this function will perform a dynamic_cast and abort
  /// the program if the cast is invalid. in release mode, this function will
  /// perform a static_cast and will not abort the program
  template<typename T, typename FromType>
  static inline T castTo(FromType node) noexcept {
    static_assert(std::is_pointer<T>::value,
                  "invalid type passed into ExecutionNode::castTo");
    static_assert(std::is_pointer<FromType>::value,
                  "invalid type passed into ExecutionNode::castTo");
    static_assert(std::remove_pointer<FromType>::type::IsExecutionNode,
                  "invalid type passed into ExecutionNode::castTo");

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(node != nullptr);
    T result = dynamic_cast<T>(node);
    TRI_ASSERT(result != nullptr)
        << "input node type " << node->getTypeString();
    return result;
#else
    // At least GraphNode is virtually inherited by its subclasses. We have to
    // use dynamic_cast for these types.
    if constexpr (can_static_cast_v<FromType, T>) {
      return static_cast<T>(node);
    } else {
      return dynamic_cast<T>(node);
    }
#endif
  }

  /// @brief return the node's id
  ExecutionNodeId id() const;

  /// @brief return the type of the node
  virtual NodeType getType() const = 0;

  /// @brief return the amount of bytes used
  virtual size_t getMemoryUsedBytes() const = 0;

  /// @brief resolve nodeType to a string_view.
  static std::string_view getTypeString(NodeType type);

  /// @brief return the type name of the node
  std::string_view getTypeString() const;

  /// @brief checks whether we know a type of this kind; throws exception if
  /// not.
  static void validateType(int type);

  /// @brief whether or not a node is located inside a subquery
  bool isInSubquery() const;

  /// @brief add a dependency
  void addDependency(ExecutionNode*);

  /// @brief add a parent
  void addParent(ExecutionNode*);

  /// @brief swap the first dependency
  ///        use with care, will modify the plan
  void swapFirstDependency(ExecutionNode* node);

  /// @brief get all dependencies
  TEST_VIRTUAL std::vector<ExecutionNode*> const& getDependencies() const;

  /// @brief returns the first dependency, or a nullptr if none present
  ExecutionNode* getFirstDependency() const;

  /// @brief whether or not the node has a dependency
  bool hasDependency() const;

  /// @brief add the node dependencies to a vector
  void dependencies(std::vector<ExecutionNode*>& result) const;

  /// @brief get all parents
  std::vector<ExecutionNode*> getParents() const;

  /// @brief whether or not the node has a parent
  bool hasParent() const;

  /// @brief returns the first parent, or a nullptr if none present
  ExecutionNode* getFirstParent() const;

  /// @brief add the node parents to a vector
  void parents(std::vector<ExecutionNode*>& result) const;

  /// @brief get the singleton node of the node
  ExecutionNode const* getSingleton() const;
  ExecutionNode* getSingleton();

  /// @brief get the node and its dependencies as a vector
  void getDependencyChain(std::vector<ExecutionNode*>& result,
                          bool includeSelf);

  /// @brief make a new node the (only) parent of the node
  void setParent(ExecutionNode* p);

  /// @brief replace a dependency, returns true if the pointer was found and
  /// replaced, please note that this does not delete oldNode!
  bool replaceDependency(ExecutionNode* oldNode, ExecutionNode* newNode);

  /// @brief remove a dependency, returns true if the pointer was found and
  /// removed, please note that this does not delete ep!
  bool removeDependency(ExecutionNode*);

  /// @brief remove all dependencies for the given node
  void removeDependencies();

  /// @brief creates corresponding ExecutionBlock
  virtual std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine) const = 0;

  /// @brief clone execution Node recursively
  virtual ExecutionNode* clone(ExecutionPlan* plan,
                               bool withDependencies) const = 0;

  /// @brief execution Node clone utility to be called by derives
  /// @return pointer to a registered node owned by a plan
  ExecutionNode* cloneHelper(std::unique_ptr<ExecutionNode> Other,
                             bool withDependencies) const;

  void cloneWithoutRegisteringAndDependencies(ExecutionPlan& plan,
                                              ExecutionNode& other) const;

  /// @brief helper for cloning, use virtual clone methods for dependencies
  void cloneDependencies(ExecutionPlan* plan, ExecutionNode* theClone) const;

  // clone register plan of dependency, needed when inserting nodes after
  // planning
  void cloneRegisterPlan(ExecutionNode* dependency);

  /// @brief replaces variables in the internals of the execution node
  /// replacements are { old variable id => new variable }
  virtual void replaceVariables(
      std::unordered_map<VariableId, Variable const*> const& replacements);

  /// @brief replaces an attribute access in the internals of the execution
  /// node with a simple variable access
  virtual void replaceAttributeAccess(ExecutionNode const* self,
                                      Variable const* searchVariable,
                                      std::span<std::string_view> attribute,
                                      Variable const* replaceVariable,
                                      size_t index);

  /// @brief check equality of ExecutionNodes
  virtual bool isEqualTo(ExecutionNode const& other) const;

  /// @brief invalidate the cost estimate for the node and its dependencies
  virtual void invalidateCost();

  /// @brief estimate the cost of the node . . .
  /// does not recalculate the estimate if already calculated
  CostEstimate getCost() const;

  /// @brief walk a complete execution plan recursively
  bool walk(WalkerWorkerBase<ExecutionNode>& worker);

  bool walkSubqueriesFirst(WalkerWorkerBase<ExecutionNode>& worker);

  bool flatWalk(WalkerWorkerBase<ExecutionNode>& worker, bool onlyFlattenAsync);

  /// serialize parents of each node (used in the explainer)
  static constexpr unsigned SERIALIZE_PARENTS = 1;
  /// include estimate cost  (used in the explainer)
  static constexpr unsigned SERIALIZE_ESTIMATES = 1 << 1;
  /// Print all ExecutionNode information required in cluster snippets
  static constexpr unsigned SERIALIZE_DETAILS = 1 << 2;
  /// include additional function info for explain
  static constexpr unsigned SERIALIZE_FUNCTIONS = 1 << 3;
  /// include addition information of the register plan for explain
  static constexpr unsigned SERIALIZE_REGISTER_INFORMATION = 1 << 4;

  /// @brief serialize this ExecutionNode to VelocyPack
  void toVelocyPack(velocypack::Builder&, unsigned flags) const;

  /// @brief exports this ExecutionNode with all its dependencies to VelocyPack.
  /// This function implicitly creates an array and serializes all nodes
  /// top-down, i.e., the upmost dependency will be the first, and this node
  /// will be the last in the array.
  void allToVelocyPack(velocypack::Builder&, unsigned flags) const;

  /** Variables used and set are disjunct!
   *   Variables that are read from must be returned by the
   *   UsedHere functions and variables that are filled by
   *   the corresponding ExecutionBlock must be added in
   *   the SetHere functions.
   */

  /// @brief getVariablesUsedHere, modifying the set in-place
  virtual void getVariablesUsedHere(VarSet& vars) const;

  /// @brief getVariablesSetHere
  virtual std::vector<Variable const*> getVariablesSetHere() const;

  /// @brief getVariableIdsUsedHere
  ::arangodb::containers::HashSet<VariableId> getVariableIdsUsedHere() const;

  /// @brief tests whether the node sets one of the passed variables
  bool setsVariable(VarSet const& which) const;

  /// @brief setVarsUsedLater
  void setVarsUsedLater(VarSetStack varStack);

  /// @brief getVarsUsedLater, this returns the set of variables that will be
  /// used later than this node, i.e. in the repeated parents.
  auto getVarsUsedLater() const noexcept -> VarSet const&;

  /// @brief Stack of getVarsUsedLater, needed for spliced subqueries. Index 0
  /// corresponds to the outermost spliced subquery, up to either the top level
  /// or a classic subquery. While the last entry corresponds to the current
  /// level and is the same as getVarsUsedLater().
  /// Is never empty (after the VarUsageFinder ran / if _varUsageValid is true).
  auto getVarsUsedLaterStack() const noexcept -> VarSetStack const&;

  /// @brief setVarsValid
  void setVarsValid(VarSetStack varStack);

  /// @brief set regs to be deleted
  void setRegsToClear(RegIdSet toClear);

  /// @brief set regs to be kept
  void setRegsToKeep(RegIdSetStack toKeep);

  /// @brief getVarsValid, this returns the set of variables that is valid
  /// for items leaving this node, this includes those that will be set here
  /// (see getVariablesSetHere).
  auto getVarsValid() const noexcept -> VarSet const&;

  /// @brief Stack of getVarsValid, needed for spliced subqueries. Index 0
  /// corresponds to the outermost spliced subquery, up to either the top level
  /// or a classic subquery. While the last entry corresponds to the current
  /// level and is the same as getVarsValid().
  /// Is never empty (after the VarUsageFinder ran / if _varUsageValid is true).
  auto getVarsValidStack() const noexcept -> VarSetStack const&;

  /// @brief setVarUsageValid
  void setVarUsageValid();

  /// @brief invalidateVarUsage
  void invalidateVarUsage();

  /// @brief whether or not the subquery is deterministic
  virtual bool isDeterministic();

  /// @brief whether or not the node is a data modification node
  virtual bool isModificationNode() const;

  virtual AsyncPrefetchEligibility canUseAsyncPrefetching() const noexcept;

  ExecutionPlan const* plan() const;

  ExecutionPlan* plan();

  /// @brief static analysis
  void planRegisters(ExplainRegisterPlan = ExplainRegisterPlan::No);

  /// @brief get RegisterPlan
  std::shared_ptr<RegisterPlan> getRegisterPlan() const;

  /// @brief get depth
  int getDepth() const;

  /// @brief get registers to clear
  RegIdSet const& getRegsToClear() const;

  /// @brief check if a variable will be used later
  bool isVarUsedLater(Variable const* variable) const noexcept;

  /// @brief whether or not the node is in an inner loop
  bool isInInnerLoop() const;

  /// @brief get the surrounding loop
  ExecutionNode const* getLoop() const;

  bool isInSplicedSubquery() const noexcept;

  void setIsInSplicedSubquery(bool) noexcept;

  bool isAsyncPrefetchEnabled() const noexcept;

  void setIsAsyncPrefetchEnabled(bool v) noexcept;

  bool isCallstackSplitEnabled() const noexcept {
    return _isCallstackSplitEnabled;
  }

  void enableCallstackSplit() noexcept { _isCallstackSplitEnabled = true; }

  [[nodiscard]] static bool isIncreaseDepth(NodeType type);
  [[nodiscard]] virtual bool isIncreaseDepth() const;
  [[nodiscard]] static bool alwaysCopiesRows(NodeType type);
  [[nodiscard]] virtual bool alwaysCopiesRows() const;

  auto getRegsToKeepStack() const -> RegIdSetStack;

 protected:
  /// @brief serialize this ExecutionNode to VelocyPack.
  /// This function is called as part of `toVelocyPack` and must be overriden in
  /// order to serialize type specific information.
  virtual void doToVelocyPack(velocypack::Builder&, unsigned flags) const = 0;

  /// @brief set the id, use with care! The purpose is to use a cloned node
  /// together with the original in the same plan.
  void setId(ExecutionNodeId id);

  /// @brief this actually estimates the costs as well as the number of items
  /// coming out of the node
  virtual CostEstimate estimateCost() const = 0;

  /// @brief factory for sort elements, used by SortNode and GatherNode
  static void getSortElements(SortElementVector& elements, ExecutionPlan* plan,
                              velocypack::Slice slice, std::string_view which);

  RegisterId variableToRegisterId(Variable const*) const;

  RegisterId variableToRegisterOptionalId(Variable const* var) const;

  RegisterInfos createRegisterInfos(RegIdSet readableInputRegisters,
                                    RegIdSet writableOutputRegisters) const;

  RegisterCount getNrInputRegisters() const;

  RegisterCount getNrOutputRegisters() const;

 protected:
  /// @brief node id
  ExecutionNodeId _id;

  /// @brief our dependent nodes
  std::vector<ExecutionNode*> _dependencies;

  /// @brief our parent nodes
  std::vector<ExecutionNode*> _parents;

  /// @brief cost estimate for the node
  CostEstimate mutable _costEstimate;

  /// @brief _varsUsedLaterStack and _varsValidStack.
  /// The back() always corresponds to the current node, while the lower indexes
  /// correspond to containing spliced subqueries, up to either the top level
  /// or a containing SubqueryNode. Is thus never empty (after VarUsageFinder
  /// ran, i.e. _varUsageValid is true).
  /// VarsUsedLater are variables that are used by any of this node's ancestors.
  /// VarsValid are variables that are either set here, of by any of its
  /// descendants.
  /// Both are set by calling ExecutionPlan::findVarUsage. After that,
  /// _varUsageValid is true.
  VarSetStack _varsUsedLaterStack;
  VarSetStack _varsValidStack;

  /// @brief depth of the current frame, will be filled in by planRegisters
  unsigned int _depth;

  /// @brief whether or not _varsUsedLater and _varsValid are actually valid
  bool _varUsageValid;

  bool _isInSplicedSubquery;

  /// @brief whether or not asynchronous prefetching is enabled for this node
  bool _isAsyncPrefetchEnabled{false};

  /// @brief whether or not this node should split calls to upstream nodes to a
  /// separate thread to avoid the problem of stackoverflows in large queries.
  bool _isCallstackSplitEnabled{false};

  /// @brief _plan, the ExecutionPlan object
  ExecutionPlan* _plan;

  /// @brief info about variables, filled in by planRegisters
  std::shared_ptr<RegisterPlan> _registerPlan;

  /// @brief the following contains the registers which should be cleared
  /// just before this node hands on results. This is computed during
  /// the static analysis for each node using the variable usage in the plan.
  RegIdSet _regsToClear;

  /// @brief contains the registers which should be copied into the next row
  /// This is computed during the static analysis for each node using the
  /// variable usage in the plan.
  RegIdSetStack _regsToKeepStack;

 public:
  /// @brief used as "type traits" for ExecutionNodes and derived classes
  static constexpr bool IsExecutionNode = true;

  enum FlattenType { NONE, INLINE_ASYNC, INLINE_ALL };

 private:
  bool doWalk(WalkerWorkerBase<ExecutionNode>& worker, bool subQueryFirst,
              FlattenType flattenType);
};

}  // namespace aql
}  // namespace arangodb
