////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionBlocks, the execution engine
///
/// @file arangod/Aql/ExecutionBlock.cpp
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

#include "Aql/ExecutionBlock.h"

using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::aql;
         
ExecutionBlock::~ExecutionBlock () {        
  for (auto i = _dependencies.begin(); i != _dependencies.end(); ++i) {
    delete *i;
  }
}

int ExecutionBlock::bind (std::map<std::string, struct TRI_json_s*>* params) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to walk an execution plan recursively
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::walk (WalkerWorker& worker) {
  worker.before(this);
  for (auto it = _dependencies.begin();
            it != _dependencies.end();
            ++it) {
    (*it)->walk(worker);
  }
  if (_exePlan->getType() == ExecutionPlan::SUBQUERY) {
    // auto p = static_cast<SubqueryBlock*>(this);
    worker.enterSubquery(this, nullptr); // p->_subquery
    // p->_subquery->walk(worker);
    worker.leaveSubquery(this, nullptr); // p->_subquery
  }
  worker.after(this);
}


// -----------------------------------------------------------------------------
// --SECTION--                                factory for instanciation of plans
// -----------------------------------------------------------------------------

ExecutionBlock* ExecutionBlock::instanciatePlan (ExecutionPlan const* ep) {
  ExecutionBlock* eb;
  switch (ep->getType()) {
    case ExecutionPlan::SINGLETON: {
      eb = new SingletonBlock(static_cast<SingletonPlan const*>(ep));
      break;
    }
    case ExecutionPlan::ENUMERATE_COLLECTION: {
      eb = new EnumerateCollectionBlock(static_cast<EnumerateCollectionPlan const*>(ep));
      break;
    }
    case ExecutionPlan::ROOT: {
      eb = new RootBlock(static_cast<RootPlan const*>(ep));
      break;
    }
    default: {
      TRI_ASSERT(false);
      break;
    }
  }
  vector<ExecutionPlan*> deps = ep->getDependencies();
  for (auto it = deps.begin(); it != deps.end();++it) {
    eb->addDependency(instanciatePlan(*it));
  }
  return eb;
}

// -----------------------------------------------------------------------------
// --SECTION--                               static analysis for ExecutionBlocks
// -----------------------------------------------------------------------------

int ExecutionBlock::staticAnalysisRecursion (
                    std::unordered_map<VariableId, VarDefPlace>& varTab,
                    int& curVar) {
  if (_dependencies.size() == 0) {
    // No dependencies, we are depth 0:
    _depth = 0;
    curVar = 0;
  }
  else {
    auto it = _dependencies.begin();
    _depth = (*it++)->staticAnalysisRecursion(varTab, curVar);
    while (it != _dependencies.end()) {
      (*it++)->staticAnalysisRecursion(varTab, curVar);
    }
  }
  switch (_exePlan->getType()) {
    case ExecutionPlan::ENUMERATE_COLLECTION: {
      _depth++;
      curVar = 0;
      auto p = static_cast<EnumerateCollectionPlan const*>(_exePlan);
      varTab.insert(make_pair(p->_outVarNumber, VarDefPlace(_depth, 0)));
      break;
    }
    case ExecutionPlan::ENUMERATE_LIST: {
      _depth++;
      curVar = 0;
      auto p = static_cast<EnumerateListPlan const*>(_exePlan);
      varTab.insert(make_pair(p->_outVarNumber, VarDefPlace(_depth, 0)));
      break;
    }
    case ExecutionPlan::CALCULATION: {
      curVar++;
      auto p = static_cast<CalculationPlan const*>(_exePlan);
      varTab.insert(make_pair(p->_varNumber, VarDefPlace(_depth, curVar)));
      break;
    }
    case ExecutionPlan::PROJECTION: {
      curVar++;
      auto p = static_cast<ProjectionPlan const*>(_exePlan);
      varTab.insert(make_pair(p->_outVar, VarDefPlace(_depth, curVar)));
      break;
    }
    // TODO: potentially more cases
    default: {
      break;
    }
  }
  return _depth;
}
                            

void ExecutionBlock::staticAnalysis () {
  std::unordered_map<VariableId, VarDefPlace> varTab;
  int curVar = 0;
  staticAnalysisRecursion(varTab, curVar);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


