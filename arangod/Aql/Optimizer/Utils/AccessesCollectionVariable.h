#pragma once

#include "Aql/types.h"

#include "Aql/ExecutionNode/ExecutionNode.h"
namespace arangodb::aql {
class ExecutionNode;
class ExecutionPlan;

bool accessesCollectionVariable(arangodb::aql::ExecutionPlan const* plan,
                                arangodb::aql::ExecutionNode const* node,
                                arangodb::aql::VarSet& vars);
}  // namespace arangodb::aql
