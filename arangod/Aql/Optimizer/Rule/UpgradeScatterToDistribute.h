// TODO(listunov): disclaimer

#pragma once

#include "Aql/ExecutionPlan.h"

namespace arangodb::aql
{
class Optimizer;
void upgradeScatterToDistributeRule(Optimizer* opt,
                                    std::unique_ptr<ExecutionPlan> plan,
                                    OptimizerRule const& rule);
} // namespace arangodb::aql