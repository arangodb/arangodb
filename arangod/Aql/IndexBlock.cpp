////////////////////////////////////////////////////////////////////////////////
/// @brief AQL IndexBlock
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
/// @author Jan Steemann
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "IndexBlock.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Functions.h"
#include "Aql/Index.h"
#include "Basics/ScopeGuard.h"
#include "Basics/json-utilities.h"
#include "Basics/Exceptions.h"
#include "Indexes/IndexIterator.h"
#include "V8/v8-globals.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;

// uncomment the following to get some debugging information
#if 0
#define ENTER_BLOCK try { (void) 0;
#define LEAVE_BLOCK } catch (...) { std::cout << "caught an exception in " << __FUNCTION__ << ", " << __FILE__ << ":" << __LINE__ << "!\n"; throw; }
#else
#define ENTER_BLOCK
#define LEAVE_BLOCK
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  class IndexBlock
// -----------------------------------------------------------------------------

IndexBlock::IndexBlock (ExecutionEngine* engine,
                        IndexNode const* en)
  : ExecutionBlock(engine, en),
    _collection(en->collection()),
    _posInDocs(0),
    _currentIndex(0),
    _indexes(en->getIndexes()),
    _context(nullptr),
    _iterator(nullptr),
    _condition(en->_condition->root()),
    _hasV8Expression(false) {

  _context = new IndexIteratorContext(en->_vocbase);

  auto trxCollection = _trx->trxCollection(_collection->cid());

  if (trxCollection != nullptr) {
    _trx->orderDitch(trxCollection);
  }
}

IndexBlock::~IndexBlock () {
  delete _iterator;
  delete _context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a UNIQUE() to a dynamic IN condition
////////////////////////////////////////////////////////////////////////////////

triagens::aql::AstNode* IndexBlock::makeUnique (triagens::aql::AstNode* node) const {
  if (node->type != triagens::aql::NODE_TYPE_ARRAY ||
      (node->type == triagens::aql::NODE_TYPE_ARRAY && node->numMembers() >= 2)) {
    // an non-array or an array with more than 1 member
    auto en = static_cast<IndexNode const*>(getPlanNode());
    auto ast = en->_plan->getAst();
    auto array = ast->createNodeArray();
    array->addMember(node);
    if (_indexes[_currentIndex]->isSorted()) {
      // the index is sorted. we need to use SORTED_UNIQUE to get the
      // result back in index order
      return ast->createNodeFunctionCall("SORTED_UNIQUE", array); 
    }
    // a regular UNIQUE will do
    return ast->createNodeFunctionCall("UNIQUE", array); 
  }

  // presumably an array with no or a single member
  return node;
}

void IndexBlock::executeExpressions () {
  // The following are needed to evaluate expressions with local data from
  // the current incoming item:

  AqlItemBlock* cur = _buffer.front();
  auto en = static_cast<IndexNode const*>(getPlanNode());
  auto ast = en->_plan->getAst();
  for (size_t posInExpressions = 0; posInExpressions < _nonConstExpressions.size(); ++posInExpressions) {
    auto& toReplace = _nonConstExpressions[posInExpressions];
    auto exp = toReplace->expression;
    TRI_document_collection_t const* myCollection = nullptr; 
    AqlValue a = exp->execute(_trx, cur, _pos, _inVars[posInExpressions], _inRegs[posInExpressions], &myCollection);
    auto jsonified = a.toJson(_trx, myCollection, true);
    a.destroy();
    AstNode* evaluatedNode = ast->nodeFromJson(jsonified.json(), true);
    // TODO: if we use the IN operator, we must make the resulting array unique
    _condition->getMember(toReplace->orMember)
              ->getMember(toReplace->andMember)
              ->changeMember(toReplace->operatorMember, evaluatedNode);
  }
}

int IndexBlock::initialize () {
  ENTER_BLOCK
  int res = ExecutionBlock::initialize();

  _nonConstExpressions.clear();
  _alreadyReturned.clear();
  auto en = static_cast<IndexNode const*>(getPlanNode());
  auto ast = en->_plan->getAst();

  // instantiate expressions:
  auto instantiateExpression = [&] (size_t i, size_t j, size_t k, AstNode const* a) -> void {
    // all new AstNodes are registered with the Ast in the Query
    std::unique_ptr<Expression> e(new Expression(ast, a));

    TRI_IF_FAILURE("IndexBlock::initialize") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasV8Expression |= e->isV8();
    
    std::unordered_set<Variable const*> inVars;
    e->variables(inVars);
    
    std::unique_ptr<NonConstExpression> nce(new NonConstExpression(i, j, k, e.get()));
    e.release();
    _nonConstExpressions.push_back(nce.get());
    nce.release();

    // Prepare _inVars and _inRegs:
    _inVars.emplace_back();
    std::vector<Variable const*>& inVarsCur = _inVars.back();
    _inRegs.emplace_back();
    std::vector<RegisterId>& inRegsCur = _inRegs.back();

    for (auto const& v : inVars) {
      inVarsCur.emplace_back(v);
      auto it = getPlanNode()->getRegisterPlan()->varInfo.find(v->id);
      TRI_ASSERT(it != getPlanNode()->getRegisterPlan()->varInfo.end());
      TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
      inRegsCur.emplace_back(it->second.registerId);
    }
  };

  if (_condition == nullptr) {
    // This Node has no condition. Iterate over the complete index. 
    return TRI_ERROR_NO_ERROR;
  }
  
  auto outVariable = static_cast<IndexNode const*>(getPlanNode())->outVariable();

  for (size_t i = 0; i < _condition->numMembers(); ++i) {
    auto andCond = _condition->getMemberUnchecked(i);
    for (size_t j = 0; j < andCond->numMembers(); ++j) {
      auto leaf = andCond->getMemberUnchecked(j);

      // We only support binary conditions
      TRI_ASSERT(leaf->numMembers() == 2);
      auto lhs = leaf->getMember(0);
      auto rhs = leaf->getMember(1);

      if (lhs->isAttributeAccessForVariable(outVariable)) {
        // Index is responsible for the left side, check if right side has to be evaluated
        if (! rhs->isConstant()) {
          if (leaf->type == NODE_TYPE_OPERATOR_BINARY_IN) {
            rhs = makeUnique(rhs);
          }
          instantiateExpression(i, j, 1, rhs);
          TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      } 
      else {
        // Index is responsible for the right side, check if left side has to be evaluated
        if (! lhs->isConstant()) {
          if (leaf->type == NODE_TYPE_OPERATOR_BINARY_IN) {
            // IN: now make IN result unique
            lhs = makeUnique(lhs);
          }
          instantiateExpression(i, j, 0, lhs);
          TRI_IF_FAILURE("IndexBlock::initializeExpressions") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      }
    }
  }

  return res;
  LEAVE_BLOCK;
}

// init the ranges for reading, this should be called once per new incoming
// block!
//
// This is either called every time we get a new incoming block. 
// If all the bounds are constant, then in the case of hash, primary or edges
// indexes it does nothing. In the case of a skiplist index, it creates a
// skiplistIterator which is used by readIndex. If at least one bound is
// variable, then this this also evaluates the IndexOrCondition required to
// determine the values of the bounds. 
//
// It is guaranteed that
//   _buffer   is not empty, in particular _buffer.front() is defined
//   _pos      points to a position in _buffer.front()
// Therefore, we can use the register values in _buffer.front() in row
// _pos to evaluate the variable bounds.

bool IndexBlock::initIndexes () {
  ENTER_BLOCK

  // We start with a different context. Return documents found in the previous context again.
  _alreadyReturned.clear();
  // Find out about the actual values for the bounds in the variable bound case:

  if (! _nonConstExpressions.empty()) {
    if (_hasV8Expression) {
      bool const isRunningInCluster = triagens::arango::ServerState::instance()->isRunningInCluster();

      // must have a V8 context here to protect Expression::execute()
      auto engine = _engine;
      triagens::basics::ScopeGuard guard{
        [&engine]() -> void { 
          engine->getQuery()->enterContext(); 
        },
        [&]() -> void {
          if (isRunningInCluster) {
            // must invalidate the expression now as we might be called from
            // different threads
            if (triagens::arango::ServerState::instance()->isRunningInCluster()) {
              for (auto const& e : _nonConstExpressions) {
                e->expression->invalidate();
              }
            }
          
            engine->getQuery()->exitContext(); 
          }
        }
      };

      ISOLATE;
      v8::HandleScope scope(isolate); // do not delete this!
    
      executeExpressions();
    }
    else {
      // no V8 context required!

      Functions::InitializeThreadContext();
      try {
        executeExpressions();
        Functions::DestroyThreadContext();
      }
      catch (...) {
        Functions::DestroyThreadContext();
        throw;
      }
    }
  }

  _currentIndex = 0;
  IndexNode const* node = static_cast<IndexNode const*>(getPlanNode());
  auto outVariable = node->outVariable();
  auto ast = node->_plan->getAst();
  if (_condition == nullptr) {
    _iterator = _indexes[_currentIndex]->getIterator(_context, ast, nullptr, outVariable, node->_reverse);
  }
  else {
    _iterator = _indexes[_currentIndex]->getIterator(_context, ast, _condition->getMember(_currentIndex), outVariable, node->_reverse);
  }

  while (_iterator == nullptr) {
    ++_currentIndex;
    if (_currentIndex < _indexes.size()) {
      if (_condition == nullptr) {
        _iterator = _indexes[_currentIndex]->getIterator(_context, ast, nullptr, outVariable, node->_reverse);
      }
      else {
        _iterator = _indexes[_currentIndex]->getIterator(_context, ast, _condition->getMember(_currentIndex), outVariable, node->_reverse);
      }
    }
    else {
      // We were not able to initialize any index with this condition
      return false;
    }
  }
  return true;
  LEAVE_BLOCK;
}

void IndexBlock::startNextIterator () {
  delete _iterator;
  _iterator = nullptr;

  ++_currentIndex;
  if (_currentIndex < _indexes.size()) {
    IndexNode const* node = static_cast<IndexNode const*>(getPlanNode());
    auto outVariable = node->outVariable();
    auto ast = node->_plan->getAst();

    _iterator = _indexes[_currentIndex]->getIterator(_context, ast, _condition->getMember(_currentIndex), outVariable, node->_reverse);
  }
  /*
  else {
    // If all indexes have been exhausted we set _iterator to nullptr;
    delete _iterator;
    _iterator = nullptr;
  }
  */
}

// this is called every time everything in _documents has been passed on

bool IndexBlock::readIndex (size_t atMost) {
  ENTER_BLOCK;
  // this is called every time we want more in _documents. 
  // For the primary key index, this only reads the index once, and never 
  // again (although there might be multiple calls to this function). 
  // For the edge, hash or skiplists indexes, initIndexes creates an iterator
  // and read*Index just reads from the iterator until it is done. 
  // Then initIndexes is read again and so on. This is to avoid reading the
  // entire index when we only want a small number of documents. 
  
  if (_documents.empty()) {
    TRI_IF_FAILURE("IndexBlock::readIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    _documents.reserve(atMost);
  }
  else { 
    _documents.clear();
  }

  if (_iterator == nullptr) {
    // All indexes exhausted
    return false;
  }

  size_t lastIndexNr = _indexes.size();
  try {
    size_t nrSent = 0;
    while (nrSent < atMost && _iterator != nullptr) {
      TRI_doc_mptr_t* indexElement = _iterator->next();
      if (indexElement == nullptr) {
        startNextIterator();
      }
      else {
        TRI_IF_FAILURE("IndexBlock::readIndex") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        if (_currentIndex == 0 ||
          _alreadyReturned.find(indexElement) == _alreadyReturned.end()) {
          if (_currentIndex < lastIndexNr) {
            _alreadyReturned.emplace(indexElement);
          }

          _documents.emplace_back(*indexElement);
          ++nrSent;
        }
        ++_engine->_stats.scannedIndex;
      }
    }
  }
  catch (...) {
    // TODO check if this is enough
    if (_iterator != nullptr) {
      delete _iterator;
      _iterator = nullptr;
    }
  }
  _posInDocs = 0;
  return (! _documents.empty());
  LEAVE_BLOCK;
}

int IndexBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
  ENTER_BLOCK;
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  _pos = 0;
  _posInDocs = 0;
  
  return TRI_ERROR_NO_ERROR; 
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* IndexBlock::getSome (size_t atLeast,
                                   size_t atMost) {
  ENTER_BLOCK;
  if (_done) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> res(nullptr);

  do {
    // repeatedly try to get more stuff from upstream
    // note that the value of the variable we have to loop over
    // can contain zero entries, in which case we have to
    // try again!

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! ExecutionBlock::getBlock(toFetch, toFetch) 
          || (! initIndexes())) {
        _done = true;
        return nullptr;
      }
      _pos = 0;           // this is in the first block

      // This is a new item, so let's read the index (it is already
      // initialized).
      readIndex(atMost);
    } 
    else if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,


      if (! readIndex(atMost)) { //no more output from this version of the index
        AqlItemBlock* cur = _buffer.front();
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          delete cur;
          _pos = 0;
        }
        if (_buffer.empty()) {
          if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize) ) {
            _done = true;
            return nullptr;
          }
          _pos = 0;           // this is in the first block
        }
        
        if (! initIndexes()) {
          _done = true;
          return nullptr;
        }
        readIndex(atMost);
      }
    }

    // If we get here, we do have _buffer.front() and _pos points into it
    AqlItemBlock* cur = _buffer.front();
    size_t const curRegs = cur->getNrRegs();

    size_t available = _documents.size() - _posInDocs;
    size_t toSend = (std::min)(atMost, available);

    if (toSend > 0) {

      res.reset(new AqlItemBlock(toSend,
            getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

      // automatically freed should we throw
      TRI_ASSERT(curRegs <= res->getNrRegs());

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(cur, res.get(), _pos);

      // set our collection for our output register
      res->setDocumentCollection(static_cast<triagens::aql::RegisterId>(curRegs),
          _trx->documentCollection(_collection->cid()));

      for (size_t j = 0; j < toSend; j++) {
        if (j > 0) {
          // re-use already copied aqlvalues
          for (RegisterId i = 0; i < curRegs; i++) {
            res->setValue(j, i, res->getValueReference(0, i));
            // Note: if this throws, then all values will be deleted
            // properly since the first one is.
          }
        }

        // The result is in the first variable of this depth,
        // we do not need to do a lookup in getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        res->setValue(j, static_cast<triagens::aql::RegisterId>(curRegs),
                      AqlValue(reinterpret_cast<TRI_df_marker_t
                               const*>(_documents[_posInDocs++].getDataPtr())));
        // No harm done, if the setValue throws!
      }
    }

  }
  while (res.get() == nullptr);

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

size_t IndexBlock::skipSome (size_t atLeast,
                             size_t atMost) {
  
  if (_done) {
    return 0;
  }

  size_t skipped = 0;

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! ExecutionBlock::getBlock(toFetch, toFetch) 
          || (! initIndexes())) {
        _done = true;
        return skipped;
      }
      _pos = 0;           // this is in the first block
      
      // This is a new item, so let's read the index if bounds are variable:
      readIndex(atMost); 
    }

    size_t available = _documents.size() - _posInDocs;
    size_t toSkip = (std::min)(atMost - skipped, available);

    _posInDocs += toSkip;
    skipped += toSkip;
    
    // Advance read position:
    if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,
      if (!readIndex(atMost)) {
        // If we get here, we do have _buffer.front() and _pos points into it
        AqlItemBlock* cur = _buffer.front();

        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          delete cur;
          _pos = 0;
        }

        // let's read the index if bounds are variable:
        if (! _buffer.empty()) {
          if (! initIndexes()) {
            _done = true;
            return skipped;
          }
          readIndex(atMost);
        }
      }
      
      // If _buffer is empty, then we will fetch a new block in the next round
      // and then read the index.
    }
  }

  return skipped; 
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
