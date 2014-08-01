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
// --SECTION--                     walker class for ExecutionNode to instanciate
// -----------------------------------------------------------------------------

struct Instanciator : public ExecutionNode::WalkerWorker {
  ExecutionBlock* root;
  ExecutionEngine* engine;
  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;

  Instanciator (ExecutionEngine* engine) : engine(engine) {};
  
  ~Instanciator () {};

  virtual void after (ExecutionNode* en) {
    ExecutionBlock* eb;
    switch (en->getType()) {
      case ExecutionNode::SINGLETON: {
        eb = new SingletonBlock(static_cast<SingletonNode const*>(en));
        break;
      }
      case ExecutionNode::ENUMERATE_COLLECTION: {
        eb = new EnumerateCollectionBlock(static_cast<EnumerateCollectionNode const*>(en));
        break;
      }
      case ExecutionNode::ROOT: {
        eb = new RootBlock(static_cast<RootNode const*>(en));
        root = eb;
        break;
      }
      default: {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      }
    }
    try {
      engine->addBlock(eb);
    }
    catch (...) {
      delete eb;
      throw;
    }

    // Now add dependencies:
    std::vector<ExecutionNode*> deps = en->getDependencies();
    for (auto it = deps.begin(); it != deps.end();++it) {
      auto it2 = cache.find(*it);
      TRI_ASSERT(it2 != cache.end());
      eb->addDependency(it2->second);
    }

    cache.insert(std::make_pair(en, eb));
  }

};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution engine from a plan
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine* ExecutionEngine::instanciateFromPlan (ExecutionNode* plan) {
  auto engine = new ExecutionEngine();

  try {
    auto inst = new Instanciator(engine);
    plan->walk(inst);
    auto root = inst->root;
    delete inst;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

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
