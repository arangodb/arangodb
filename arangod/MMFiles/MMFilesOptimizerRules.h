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

#ifndef ARANGOD_MMFILES_MMFILES_OPTIMIZER_RULES_H
#define ARANGOD_MMFILES_MMFILES_OPTIMIZER_RULES_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace aql {
class ExecutionPlan;
class Optimizer;
struct OptimizerRule;
}  // namespace aql

struct MMFilesOptimizerRules {
  static void registerResources();

  static void removeSortRandRule(aql::Optimizer* opt,
                                 std::unique_ptr<aql::ExecutionPlan> plan,
                                 aql::OptimizerRule const* rule);
};

}  // namespace arangodb

#endif
