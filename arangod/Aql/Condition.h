////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AstNode.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"
#include "Containers/HashSet.h"
#include "Transaction/Methods.h"

#include <string>
#include <vector>

namespace arangodb {
class Index;

namespace aql {

class Ast;
class EnumerateCollectionNode;
class ExecutionPlan;
class SortCondition;
struct AstNode;
struct Variable;

// note to maintainers:
// 
enum class ConditionOptimization {
  None,       // only generic optimizations are made (e.g. AND to n-ry AND, sorting and deduplicating IN nodes )
  NoNegation, // no conversions to negation normal form. Implies NoDNF and no optimization.
  NoDNF,      // no conversions to DNF are made and no condition optimization
  Auto,       // all existing condition optimizations are applied

};

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
  ConditionPart() = delete;

  ConditionPart(Variable const*, std::string const&, AstNode const*,
                AttributeSideType, void*);

  ConditionPart(Variable const*, std::vector<arangodb::basics::AttributeName> const&,
                AstNode const*, AttributeSideType, void*);

  ~ConditionPart();

  int whichCompareOperation() const;

  /// @brief returns the lower bound
  AstNode const* lowerBound() const;

  /// @brief returns if the lower bound is inclusive
  bool isLowerInclusive() const;

  /// @brief returns the upper bound
  AstNode const* upperBound() const;

  /// @brief returns if the upper bound is inclusive
  bool isUpperInclusive() const;

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
  typedef std::unordered_map<Variable const*, AttributeUsageType> VariableUsageType;

 public:
  Condition(Condition const&) = delete;
  Condition& operator=(Condition const&) = delete;
  Condition() = delete;

  /// @brief create the condition
  explicit Condition(Ast*);

  /// @brief destroy the condition
  ~Condition();

 public:
  /// @brief: note: index may be a nullptr
  static void collectOverlappingMembers(ExecutionPlan const* plan, Variable const* variable,
                                        AstNode const* andNode, AstNode const* otherAndNode,
                                        ::arangodb::containers::HashSet<size_t>& toRemove,
                                        Index const* index, bool isFromTraverser);

  /// @brief return the condition root
  AstNode* root() const;

  /// @brief whether or not the condition is empty
  bool isEmpty() const;

  /// @brief whether or not the condition results will be sorted (this is only
  /// relevant if the condition consists of multiple ORs)
  bool isSorted() const;

  /// @brief export the condition as VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder&, bool) const;

  /// @brief create a condition from VPack
  static std::unique_ptr<Condition> fromVPack(ExecutionPlan*, arangodb::velocypack::Slice const&);

  /// @brief clone the condition
  std::unique_ptr<Condition> clone() const;

  /// @brief add a sub-condition to the condition
  /// the sub-condition will be AND-combined with the existing condition(s)
  void andCombine(AstNode const*);

  /// @brief normalize the condition
  /// this will convert the condition into its disjunctive normal form
  /// @param mutlivalued attributes may have more than one value
  ///                    (ArangoSearch view case)
  /// @param conditionOptimization  allowed condition optimizations
  void normalize(ExecutionPlan*, bool multivalued = false, 
                 ConditionOptimization conditionOptimization = ConditionOptimization::Auto);

  /// @brief normalize the condition
  /// this will convert the condition into its disjunctive normal form
  /// in this case we don't re-run the optimizer. Its expected that you
  /// don't want to remove eventually unneccessary filters.
  void normalize();

  /// @brief removes condition parts from another
  AstNode* removeIndexCondition(ExecutionPlan const* plan, Variable const* variable,
                                AstNode const* condition, Index const* index);

  /// @brief removes condition parts from another
  AstNode* removeTraversalCondition(ExecutionPlan const*, Variable const*, AstNode*);

  /// @brief remove (now) invalid variables from the condition
  bool removeInvalidVariables(VarSet const&);

  /// @brief locate indexes which can be used for conditions
  /// return value is a pair indicating whether the index can be used for
  /// filtering(first) and sorting(second)
  std::pair<bool, bool> findIndexes(EnumerateCollectionNode const*,
                                    std::vector<transaction::Methods::IndexHandle>&,
                                    SortCondition const*);

  /// @brief get the attributes for a sub-condition that are const
  /// (i.e. compared with equality)
  std::vector<std::vector<arangodb::basics::AttributeName>> getConstAttributes(
      Variable const*, bool includeNull) const;

  /// @brief get the attributes for a sub-condition that are not-null
  ::arangodb::containers::HashSet<std::vector<arangodb::basics::AttributeName>> getNonNullAttributes(
      Variable const*) const;

 private:
  /// @brief optimize the condition expression tree
  void optimize(ExecutionPlan*, bool multivalued);

  /// @brief optimize the condition expression tree which is non-DnfConverted
  /// does only basic deduplicating of conditions
  void optimizeNonDnf();

  /// @brief deduplicates conditions in AND/OR node
  void deduplicateJunctionNode(AstNode* unlockedNode);

  /// @brief recursively deduplicates and sorts members in  IN/AND/OR nodes in subtree
  void deduplicateComparisonsRecursive(AstNode* p);

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
  static bool canRemove(ExecutionPlan const*, ConditionPart const&,
                        AstNode const*, bool isFromTraverser);

  /// @brief deduplicate IN condition values
  /// this may modify the node in place
  AstNode* deduplicateInOperation(AstNode*);

  /// @brief merge the values from two IN operations
  AstNode* mergeInOperations(AstNode const*, AstNode const*);

  /// @brief merges the current node with the sub nodes of same type
  AstNode* collapse(AstNode const*);

  /// @brief converts binary to n-ary, comparison normal and negation normal form
  AstNode* transformNodePreorder(
      AstNode*, 
      ConditionOptimization conditionOptimization = ConditionOptimization::Auto);

  /// @brief converts from negation normal to disjunctive normal form
  AstNode* transformNodePostorder(
      AstNode*, 
      ConditionOptimization conditionOptimization = ConditionOptimization::Auto);

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
}  // namespace aql
}  // namespace arangodb

#endif
