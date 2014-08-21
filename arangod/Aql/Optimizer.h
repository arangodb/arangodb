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

      public:
////////////////////////////////////////////////////////////////////////////////
/// @brief the following struct keeps a list (deque) of ExecutionPlan*
/// and has some automatic convenience functions.
////////////////////////////////////////////////////////////////////////////////

        struct PlanList {
          std::deque<ExecutionPlan*> list;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

          PlanList () {};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a plan
////////////////////////////////////////////////////////////////////////////////

          PlanList (ExecutionPlan* p) {
            list.push_back(p);
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor, deleting contents
////////////////////////////////////////////////////////////////////////////////

          ~PlanList () {
            for (auto p : list) {
              delete p;
            }
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief get number of plans contained
////////////////////////////////////////////////////////////////////////////////

          size_t size () {
            return list.size();
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief pop the first one
////////////////////////////////////////////////////////////////////////////////

          ExecutionPlan* pop_front () {
            auto p = list.front();
            list.pop_front();
            return p;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief push_back
////////////////////////////////////////////////////////////////////////////////

          void push_back (ExecutionPlan* p) {
            list.push_back(p);
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief steals all the plans in b and clears b at the same time
////////////////////////////////////////////////////////////////////////////////

          void steal (PlanList& b) {
            list.swap(b.list);
            for (auto p : b.list) {
              delete p;
            }
            b.list.clear();
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief appends all the plans to the target and clears *this at the same time
////////////////////////////////////////////////////////////////////////////////

          void appendTo (std::vector<ExecutionPlan*>& target) {
            while (list.size() > 0) {
              auto p = list.front();
              list.pop_front();
              try {
                target.push_back(p);
              }
              catch (...) {
                delete p;
                throw;
              }
            }
          }

        };

////////////////////////////////////////////////////////////////////////////////
/// @brief type of an optimizer rule function, the function gets an optimiser,
/// an ExecutionPlan and has to append one or more plans to the resulting
/// deque. This must not include the original plan. The rule has to set keep
/// to indicate whether or not the original plan is kept in the resulting
/// list. Note that the optimization is done in multiple passes 
////////////////////////////////////////////////////////////////////////////////

        typedef std::function<int(Optimizer* opt, 
                                  ExecutionPlan* plan, 
                                  PlanList& out,
                                  bool& keep)>
                RuleFunction;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of an optimizer rule
////////////////////////////////////////////////////////////////////////////////

        struct Rule {
          RuleFunction func;
          int rank;

          Rule (RuleFunction f, int r)
            : func(f), rank(r) {
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief operator< for two rules, used to stable_sort by pass 
////////////////////////////////////////////////////////////////////////////////

          bool operator< (Rule const& b) const {
            return rank > b.rank;
          }

        };

////////////////////////////////////////////////////////////////////////////////
/// @brief number of passes in optimization
////////////////////////////////////////////////////////////////////////////////

        static int const numberOfPasses = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal number of plans to produce:
////////////////////////////////////////////////////////////////////////////////

        static int const maxNumberOfPlans = 1000;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, this will initialize the rules database
////////////////////////////////////////////////////////////////////////////////

        Optimizer ();   // the .cpp file includes Aql/OptimizerRules.h
                        // and add all methods there to the rules database

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~Optimizer () {
          for (auto p : _plans) {
            delete p;
          }
          _plans.clear();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief do the optimization, this does the optimization, the resulting
/// plans are all estimated, sorted by that estimate and can then be got
/// by getPlans, until the next initialize is called. Note that the optimizer
/// object takes ownership of the execution plan and will delete it 
/// automatically on destruction. It will also have ownership of all the
/// newly created plans it recalls and will automatically delete them.
/// If you need to extract the plans from the optimizer use stealBest or
/// stealPlans.
////////////////////////////////////////////////////////////////////////////////

        int createPlans (ExecutionPlan* p);

////////////////////////////////////////////////////////////////////////////////
/// @brief getBest, ownership of the plan remains with the optimizer
////////////////////////////////////////////////////////////////////////////////

        ExecutionPlan* getBest () {
          if (_plans.size() == 0) {
            return nullptr;
          }
          return _plans[0];
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getPlans, ownership of the plans remains with the optimizer
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionPlan*>& getPlans () {
          return _plans;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief stealBest, ownership of the plan is handed over to the caller,
/// all other plans are deleted
////////////////////////////////////////////////////////////////////////////////

        ExecutionPlan* stealBest () {
          if (_plans.size() == 0) {
            return nullptr;
          }
          auto res = _plans[0];
          for (size_t i = 1; i < _plans.size(); i++) {
            delete _plans[i];
          }
          _plans.clear();
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief stealPlans, ownership of the plans is handed over to the caller,
/// the optimizer will forget about them!
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionPlan*> stealPlans () {
          std::vector<ExecutionPlan*> res;
          res.swap(_plans);
          return res;
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

        std::vector<Rule> _rules;

////////////////////////////////////////////////////////////////////////////////
/// @brief the current set of plans to be optimised
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionPlan*> _plans;

    };

  }  // namespace aql
}  // namespace triagens
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


