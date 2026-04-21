////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ConditionPart.h"

#include "Aql/Ast.h"
#include "Aql/Quantifier.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/debugging.h"
#include "Containers/FlatHashSet.h"

namespace arangodb::aql {

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

CompareResult const ResultsTable[3][7][7] = {
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

CompareResult const ResultsTableMultiValued[3][7][7] = {
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
     {IMPOSSIBLE, OTHER_CONTAINED_IN_SELF, DISJOINT, DISJOINT, DISJOINT,
      DISJOINT, DISJOINT},
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

void clearAttributeAccess(
    std::pair<Variable const*, std::vector<basics::AttributeName>>& parts) {
  parts.first = nullptr;
  parts.second.clear();
}

ConditionPart::ConditionPart(Variable const* variable,
                             std::string const& attributeName,
                             AstNode const* operatorNode,
                             AttributeSideType side, void* data)
    : variable(variable),
      attributeName(attributeName),
      operatorType(operatorNode->type),
      isExpanded(false),
      operatorNode(operatorNode),
      valueNode(nullptr),
      data(data) {
  if (side == ATTRIBUTE_LEFT) {
    valueNode = operatorNode->getMember(1);
  } else {
    valueNode = operatorNode->getMember(0);
    if (Ast::isReversibleOperator(operatorType)) {
      operatorType = Ast::reverseOperator(operatorType);
    }
  }

  isExpanded = (attributeName.find("[*]") != std::string::npos);
}

ConditionPart::ConditionPart(
    Variable const* variable,
    std::vector<basics::AttributeName> const& attributeNames,
    AstNode const* operatorNode, AttributeSideType side, void* data)
    : ConditionPart(variable, "", operatorNode, side, data) {
  TRI_AttributeNamesToString(attributeNames, attributeName, false);
  isExpanded = (attributeName.find("[*]") != std::string::npos);
}

ConditionPart::~ConditionPart() = default;

/// @brief true if the condition is completely covered by the other condition
bool ConditionPart::isCoveredBy(ConditionPart const& other,
                                bool isReversed) const {
  if (variable != other.variable || attributeName != other.attributeName) {
    return false;
  }

  if (!isExpanded && !other.isExpanded &&
      other.operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.valueNode->isConstant() && isReversed) {
    if (compareAstNodes(other.valueNode, valueNode, false) == 0) {
      return true;
    }
  }

  TRI_ASSERT(valueNode != nullptr);
  TRI_ASSERT(other.valueNode != nullptr);

  if (!valueNode->isConstant() || !other.valueNode->isConstant()) {
    return false;
  }

  // special cases for IN...
  if (!isExpanded && !other.isExpanded &&
      other.operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.valueNode->isConstant() && other.valueNode->isArray()) {
    if (operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
        valueNode->isConstant() && valueNode->isArray()) {
      // compare IN with an IN
      // this has quadratic complexity
      size_t const n1 = valueNode->numMembers();
      size_t const n2 = other.valueNode->numMembers();

      // maximum number of comparisons that we will accept
      // otherwise the optimization will be aborted
      static constexpr size_t maxComparisons = 2048;

      if (n1 * n2 < maxComparisons) {
        for (size_t i = 0; i < n1; ++i) {
          auto v = valueNode->getMemberUnchecked(i);
          for (size_t j = 0; j < n2; ++j) {
            auto w = other.valueNode->getMemberUnchecked(j);

            CompareResult res =
                ResultsTable[compareAstNodes(v, w, true) + 1][0][0];

            if (res != CompareResult::OTHER_CONTAINED_IN_SELF &&
                res != CompareResult::CONVERT_EQUAL &&
                res != CompareResult::IMPOSSIBLE) {
              return false;
            }
          }
        }
      } else {
        containers::FlatHashSet<AstNode const*, AstNodeValueHash,
                                AstNodeValueEqual>
            values(512, AstNodeValueHash(), AstNodeValueEqual());

        for (size_t i = 0; i < n2; ++i) {
          values.emplace(other.valueNode->getMemberUnchecked(i));
        }

        for (size_t i = 0; i < n1; ++i) {
          auto node = valueNode->getMemberUnchecked(i);
          if (!values.contains(node)) {
            return false;
          }
        }
      }

      return true;
    }

    return false;
  }

  if (isExpanded && other.isExpanded &&
      operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.operatorType == NODE_TYPE_OPERATOR_BINARY_IN &&
      other.valueNode->isConstant()) {
    return compareAstNodes(other.valueNode, valueNode, false) == 0;
  }

  bool a = operatorNode->isArrayComparisonOperator();
  bool b = other.operatorNode->isArrayComparisonOperator();
  if (a || b) {
    if (a != b) {
      return false;
    }
    TRI_ASSERT(operatorNode->numMembers() == 3 &&
               other.operatorNode->numMembers() == 3);

    AstNode* q1 = operatorNode->getMemberUnchecked(2);
    TRI_ASSERT(q1->type == NODE_TYPE_QUANTIFIER);
    AstNode* q2 = other.operatorNode->getMemberUnchecked(2);
    TRI_ASSERT(q2->type == NODE_TYPE_QUANTIFIER);
    // do only cover ALL and NONE when both sides have same quantifier
    if (q1->getIntValue() != q2->getIntValue() || Quantifier::isAny(q1)) {
      return false;
    }

    if (isExpanded && other.isExpanded &&
        operatorType == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN &&
        other.operatorType == NODE_TYPE_OPERATOR_BINARY_ARRAY_IN &&
        other.valueNode->isConstant()) {
      return compareAstNodes(other.valueNode, valueNode, false) == 0;
    }
  }

  // Results are -1, 0, 1, move to 0, 1, 2 for the lookup:
  CompareResult res =
      ResultsTable[compareAstNodes(other.valueNode, valueNode, true) + 1]
                  [other.whichCompareOperation()][whichCompareOperation()];

  if (res == CompareResult::OTHER_CONTAINED_IN_SELF ||
      res == CompareResult::CONVERT_EQUAL || res == CompareResult::IMPOSSIBLE) {
    return true;
  }

  return false;
}

int ConditionPart::whichCompareOperation() const noexcept {
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
  if (operatorType == NODE_TYPE_OPERATOR_BINARY_GT ||
      operatorType == NODE_TYPE_OPERATOR_BINARY_GE ||
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

bool ConditionPart::isLowerInclusive() const noexcept {
  return (operatorType == NODE_TYPE_OPERATOR_BINARY_GE ||
          operatorType == NODE_TYPE_OPERATOR_BINARY_EQ ||
          operatorType == NODE_TYPE_OPERATOR_BINARY_IN);
}

AstNode const* ConditionPart::upperBound() const {
  if (operatorType == NODE_TYPE_OPERATOR_BINARY_LT ||
      operatorType == NODE_TYPE_OPERATOR_BINARY_LE ||
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

bool ConditionPart::isUpperInclusive() const noexcept {
  return (operatorType == NODE_TYPE_OPERATOR_BINARY_LE ||
          operatorType == NODE_TYPE_OPERATOR_BINARY_EQ ||
          operatorType == NODE_TYPE_OPERATOR_BINARY_IN);
}

}  // namespace arangodb::aql
