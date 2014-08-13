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
#include "Utils/Exception.h"

using namespace triagens::arango;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                              class ExecutionBlock
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size value
////////////////////////////////////////////////////////////////////////////////
  
size_t const ExecutionBlock::DefaultBatchSize = 1000;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////
         
ExecutionBlock::~ExecutionBlock () {        
  for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
    delete *it;
  }
  _buffer.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

int ExecutionBlock::bind (AqlItemBlock* items, size_t pos) {
  int res;
  for (auto d : _dependencies) {
    res = d->bind(items, pos);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to walk an execution block recursively
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::walk (WalkerWorker* worker) {
  // Only do every node exactly once:
  if (worker->done(this)) {
    return;
  }

  worker->before(this);
  // Now the children in their natural order:
  for (auto c : _dependencies) {
    c->walk(worker);
  }
  // Now handle a subquery:
  if (_exeNode->getType() == ExecutionNode::SUBQUERY) {
    auto p = static_cast<SubqueryBlock*>(this);
    if (worker->enterSubquery(this, p->getSubquery())) {
      p->getSubquery()->walk(worker);
      worker->leaveSubquery(this, p->getSubquery());
    }
  }
  worker->after(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief static analysis
////////////////////////////////////////////////////////////////////////////////

void ExecutionBlock::staticAnalysis (ExecutionBlock* super) {
  // The super is only for the case of subqueries.
  shared_ptr<VarOverview> v;
  if (super == nullptr) {
    v.reset(new VarOverview());
  }
  else {
    v.reset(new VarOverview(*(super->_varOverview), super->_depth));
  }
  v->setSharedPtr(&v);
  walk(v.get());
  // Now handle the subqueries:
  for (auto s : v->subQueryBlocks) {
    auto sq = static_cast<SubqueryBlock*>(s);
    sq->getSubquery()->staticAnalysis(s);
  }
  v->reset();
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

