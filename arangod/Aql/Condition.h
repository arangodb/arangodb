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

#ifndef ARANGOD_AQL_CONDITION_H
#define ARANGOD_AQL_CONDITION_H 1

#include "Basics/Common.h"
#include "Aql/AstNode.h"
#include "Basics/AttributeNameParser.h"
#include "Transaction/Methods.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {

class Ast;
class EnumerateCollectionNode;
class ExecutionPlan;
struct Index;
class SortCondition;
struct Variable;

enum ConditionPartCompareResult {
  IMPOSSIBLE = 0,
  SELF_CONTAINED_IN_OTHER = 1,
  OTHER_CONTAINED_IN_SELF = 2,
  DISJOINT = 3,
  CONVERT_EQUAL = 4,
  UNKNOWN = 5
};

/// @brief side on which an attribute occurs in a condition
enum AttributeSideType { ATTRIBUTE_LEFT, ATTRIBUTE_RIGHT };

struct ConditionPart {
  static ConditionPartCompareResult const ResultsTable[3][7][7];

  ConditionPart() = delete;

  ConditionPart(Variable const*, std::string const&, AstNode const*,
                AttributeSideType, void*);

  ConditionPart(Variable const*,
                std::vector<arangodb::basics::AttributeName> const&,
                AstNode const*, AttributeSideType, void*);

  ~ConditionPart();

  inline int whichCompareOperation() const {
    switch (operatorType) {
      case NODE_TYPE_OPERATOR_BINARY_EQ:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
        return 0;
      case NODE_TYPE_OPERATOR_BINARY_NE:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
        return 1;
      case NODE_TYPE_OPERATOR_BINARY_LT:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
        return 2;
      case NODE_TYPE_OPERATOR_BINARY_LE:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
        return 3;
      case NODE_TYPE_OPERATOR_BINARY_GE:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
        return 4;
      case NODE_TYPE_OPERATOR_BINARY_GT:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
        return 5;
      default:
        return 6;  // not a compare operator.
    }
  }

  /// @brief returns the lower bound
  inline AstNode const* lowerBound() const {
    if (operatorType == NODE_TYPE_OPERATOR_BINARY_GT ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_GE ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_EQ) {
      return valueNode;
    }

    if (operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
        valueNode->isConstant() && valueNode->isArray() &&
        valueNode->numMembers() > 0) {
      // return first item from IN array.
      // this requires IN arrays to be sorted, which they should be when
      // we get here
      return valueNode->getMember(0);
    }

    return nullptr;
  }

  /// @brief returns if the lower bound is inclusive
  inline bool isLowerInclusive() const {
    if (operatorType == NODE_TYPE_OPERATOR_BINARY_GE ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_EQ ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_IN) {
      return true;
    }

    return false;
  }

  /// @brief returns the upper bound
  inline AstNode const* upperBound() const {
    if (operatorType == NODE_TYPE_OPERATOR_BINARY_LT ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_LE ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_EQ) {
      return valueNode;
    }

    if (operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
        valueNode->isConstant() && valueNode->isArray() &&
        valueNode->numMembers() > 0) {
      // return last item from IN array.
      // this requires IN arrays to be sorted, which they should be when
      // we get here
      return valueNode->getMember(valueNode->numMembers() - 1);
    }

    return nullptr;
  }

  /// @brief returns if the upper bound is inclusive
  inline bool isUpperInclusive() const {
    if (operatorType == NODE_TYPE_OPERATOR_BINARY_LE ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_EQ ||
        operatorType == NODE_TYPE_OPERATOR_BINARY_IN) {
      return true;
    }
    return false;
  }

  /// @brief true if the condition is completely covered by the other condition
  bool isCoveredBy(ConditionPart const&, bool) const;

  Variable const* variable;
  std::string attributeName;
  AstNodeType operatorType;
  AstNode const* operatorNode;
  AstNode const* valueNode;
  void* data;
  bool isExpanded;
};

class Condition {
 private:
  typedef std::vector<std::pair<size_t, AttributeSideType>> UsagePositionType;
  typedef std::unordered_map<std::string, UsagePositionType> AttributeUsageType;
  typedef std::unordered_map<Variable const*, AttributeUsageType>
      VariableUsageType;

 public:
  Condition(Condition const&) = delete;
  Condition& operator=(Condition const&) = delete;
  Condition() = delete;

  /// @brief create the condition
  explicit Condition(Ast*);

  /// @brief destroy the condition
  ~Condition();

 public:
  static void CollectOverlappingMembers(
      ExecutionPlan const* plan, Variable const* variable, AstNode* andNode,
      AstNode* otherAndNode, std::unordered_set<size_t>& toRemove,
                                        bool isFromTraverser);

  /// @brief return the condition root
  inline AstNode* root() const { return _root; }

  /// @brief whether or not the condition is empty
  inline bool isEmpty() const {
    if (_root == nullptr) {
      return true;
    }

    return (_root->numMembers() == 0);
  }

  /// @brief whether or not the condition results will be sorted (this is only
  /// relevant if the condition consists of multiple ORs)
  inline bool isSorted() const { return _isSorted; }

  /// @brief export the condition as VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&, bool) const;

  /// @brief create a condition from VPack
  static Condition* fromVPack(ExecutionPlan*, arangodb::velocypack::Slice const&);

  /// @brief clone the condition
  Condition* clone() const;

  /// @brief add a sub-condition to the condition
  /// the sub-condition will be AND-combined with the existing condition(s)
  void andCombine(AstNode const*);

  /// @brief normalize the condition
  /// this will convert the condition into its disjunctive normal form
  void normalize(ExecutionPlan*);

  /// @brief normalize the condition
  /// this will convert the condition into its disjunctive normal form
  /// in this case we don't re-run the optimizer. Its expected that you
  /// don't want to remove eventually unneccessary filters.
  void normalize();

  /// @brief removes condition parts from another
  AstNode* removeIndexCondition(ExecutionPlan const*, Variable const*, AstNode*);
  
  /// @brief removes condition parts from another
  AstNode* removeTraversalCondition(ExecutionPlan const*, Variable const*, AstNode*);

  /// @brief remove (now) invalid variables from the condition
  bool removeInvalidVariables(std::unordered_set<Variable const*> const&);

  /// @brief locate indexes which can be used for conditions
  /// return value is a pair indicating whether the index can be used for
  /// filtering(first) and sorting(second)
  std::pair<bool, bool> findIndexes(EnumerateCollectionNode const*,
                                    std::vector<transaction::Methods::IndexHandle>&,
                                    SortCondition const*);

  /// @brief get the attributes for a sub-condition that are const
  /// (i.e. compared with equality)
  std::vector<std::vector<arangodb::basics::AttributeName>> getConstAttributes (Variable const*, bool);

 private:

  /// @brief sort ORs for the same attribute so they are in ascending value
  /// order. this will only work if the condition is for a single attribute
  bool sortOrs(Variable const*, std::vector<Index const*>&);

  /// @brief optimize the condition expression tree
  void optimize(ExecutionPlan*);

  /// @brief registers an attribute access for a particular (collection)
  /// variable
  void storeAttributeAccess(
      std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>& varAccess,
      VariableUsageType&, AstNode const*, size_t, AttributeSideType);

/// @brief validate the condition's AST
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  void validateAst(AstNode const*, int);
#endif

  /// @brief checks if the current condition covers the other
  static bool CanRemove(ExecutionPlan const*, ConditionPart const&, AstNode const*,
                        bool isFromTraverser);

  /// @brief deduplicate IN condition values
  /// this may modify the node in place
  void deduplicateInOperation(AstNode*);

  /// @brief merge the values from two IN operations
  AstNode* mergeInOperations(transaction::Methods* trx, AstNode const*, AstNode const*);

  /// @brief merges the current node with the sub nodes of same type
  AstNode* collapse(AstNode const*);

  /// @brief converts binary logical operators into n-ary operators
  AstNode* transformNode(AstNode*);

  /// @brief Creates a top-level OR node if it does not already exist, and make
  /// sure that all second level nodes are AND nodes. Additionally, this step
  /// will
  /// remove all NOP nodes.
  AstNode* fixRoot(AstNode*, int);

 private:
  /// @brief the AST, used for memory management
  Ast* _ast;

  /// @brief root node of the condition
  AstNode* _root;

  /// @brief whether or not the condition was already normalized
  bool _isNormalized;

  /// @brief whether or not the condition will return a sorted result
  bool _isSorted;
};
}
}

#endif
