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

#include "Aql/IndexBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Functions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/json-utilities.h"
#include "Basics/Exceptions.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/HashIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "V8/v8-globals.h"
#include "VocBase/edge-collection.h"
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
// --SECTION--                                             class IndexRangeBlock
// -----------------------------------------------------------------------------

IndexBlock::IndexBlock (ExecutionEngine* engine,
                        IndexNode const* en)
  : ExecutionBlock(engine, en),
    _collection(en->collection()),
    _posInDocs(0),
    _anyBoundVariable(false),
    _indexes(en->getIndexes()),
    _iterator(nullptr),
    _condition(en->_condition->getConditions()),
    _freeCondition(true),
    _hasV8Expression(false) {

  auto trxCollection = _trx->trxCollection(_collection->cid());

  if (trxCollection != nullptr) {
    _trx->orderDitch(trxCollection);
  }

  TRI_ASSERT(! en->getIndexes().empty());

  std::vector<Index const*> indexes = en->getIndexes();

  _allBoundsConstant.reserve(_condition->numMembers());

  // TODO Check whether all Condition Entries are constant
  for (size_t i = 0; i < _condition->numMembers(); ++i) {
    _allBoundsConstant.push_back(true);
  }
}

IndexBlock::~IndexBlock () {
  delete _iterator;

  if (_freeCondition && _condition != nullptr) {
    delete _condition;
  }
}

bool IndexBlock::hasV8Expression () const {
  // TODO
  for (auto const& expression : _allVariableBoundExpressions) {
    TRI_ASSERT(expression != nullptr);

    if (expression->isV8()) {
      return true;
    }
  }
  return false;
}

void IndexBlock::buildExpressions () {
  /*
  bool const useHighBounds = this->useHighBounds(); 

  size_t posInExpressions = 0;
    
  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  AqlItemBlock* cur = _buffer.front();
        
  // TODO
  auto en = static_cast<IndexNode const*>(getPlanNode());
  std::unique_ptr<IndexOrCondition> newCondition;

  for (size_t i = 0; i < en->_ranges.size(); i++) {
    size_t const n = en->_ranges[i].size(); 
    // prefill with n default-constructed vectors
    std::vector<std::vector<RangeInfo>> collector(n);   

    // collect the evaluated bounds here
    for (size_t k = 0; k < n; k++) {
      auto const& r = en->_ranges[i][k];

      {
        // First create a new RangeInfo containing only the constant 
        // low and high bound of r:
        RangeInfo riConst(r._var, r._attr, r._lowConst, r._highConst, r.is1ValueRangeInfo());
        collector[k].emplace_back(std::move(riConst));
      }

      // Now work the actual values of the variable lows and highs into 
      // this constant range:
      for (auto const& l : r._lows) {
        Expression* e = _allVariableBoundExpressions[posInExpressions];
        TRI_ASSERT(e != nullptr);
        TRI_document_collection_t const* myCollection = nullptr; 
        AqlValue a = e->execute(_trx, cur, _pos, _inVars[posInExpressions], _inRegs[posInExpressions], &myCollection);
        posInExpressions++;

        Json bound;
        if (a._type == AqlValue::JSON) {
          bound = *(a._json);
          a.destroy();  // the TRI_json_t* of a._json has been stolen
        } 
        else if (a._type == AqlValue::SHAPED || a._type == AqlValue::DOCVEC) {
          bound = a.toJson(_trx, myCollection, true);
          a.destroy();  // the TRI_json_t* of a._json has been stolen
        } 
        else {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, 
              "AQL: computed a variable bound and got non-JSON");
        }

        if (! bound.isArray()) {
          if (useHighBounds) {
            auto b(bound.copy());

            RangeInfo ri(r._var, 
                         r._attr, 
                         RangeInfoBound(l.inclusive(), true, b), // will steal b's JSON
                         RangeInfoBound(), 
                         false);

            for (size_t j = 0; j < collector[k].size(); j++) {
              collector[k][j].fuse(ri);
            }
          }
          else {
            auto b1(bound.copy()); // first instance of bound
            auto b2(bound.copy()); // second instance of same bound

            RangeInfo ri(r._var, 
                         r._attr, 
                         RangeInfoBound(l.inclusive(), true, b1), // will steal b1's JSON
                         RangeInfoBound(l.inclusive(), true, b2), // will steal b2's JSON
                         false);

            for (size_t j = 0; j < collector[k].size(); j++) {
              collector[k][j].fuse(ri);
            }
          }
        } 
        else {
          std::vector<RangeInfo> riv; 
          riv.reserve(bound.size());

          for (size_t j = 0; j < bound.size(); j++) {
            auto b1(bound.at(static_cast<int>(j)).copy()); // first instance of bound
            auto b2(bound.at(static_cast<int>(j)).copy()); // second instance of same bound

            riv.emplace_back(RangeInfo(r._var, 
                                       r._attr, 
                                       RangeInfoBound(l.inclusive(), true, b1), // will steal b1's JSON
                                       RangeInfoBound(l.inclusive(), true, b2), // will steal b2's JSON
                                       true));
          }

          collector[k] = std::move(andCombineRangeInfoVecs(collector[k], riv));
        } 
      }

      if (useHighBounds) {
        for (auto const& h : r._highs) {
          Expression* e = _allVariableBoundExpressions[posInExpressions];
          TRI_ASSERT(e != nullptr);
          TRI_document_collection_t const* myCollection = nullptr; 
          AqlValue a = e->execute(_trx, cur, _pos, _inVars[posInExpressions], _inRegs[posInExpressions], &myCollection);
          posInExpressions++;

          Json bound;
          if (a._type == AqlValue::JSON) {
            bound = *(a._json);
            a.destroy();  // the TRI_json_t* of a._json has been stolen
          } 
          else if (a._type == AqlValue::SHAPED || a._type == AqlValue::DOCVEC) {
            bound = a.toJson(_trx, myCollection, true);
            a.destroy();  // the TRI_json_t* of a._json has been stolen
          } 
          else {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, 
                "AQL: computed a variable bound and got non-JSON");
          }
          if (! bound.isArray()) {
            auto b(bound.copy());
            RangeInfo ri(r._var, 
                          r._attr, 
                          RangeInfoBound(), 
                          RangeInfoBound(h.inclusive(), true, b), // will steal b's JSON
                          false);

            for (size_t j = 0; j < collector[k].size(); j++) {
              collector[k][j].fuse(ri);
            }
          } 
          else {
            std::vector<RangeInfo> riv; 
            riv.reserve(bound.size());

            for (size_t j = 0; j < bound.size(); j++) {
              auto b1(bound.at(static_cast<int>(j)).copy()); // first instance of bound
              auto b2(bound.at(static_cast<int>(j)).copy()); // second instance of same bound

              riv.emplace_back(RangeInfo(r._var, 
                                          r._attr, 
                                          RangeInfoBound(h.inclusive(), true, b1), // will steal b1's JSON
                                          RangeInfoBound(h.inclusive(), true, b2), // will steal b2's JSON
                                          true));
            } 

            collector[k] = std::move(andCombineRangeInfoVecs(collector[k], riv));
          } 
        }
      }
    }


    bool isEmpty = false;

    for (auto const& x : collector) {
      if (x.empty()) { 
        isEmpty = true;
        break;
      }
    }

    if (! isEmpty) { 
      // otherwise the condition is impossible to fulfill
      // the elements of the direct product of the collector are and
      // conditions which should be added to newCondition 

      // create cartesian product
      std::unique_ptr<IndexOrCondition> indexAnds(cartesian(collector));

      if (newCondition == nullptr) {
        newCondition.reset(indexAnds.release());
      }
      else {
        for (auto const& indexAnd : *indexAnds) {
          newCondition->emplace_back(std::move(indexAnd));
        }
      } 
    }

  }
    
  freeCondition(); 
    
  if (newCondition != nullptr) {
    _condition = newCondition.release();
    _freeCondition = true;
 
    // remove duplicates . . .
    removeOverlapsIndexOr(*_condition);
  }
  else {
    _condition = new IndexOrCondition;
    _freeCondition = true;
  }
  */
}

int IndexBlock::initialize () {
  ENTER_BLOCK
  int res = ExecutionBlock::initialize();

  if (res == TRI_ERROR_NO_ERROR) {
    if (_trx->orderDitch(_trx->trxCollection(_collection->cid())) == nullptr) {
      res = TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  _allVariableBoundExpressions.clear();

  // instantiate expressions:
  auto instantiateExpression = [&] (RangeInfoBound const& b) -> void {
    AstNode const* a = b.getExpressionAst(_engine->getQuery()->ast());
    Expression* expression = nullptr;
 
    {
      // all new AstNodes are registered with the Ast in the Query
      std::unique_ptr<Expression> e(new Expression(_engine->getQuery()->ast(), a));

      TRI_IF_FAILURE("IndexRangeBlock::initialize") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      _allVariableBoundExpressions.emplace_back(e.get());
      expression = e.release();
    }

    // Prepare _inVars and _inRegs:
    _inVars.emplace_back();
    std::vector<Variable const*>& inVarsCur = _inVars.back();
    _inRegs.emplace_back();
    std::vector<RegisterId>& inRegsCur = _inRegs.back();

    std::unordered_set<Variable const*> inVars;
    expression->variables(inVars);

    for (auto const& v : inVars) {
      inVarsCur.emplace_back(v);
      auto it = getPlanNode()->getRegisterPlan()->varInfo.find(v->id);
      TRI_ASSERT(it != getPlanNode()->getRegisterPlan()->varInfo.end());
      TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
      inRegsCur.emplace_back(it->second.registerId);
    }
  };

  // Get the ranges from the node:
  for (size_t i = 0; i < _condition->numMembers(); ++i) {
    if (! _allBoundsConstant[i]) {
      try {
        auto andCondition = _condition->getMember(i);
        // STILL TODO
        TRI_ASSERT(false);
        /*
        for (auto const& r : orRanges[i]) {
          for (auto const& l : r._lows) {
            instantiateExpression(l);
          }

          if (useHighBounds()) {
            for (auto const& h : r._highs) {
              instantiateExpression(h);
            }
          }
        }
        TRI_IF_FAILURE("IndexRangeBlock::initializeExpressions") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        */
      }
      catch (...) {
        for (auto& e : _allVariableBoundExpressions) {
          delete e;
        }
        _allVariableBoundExpressions.clear();
        throw;
      }
    }
  }
  /* TODO
  auto en = static_cast<IndexNode const*>(getPlanNode());
  std::vector<std::vector<RangeInfo>> const& orRanges = en->_ranges;
  
  for (size_t i = 0; i < orRanges.size(); i++) {
    if (! _allBoundsConstant[i]) {
      try {
        for (auto const& r : orRanges[i]) {
          for (auto const& l : r._lows) {
            instantiateExpression(l);
          }

          if (useHighBounds()) {
            for (auto const& h : r._highs) {
              instantiateExpression(h);
            }
          }
        }
        TRI_IF_FAILURE("IndexRangeBlock::initializeExpressions") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      }
      catch (...) {
        for (auto& e : _allVariableBoundExpressions) {
          delete e;
        }
        _allVariableBoundExpressions.clear();
        throw;
      }
    }
  }
    
  _hasV8Expression = hasV8Expression();
  */

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
  _flag = true; 

  // Find out about the actual values for the bounds in the variable bound case:

  if (_anyBoundVariable) {
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
              for (auto const& e : _allVariableBoundExpressions) {
                e->invalidate();
              }
            }
          
            engine->getQuery()->exitContext(); 
          }
        }
      };

      ISOLATE;
      v8::HandleScope scope(isolate); // do not delete this!
    
      buildExpressions();
    }
    else {
      // no V8 context required!

      Functions::InitializeThreadContext();
      try {
        buildExpressions();
        Functions::DestroyThreadContext();
      }
      catch (...) {
        Functions::DestroyThreadContext();
        throw;
      }
    }
  }
  
  // TODO _condition has to be evaluated at this point!
  _currentIndex = 0;
  _iterator = _indexes[_currentIndex]->getIterator(_condition);
  // TODO Not sure if this loop is necessary after all.
  while (_iterator == nullptr) {
    ++_currentIndex;
    if (_currentIndex < _indexes.size()) {
      _iterator = _indexes[_currentIndex]->getIterator(_condition);
    }
    else {
      // We were not able to initialize any index with this condition
      return false;
    }
  }
  _posInRanges = 0;
  return true;
  LEAVE_BLOCK;
}

void IndexBlock::freeCondition () {
  if (_condition != nullptr && _freeCondition) {
    delete _condition;
    _condition = nullptr;
    _freeCondition = false;
  }
}

void IndexBlock::startNextIterator () {
  ++_currentIndex;
  if (_currentIndex < _indexes.size()) {
    // TODO _condition has to be evaluated at this point!
    _iterator = _indexes[_currentIndex]->getIterator(_condition);
  }
  else {
    // If all indexes have been exhausted we set _iterator to nullptr;
    _iterator = nullptr;
  }
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
    TRI_IF_FAILURE("IndexRangeBlock::readIndex") {
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

  try {
    size_t nrSent = 0;
    while (nrSent < atMost && _iterator != nullptr) {
      TRI_index_element_t* indexElement = _iterator->next();
      if (indexElement == nullptr) {
        startNextIterator();
      }
      else {
        TRI_IF_FAILURE("IndexBlock::readIndex") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _documents.emplace_back(*(indexElement->document()));
        ++nrSent;
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
      _posInDocs = 0;     // position in _documents . . .
    } 
    else if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,

      _posInDocs = 0;
      AqlItemBlock* cur = _buffer.front();

      if (! readIndex(atMost)) { //no more output from this version of the index
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
      _posInDocs = 0;     // position in _documents . . .
    }

    // If we get here, we do have _buffer.front() and _pos points into it
    AqlItemBlock* cur = _buffer.front();

    size_t available = _documents.size() - _posInDocs;
    size_t toSkip = (std::min)(atMost - skipped, available);

    _posInDocs += toSkip;
    skipped += toSkip;
    
    // Advance read position:
    if (_posInDocs >= _documents.size()) {
      // we have exhausted our local documents buffer,

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
      _posInDocs = 0;
      
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
