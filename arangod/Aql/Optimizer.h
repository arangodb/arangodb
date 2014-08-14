////////////////////////////////////////////////////////////////////////////////
/// @brief infrastructure for query optimizer
///
/// @file arangod/Aql/Optimizer.h
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_OPTIMIZER_H
#define ARANGOD_AQL_OPTIMIZER_H 1

#include <Basics/Common.h>

#include "Aql/ExecutionPlan.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                               the optimizer class
// -----------------------------------------------------------------------------

    class Optimizer {

////////////////////////////////////////////////////////////////////////////////
/// @brief type of an optimizer rule function
////////////////////////////////////////////////////////////////////////////////

      public:

        typedef std::function<int(Optimizer*, ExecutionPlan*, ExecutionNode*)>
                RuleFunction;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of an optimizer rule
////////////////////////////////////////////////////////////////////////////////

        struct Rule {
          RuleFunction func;
          int pass;

          Rule (RuleFunction f, int p)
            : func(f), pass(p) {
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief operator< for two rules, used to stable_sort by pass 
////////////////////////////////////////////////////////////////////////////////

          bool operator< (Rule const& b) const {
            return pass < b.pass;
          }

        };

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, this will initialize the rules database
////////////////////////////////////////////////////////////////////////////////

        Optimizer ();   // the .cpp file includes Aql/OptimizerRules.h
                        // and add all methods there to the rules database

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~Optimizer () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize optimiser
////////////////////////////////////////////////////////////////////////////////

        void initialize (ExecutionPlan* p) {
          _plans.clear();
          _plans.push_back(p);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief do the optimization, this does the optimization, the resulting
/// plans are all estimated, sorted by that estimate and can then be got
/// by getPlans, until the next initialize is called
////////////////////////////////////////////////////////////////////////////////

        int optimize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief getPlans
////////////////////////////////////////////////////////////////////////////////

        vector<ExecutionPlan*>& getPlans () {
          return _plans;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief registerRule
////////////////////////////////////////////////////////////////////////////////

      private:

        void registerRule (RuleFunction f, int pass) {
          _rules.emplace_back(f, pass);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief estimatePlans
////////////////////////////////////////////////////////////////////////////////

        void estimatePlans ();

////////////////////////////////////////////////////////////////////////////////
/// @brief sortPlans
////////////////////////////////////////////////////////////////////////////////

        void sortPlans ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private members
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the rules database
////////////////////////////////////////////////////////////////////////////////

        vector<Rule> _rules;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current set of plans to be optimised
////////////////////////////////////////////////////////////////////////////////

        vector<ExecutionPlan*> _plans;

    };

  }  // namespace aql
}  // namespace triagens
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


