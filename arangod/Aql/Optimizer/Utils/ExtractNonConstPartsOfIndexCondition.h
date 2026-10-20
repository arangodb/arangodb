#pragma once

#include "Aql/RegisterResolver.h"

namespace arangodb {

class Index;

namespace aql {

class Ast;
struct AstNode;
struct Variable;
struct NonConstExpressionContainer;

namespace optimizer {
NonConstExpressionContainer extractNonConstPartsOfIndexCondition(
    Ast* ast, RegisterResolver const& registers, bool evaluateFCalls,
    Index* index, AstNode const* condition, Variable const* indexVariable);

}
}  // namespace aql
}  // namespace arangodb
