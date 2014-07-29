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

// -----------------------------------------------------------------------------
// --SECTION--                                factory for instanciation of plans
// -----------------------------------------------------------------------------

ExecutionBlock* ExecutionBlock::instanciatePlan (ExecutionPlan const* ep) {
  ExecutionBlock* eb;
  switch (ep->getType()) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


