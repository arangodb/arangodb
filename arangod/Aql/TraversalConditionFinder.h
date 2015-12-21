////////////////////////////////////////////////////////////////////////////////
/// @brief Condition finder, used to build up the Condition object
///
/// @file 
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CONDITION_FINDER_H
#define ARANGOD_AQL_CONDITION_FINDER 1

#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief Traversal condition finder
////////////////////////////////////////////////////////////////////////////////

    class TraversalConditionFinder : public WalkerWorker<ExecutionNode> {

      public:

        TraversalConditionFinder (ExecutionPlan* plan,
                                  bool* planAltered)
          : _plan(plan),
            _variableDefinitions(),
            _planAltered(planAltered) {
        }

        ~TraversalConditionFinder () {
        }

        bool before (ExecutionNode*) override final;

        bool enterSubquery (ExecutionNode*, ExecutionNode*) override final;

      private:

        ExecutionPlan*                                          _plan;
        std::unordered_map<VariableId, CalculationNode const*>  _variableDefinitions;
        std::unordered_map<VariableId, ExecutionNode const*>    _filters;
        bool*                                                   _planAltered;
    
    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

