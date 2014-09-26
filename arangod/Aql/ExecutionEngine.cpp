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
#include "Aql/ExecutionPlan.h"
#include "Aql/WalkerWorker.h"
#include "Utils/Exception.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the engine
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine::ExecutionEngine (AQL_TRANSACTION_V8* trx, Query* query)
  : _stats(),
    _blocks(),
    _root(nullptr),
    _trx(trx),
    _query(query) {

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

struct Instanciator : public WalkerWorker<ExecutionNode> {
  ExecutionEngine* engine;
  ExecutionBlock* root;
  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;

  Instanciator (ExecutionEngine* engine) 
    : engine(engine),
      root(nullptr) {
  }
  
  ~Instanciator () {
  }

  virtual void after (ExecutionNode* en) {
    ExecutionBlock* eb = nullptr;

    switch (en->getType()) {
      case ExecutionNode::SINGLETON: {
        eb = new SingletonBlock(engine,
                                static_cast<SingletonNode const*>(en));
        break;
      }
      case ExecutionNode::INDEX_RANGE: {
        eb = new IndexRangeBlock(engine,
                                 static_cast<IndexRangeNode const*>(en));
        break;
      }
      case ExecutionNode::ENUMERATE_COLLECTION: {
        eb = new EnumerateCollectionBlock(engine,
                                          static_cast<EnumerateCollectionNode const*>(en));
        break;
      }
      case ExecutionNode::ENUMERATE_LIST: {
        eb = new EnumerateListBlock(engine,
                                    static_cast<EnumerateListNode const*>(en));
        break;
      }
      case ExecutionNode::CALCULATION: {
        eb = new CalculationBlock(engine,
                                  static_cast<CalculationNode const*>(en));
        break;
      }
      case ExecutionNode::FILTER: {
        eb = new FilterBlock(engine,
                             static_cast<FilterNode const*>(en));
        break;
      }
      case ExecutionNode::LIMIT: {
        eb = new LimitBlock(engine,
                            static_cast<LimitNode const*>(en));
        break;
      }
      case ExecutionNode::SORT: {
        eb = new SortBlock(engine,
                           static_cast<SortNode const*>(en));
        break;
      }
      case ExecutionNode::AGGREGATE: {
        eb = new AggregateBlock(engine,
                                static_cast<AggregateNode const*>(en));
        break;
      }
      case ExecutionNode::SUBQUERY: {
        auto es = static_cast<SubqueryNode*>(en);
        auto it = cache.find(es->getSubquery());

        TRI_ASSERT(it != cache.end());

        eb = new SubqueryBlock(engine,
                               static_cast<SubqueryNode const*>(en),
                               it->second);
        break;
      }
      case ExecutionNode::RETURN: {
        eb = new ReturnBlock(engine,
                             static_cast<ReturnNode const*>(en));

        root = eb;
        break;
      }
      case ExecutionNode::REMOVE: {
        eb = new RemoveBlock(engine,
                             static_cast<RemoveNode const*>(en));

        root = eb;
        break;
      }
      case ExecutionNode::INSERT: {
        eb = new InsertBlock(engine,
                             static_cast<InsertNode const*>(en));

        root = eb;
        break;
      }
      case ExecutionNode::UPDATE: {
        eb = new UpdateBlock(engine,
                             static_cast<UpdateNode const*>(en));

        root = eb;
        break;
      }
      case ExecutionNode::REPLACE: {
        eb = new ReplaceBlock(engine,
                              static_cast<ReplaceNode const*>(en));

        root = eb;
        break;
      }
      case ExecutionNode::NORESULTS: {
        eb = new NoResultsBlock(engine,
                                static_cast<NoResultsNode const*>(en));
        break;
      }

      default: {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "node type not handled");
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
/// @brief add a block to the engine
////////////////////////////////////////////////////////////////////////////////

void ExecutionEngine::addBlock (ExecutionBlock* block) {
  TRI_ASSERT(block != nullptr);

  _blocks.push_back(block);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution engine from a plan
////////////////////////////////////////////////////////////////////////////////

ExecutionEngine* ExecutionEngine::instanciateFromPlan (AQL_TRANSACTION_V8* trx,
                                                       Query* query,
                                                       ExecutionPlan* plan) {
  auto engine = new ExecutionEngine(trx, query);

  try {
    if (! plan->varUsageComputed()) {
      plan->findVarUsage();
    }
    std::unique_ptr<Instanciator> inst(new Instanciator(engine));
    plan->root()->walk(inst.get());
    auto root = inst.get()->root;

    root->staticAnalysis();
    root->initialize();
    root->initializeCursor(nullptr, 0);

    engine->_root = root;
  
    return engine;
  }
  catch (...) {
    delete engine;
    throw;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
