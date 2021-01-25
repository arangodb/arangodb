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

#include "Condition.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Transaction/CountCache.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#ifdef _WIN32
// turn off warnings about too long type name for debug symbols blabla in MSVC
// only...
#pragma warning(disable : 4503)
#endif

using namespace arangodb;
using namespace arangodb::aql;
using CompareResult = ConditionPartCompareResult;

namespace {
// sort comparisons so that > and >= come before < and <=, and that
// != and > come before ==
// we use this to some advantage when we check the conditions for a sparse
// index later.
// if a sparse index is asked whether it can supported a condition such as
// `attr < value1`, this range would include `null`, which the sparse index
// cannot provide.
// however, if we first check other conditions we may find a condition on
// the same attribute, e.g. `attr > value2`.
// this other condition may exclude `null` so we then use the full range
// `value2 < attr < value1` and do not have to discard sub-conditions anymore
// we can also benefit from sorting != before == for hash indexes, if there
// is a condition that excludes null (e.g. != null). if this is tracked first,
// we are sure the index attribute value cannot be null and we can still use
// the sparse index
std::function<int(AstNode const*)> const operationWeight = [](AstNode const* node) {
  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_NE:
      // != before ==, e.g. attr != null && attr == FUNC(abc) for hash
      // indexes
      return 1;
    case NODE_TYPE_OPERATOR_BINARY_GT:
      // > before others <, e.g. attr > null && attr < abc
      return 2;
    case NODE_TYPE_OPERATOR_BINARY_GE:
      // >= before others <, e.g. attr >= null && attr < abc
      return 3;
    case NODE_TYPE_OPERATOR_BINARY_EQ:
      // != before ==, e.g. attr != null && attr == FUNC(abc) for hash
      // indexes
      return 4;
    case NODE_TYPE_OPERATOR_BINARY_IN:
      return 5;
    case NODE_TYPE_OPERATOR_BINARY_NIN:
      return 6;
    case NODE_TYPE_OPERATOR_BINARY_LT:
      // < after others, e.g. attr > null && attr < abc
      return 7;
    case NODE_TYPE_OPERATOR_BINARY_LE:
      // <= after others, e.g. attr >= null && attr <= abc
      return 8;
    default:
      // non-comparison types can come after comparisons
      return 9;
  }
};

struct PermutationState {
  PermutationState(arangodb::aql::AstNode const* value, size_t n)
      : value(value), current(0), n(n) {}

  arangodb::aql::AstNode const* getValue() const {
    if (value->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR ||
        value->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR) {
      TRI_ASSERT(current < n);
      return value->getMember(current);
    }

    TRI_ASSERT(current == 0);
    return value;
  }

  arangodb::aql::AstNode const* value;
  size_t current;
  size_t const n;
};

//------------------------------------------------------------------------
// Rules for single-valued variables
//------------------------------------------------------------------------
//        |         | a == y | a != y | a <  y | a <= y | a >= y | a > y
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP
// x == y |  a == x |   OIS  |   IMP  |   IMP  |  OIS   |   OIS  |  IMP
// x  > y |         |   IMP  |   OIS  |   IMP  |  IMP   |   OIS  |  OIS
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |  DIJ   |   SIO  |  SIO
// x == y |  a != x |   IMP  |   OIS  |   SIO  |  DIJ   |   DIJ  |  SIO
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP
// x == y |  a <  x |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   IMP  |   OIS  |   OIS  |  OIS   |   IMP  |  IMP
// x == y |  a <= x |   SIO  |   DIJ  |   SIO  |  OIS   |   CEQ  |  IMP
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |  DIJ   |   SIO  |  SIO
// x == y |  a >= x |   SIO  |   DIJ  |   IMP  |  CEQ   |   OIS  |  SIO
// x  > y |         |   IMP  |   OIS  |   IMP  |  IMP   |   OIS  |  OIS
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |   DIJ  |   SIO  |  SIO
// x == y |  a >  x |   IMP  |   OIS  |   IMP  |   IMP  |   OIS  |  OIS
// x  > y |         |   IMP  |   OIS  |   IMP  |   IMP  |   OIS  |  OIS
//------------------------------------------------------------------------
// the 7th column is here as fallback if the operation is not in the table
// above.
// IMP -> IMPOSSIBLE -> empty result -> the complete AND set of conditions can
// be dropped.
// CEQ -> CONVERT_EQUAL -> both conditions can be combined to a equals x.
// DIJ -> DISJOINT -> neither condition is a consequence of the other -> both
// have to stay in place.
// SIO -> SELF_CONTAINED_IN_OTHER -> the left condition is a consequence of the
// right condition
// OIS -> OTHER_CONTAINED_IN_SELF -> the right condition is a consequence of the
// left condition
// If a condition (A) is a consequence of another (B), the solution set of A is
// larger than that of B
//  -> A can be dropped.

ConditionPartCompareResult const ResultsTable[3][7][7] = {
    {// X < Y
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT,
      SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT,
      SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT,
      SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}},
    {// X == Y
     {OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, SELF_CONTAINED_IN_OTHER, DISJOINT,
      DISJOINT, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER,
      OTHER_CONTAINED_IN_SELF, CONVERT_EQUAL, IMPOSSIBLE, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, IMPOSSIBLE, CONVERT_EQUAL,
      OTHER_CONTAINED_IN_SELF, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE,
      OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}},
    {// X > Y
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE,
      OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER,
      SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER,
      SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER,
      SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE,
      OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, IMPOSSIBLE,
      OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}}};

//------------------------------------------------------------------------
// Rules for multi-valued variables
//------------------------------------------------------------------------
//        |         | a == y | a != y | a <  y | a <= y | a >= y | a > y
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   DIJ  |   DIJ  |   OIS  |  OIS   |   DIJ  |  DIJ
// x == y |  a == x |   OIS  |   IMP  |   DIJ  |  OIS   |   OIS  |  DIJ
// x  > y |         |   DIJ  |   DIJ  |   DIJ  |  DIJ   |   OIS  |  OIS
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   DIJ  |   DIJ  |   DIJ  |  DIJ   |   DIJ  |  DIJ
// x == y |  a != x |   IMP  |   OIS  |   DIJ  |  DIJ   |   DIJ  |  DIJ
// x  > y |         |   DIJ  |   DIJ  |   DIJ  |  DIJ   |   DIJ  |  DIJ
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   DIJ  |   DIJ  |   OIS  |  OIS   |   DIJ  |  DIJ
// x == y |  a <  x |   DIJ  |   DIJ  |   OIS  |  OIS   |   DIJ  |  DIJ
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   DIJ  |   DIJ  |   OIS  |  OIS   |   DIJ  |  DIJ
// x == y |  a <= x |   SIO  |   DIJ  |   SIO  |  OIS   |   DIJ  |  DIJ
// x  > y |         |   SIO  |   DIJ  |   SIO  |  SIO   |   DIJ  |  DIJ
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |  DIJ   |   SIO  |  SIO
// x == y |  a >= x |   SIO  |   DIJ  |   DIJ  |  DIJ   |   OIS  |  SIO
// x  > y |         |   DIJ  |   DIJ  |   DIJ  |  DIJ   |   OIS  |  OIS
// -------|------------------|--------|--------|--------|--------|--------
// x  < y |         |   SIO  |   DIJ  |   DIJ  |  DIJ   |   SIO  |  SIO
// x == y |  a >  x |   DIJ  |   DIJ  |   DIJ  |  DIJ   |   OIS  |  OIS
// x  > y |         |   DIJ  |   DIJ  |   DIJ  |  DIJ   |   OIS  |  OIS
//------------------------------------------------------------------------
// the 7th column is here as fallback if the operation is not in the table
// above.
// IMP -> IMPOSSIBLE -> empty result -> the complete AND set of conditions can
// be dropped.
// CEQ -> CONVERT_EQUAL -> both conditions can be combined to a equals x.
// DIJ -> DISJOINT -> neither condition is a consequence of the other -> both
// have to stay in place.
// SIO -> SELF_CONTAINED_IN_OTHER -> the left condition is a consequence of the
// right condition
// OIS -> OTHER_CONTAINED_IN_SELF -> the right condition is a consequence of the
// left condition
// If a condition (A) is a consequence of another (B), the solution set of A is
// larger than that of B
//  -> A can be dropped.

ConditionPartCompareResult const ResultsTableMultiValued[3][7][7] = {
    {// X < Y
     {DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      DISJOINT, DISJOINT, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT},
     {DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      DISJOINT, DISJOINT, DISJOINT},
     {DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      DISJOINT, DISJOINT, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT,
      SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT,
      SELF_CONTAINED_IN_OTHER, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}},
    {// X == Y
     {OTHER_CONTAINED_IN_SELF, IMPOSSIBLE, DISJOINT, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, DISJOINT, DISJOINT},
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT},
     {DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF, OTHER_CONTAINED_IN_SELF,
      DISJOINT, DISJOINT, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER,
      OTHER_CONTAINED_IN_SELF, DISJOINT, DISJOINT, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT,
      OTHER_CONTAINED_IN_SELF, SELF_CONTAINED_IN_OTHER, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}},
    {// X > Y
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER,
      SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
     {SELF_CONTAINED_IN_OTHER, DISJOINT, SELF_CONTAINED_IN_OTHER,
      SELF_CONTAINED_IN_OTHER, DISJOINT, DISJOINT, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, OTHER_CONTAINED_IN_SELF,
      OTHER_CONTAINED_IN_SELF, DISJOINT},
     {DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT, DISJOINT}}};
}  // namespace

ConditionPart::ConditionPart(Variable const* variable, std::string const& attributeName,
                             AstNode const* operatorNode, AttributeSideType side, void* data)
    : variable(variable),
      attributeName(attributeName),
      operatorType(operatorNode->type),
      operatorNode(operatorNode),
      valueNode(nullptr),
      data(data),
      isExpanded(false) {
  if (side == ATTRIBUTE_LEFT) {
    valueNode = operatorNode->getMember(1);
  } else {
    valueNode = operatorNode->getMember(0);
    if (Ast::IsReversibleOperator(operatorType)) {
      operatorType = Ast::ReverseOperator(operatorType);
    }
  }

  isExpanded = (attributeName.find("[*]") != std::string::npos);
}

ConditionPart::ConditionPart(Variable const* variable,
                             std::vector<arangodb::basics::AttributeName> const& attributeNames,
                             AstNode const* operatorNode, AttributeSideType side, void* data)
    : ConditionPart(variable, "", operatorNode, side, data) {
  TRI_AttributeNamesToString(attributeNames, attributeName, false);
  isExpanded = (attributeName.find("[*]") != std::string::npos);
}

ConditionPart::~ConditionPart() = default;

/// @brief true if the condition is completely covered by the other condition
bool ConditionPart::isCoveredBy(ConditionPart const& other, bool isReversed) const {
  if (variable != other.variable || attributeName != other.attributeName) {
    return false;
  }

  if (!isExpanded && !other.isExpanded && other.operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.valueNode->isConstant() && isReversed) {
    if (CompareAstNodes(other.valueNode, valueNode, false) == 0) {
      return true;
    }
  }

  TRI_ASSERT(valueNode != nullptr);
  TRI_ASSERT(other.valueNode != nullptr);

  if (!valueNode->isConstant() || !other.valueNode->isConstant()) {
    return false;
  }

  // special cases for IN...
  if (!isExpanded && !other.isExpanded && other.operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.valueNode->isConstant() && other.valueNode->isArray()) {
    if (operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
        valueNode->isConstant() && valueNode->isArray()) {
      // compare IN with an IN
      // this has quadratic complexity
      size_t const n1 = valueNode->numMembers();
      size_t const n2 = other.valueNode->numMembers();

      // maximum number of comparisons that we will accept
      // otherwise the optimization will be aborted
      static size_t const MaxComparisons = 2048;

      if (n1 * n2 < MaxComparisons) {
        for (size_t i = 0; i < n1; ++i) {
          auto v = valueNode->getMemberUnchecked(i);
          for (size_t j = 0; j < n2; ++j) {
            auto w = other.valueNode->getMemberUnchecked(j);

            ConditionPartCompareResult res =
                ResultsTable[CompareAstNodes(v, w, true) + 1][0][0];

            if (res != CompareResult::OTHER_CONTAINED_IN_SELF &&
                res != CompareResult::CONVERT_EQUAL && res != CompareResult::IMPOSSIBLE) {
              return false;
            }
          }
        }
      } else {
        std::unordered_set<AstNode const*, AstNodeValueHash, AstNodeValueEqual> values(
            512, AstNodeValueHash(), AstNodeValueEqual());

        for (size_t i = 0; i < n2; ++i) {
          values.emplace(other.valueNode->getMemberUnchecked(i));
        }

        for (size_t i = 0; i < n1; ++i) {
          auto node = valueNode->getMemberUnchecked(i);
          if (values.find(node) == values.end()) {
            return false;
          }
        }
      }

      return true;
    }

    return false;
  }

  if (isExpanded && other.isExpanded && operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.valueNode->isConstant()) {
    return CompareAstNodes(other.valueNode, valueNode, false) == 0;
  }

  bool a = operatorNode->isArrayComparisonOperator();
  bool b = other.operatorNode->isArrayComparisonOperator();
  if (a || b) {
    if (a != b) {
      return false;
    }
    TRI_ASSERT(operatorNode->numMembers() == 3 && other.operatorNode->numMembers() == 3);

    AstNode* q1 = operatorNode->getMemberUnchecked(2);
    TRI_ASSERT(q1->type == NODE_TYPE_QUANTIFIER);
    AstNode* q2 = other.operatorNode->getMemberUnchecked(2);
    TRI_ASSERT(q2->type == NODE_TYPE_QUANTIFIER);
    // do only cover ALL and NONE when both sides have same quantifier
    if (q1->getIntValue() != q2->getIntValue() || q1->getIntValue() == Quantifier::ANY) {
      return false;
    }

    if (isExpanded && other.isExpanded && operatorType == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN &&
        other.operatorType == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN &&
        other.valueNode->isConstant()) {
      return CompareAstNodes(other.valueNode, valueNode, false) == 0;
    }
  }

  // Results are -1, 0, 1, move to 0, 1, 2 for the lookup:
  ConditionPartCompareResult res =
      ResultsTable[CompareAstNodes(other.valueNode, valueNode, true) + 1]
                  [other.whichCompareOperation()][whichCompareOperation()];

  if (res == CompareResult::OTHER_CONTAINED_IN_SELF ||
      res == CompareResult::CONVERT_EQUAL || res == CompareResult::IMPOSSIBLE) {
    return true;
  }

  return false;
}

int ConditionPart::whichCompareOperation() const {
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
AstNode const* ConditionPart::lowerBound() const {
  if (operatorType == NODE_TYPE_OPERATOR_BINARY_GT || operatorType == NODE_TYPE_OPERATOR_BINARY_GE ||
      operatorType == NODE_TYPE_OPERATOR_BINARY_EQ) {
    return valueNode;
  }

  if (operatorType == NODE_TYPE_OPERATOR_BINARY_IN && valueNode->isConstant() &&
      valueNode->isArray() && valueNode->numMembers() > 0) {
    // return first item from IN array.
    // this requires IN arrays to be sorted, which they should be when
    // we get here
    return valueNode->getMember(0);
  }

  return nullptr;
}
bool ConditionPart::isLowerInclusive() const {
  if (operatorType == NODE_TYPE_OPERATOR_BINARY_GE || operatorType == NODE_TYPE_OPERATOR_BINARY_EQ ||
      operatorType == NODE_TYPE_OPERATOR_BINARY_IN) {
    return true;
  }

  return false;
}
AstNode const* ConditionPart::upperBound() const {
  if (operatorType == NODE_TYPE_OPERATOR_BINARY_LT || operatorType == NODE_TYPE_OPERATOR_BINARY_LE ||
      operatorType == NODE_TYPE_OPERATOR_BINARY_EQ) {
    return valueNode;
  }

  if (operatorType == NODE_TYPE_OPERATOR_BINARY_IN && valueNode->isConstant() &&
      valueNode->isArray() && valueNode->numMembers() > 0) {
    // return last item from IN array.
    // this requires IN arrays to be sorted, which they should be when
    // we get here
    return valueNode->getMember(valueNode->numMembers() - 1);
  }

  return nullptr;
}
bool ConditionPart::isUpperInclusive() const {
  if (operatorType == NODE_TYPE_OPERATOR_BINARY_LE || operatorType == NODE_TYPE_OPERATOR_BINARY_EQ ||
      operatorType == NODE_TYPE_OPERATOR_BINARY_IN) {
    return true;
  }
  return false;
}

/// @brief clears the attribute access data
static inline void clearAttributeAccess(
    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>& parts) {
  parts.first = nullptr;
  parts.second.clear();
}

/// @brief create the condition
Condition::Condition(Ast* ast)
    : _ast(ast), _root(nullptr), _isNormalized(false), _isSorted(false) {}

/*namespace {
size_t countNodes(AstNode* node) {
  if (node == nullptr) {
    return 0;
  }

  size_t n = node->numMembers();
  size_t sum = 1;
  for (size_t i = 0; i < n; i++) {
    sum += countNodes(node->getMember(i));
  }

  return sum;
}
}*/

/// @brief destroy the condition
Condition::~Condition() {
  // memory for nodes is not owned and thus not freed by the condition
  // all nodes belong to the AST
  // LOG_TOPIC("12fb9", ERR, Logger::FIXME) << "nodes in tree: " << ::countNodes(_root);
}

/// @brief export the condition as VelocyPack
void Condition::toVelocyPack(arangodb::velocypack::Builder& builder, bool verbose) const {
  if (_root == nullptr) {
    VPackObjectBuilder guard(&builder);
  } else {
    _root->toVelocyPack(builder, verbose);
  }
}

/// @brief create a condition from VPack
std::unique_ptr<Condition> Condition::fromVPack(ExecutionPlan* plan, arangodb::velocypack::Slice const& slice) {
  auto condition = std::make_unique<Condition>(plan->getAst());

  if (slice.isObject() && slice.length() != 0) {
    // note: the AST is responsible for freeing the AstNode later!
    AstNode* node = plan->getAst()->createNode(slice);
    condition->andCombine(node);
  }

  condition->_isNormalized = true;
  condition->_isSorted = false;

  return condition;
}

/// @brief clone the condition
std::unique_ptr<Condition> Condition::clone() const {
  auto copy = std::make_unique<Condition>(_ast);

  if (_root != nullptr) {
    copy->_root = _root->clone(_ast);
  }

  copy->_isNormalized = _isNormalized;

  return copy;
}

/// @brief add a sub-condition to the condition
/// the sub-condition will be AND-combined with the existing condition(s)
void Condition::andCombine(AstNode const* node) {
  if (_isNormalized) {
    // already normalized
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot and-combine normalized condition");
  }

  if (_root == nullptr) {
    // condition was empty before
    _root = _ast->clone(node);
  } else {
    // condition was not empty before, now AND-merge
    _root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, _root,
                                           _ast->clone(node));
  }

  TRI_ASSERT(_root != nullptr);
}

/// @brief locate indexes for each condition
/// return value is a pair indicating whether the index can be used for
/// filtering(first) and sorting(second)
std::pair<bool, bool> Condition::findIndexes(EnumerateCollectionNode const* node,
                                             std::vector<transaction::Methods::IndexHandle>& usedIndexes,
                                             SortCondition const* sortCondition) {
  TRI_ASSERT(usedIndexes.empty());
  Variable const* reference = node->outVariable();
  aql::Collection const& coll = *node->collection();
  std::string collectionName = coll.name();

  transaction::Methods& trx = _ast->query().trxForOptimization();
  if (trx.isInaccessibleCollection(collectionName)) {
    return {false, false};
  }
  
  size_t itemsInIndex;
  if (!collectionName.empty() && collectionName[0] == '_' &&
      collectionName.compare(0, 11, StaticStrings::StatisticsCollection, 11) == 0) {
    // use hard-coded number of items in index, because we are dealing with
    // the statistics collection here. this saves a roundtrip to the DB servers
    // for statistics queries that do not need a fully accurate collection count
    itemsInIndex = 1024;
  } else {
    // estimate for the number of documents in the index. may be outdated...
    itemsInIndex = coll.count(&trx, transaction::CountType::TryCache);
  }
  if (_root == nullptr) {
    size_t dummy;
    return std::make_pair<bool, bool>(
        false, arangodb::aql::utils::getIndexForSortCondition(coll, sortCondition, reference, itemsInIndex,
        node->hint(), usedIndexes, dummy));
  }
  
  return arangodb::aql::utils::getBestIndexHandlesForFilterCondition(coll, _ast, _root,
                                                                     reference, sortCondition,
                                                                     itemsInIndex, node->hint(),
                                                                     usedIndexes, _isSorted);
}

/// @brief get the attributes for a sub-condition that are const
/// (i.e. compared with equality)
std::vector<std::vector<arangodb::basics::AttributeName>> Condition::getConstAttributes(
    Variable const* reference, bool includeNull) const {
  std::vector<std::vector<arangodb::basics::AttributeName>> result;

  if (_root == nullptr) {
    return result;
  }

  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  size_t n = _root->numMembers();

  if (n != 1) {
    // multiple ORs
    return result;
  }

  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> parts;
  AstNode const* node = _root->getMember(0);
  n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
      clearAttributeAccess(parts);

      auto lhs = member->getMember(0);
      auto rhs = member->getMember(1);

      if (lhs->isAttributeAccessForVariable(parts) && parts.first == reference) {
        if (includeNull || ((rhs->isConstant() || rhs->type == NODE_TYPE_REFERENCE) &&
                            !rhs->isNullValue())) {
          result.emplace_back(std::move(parts.second));
        }
      } else if (rhs->isAttributeAccessForVariable(parts) && parts.first == reference) {
        if (includeNull || ((lhs->isConstant() || lhs->type == NODE_TYPE_REFERENCE) &&
                            !lhs->isNullValue())) {
          result.emplace_back(std::move(parts.second));
        }
      }
    }
  }

  return result;
}

/// @brief get the attributes for a sub-condition that are not-null
::arangodb::containers::HashSet<std::vector<arangodb::basics::AttributeName>> Condition::getNonNullAttributes(
    Variable const* reference) const {
  ::arangodb::containers::HashSet<std::vector<arangodb::basics::AttributeName>> result;

  if (_root == nullptr) {
    return result;
  }

  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  size_t n = _root->numMembers();

  if (n != 1) {
    // multiple ORs
    return result;
  }

  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> parts;
  AstNode const* node = _root->getMember(0);
  n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member->type == NODE_TYPE_OPERATOR_BINARY_NE || member->type == NODE_TYPE_OPERATOR_BINARY_GT ||
        member->type == NODE_TYPE_OPERATOR_BINARY_LT) {
      clearAttributeAccess(parts);

      AstNode const* lhs = member->getMember(0);
      AstNode const* rhs = member->getMember(1);
      AstNode const* check = nullptr;

      if (lhs->isConstant() && lhs->isNullValue() && rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS &&
          member->type != NODE_TYPE_OPERATOR_BINARY_GT) {
        // null != doc.value
        // null < doc.value
        check = rhs;
      } else if (rhs->isConstant() && rhs->isNullValue() && lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS &&
                 node->type != NODE_TYPE_OPERATOR_BINARY_LT) {
        // doc.value != null
        // doc.value > null
        check = lhs;
      }

      if (check != nullptr && check->isAttributeAccessForVariable(parts, false) &&
          parts.first == reference) {
        result.emplace(std::move(parts.second));
      }
    }
  }

  return result;
}

/// @brief normalize the condition
/// this will convert the condition into its disjunctive normal form
void Condition::normalize(ExecutionPlan* plan, bool multivalued /*= false*/,
                          ConditionOptimization conditionOptimization /*= ConditionOptimization::Auto*/) {
  if (_isNormalized) {
    // already normalized
    return;
  }

  _root = transformNodePreorder(_root, conditionOptimization);
  _root = transformNodePostorder(_root, conditionOptimization);
  _root = fixRoot(_root, 0);
  if (conditionOptimization != ConditionOptimization::Auto) {
    // DNF conversion is skipped. So condition tree could have arbitrary depth and
    // standart optimization cold not be applied. Let`s do simpler version
    optimizeNonDnf();
  } else {
    optimize(plan, multivalued);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_root != nullptr && conditionOptimization == ConditionOptimization::Auto) {
    // _root->dump(0);
    validateAst(_root, 0);
  }
#endif
}

/// @brief normalize the condition
/// this will convert the condition into its disjunctive normal form
/// in this case we don't re-run the optimizer. Its expected that you
/// don't want to remove eventually unneccessary filters.
void Condition::normalize() {
  if (_isNormalized) {
    // already normalized
    return;
  }

  _root = transformNodePreorder(_root);
  _root = transformNodePostorder(_root);
  _root = fixRoot(_root, 0);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_root != nullptr) {
    validateAst(_root, 0);
  }
#endif
}

void Condition::collectOverlappingMembers(ExecutionPlan const* plan, Variable const* variable,
                                          AstNode const* andNode, AstNode const* otherAndNode,
                                          ::arangodb::containers::HashSet<size_t>& toRemove,
                                          Index const* index, /* may be nullptr */
                                          bool isFromTraverser) {
  bool const isSparse = (index != nullptr && index->sparse());

  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> result;

  size_t const n = andNode->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto operand = andNode->getMemberUnchecked(i);
    bool allowOps = operand->isComparisonOperator();

    if (isSparse && allowOps && !isFromTraverser &&
        (operand->type == NODE_TYPE_OPERATOR_BINARY_NE ||
         operand->type == NODE_TYPE_OPERATOR_BINARY_GT)) {
      // look   for != null   and   > null
      // these can be removed if we are working with a sparse index!
      auto lhs = operand->getMember(0);
      auto rhs = operand->getMember(1);

      lhs = const_cast<AstNode*>(plan->resolveVariableAlias(lhs));
      rhs = const_cast<AstNode*>(plan->resolveVariableAlias(rhs));

      clearAttributeAccess(result);

      // only remove the condition if the index is exactly on the same attribute
      // as the condition
      if (rhs->isNullValue() && lhs->isAttributeAccessForVariable(result, isFromTraverser) &&
          result.first == variable && index->fields().size() == 1 &&
          arangodb::basics::AttributeName::isIdentical(result.second,
                                                       index->fields()[0], false)) {
        toRemove.emplace(i);
        // removed, no need to go on below...
        continue;
      }
    }

    if (isFromTraverser) {
      allowOps = allowOps || operand->isArrayComparisonOperator();
    } else {
      allowOps = allowOps && operand->type != NODE_TYPE_OPERATOR_BINARY_NE &&
                 operand->type != NODE_TYPE_OPERATOR_BINARY_NIN;
    }

    if (allowOps) {
      auto lhs = operand->getMember(0);
      auto rhs = operand->getMember(1);
      
      lhs = const_cast<AstNode*>(plan->resolveVariableAlias(lhs));
      rhs = const_cast<AstNode*>(plan->resolveVariableAlias(rhs));

      if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
          (isFromTraverser && lhs->type == NODE_TYPE_EXPANSION)) {
        clearAttributeAccess(result);

        if (lhs->isAttributeAccessForVariable(result, isFromTraverser) &&
            result.first == variable) {
          ConditionPart current(variable, result.second, operand, ATTRIBUTE_LEFT, nullptr);

          if (canRemove(plan, current, otherAndNode, isFromTraverser)) {
            toRemove.emplace(i);
          }
        }
      }

      if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS || rhs->type == NODE_TYPE_EXPANSION) {
        clearAttributeAccess(result);

        if (rhs->isAttributeAccessForVariable(result, isFromTraverser) &&
            result.first == variable) {
          ConditionPart current(variable, result.second, operand, ATTRIBUTE_RIGHT, nullptr);

          if (canRemove(plan, current, otherAndNode, isFromTraverser)) {
            toRemove.emplace(i);
          }
        }
      }
    }
  }
}

/// @brief removes condition parts from another
AstNode* Condition::removeIndexCondition(ExecutionPlan const* plan, Variable const* variable,
                                         AstNode const* condition, Index const* index) {
  TRI_ASSERT(index != nullptr);

  if (_root == nullptr || condition == nullptr) {
    return _root;
  }

  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  TRI_ASSERT(condition != nullptr);
  TRI_ASSERT(condition->type == NODE_TYPE_OPERATOR_NARY_OR);

  if (condition->numMembers() != 1 && _root->numMembers() != 1) {
    return _root;
  }

  auto andNode = _root->getMemberUnchecked(0);
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
  size_t const n = andNode->numMembers();

  auto conditionAndNode = condition->getMemberUnchecked(0);
  TRI_ASSERT(conditionAndNode->type == NODE_TYPE_OPERATOR_NARY_AND);

  ::arangodb::containers::HashSet<size_t> toRemove;
  collectOverlappingMembers(plan, variable, andNode, conditionAndNode, toRemove, index, false);

  if (toRemove.empty()) {
    return _root;
  }

  // build a new AST condition
  AstNode* newNode = nullptr;

  for (size_t i = 0; i < n; ++i) {
    if (toRemove.find(i) == toRemove.end()) {
      auto what = andNode->getMemberUnchecked(i);

      if (newNode == nullptr) {
        // the only node so far
        newNode = what;
      } else {
        // AND-combine with existing node
        newNode = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                 newNode, what);
      }
    }
  }

  return newNode;
}

/// @brief remove filter conditions already covered by the traversal
AstNode* Condition::removeTraversalCondition(ExecutionPlan const* plan,
                                             Variable const* variable, AstNode* other) {
  if (_root == nullptr || other == nullptr) {
    return _root;
  }
  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  TRI_ASSERT(other != nullptr);
  TRI_ASSERT(other->type == NODE_TYPE_OPERATOR_NARY_OR);
  if (other->numMembers() != 1 && _root->numMembers() != 1) {
    return _root;
  }

  auto andNode = _root->getMemberUnchecked(0);
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
  auto otherAndNode = other->getMemberUnchecked(0);
  TRI_ASSERT(otherAndNode->type == NODE_TYPE_OPERATOR_NARY_AND);
  size_t const n = andNode->numMembers();

  ::arangodb::containers::HashSet<size_t> toRemove;
  collectOverlappingMembers(plan, variable, andNode, otherAndNode, toRemove, nullptr, true);

  if (toRemove.empty()) {
    return _root;
  }

  // build a new AST condition
  AstNode* newNode = nullptr;
  for (size_t i = 0; i < n; ++i) {
    if (toRemove.find(i) == toRemove.end()) {
      auto what = andNode->getMemberUnchecked(i);

      if (newNode == nullptr) {
        // the only node so far
        newNode = what;
      } else {
        // AND-combine with existing node
        newNode = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                 newNode, what);
      }
    }
  }

  return newNode;
}

/// @brief remove (now) invalid variables from the condition
bool Condition::removeInvalidVariables(VarSet const& validVars) {
  if (_root == nullptr) {
    return false;
  }

  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  auto oldRoot = _root;
  _root = _ast->shallowCopyForModify(oldRoot);
  TRI_DEFER(FINALIZE_SUBTREE(_root));

  bool isEmpty = false;

  // handle sub nodes of top-level OR node
  size_t const n = _root->numMembers();
  VarSet varsUsed;

  for (size_t i = 0; i < n; ++i) {
    auto oldAndNode = _root->getMemberUnchecked(i);
    auto andNode = _ast->shallowCopyForModify(oldAndNode);
    TRI_DEFER(FINALIZE_SUBTREE(andNode));
    _root->changeMember(i, andNode);

    TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

    size_t nAnd = andNode->numMembers();
    for (size_t j = 0; j < nAnd; /* no hoisting */) {
      // check which variables are used in each AND
      varsUsed.clear();
      Ast::getReferencedVariables(andNode->getMemberUnchecked(j), varsUsed);

      bool invalid = false;
      for (auto& it : varsUsed) {
        if (validVars.find(it) == validVars.end()) {
          // found an invalid variable here...
          invalid = true;
          break;
        }
      }

      if (invalid) {
        andNode->removeMemberUncheckedUnordered(j);
        // repeat with some member index
        TRI_ASSERT(nAnd > 0);
        --nAnd;
        if (nAnd == 0) {
          isEmpty = true;
        }
      } else {
        ++j;
      }
    }
  }

  return isEmpty;
}


/// @brief recursively deduplicates and sorts members in  IN nodes in subtree
/// also deduplicated AND/OR  nodes
void Condition::deduplicateComparisonsRecursive(AstNode* p) {
  const auto numMembers = p->numMembers();
  for (size_t j = 0; j < numMembers; ++j) {
    auto op = p->getMemberUnchecked(j);
    auto newNode = _ast->shallowCopyForModify(op);
    p->changeMember(j, newNode);
    TRI_DEFER(FINALIZE_SUBTREE(newNode));
    if (newNode->type == NODE_TYPE_OPERATOR_BINARY_IN) {
      auto deduplicated = deduplicateInOperation(newNode);
      p->changeMember(j, deduplicated);
      continue;
    }
    deduplicateComparisonsRecursive(newNode);
    // now all internal nodes collapsed where possible
    // could try to collapse this node
    if (op->type == NODE_TYPE_OPERATOR_NARY_AND ||
        op->type == NODE_TYPE_OPERATOR_NARY_OR) {
      deduplicateJunctionNode(newNode);
    }
  }
}

/// @brief optimize the condition expression tree which is non-DnfConverted
void Condition::optimizeNonDnf() {
  auto oldRoot = _root;
  _root = _ast->shallowCopyForModify(oldRoot);
  TRI_DEFER(FINALIZE_SUBTREE(_root));
  // Sorting and deduplicating all IN/AND/OR nodes
  deduplicateComparisonsRecursive(_root);
}

/// @brief deduplicates conditions in AND/OR node
void Condition::deduplicateJunctionNode(AstNode* unlockedNode) {
  TRI_ASSERT(unlockedNode != nullptr);
  TRI_ASSERT(unlockedNode->type == NODE_TYPE_OPERATOR_NARY_AND ||
             unlockedNode->type == NODE_TYPE_OPERATOR_NARY_OR);
  TRI_ASSERT(!unlockedNode->hasFlag(AstNodeFlagType::FLAG_FINALIZED));
  bool duplicateFound;
  do {
    duplicateFound = false;
    size_t andNumMembers = unlockedNode->numMembers();
    VariableUsageType variableUsage;
    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> varAccess;
    for (size_t j = 0; j < andNumMembers; ++j) {
      auto operand = unlockedNode->getMemberUnchecked(j);
      if (operand->isComparisonOperator()) {
        AstNode const* lhs = operand->getMember(0);
        AstNode const* rhs = operand->getMember(1);

        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          if (lhs->isConstant()) {
            lhs = _ast->resolveConstAttributeAccess(lhs);
          }
          storeAttributeAccess(varAccess, variableUsage, lhs, j, ATTRIBUTE_LEFT);
        }
        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS || rhs->type == NODE_TYPE_EXPANSION) {
          if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->isConstant()) {
            rhs = _ast->resolveConstAttributeAccess(rhs);
          }
          storeAttributeAccess(varAccess, variableUsage, rhs, j, ATTRIBUTE_RIGHT);
        }
      }
    }
    for (auto const& it : variableUsage) {  // iterate node with itself only
      auto variable = it.first;
      for (auto const& it2 : it.second) {
        auto const& attributeName = it2.first;
        auto const& positions = it2.second;

        if (positions.size() <= 1) {
          // none or only one occurence of the attribute
          continue;
        }
        // multiple occurrences of the same attribute
        size_t i = 0;
        do {
          size_t leftPos = positions[i].first;
          auto leftNode = unlockedNode->getMemberUnchecked(leftPos);
          ConditionPart current(variable, attributeName, leftNode, positions[i].second, nullptr);
          if (!current.valueNode->isConstant()) {
            continue;
          }
          size_t j = i + 1;
          while (j < positions.size()) {
            TRI_ASSERT(j != 0);
            auto rightPos = positions[j].first;
            auto rightNode = unlockedNode->getMemberUnchecked(rightPos);
            ConditionPart other(variable, attributeName, rightNode, positions[j].second, nullptr);
            if (!other.valueNode->isConstant()) {
              ++j;
              continue;
            }
            if (current.whichCompareOperation() == other.whichCompareOperation() &&
              CompareAstNodes(current.valueNode, other.valueNode, true) == 0) {// duplicate comparison detected - remove it
              TRI_ASSERT(!positions.empty());
              TRI_ASSERT(j < positions.size());
              unlockedNode->removeMemberUncheckedUnordered(rightPos);
              duplicateFound = true; // one duplicate collapsed. Need to re check resulting node and refill positions
              break;
            }
            ++j;
          }
          ++i;
        } while (i < positions.size() - 1 && !duplicateFound);
        if (duplicateFound) {
          break;
        }
      }
      if (duplicateFound) {
        break;
      }
    }
  } while (duplicateFound);
}

/// @brief optimize the condition expression tree
void Condition::optimize(ExecutionPlan* plan, bool multivalued) {
  if (_root == nullptr) {
    return;
  }

  TRI_ASSERT(_root != nullptr);
  TRI_ASSERT(_root->type == NODE_TYPE_OPERATOR_NARY_OR);

  auto oldRoot = _root;
  _root = _ast->shallowCopyForModify(oldRoot);
  TRI_DEFER(FINALIZE_SUBTREE(_root));

  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> varAccess;

  // handle sub nodes of top-level OR node
  size_t n = _root->numMembers();
  size_t r = 0;

  const auto* resultsTable = multivalued ? ResultsTableMultiValued : ResultsTable;

  while (r < n) {  // foreach OR-Node
    bool retry = false;
    auto oldAnd = _root->getMemberUnchecked(r);
    TRI_ASSERT(oldAnd->type == NODE_TYPE_OPERATOR_NARY_AND);
    auto andNode = _ast->shallowCopyForModify(oldAnd);
    _root->changeMember(r, andNode);
    TRI_DEFER(FINALIZE_SUBTREE(andNode));

  restartThisOrItem:
    size_t andNumMembers = andNode->numMembers();

    // deduplicate and sort all IN arrays
    size_t inComparisons = 0;
    for (size_t j = 0; j < andNumMembers; ++j) {
      auto op = andNode->getMemberUnchecked(j);

      if (op->type == NODE_TYPE_OPERATOR_BINARY_IN) {
        ++inComparisons;
        auto deduplicated = deduplicateInOperation(op);
        andNode->changeMember(j, deduplicated);
      }
    }
    andNumMembers = andNode->numMembers();

    if (andNumMembers <= 1) {
      // simple AND item with 0 or 1 members. nothing to do
      ++r;
      n = _root->numMembers();
      continue;
    }

    TRI_ASSERT(andNumMembers > 1);

    // sort AND parts of each sub-condition so > and >= come before < and <=
    // we use this to some advantage when we check the conditions for a sparse
    // index later.
    // if a sparse index is asked whether it can supported a condition such as
    // `attr < value1`, this range would include `null`, which the sparse index
    // cannot provide.
    // however, if we first check other conditions we may find a condition on
    // the same attribute, e.g. `attr > value2`.
    // this other condition may exclude `null` so we then use the full range
    // `value2 < attr < value1`
    // and do not have to discard sub-conditions anymore
    andNode->sortMembers([](AstNode const* lhs, AstNode const* rhs) {
      // try to re-order comparison operators
      int l = ::operationWeight(lhs);
      int r = ::operationWeight(rhs);
      if (l != r) {
        return l < r;
      }

      // all equal, now check if original types are different
      if (lhs->type != rhs->type) {
        return lhs->type < rhs->type;
      }

      // still all equal
      return false;
    });

    if (inComparisons > 0) {
      // move IN operations to the front to make comparison code below simpler
      std::vector<AstNode*> stack;
      size_t p = andNumMembers - 1;

      for (size_t j = p;; --j) {
        auto op = andNode->getMemberUnchecked(j);

        if (op->type == NODE_TYPE_OPERATOR_BINARY_IN) {
          stack.push_back(op);
        } else {
          if (p != j) {
            andNode->changeMember(p, op);
          }
          --p;
        }
        if (j == 0) {
          break;
        }
      }

      p = 0;
      while (!stack.empty()) {
        auto it = stack.back();
        andNode->changeMember(p++, it);
        stack.pop_back();
      }
    }

    // optimization is only necessary if an AND node has multiple members
    VariableUsageType variableUsage;

    for (size_t j = 0; j < andNumMembers; ++j) {
      auto operand = andNode->getMemberUnchecked(j);

      if (operand->isComparisonOperator()) {
        AstNode const* lhs = operand->getMember(0);
        AstNode const* rhs = operand->getMember(1);

        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          if (lhs->isConstant()) {
            lhs = _ast->resolveConstAttributeAccess(lhs);
          } else {
            lhs = plan->resolveVariableAlias(lhs);
          }
          storeAttributeAccess(varAccess, variableUsage, lhs, j, ATTRIBUTE_LEFT);
        }
        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS || rhs->type == NODE_TYPE_EXPANSION) {
          if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->isConstant()) {
            rhs = _ast->resolveConstAttributeAccess(rhs);
          } else {
            rhs = plan->resolveVariableAlias(rhs);
          }
          storeAttributeAccess(varAccess, variableUsage, rhs, j, ATTRIBUTE_RIGHT);
        }
      }
    }

    // now find the variables and attributes for which there are multiple
    // conditions
    for (auto const& it : variableUsage) {  // foreach sub-and-node
      auto variable = it.first;

      for (auto const& it2 : it.second) {  // cross compare sub-and-nodes
        auto const& attributeName = it2.first;
        auto const& positions = it2.second;

        if (positions.size() <= 1) {
          // none or only one occurence of the attribute
          continue;
        }

        // multiple occurrences of the same attribute
        size_t leftPos = positions[0].first;
        // copy & modify leftNode
        auto oldLeft = andNode->getMemberUnchecked(leftPos);
        auto leftNode = _ast->shallowCopyForModify(oldLeft);
        TRI_DEFER(FINALIZE_SUBTREE(leftNode));
        andNode->changeMember(leftPos, leftNode);

        ConditionPart current(variable, attributeName, leftNode, positions[0].second, nullptr);

        if (!current.valueNode->isConstant()) {
          continue;
        }

        size_t j = 1;

        while (j < positions.size()) {
          TRI_ASSERT(j != 0);
          auto rightPos = positions[j].first;
          auto rightNode = andNode->getMemberUnchecked(rightPos);

          ConditionPart other(variable, attributeName, rightNode, positions[j].second, nullptr);

          if (!other.valueNode->isConstant()) {
            ++j;
            continue;
          }

          // IN-merging
          if (leftNode->type == NODE_TYPE_OPERATOR_BINARY_IN &&
              leftNode->getMemberUnchecked(1)->isConstant() && !multivalued) {
            TRI_ASSERT(leftNode->numMembers() == 2);

            if (rightNode->type == NODE_TYPE_OPERATOR_BINARY_IN &&
                rightNode->getMemberUnchecked(1)->isConstant()) {
              // merge IN with IN on same attribute
              TRI_ASSERT(rightNode->numMembers() == 2);

              auto merged =
                  _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN,
                                                 leftNode->getMemberUnchecked(0),
                                                 mergeInOperations(leftNode, rightNode));
              andNode->removeMemberUncheckedUnordered(rightPos);
              andNode->changeMember(leftPos, merged);
              goto restartThisOrItem;
            } else if (rightNode->isSimpleComparisonOperator()) {
              // merge other comparison operator with IN
              TRI_ASSERT(rightNode->numMembers() == 2);

              auto values = leftNode->getMemberUnchecked(1);
              auto inNode = _ast->createNodeArray(values->numMembers());

              // enumerate over IN list
              for (size_t k = 0; k < values->numMembers(); ++k) {
                auto value = values->getMemberUnchecked(k);
                ConditionPartCompareResult res =
                    ResultsTable[CompareAstNodes(value, other.valueNode, true) + 1][0 /*NODE_TYPE_OPERATOR_BINARY_EQ*/]
                                [other.whichCompareOperation()];

                bool const keep = (res == CompareResult::OTHER_CONTAINED_IN_SELF ||
                                   res == CompareResult::CONVERT_EQUAL);

                if (keep) {
                  inNode->addMember(value);
                }
              }

              if (inNode->numMembers() == 0) {
                // no values left after merging -> IMPOSSIBLE
                _root->removeMemberUncheckedUnordered(r);
                retry = true;
                goto fastForwardToNextOrItem;
              }

              // use the new array of values
              leftNode->changeMember(1, inNode);

              // remove the other operator
              andNode->removeMemberUncheckedUnordered(rightPos);
              goto restartThisOrItem;
            }
          }
          // end of IN-merging

          // Results are -1, 0, 1, move to 0, 1, 2 for the lookup:
          ConditionPartCompareResult res =
              resultsTable[CompareAstNodes(current.valueNode, other.valueNode, true) + 1]
                          [current.whichCompareOperation()][other.whichCompareOperation()];

          switch (res) {
            case CompareResult::IMPOSSIBLE: {
              // impossible condition
              // j = positions.size();
              // we remove this one, so fast forward the loops to their end:
              _root->removeMemberUncheckedUnordered(r);
              retry = true;
              goto fastForwardToNextOrItem;
            }
            case CompareResult::SELF_CONTAINED_IN_OTHER: {
              TRI_ASSERT(!positions.empty());
              andNode->removeMemberUncheckedUnordered(positions.at(0).first);
              goto restartThisOrItem;
            }
            case CompareResult::OTHER_CONTAINED_IN_SELF: {
              TRI_ASSERT(j < positions.size());
              andNode->removeMemberUncheckedUnordered(positions.at(j).first);
              goto restartThisOrItem;
            }
            case CompareResult::CONVERT_EQUAL: {  // both ok, now transform to a
                                                  // == x (== y)
              TRI_ASSERT(!positions.empty());
              TRI_ASSERT(j < positions.size());
              TRI_ASSERT(positions.at(j).first > positions.at(0).first); // in this case remove will not spoil members indexes
              andNode->removeMemberUncheckedUnordered(positions.at(j).first);
              auto origNode = andNode->getMemberUnchecked(positions.at(0).first);
              auto newNode = plan->getAst()->createNode(NODE_TYPE_OPERATOR_BINARY_EQ);
              for (size_t iMemb = 0; iMemb < origNode->numMembers(); iMemb++) {
                newNode->addMember(origNode->getMemberUnchecked(iMemb));
              }
              TRI_DEFER(FINALIZE_SUBTREE(newNode));

              andNode->changeMember(positions.at(0).first, newNode);
              goto restartThisOrItem;
            }
            case CompareResult::DISJOINT: {
              break;
            }
            case CompareResult::UNKNOWN: {
              break;
            }
          }

          ++j;
        }
      }  // cross compare sub-and-nodes
    }    // foreach sub-and-node

  fastForwardToNextOrItem:
    if (!retry) {
      // root nodes hasn't changed. go to next sub-node!
      ++r;
    }
    // number of root sub-nodes has probably changed.
    // now recalculate the number and don't modify r!
    n = _root->numMembers();
  }
}

/// @brief registers an attribute access for a particular (collection) variable
void Condition::storeAttributeAccess(
    std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>>& varAccess,
    VariableUsageType& variableUsage, AstNode const* node, size_t position,
    AttributeSideType side) {
  if (!node->isAttributeAccessForVariable(varAccess)) {
    return;
  }

  auto variable = varAccess.first;

  if (variable != nullptr) {
    std::string attributeName;
    TRI_AttributeNamesToString(varAccess.second, attributeName, false);

    auto& dst = variableUsage[variable][attributeName];
    if (!dst.empty() && dst.back().first == position) {
      // already have this attribute for this variable. can happen in case a
      // condition refers to itself (e.g. a.x == a.x)
      // in this case, we won't optimize it
      dst.erase(dst.begin() + dst.size() - 1);
    } else {
      dst.emplace_back(position, side);
    }
  }
}

/// @brief validate the condition's AST
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void Condition::validateAst(AstNode const* node, int level) {
  if (level == 0) {
    TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_OR);
  }

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);

    if (level == 0) {
      TRI_ASSERT(sub->type == NODE_TYPE_OPERATOR_NARY_AND);
    } else {
      TRI_ASSERT(sub->type != NODE_TYPE_OPERATOR_NARY_OR &&
                 sub->type != NODE_TYPE_OPERATOR_NARY_AND);
    }

    validateAst(sub, level + 1);
  }
}
#endif

/// @brief checks if the current condition is covered by the other
bool Condition::canRemove(ExecutionPlan const* plan, ConditionPart const& me,
                          arangodb::aql::AstNode const* andNode, bool isFromTraverser) {
  TRI_ASSERT(andNode != nullptr);
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

  std::pair<Variable const*, std::vector<arangodb::basics::AttributeName>> result;

  size_t const n = andNode->numMembers();

  auto normalize = [&plan](AstNode const* node) -> std::string {
    if (node->type == NODE_TYPE_REFERENCE) {
      auto setter =
          plan->getVarSetBy(static_cast<Variable const*>(node->getData())->id);
      if (setter != nullptr && setter->getType() == ExecutionNode::CALCULATION) {
        auto cn = ExecutionNode::castTo<CalculationNode const*>(setter);
        // use expression node instead
        node = cn->expression()->node();
      }
    }
    // return string representation
    return node->toString();
  };

  std::string temp;

  try {
    for (size_t i = 0; i < n; ++i) {
      auto operand = andNode->getMemberUnchecked(i);

      if (operand->isComparisonOperator() ||
          (isFromTraverser && operand->isArrayComparisonOperator())) {
        auto lhs = operand->getMember(0);
        auto rhs = operand->getMember(1);

        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
            (isFromTraverser && lhs->type == NODE_TYPE_EXPANSION)) {
          clearAttributeAccess(result);

          if (lhs->isAttributeAccessForVariable(result, isFromTraverser)) {
            temp.clear();
            TRI_AttributeNamesToString(result.second, temp);
            if (temp == me.attributeName) {
              if (rhs->isConstant()) {
                ConditionPart indexCondition(result.first, result.second,
                                             operand, ATTRIBUTE_LEFT, nullptr);

                if (me.isCoveredBy(indexCondition, false)) {
                  return true;
                }
              }
              // non-constant condition
              else if (me.operatorType == operand->type &&
                       normalize(me.valueNode) == normalize(rhs)) {
                return true;
              }
            }
          }
        }

        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS || rhs->type == NODE_TYPE_EXPANSION) {
          clearAttributeAccess(result);

          if (rhs->isAttributeAccessForVariable(result, isFromTraverser)) {
            temp.clear();
            TRI_AttributeNamesToString(result.second, temp);
            if (temp == me.attributeName) {
              if (lhs->isConstant()) {
                ConditionPart indexCondition(result.first, result.second,
                                             operand, ATTRIBUTE_RIGHT, nullptr);

                if (me.isCoveredBy(indexCondition, true)) {
                  return true;
                }
              }
              // non-constant condition
              else if (me.operatorType == operand->type &&
                       normalize(me.valueNode) == normalize(lhs)) {
                return true;
              }
            }
          }
        }
      }
    }
  } catch (...) {
    // simply ignore any errors and return false
  }

  return false;
}

/// @brief deduplicate IN condition values (and sort them)
/// this may modify the node in place
AstNode* Condition::deduplicateInOperation(AstNode* operation) {
  TRI_ASSERT(operation->numMembers() == 2);

  auto rhs = operation->getMemberUnchecked(1);
  if (!rhs->isArray() || !rhs->isConstant()) {
    return operation;
  }

  auto deduplicated = _ast->deduplicateArray(rhs);
  if (deduplicated != rhs) {
    // there were duplicates
    auto newOperation = _ast->shallowCopyForModify(operation);
    TRI_DEFER(FINALIZE_SUBTREE(newOperation));

    newOperation->changeMember(1, const_cast<AstNode*>(deduplicated));
    return newOperation;
  }

  return operation;
}

/// @brief merge the values from two IN operations
AstNode* Condition::mergeInOperations(AstNode const* lhs, AstNode const* rhs) {
  TRI_ASSERT(lhs->type == NODE_TYPE_OPERATOR_BINARY_IN);
  TRI_ASSERT(rhs->type == NODE_TYPE_OPERATOR_BINARY_IN);

  auto lValue = lhs->getMemberUnchecked(1);
  auto rValue = rhs->getMemberUnchecked(1);

  TRI_ASSERT(lValue->isArray() && lValue->isConstant());
  TRI_ASSERT(rValue->isArray() && rValue->isConstant());

  return _ast->createNodeIntersectedArray(lValue, rValue);
}

/// @brief merges the current node with the sub nodes of same type
AstNode* Condition::collapse(AstNode const* node) {
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_OR || node->type == NODE_TYPE_OPERATOR_NARY_AND);

  auto newOperator = _ast->createNode(node->type);

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);
    bool const isSame = (node->type == sub->type) ||
                        (node->type == NODE_TYPE_OPERATOR_NARY_OR &&
                         sub->type == NODE_TYPE_OPERATOR_BINARY_OR) ||
                        (node->type == NODE_TYPE_OPERATOR_NARY_AND &&
                         sub->type == NODE_TYPE_OPERATOR_BINARY_AND);

    if (isSame) {
      // merge children one level up
      for (size_t j = 0; j < sub->numMembers(); ++j) {
        newOperator->addMember(sub->getMemberUnchecked(j));
      }
    } else {
      newOperator->addMember(sub);
    }
  }

  return newOperator;
}

// this may modify the node in place
AstNode* switchSidesInCompare(Ast* ast, AstNode* node) {
  // switch members of BINARY_LT/GT/LE/GE_NODES
  // and change operator accordingly

  auto first = node->getMemberUnchecked(0);
  auto second = node->getMemberUnchecked(1);

  auto newOperator = ast->shallowCopyForModify(node);
  TRI_DEFER(FINALIZE_SUBTREE(newOperator));

  newOperator->changeMember(0, second);
  newOperator->changeMember(1, first);

  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_LT:
      newOperator->type = NODE_TYPE_OPERATOR_BINARY_GT;
      break;
    case NODE_TYPE_OPERATOR_BINARY_GT:
      newOperator->type = NODE_TYPE_OPERATOR_BINARY_LT;
      break;
    case NODE_TYPE_OPERATOR_BINARY_LE:
      newOperator->type = NODE_TYPE_OPERATOR_BINARY_GE;
      break;
    case NODE_TYPE_OPERATOR_BINARY_GE:
      newOperator->type = NODE_TYPE_OPERATOR_BINARY_LE;
      break;
    default:
      LOG_TOPIC("14324", ERR, Logger::QUERIES)
          << "normalize condition tries to swap children"
          << "of wrong node type - this needs to be fixed";
      TRI_ASSERT(false);
  }

  return newOperator;
}

AstNode* normalizeCompare(Ast* ast, AstNode* node) {
  // Moves attribute access to the LHS of a comparison.
  // If there are 2 attribute accesses it does a
  // string compare of the access path and makes sure
  // the one that compares less ends up on the LHS
  if (node->type != NODE_TYPE_OPERATOR_BINARY_LE &&
      node->type != NODE_TYPE_OPERATOR_BINARY_LT && node->type != NODE_TYPE_OPERATOR_BINARY_GE &&
      node->type != NODE_TYPE_OPERATOR_BINARY_GT) {
    // no binary compare in node
    return node;
  }

  auto first = node->getMemberUnchecked(0);
  auto second = node->getMemberUnchecked(1);

  if (second->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    if (first->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
      return switchSidesInCompare(ast, node);
    }

    // both are of type attribute access
    if (first->toString() > second->toString()) {
      return switchSidesInCompare(ast, node);
    }
  }

  return node;
}

/// @brief converts binary to n-ary, comparison normal and negation normal form
AstNode* Condition::transformNodePreorder(
    AstNode* node,
    ConditionOptimization conditionOptimization /*= ConditionOptimization::Auto*/) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->type == NODE_TYPE_OPERATOR_BINARY_AND || node->type == NODE_TYPE_OPERATOR_BINARY_OR) {
    // convert binary AND/OR into n-ary AND/OR
    TRI_ASSERT(node->numMembers() == 2);
    auto old = node;

    // create a new n-ary node
    node = _ast->createNode(Ast::NaryOperatorType(old->type));
    node->reserve(2);
    node->addMember(transformNodePreorder(old->getMember(0), conditionOptimization));
    node->addMember(transformNodePreorder(old->getMember(1), conditionOptimization));
    return node;
  }

  if (node->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
    // push down logical negations
    auto sub = node->getMemberUnchecked(0);
    const bool negationConversion =   conditionOptimization != ConditionOptimization::None &&
                                      conditionOptimization != ConditionOptimization::NoNegation;
    if (negationConversion && (sub->type == NODE_TYPE_OPERATOR_NARY_AND || sub->type == NODE_TYPE_OPERATOR_BINARY_AND ||
        sub->type == NODE_TYPE_OPERATOR_NARY_OR || sub->type == NODE_TYPE_OPERATOR_BINARY_OR)) {
      size_t const n = sub->numMembers();

      AstNode* newOperator = nullptr;
      if (sub->type == NODE_TYPE_OPERATOR_NARY_AND || sub->type == NODE_TYPE_OPERATOR_BINARY_AND) {
        // ! (a && b)  =>  (! a) || (! b)
        newOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_OR);
      } else {
        // ! (a || b)  =>  (! a) && (! b)
        newOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_AND);
      }

      for (size_t i = 0; i < n; ++i) {
        auto negated = transformNodePreorder(
            _ast->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT,
                                          sub->getMemberUnchecked(i)));
        auto optimized = _ast->optimizeNotExpression(negated);
        newOperator->addMember(optimized);
      }

      return newOperator;
    }

    if (sub->type == NODE_TYPE_OPERATOR_UNARY_NOT) {
      // eliminate double-negatives
      return transformNodePreorder(sub->getMemberUnchecked(0), conditionOptimization);
    }

    auto replacement = _ast->shallowCopyForModify(node);
    replacement->changeMember(0, transformNodePreorder(sub, conditionOptimization));

    return replacement;
  }

  // normalize any comparisons
  return normalizeCompare(_ast, node);
}

/// @brief converts from negation normal to disjunctive normal form
AstNode* Condition::transformNodePostorder(
    AstNode* node,
    ConditionOptimization conditionOptimization /*= ConditionOptimization::Auto*/) {
  if (node == nullptr) {
    return node;
  }

  if (node->type == NODE_TYPE_OPERATOR_NARY_AND) {
    auto old = node;
    node = _ast->shallowCopyForModify(old);
    TRI_DEFER(FINALIZE_SUBTREE(node));

    bool distributeOverChildren = false;
    bool mustCollapse = false;
    size_t n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      // process subnodes first
      auto sub = transformNodePostorder(node->getMemberUnchecked(i), conditionOptimization);
      node->changeMember(i, sub);

      if (sub->type == NODE_TYPE_OPERATOR_NARY_OR) {
        distributeOverChildren = true;
      } else if (sub->type == NODE_TYPE_OPERATOR_NARY_AND) {
        mustCollapse = true;
      }
    }

    if (mustCollapse) {
      node = collapse(node);
      // collapsing may change n
      n = node->numMembers();
    }

    if (distributeOverChildren && conditionOptimization == ConditionOptimization::Auto) {
      // we found an AND with at least one OR child, e.g.
      //        AND
      //   OR          c
      // a    b
      //
      // we need to move the OR to the top by converting the condition to:
      //         OR
      //   AND        AND
      //  a   c      b   c
      //

      auto newOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_OR);

      std::vector<::PermutationState> clauses;
      clauses.reserve(n);

      for (size_t i = 0; i < n; ++i) {
        auto sub = node->getMemberUnchecked(i);

        if (sub->type == NODE_TYPE_OPERATOR_NARY_OR) {
          clauses.emplace_back(sub, sub->numMembers());
        } else {
          clauses.emplace_back(sub, 1);
        }
      }

      size_t current = 0;
      bool done = false;
      size_t const numClauses = clauses.size();

      while (!done) {
        auto andOperator = _ast->createNode(NODE_TYPE_OPERATOR_NARY_AND);
        andOperator->reserve(numClauses);

        for (size_t i = 0; i < numClauses; ++i) {
          auto const& clause = clauses[i];
          auto sub = clause.getValue();
          // make sure the subtree is finalized so we can avoid cloning it
          FINALIZE_SUBTREE(sub);
          if (sub->type == NODE_TYPE_OPERATOR_NARY_AND) {
            // collapse, add children directly
            for (size_t j = 0; j < sub->numMembers(); j++) {
              andOperator->addMember(sub->getMember(j));
            }
          } else {
            andOperator->addMember(sub);
          }
        }

        newOperator->addMember(andOperator);

        // now advance the clause permutation state
        while (true) {
          auto& currentClause = clauses[current];
          if (++currentClause.current < currentClause.n) {
            current = 0;
            // still have at least one more permutation with current position
            // in current clause
            break;
          }

          // done with current clause, reset it
          currentClause.current = 0;

          // move on to next clause
          if (++current >= n) {
            // no more clauses left!
            done = true;
            break;
          }
        }
      }

      node = newOperator;
    }

    return node;
  }

  if (node->type == NODE_TYPE_OPERATOR_NARY_OR) {
    auto old = node;
    node = _ast->shallowCopyForModify(old);
    TRI_DEFER(FINALIZE_SUBTREE(node));

    size_t const n = node->numMembers();
    bool mustCollapse = false;

    for (size_t i = 0; i < n; ++i) {
      auto sub = transformNodePostorder(node->getMemberUnchecked(i), conditionOptimization);
      node->changeMember(i, sub);

      if (sub->type == NODE_TYPE_OPERATOR_NARY_OR) {
        mustCollapse = true;
      }
    }

    if (mustCollapse) {
      node = collapse(node);
    }
  }

  // we only need to handle nary and/or, the rest was handled in preorder

  return node;
}

/// @brief Creates a top-level OR node if it does not already exist, and make
/// sure that all second level nodes are AND nodes. Additionally, this step will
/// remove all NOP nodes.
AstNode* Condition::fixRoot(AstNode* node, int level) {
  if (node == nullptr) {
    return nullptr;
  }

  AstNodeType type;

  if (level == 0) {
    type = NODE_TYPE_OPERATOR_NARY_OR;
  } else {
    type = NODE_TYPE_OPERATOR_NARY_AND;
  }
  // check if first-level node is an OR node
  if (node->type != type) {
    // create new root node
    node = _ast->createNodeNaryOperator(type, node);
  }

  size_t const n = node->numMembers();
  size_t j = 0;

  auto old = node;
  node = _ast->shallowCopyForModify(old);
  TRI_DEFER(FINALIZE_SUBTREE(node));

  for (size_t i = 0; i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);

    if (sub->type == NODE_TYPE_NOP) {
      // ignore this node
      continue;
    }

    if (level == 0) {
      // recurse into next level
      node->changeMember(j, fixRoot(sub, 1));
    } else if (i != j) {
      node->changeMember(j, sub);
    }
    ++j;
  }

  if (j != n) {
    // adjust number of members (because of the NOP nodes removes)
    node->reduceMembers(j);
  }

  return node;
}

AstNode* Condition::root() const { return _root; }

bool Condition::isEmpty() const {
  if (_root == nullptr) {
    return true;
  }

  return (_root->numMembers() == 0);
}

bool Condition::isSorted() const { return _isSorted; }
