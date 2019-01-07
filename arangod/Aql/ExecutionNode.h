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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

// Execution plans like the one below are made of Nodes that inherit the
// ExecutionNode class as a base class.
//
// Execution plan:
//  Id   NodeType                  Est.   Comment
//   1   SingletonNode                1   * ROOT
//   2   EnumerateCollectionNode   6400     - FOR d IN coll   /* full collection
//   scan */ 3   CalculationNode           6400       - LET #1 =
//   DISTANCE(d.`lat`, d.`lon`, 0, 0)   /* simple expression */   /* collections
//   used: d : coll */ 4   SortNode                  6400       - SORT #1 ASC 5
//   LimitNode                    5       - LIMIT 0, 5 6   ReturnNode 5       -
//   RETURN d
//
// Even though the Singleton Node has a label saying it is the "ROOT" node it
// is not in our definiton. Root Nodes are leaf nodes (at the bottom of the
// list).
//
// To get down (direction to root) from 4 to 5 you need to call getFirst Parent
// on the SortNode(4) to receive a pointer to the LimitNode(5). If you want to
// go up from 5 to 4 (away from root) you need to call getFirstDependency at
// the LimitNode (5) to get a pointer to the SortNode(4).
//
// For most maybe all operations you will only need to operate on the
// Dependencies the parents will be updated automatically.
//
// If you wish to unlink (remove) or replace a node you should do it by using
// one of the plans operations.

#ifndef ARANGOD_AQL_EXECUTION_NODE_H
#define ARANGOD_AQL_EXECUTION_NODE_H 1

#include "Aql/CollectionAccessingNode.h"
#include "Aql/CostEstimate.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/Expression.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "VocBase/LogicalView.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <type_traits>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

class Index;

namespace aql {
class Ast;
struct Collection;
class Condition;
class ExecutionBlock;
class ExecutionEngine;
class TraversalBlock;
class ExecutionPlan;
class RedundantCalculationsReplacer;

/// @brief sort element, consisting of variable, sort direction, and a possible
/// attribute path to dig into the document

struct SortElement {
  Variable const* var;
  bool ascending;
  std::vector<std::string> attributePath;

  SortElement(Variable const* v, bool asc) : var(v), ascending(asc) {}

  SortElement(Variable const* v, bool asc, std::vector<std::string> const& path)
      : var(v), ascending(asc), attributePath(path) {}

  /// @brief stringify a sort element. note: the output of this should match the
  /// stringification output of an AstNode for an attribute access
  /// (e.g. foo.bar => $0.bar)
  std::string toString() const {
    std::string result("$");
    result += std::to_string(var->id);
    for (auto const& it : attributePath) {
      result += "." + it;
    }
    return result;
  }
};

typedef std::vector<SortElement> SortElementVector;

/// @brief class ExecutionNode, abstract base class of all execution Nodes
class ExecutionNode {
  /// @brief node type
  friend class ExecutionBlock;
  friend class TraversalBlock;

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
    REMOTESINGLE = 25,
#ifdef USE_IRESEARCH
    ENUMERATE_IRESEARCH_VIEW,
#endif
    MAX_NODE_TYPE_VALUE
  };

  ExecutionNode() = delete;
  ExecutionNode(ExecutionNode const&) = delete;
  ExecutionNode& operator=(ExecutionNode const&) = delete;

  /// @brief constructor using an id
  ExecutionNode(ExecutionPlan* plan, size_t id)
      : _id(id), _depth(0), _varUsageValid(false), _plan(plan) {}

  /// @brief constructor using a VPackSlice
  ExecutionNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& slice);

  /// @brief destructor, free dependencies
  virtual ~ExecutionNode() {}

 public:
  /// @brief factory from JSON
  static ExecutionNode* fromVPackFactory(ExecutionPlan* plan,
                                         arangodb::velocypack::Slice const& slice);

  /// @brief cast an ExecutionNode to a specific sub-type
  /// in maintainer mode, this function will perform a dynamic_cast and abort
  /// the program if the cast is invalid. in release mode, this function will
  /// perform a static_cast and will not abort the program
  template <typename T, typename FromType>
  static inline T castTo(FromType node) noexcept {
    static_assert(std::is_pointer<T>::value,
                  "invalid type passed into ExecutionNode::castTo");
    static_assert(std::is_pointer<FromType>::value,
                  "invalid type passed into ExecutionNode::castTo");
    static_assert(std::remove_pointer<FromType>::type::IsExecutionNode,
                  "invalid type passed into ExecutionNode::castTo");

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    T result = dynamic_cast<T>(node);
    TRI_ASSERT(result != nullptr);
    return result;
#else
    return static_cast<T>(node);
#endif
  }

  /// @brief return the node's id
  inline size_t id() const { return _id; }

  /// @brief return the type of the node
  virtual NodeType getType() const = 0;

  /// @brief resolve nodeType to a string.
  static std::string const& getTypeString(NodeType type);

  /// @brief return the type name of the node
  std::string const& getTypeString() const;

  /// @brief checks whether we know a type of this kind; throws exception if
  /// not.
  static void validateType(int type);

  /// @brief add a dependency
  void addDependency(ExecutionNode*);

  /// @brief add a parent
  void addParent(ExecutionNode*);

  /// @brief get all dependencies
  TEST_VIRTUAL std::vector<ExecutionNode*> const& getDependencies() const {
    return _dependencies;
  }

  /// @brief returns the first dependency, or a nullptr if none present
  ExecutionNode* getFirstDependency() const {
    if (_dependencies.empty()) {
      return nullptr;
    }
    TRI_ASSERT(_dependencies[0] != nullptr);
    return _dependencies[0];
  }

  /// @brief whether or not the node has a dependency
  bool hasDependency() const { return (_dependencies.size() == 1); }

  /// @brief add the node dependencies to a vector
  void dependencies(std::vector<ExecutionNode*>& result) const {
    for (auto const& it : _dependencies) {
      TRI_ASSERT(it != nullptr);
      result.emplace_back(it);
    }
  }

  /// @brief get all parents
  std::vector<ExecutionNode*> getParents() const { return _parents; }

  /// @brief whether or not the node has a parent
  bool hasParent() const { return (_parents.size() == 1); }

  /// @brief returns the first parent, or a nullptr if none present
  ExecutionNode* getFirstParent() const {
    if (_parents.empty()) {
      return nullptr;
    }
    TRI_ASSERT(_parents[0] != nullptr);
    return _parents[0];
  }

  /// @brief add the node parents to a vector
  void parents(std::vector<ExecutionNode*>& result) const {
    for (auto const& it : _parents) {
      TRI_ASSERT(it != nullptr);
      result.emplace_back(it);
    }
  }

  /// @brief get the node and its dependencies as a vector
  void getDependencyChain(std::vector<ExecutionNode*>& result, bool includeSelf) {
    auto current = this;
    while (current != nullptr) {
      if (includeSelf || current != this) {
        result.emplace_back(current);
      }
      current = current->getFirstDependency();
    }
  }

  /// @brief inspect one index; only skiplist indices which match attrs in
  /// sequence.
  /// returns a a qualification how good they match;
  ///      match->index==nullptr means no match at all.
  enum MatchType {
    FORWARD_MATCH,
    REVERSE_MATCH,
    NOT_COVERED_IDX,
    NOT_COVERED_ATTR,
    NO_MATCH
  };

  /// @brief make a new node the (only) parent of the node
  void setParent(ExecutionNode* p) {
    _parents.clear();
    _parents.emplace_back(p);
  }

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
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const& cache) const = 0;

  /// @brief clone execution Node recursively, this makes the class abstract
  virtual ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                               bool withProperties) const = 0;

  /// @brief execution Node clone utility to be called by derives
  /// @return pointer to a registered node owned by a plan
  ExecutionNode* cloneHelper(std::unique_ptr<ExecutionNode> Other,
                             bool withDependencies, bool withProperties) const;

  /// @brief helper for cloning, use virtual clone methods for dependencies
  void cloneDependencies(ExecutionPlan* plan, ExecutionNode* theClone, bool withProperties) const;

  /// @brief invalidate the cost estimate for the node and its dependencies
  virtual void invalidateCost();

  /// @brief estimate the cost of the node . . .
  /// does not recalculate the estimate if already calculated
  CostEstimate getCost() const;

  /// @brief walk a complete execution plan recursively
  bool walk(WalkerWorker<ExecutionNode>& worker);

  /// serialize parents of each node (used in the explainer)
  static constexpr unsigned SERIALIZE_PARENTS = 1;
  /// include estimate cost  (used in the explainer)
  static constexpr unsigned SERIALIZE_ESTIMATES = 1 << 1;
  /// Print all ExecutionNode information required in cluster snippets
  static constexpr unsigned SERIALIZE_DETAILS = 1 << 2;
  /// include additional function info for explain
  static constexpr unsigned SERIALIZE_FUNCTIONS = 1 << 3;

  /// @brief toVelocyPack, export an ExecutionNode to VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&, unsigned flags, bool keepTopLevelOpen) const;

  /// @brief toVelocyPack
  virtual void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const = 0;

  /** Variables used and set are disjunct!
   *   Variables that are read from must be returned by the
   *   UsedHere functions and variables that are filled by
   *   the corresponding ExecutionBlock must be added in
   *   the SetHere functions.
   */

  /// @brief getVariablesUsedHere, returning a vector
  virtual std::vector<Variable const*> getVariablesUsedHere() const {
    return std::vector<Variable const*>();
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  virtual void getVariablesUsedHere(std::unordered_set<Variable const*>&) const {
    // do nothing!
  }

  /// @brief getVariablesSetHere
  virtual std::vector<Variable const*> getVariablesSetHere() const {
    return std::vector<Variable const*>();
  }

  /// @brief getVariableIdsUsedHere
  std::unordered_set<VariableId> getVariableIdsUsedHere() const {
    auto v(getVariablesUsedHere());

    std::unordered_set<VariableId> ids;
    for (auto& it : v) {
      ids.emplace(it->id);
    }
    return ids;
  }

  /// @brief tests whether the node sets one of the passed variables
  bool setsVariable(std::unordered_set<Variable const*> const& which) const {
    for (auto const& v : getVariablesSetHere()) {
      if (which.find(v) != which.end()) {
        return true;
      }
    }
    return false;
  }

  /// @brief setVarsUsedLater
  void setVarsUsedLater(std::unordered_set<Variable const*>& v) {
    _varsUsedLater = v;
  }

  /// @brief getVarsUsedLater, this returns the set of variables that will be
  /// used later than this node, i.e. in the repeated parents.
  std::unordered_set<Variable const*> const& getVarsUsedLater() const {
    TRI_ASSERT(_varUsageValid);
    return _varsUsedLater;
  }

  /// @brief setVarsValid
  void setVarsValid(std::unordered_set<Variable const*>& v) { _varsValid = v; }

  /// @brief getVarsValid, this returns the set of variables that is valid
  /// for items leaving this node, this includes those that will be set here
  /// (see getVariablesSetHere).
  std::unordered_set<Variable const*> const& getVarsValid() const {
    TRI_ASSERT(_varUsageValid);
    return _varsValid;
  }

  /// @brief setVarUsageValid
  void setVarUsageValid() { _varUsageValid = true; }

  /// @brief invalidateVarUsage
  void invalidateVarUsage() {
    _varsUsedLater.clear();
    _varsValid.clear();
    _varUsageValid = false;
  }

  /// @brief whether or not the subquery is deterministic
  virtual bool isDeterministic() { return true; }

  /// @brief whether or not the node is a data modification node
  virtual bool isModificationNode() const {
    // derived classes can change this
    return false;
  }

  ExecutionPlan const* plan() const { return _plan; }

  ExecutionPlan* plan() { return _plan; }

  /// @brief static analysis, walker class and information collector
  struct VarInfo {
    unsigned int depth;
    RegisterId registerId;

    VarInfo() = delete;
    VarInfo(int depth, RegisterId registerId)
        : depth(depth), registerId(registerId) {
      TRI_ASSERT(registerId < MaxRegisterId);
    }
  };

  struct RegisterPlan final : public WalkerWorker<ExecutionNode> {
    // The following are collected for global usage in the ExecutionBlock,
    // although they are stored here in the node:

    // map VariableIds to their depth and registerId:
    std::unordered_map<VariableId, VarInfo> varInfo;

    // number of variables in the frame of the current depth:
    std::vector<RegisterId> nrRegsHere;

    // number of variables in this and all outer frames together,
    // the entry with index i here is always the sum of all values
    // in nrRegsHere from index 0 to i (inclusively) and the two
    // have the same length:
    std::vector<RegisterId> nrRegs;

    // We collect the subquery nodes to deal with them at the end:
    std::vector<ExecutionNode*> subQueryNodes;

    // Local for the walk:
    unsigned int depth;
    unsigned int totalNrRegs;

   private:
    // This is used to tell all nodes and share a pointer to ourselves
    std::shared_ptr<RegisterPlan>* me;

   public:
    RegisterPlan() : depth(0), totalNrRegs(0), me(nullptr) {
      nrRegsHere.emplace_back(0);
      nrRegs.emplace_back(0);
    }

    void clear();

    void setSharedPtr(std::shared_ptr<RegisterPlan>* shared) { me = shared; }

    // Copy constructor used for a subquery:
    RegisterPlan(RegisterPlan const& v, unsigned int newdepth);
    ~RegisterPlan() {}

    virtual bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
      return false;  // do not walk into subquery
    }

    virtual void after(ExecutionNode* eb) override final;

    RegisterPlan* clone(ExecutionPlan* otherPlan, ExecutionPlan* plan);
  };

  /// @brief static analysis
  void planRegisters(ExecutionNode* super = nullptr);

  /// @brief get RegisterPlan
  RegisterPlan const* getRegisterPlan() const {
    TRI_ASSERT(_registerPlan.get() != nullptr);
    return _registerPlan.get();
  }

  /// @brief get depth
  int getDepth() const { return _depth; }

  /// @brief get registers to clear
  std::unordered_set<RegisterId> const& getRegsToClear() const {
    return _regsToClear;
  }

  /// @brief check if a variable will be used later
  bool isVarUsedLater(Variable const* variable) const {
    return (_varsUsedLater.find(variable) != _varsUsedLater.end());
  }

  /// @brief whether or not the node is in an inner loop
  bool isInInnerLoop() const { return getLoop() != nullptr; }

  /// @brief get the surrounding loop
  ExecutionNode const* getLoop() const;

 protected:
  /// @brief set the id, use with care! The purpose is to use a cloned node
  /// together with the original in the same plan.
  void setId(size_t id) { _id = id; }

  /// @brief this actually estimates the costs as well as the number of items
  /// coming out of the node
  virtual CostEstimate estimateCost() const = 0;

  /// @brief factory for sort elements
  static void getSortElements(SortElementVector& elements, ExecutionPlan* plan,
                              arangodb::velocypack::Slice const& slice, char const* which);

  /// @brief toVelocyPackHelper, for a generic node
  void toVelocyPackHelperGeneric(arangodb::velocypack::Builder&, unsigned flags) const;

  /// @brief set regs to be deleted
  void setRegsToClear(std::unordered_set<RegisterId>&& toClear) {
    _regsToClear = std::move(toClear);
  }

 protected:
  /// @brief node id
  size_t _id;

  /// @brief our dependent nodes
  std::vector<ExecutionNode*> _dependencies;

  /// @brief our parent nodes
  std::vector<ExecutionNode*> _parents;

  /// @brief cost estimate for the node
  CostEstimate mutable _costEstimate;

  /// @brief _varsUsedLater and _varsValid, the former contains those
  /// variables that are still needed further down in the chain. The
  /// latter contains the variables that are set from the dependent nodes
  /// when an item comes into the current node. Both are only valid if
  /// _varUsageValid is true. Use ExecutionPlan::findVarUsage to set
  /// this.
  std::unordered_set<Variable const*> _varsUsedLater;

  std::unordered_set<Variable const*> _varsValid;

  /// @brief depth of the current frame, will be filled in by planRegisters
  int _depth;

  /// @brief whether or not _varsUsedLater and _varsValid are actually valid
  bool _varUsageValid;

  /// @brief _plan, the ExecutionPlan object
  ExecutionPlan* _plan;

  /// @brief info about variables, filled in by planRegisters
  std::shared_ptr<RegisterPlan> _registerPlan;

  /// @brief the following contains the registers which should be cleared
  /// just before this node hands on results. This is computed during
  /// the static analysis for each node using the variable usage in the plan.
  std::unordered_set<RegisterId> _regsToClear;

 public:
  /// @brief maximum register id that can be assigned.
  /// this is used for assertions
  static constexpr RegisterId MaxRegisterId = 1000;

  /// @brief used as "type traits" for ExecutionNodes and derived classes
  static constexpr bool IsExecutionNode = true;
};

/// @brief class SingletonNode
class SingletonNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class SingletonBlock;

  /// @brief constructor with an id
 public:
  SingletonNode(ExecutionPlan* plan, size_t id) : ExecutionNode(plan, id) {}

  SingletonNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
      : ExecutionNode(plan, base) {}

  /// @brief return the type of the node
  NodeType getType() const override final { return SINGLETON; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    return cloneHelper(std::make_unique<SingletonNode>(plan, _id),
                       withDependencies, withProperties);
  }

  /// @brief the cost of a singleton is 1
  CostEstimate estimateCost() const override final;
};

/// @brief class EnumerateCollectionNode
class EnumerateCollectionNode : public ExecutionNode,
                                public DocumentProducingNode,
                                public CollectionAccessingNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class EnumerateCollectionBlock;

  /// @brief constructor with a vocbase and a collection name
 public:
  EnumerateCollectionNode(ExecutionPlan* plan, size_t id, aql::Collection const* collection,
                          Variable const* outVariable, bool random)
      : ExecutionNode(plan, id),
        DocumentProducingNode(outVariable),
        CollectionAccessingNode(collection),
        _random(random) {}

  EnumerateCollectionNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return ENUMERATE_COLLECTION; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief the cost of an enumerate collection node is a multiple of the cost
  /// of its unique dependency
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  /// @brief the node is only non-deterministic if it uses a random sort order
  bool isDeterministic() override final { return !_random; }

  /// @brief enable random iteration of documents in collection
  void setRandom() { _random = true; }

 private:
  /// @brief whether or not we want random iteration
  bool _random;
};

/// @brief class EnumerateListNode
class EnumerateListNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class EnumerateListBlock;
  friend class RedundantCalculationsReplacer;

 public:
  EnumerateListNode(ExecutionPlan* plan, size_t id, Variable const* inVariable,
                    Variable const* outVariable)
      : ExecutionNode(plan, id), _inVariable(inVariable), _outVariable(outVariable) {
    TRI_ASSERT(_inVariable != nullptr);
    TRI_ASSERT(_outVariable != nullptr);
  }

  EnumerateListNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return ENUMERATE_LIST; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief the cost of an enumerate list node
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override final {
    return std::vector<Variable const*>{_inVariable};
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const override final {
    vars.emplace(_inVariable);
  }

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  /// @brief return in variable
  Variable const* inVariable() const { return _inVariable; }

  /// @brief return out variable
  Variable const* outVariable() const { return _outVariable; }

 private:
  /// @brief input variable to read from
  Variable const* _inVariable;

  /// @brief output variable to write to
  Variable const* _outVariable;
};

/// @brief class LimitNode
class LimitNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class LimitBlock;

 public:
  LimitNode(ExecutionPlan* plan, size_t id, size_t offset, size_t limit)
      : ExecutionNode(plan, id), _offset(offset), _limit(limit), _fullCount(false) {}

  LimitNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return LIMIT; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    auto c = std::make_unique<LimitNode>(plan, _id, _offset, _limit);

    if (_fullCount) {
      c->setFullCount();
    }

    return cloneHelper(std::move(c), withDependencies, withProperties);
  }

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief tell the node to fully count what it will limit
  void setFullCount() { _fullCount = true; }

  /// @brief return the offset value
  size_t offset() const { return _offset; }

  /// @brief return the limit value
  size_t limit() const { return _limit; }

 private:
  /// @brief the offset
  size_t _offset;

  /// @brief the limit
  size_t _limit;

  /// @brief whether or not the node should fully count what it limits
  bool _fullCount;
};

/// @brief class CalculationNode
class CalculationNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class CalculationBlock;
  friend class RedundantCalculationsReplacer;

 public:
  CalculationNode(ExecutionPlan* plan, size_t id, Expression* expr,
                  Variable const* conditionVariable, Variable const* outVariable)
      : ExecutionNode(plan, id),
        _conditionVariable(conditionVariable),
        _outVariable(outVariable),
        _expression(expr) {
    TRI_ASSERT(_expression != nullptr);
    TRI_ASSERT(_outVariable != nullptr);
  }

  CalculationNode(ExecutionPlan* plan, size_t id, Expression* expr, Variable const* outVariable)
      : CalculationNode(plan, id, expr, nullptr, outVariable) {}

  CalculationNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  ~CalculationNode() { delete _expression; }

  /// @brief return the type of the node
  NodeType getType() const override final { return CALCULATION; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief return out variable
  Variable const* outVariable() const { return _outVariable; }

  /// @brief return the expression
  Expression* expression() const { return _expression; }

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override final {
    std::unordered_set<Variable const*> vars;
    _expression->variables(vars);

    std::vector<Variable const*> v;
    v.reserve(vars.size());

    for (auto const& vv : vars) {
      v.emplace_back(vv);
    }

    if (_conditionVariable != nullptr) {
      v.emplace_back(_conditionVariable);
    }

    return v;
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const override final {
    _expression->variables(vars);

    if (_conditionVariable != nullptr) {
      vars.emplace(_conditionVariable);
    }
  }

  /// @brief getVariablesSetHere
  virtual std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  bool isDeterministic() override final {
    return _expression->isDeterministic();
  }

 private:
  /// @brief an optional condition variable for the calculation
  Variable const* _conditionVariable;

  /// @brief output variable to write to
  Variable const* _outVariable;

  /// @brief we need to have an expression and where to write the result
  Expression* _expression;
};

/// @brief class SubqueryNode
class SubqueryNode : public ExecutionNode {
  friend class ExecutionNode;
  friend class ExecutionBlock;
  friend class SubqueryBlock;

 public:
  SubqueryNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  SubqueryNode(ExecutionPlan* plan, size_t id, ExecutionNode* subquery, Variable const* outVariable)
      : ExecutionNode(plan, id), _subquery(subquery), _outVariable(outVariable) {
    TRI_ASSERT(_subquery != nullptr);
    TRI_ASSERT(_outVariable != nullptr);
  }

  /// @brief return the type of the node
  NodeType getType() const override final { return SUBQUERY; }

  /// @brief invalidate the cost estimate for the node and its dependencies
  void invalidateCost() override;

  /// @brief return the out variable
  Variable const* outVariable() const { return _outVariable; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief whether or not the subquery is a data-modification operation
  bool isModificationSubquery() const;

  /// @brief getter for subquery
  ExecutionNode* getSubquery() const { return _subquery; }

  /// @brief setter for subquery
  void setSubquery(ExecutionNode* subquery, bool forceOverwrite) {
    TRI_ASSERT(subquery != nullptr);
    TRI_ASSERT((forceOverwrite && _subquery != nullptr) ||
               (!forceOverwrite && _subquery == nullptr));
    _subquery = subquery;
  }

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override final;

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const override final;

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    return std::vector<Variable const*>{_outVariable};
  }

  /// @brief replace the out variable, so we can adjust the name.
  void replaceOutVariable(Variable const* var);

  bool isDeterministic() override final;

  bool isConst();
  bool mayAccessCollections();

 private:
  /// @brief we need to have an expression and where to write the result
  ExecutionNode* _subquery;

  /// @brief variable to write to
  Variable const* _outVariable;
};

/// @brief class FilterNode
class FilterNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class FilterBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructors for various arguments, always with offset and limit
 public:
  FilterNode(ExecutionPlan* plan, size_t id, Variable const* inVariable)
      : ExecutionNode(plan, id), _inVariable(inVariable) {
    TRI_ASSERT(_inVariable != nullptr);
  }

  FilterNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override { return FILTER; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override final {
    return std::vector<Variable const*>{_inVariable};
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const override final {
    vars.emplace(_inVariable);
  }

  Variable const* inVariable() const { return _inVariable; }

 private:
  /// @brief input variable to read from
  Variable const* _inVariable;
};

/// @brief this is an auxilliary struct for processed sort criteria information
struct SortInformation {
  enum Match {
    unequal,                // criteria are unequal
    otherLessAccurate,      // leftmost sort criteria are equal, but other sort
                            // criteria are less accurate than ourselves
    ourselvesLessAccurate,  // leftmost sort criteria are equal, but our own
                            // sort criteria is less accurate than the other
    allEqual                // all criteria are equal
  };

  std::vector<std::tuple<ExecutionNode const*, std::string, bool>> criteria;
  bool isValid = true;
  bool isDeterministic = true;
  bool isComplex = false;

  Match isCoveredBy(SortInformation const& other) {
    if (!isValid || !other.isValid) {
      return unequal;
    }

    if (isComplex || other.isComplex) {
      return unequal;
    }

    size_t const n = criteria.size();
    for (size_t i = 0; i < n; ++i) {
      if (other.criteria.size() <= i) {
        return otherLessAccurate;
      }

      auto ours = criteria[i];
      auto theirs = other.criteria[i];

      if (std::get<2>(ours) != std::get<2>(theirs)) {
        // sort order is different
        return unequal;
      }

      if (std::get<1>(ours) != std::get<1>(theirs)) {
        // sort criterion is different
        return unequal;
      }
    }

    if (other.criteria.size() > n) {
      return ourselvesLessAccurate;
    }

    return allEqual;
  }
};

/// @brief class ReturnNode
class ReturnNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class ReturnBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructors for various arguments, always with offset and limit
 public:
  ReturnNode(ExecutionPlan* plan, size_t id, Variable const* inVariable)
      : ExecutionNode(plan, id), _inVariable(inVariable), _count(false) {
    TRI_ASSERT(_inVariable != nullptr);
  }

  ReturnNode(ExecutionPlan*, arangodb::velocypack::Slice const& base);

  /// @brief return the type of the node
  NodeType getType() const override final { return RETURN; }

  /// @brief tell the node to count the returned values
  void setCount() { _count = true; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief estimateCost
  CostEstimate estimateCost() const override final;

  /// @brief getVariablesUsedHere, returning a vector
  std::vector<Variable const*> getVariablesUsedHere() const override final {
    return std::vector<Variable const*>{_inVariable};
  }

  /// @brief getVariablesUsedHere, modifying the set in-place
  void getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const override final {
    vars.emplace(_inVariable);
  }

  Variable const* inVariable() const { return _inVariable; }

  void inVariable(Variable const* v) { _inVariable = v; }

 private:
  /// @brief the variable produced by Return
  Variable const* _inVariable;

  bool _count;
};

/// @brief class NoResultsNode
class NoResultsNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class NoResultsBlock;

  /// @brief constructor with an id
 public:
  NoResultsNode(ExecutionPlan* plan, size_t id) : ExecutionNode(plan, id) {}

  NoResultsNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
      : ExecutionNode(plan, base) {}

  /// @brief return the type of the node
  NodeType getType() const override final { return NORESULTS; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags) const override final;

  /// @brief creates corresponding ExecutionBlock
  std::unique_ptr<ExecutionBlock> createBlock(
      ExecutionEngine& engine,
      std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const override;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final {
    return cloneHelper(std::make_unique<NoResultsNode>(plan, _id),
                       withDependencies, withProperties);
  }

  /// @brief the cost of a NoResults is 0
  CostEstimate estimateCost() const override final;
};

}  // namespace aql
}  // namespace arangodb

#endif
