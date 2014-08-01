////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, execution engine
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Utils/Exception.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the engine
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine::ExecutionEngine () 
  : _blocks(),
    _root(nullptr) {

  _blocks.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the engine, frees all assigned blocks
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine::~ExecutionEngine () {
  if (_root != nullptr) {
    _root->shutdown();
  }

  for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
    delete (*it);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution engine from a plan
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine* ExecutionEngine::instanciateFromPlan (ExecutionNode const* plan) {
  auto engine = new ExecutionEngine();

  try {
    auto root = ExecutionBlock::instanciatePlan(plan); //engine, plan);
    root->staticAnalysis();
    root->initialize();
    
    engine->_root = root;
  }
  catch (...) {
    delete engine;
    throw;
  }

  return engine;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a block to the engine
////////////////////////////////////////////////////////////////////////////////

void ExecutionEngine::addBlock (ExecutionBlock* block) {
  TRI_ASSERT(block != nullptr);

  _blocks.push_back(block);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
