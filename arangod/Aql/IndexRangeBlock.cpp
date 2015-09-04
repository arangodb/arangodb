////////////////////////////////////////////////////////////////////////////////
/// @brief AQL IndexRangeBlock
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/IndexRangeBlock.h"
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

IndexRangeBlock::IndexRangeBlock (ExecutionEngine* engine,
                                  IndexRangeNode const* en)
  : ExecutionBlock(engine, en),
    _collection(en->collection()),
    _posInDocs(0),
    _anyBoundVariable(false),
    _skiplistIterator(nullptr),
    _edgeIndexIterator(nullptr),
    _hashIndexSearchValue({ 0, nullptr }),
    _hashNextElement(nullptr),
    _condition(new IndexOrCondition()),
    _posInRanges(0),
    _sortCoords(),
    _freeCondition(true),
    _hasV8Expression(false) {

  auto trxCollection = _trx->trxCollection(_collection->cid());

  if (trxCollection != nullptr) {
    _trx->orderDitch(trxCollection);
  }
    
  std::vector<std::vector<RangeInfo>> const& orRanges = en->_ranges;
  size_t const n = orRanges.size(); 

  for (size_t i = 0; i < n; i++) {
    _condition->emplace_back(IndexAndCondition());

    for (auto const& ri : en->_ranges[i]) {
      _condition->at(i).emplace_back(ri.clone());
    }
  }

  if (_condition->size() > 1) {
    removeOverlapsIndexOr(*_condition);
  }

  TRI_ASSERT(en->_index != nullptr);

  _allBoundsConstant.clear();
  _allBoundsConstant.reserve(orRanges.size());

  // Detect, whether all ranges are constant:
  for (size_t i = 0; i < orRanges.size(); i++) {
    bool isConstant = true;

    std::vector<RangeInfo> const& attrRanges = orRanges[i];
    for (auto const& r : attrRanges) {
      isConstant &= r.isConstant();
    }
    _anyBoundVariable |= ! isConstant;
    _allBoundsConstant.push_back(isConstant); // note: emplace_back() is not supported in C++11 but only from C++14
  }
}

IndexRangeBlock::~IndexRangeBlock () {
  destroyHashIndexSearchValues();

  for (auto& e : _allVariableBoundExpressions) {
    delete e;
  }

  if (_freeCondition && _condition != nullptr) {
    delete _condition;
  }
    
  delete _skiplistIterator;
  delete _edgeIndexIterator; 
}

bool IndexRangeBlock::useHighBounds () const {
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  return (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX);
}

bool IndexRangeBlock::hasV8Expression () const {
  for (auto const& expression : _allVariableBoundExpressions) {
    TRI_ASSERT(expression != nullptr);

    if (expression->isV8()) {
      return true;
    }
  }
  return false;
}

void IndexRangeBlock::buildExpressions () {
  bool const useHighBounds = this->useHighBounds(); 

  size_t posInExpressions = 0;
    
  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  AqlItemBlock* cur = _buffer.front();
        
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
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
}

int IndexRangeBlock::initialize () {
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
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
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

bool IndexRangeBlock::initRanges () {
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
  
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  TRI_ASSERT(en->_index != nullptr);
   
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return true; //no initialization here!
  }
  
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    if (_condition == nullptr || _condition->empty()) {
      return false;
    }

    _posInRanges = 0;
    getEdgeIndexIterator(_condition->at(_posInRanges));
    return (_edgeIndexIterator != nullptr);
  }
      
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX) {
    if (_condition == nullptr || _condition->empty()) {
      return false;
    }

    _posInRanges = 0;
    getHashIndexIterator(_condition->at(_posInRanges));
    return (_hashIndexSearchValue._values != nullptr); 
  }
  
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    if (_condition == nullptr || _condition->empty()) {
      return false;
    }

    sortConditions();
    _posInRanges = 0;

    getSkiplistIterator(_condition->at(_sortCoords[_posInRanges]));
    return (_skiplistIterator != nullptr);
  }
          
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected index type"); 
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
// @brief: sorts the index range conditions and resets _posInRanges to 0
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::sortConditions () {
  size_t const n = _condition->size(); 

  if (! _sortCoords.empty()) {
    _sortCoords.clear();
    _sortCoords.reserve(n);
  }
  
  if (n == 1) {
    // nothing to do
    _sortCoords.emplace_back(0);
    TRI_IF_FAILURE("IndexRangeBlock::sortConditions") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    return;
  }
  
  // first sort by the prefix of the index
  std::vector<std::vector<size_t>> prefix;
  prefix.reserve(n);

  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  size_t const numFields = en->_index->fields.size();

  for (size_t s = 0; s < n; s++) {
    _sortCoords.emplace_back(s);
        
    TRI_IF_FAILURE("IndexRangeBlock::sortConditions") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    {
      std::vector<size_t> next;
      next.reserve(numFields);
      prefix.emplace_back(std::move(next));
    }

    // prefix[s][t] = position in _condition[s] corresponding to the <t>th index
    // field
    for (size_t t = 0; t < numFields; t++) {
      for (size_t u = 0; u < _condition->at(s).size(); u++) {
        auto const& ri = _condition->at(s)[u];
        std::string fieldString;
        TRI_AttributeNamesToString(en->_index->fields[t], fieldString, true);

        if (fieldString.compare(ri._attr) == 0) {
    
          TRI_IF_FAILURE("IndexRangeBlock::sortConditionsInner") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          prefix.at(s).insert(prefix.at(s).begin() + t, u);
          break;
        }
      }
    }
  }

  SortFunc sortFunc(prefix, _condition, en->_reverse);

  // then sort by the values of the bounds
  std::sort(_sortCoords.begin(), _sortCoords.end(), sortFunc);

  _posInRanges = 0;
}

////////////////////////////////////////////////////////////////////////////////
// @brief: is _condition[i] < _condition[j]? these are IndexAndConditions. 
////////////////////////////////////////////////////////////////////////////////

bool IndexRangeBlock::SortFunc::operator() (size_t const& i, size_t const& j) const {
  size_t l, r;

  if (! _reverse) {
    l = i;
    r = j;
  } 
  else {
    l = j; 
    r = i;
  }

  size_t shortest = (std::min)(_prefix[i].size(), _prefix[j].size());

  for (size_t k = 0; k < shortest; k++) {
    RangeInfo const& lhs = _condition->at(l).at(_prefix[l][k]);
    RangeInfo const& rhs = _condition->at(r).at(_prefix[r][k]);
    int cmp;
      
    if (lhs.is1ValueRangeInfo() && rhs.is1ValueRangeInfo()) {
      cmp = TRI_CompareValuesJson(lhs._lowConst.bound().json(), 
                                  rhs._lowConst.bound().json());
      if (cmp != 0) {
        return (cmp < 0);
      }
    } 
    else {
      // assuming lhs and rhs are disjoint!!
      TRI_ASSERT_EXPENSIVE(areDisjointRangeInfos(lhs, rhs));
      if (lhs._highConst.isDefined() && rhs._lowConst.isDefined()) {
        cmp = (TRI_CompareValuesJson(lhs._highConst.bound().json(), 
                                     rhs._lowConst.bound().json()));
        return (cmp == 0 || cmp < 0);
      } 
      // lhs._lowConst.isDefined() && rhs._highConst.isDefined()
      return false;
    }
  }
  TRI_ASSERT(false); 
  // shouldn't get here since the IndexAndConditions in _condition should be 
  // disjoint!
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineRangeInfoVecs: combine the arguments into a single vector,
/// by intersecting every pair of range infos and inserting them in the returned
/// value if the intersection is valid. 
////////////////////////////////////////////////////////////////////////////////

std::vector<RangeInfo> IndexRangeBlock::andCombineRangeInfoVecs (std::vector<RangeInfo> const& riv1, 
                                                                 std::vector<RangeInfo> const& riv2) const {
  std::vector<RangeInfo> out;
  
  std::unordered_set<TRI_json_t*, triagens::basics::JsonHash, triagens::basics::JsonEqual> cache(
    16, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );
      
  triagens::basics::ScopeGuard guard{
    []() -> void { },
    [&cache]() -> void {
      // free the JSON values in the cache
      for (auto& it : cache) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it);
      }
    }
  };

  for (RangeInfo const& ri1: riv1) {
    for (RangeInfo const& ri2: riv2) {
      RangeInfo x(ri1.clone());
      x.fuse(ri2);

      if (x.isValid()) {
        TRI_IF_FAILURE("IndexRangeBlock::andCombineRangeInfoVecs") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        if (x.is1ValueRangeInfo()) {
          // de-duplicate
          auto lowBoundValue = x._lowConst.bound().json();

          if (cache.find(lowBoundValue) != cache.end()) {
            // already seen the same value
            continue;
          }

          std::unique_ptr<TRI_json_t> copy(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, lowBoundValue));

          if (copy == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }

          // every JSON in the cache is a copy
          cache.emplace(copy.get());
          copy.release();
        }

        out.emplace_back(std::move(x));
      }
    }
  }
  
  return out;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cartesian: form the cartesian product of the inner vectors. This is
/// required in case a dynamic bound evaluates to a list, then we have an 
/// "and" condition containing an "or" condition, which we must then distribute. 
////////////////////////////////////////////////////////////////////////////////

IndexOrCondition* IndexRangeBlock::cartesian (std::vector<std::vector<RangeInfo>> const& collector) const {
  size_t const n = collector.size();
  
  std::vector<size_t> indexes;
  indexes.reserve(n);

  for (size_t i = 0; i < n; i++) {
    indexes.emplace_back(0);
  }
  
  std::unique_ptr<IndexOrCondition> out(new IndexOrCondition());

  while (true) {
    IndexAndCondition next;
    for (size_t i = 0; i < n; i++) {
      next.emplace_back(collector[i][indexes[i]].clone());
    }

    TRI_IF_FAILURE("IndexRangeBlock::cartesian") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    out->emplace_back(next);
    size_t j = n - 1;

    while (true) {
      indexes[j]++;
      if (indexes[j] < collector[j].size()) {
        break;
      }

      indexes[j] = 0;
      if (j == 0) { 
        return out.release();
      }
      j--;
    }
  }
}

void IndexRangeBlock::freeCondition () {
  if (_condition != nullptr && _freeCondition) {
    delete _condition;
    _condition = nullptr;
    _freeCondition = false;
  }
}

// this is called every time everything in _documents has been passed on

bool IndexRangeBlock::readIndex (size_t atMost) {
  ENTER_BLOCK;
  // this is called every time we want more in _documents. 
  // For the primary key index, this only reads the index once, and never 
  // again (although there might be multiple calls to this function). 
  // For the edge, hash or skiplists indexes, initRanges creates an iterator
  // and read*Index just reads from the iterator until it is done. 
  // Then initRanges is read again and so on. This is to avoid reading the
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
  
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  
  if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
    if (_flag && _condition != nullptr) {
      readPrimaryIndex(*_condition);
    }
  }
  else if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    readEdgeIndex(atMost);
  }
  else if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX) {
    readHashIndex(atMost);
  }
  else if (en->_index->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    readSkiplistIndex(atMost);
  }
  else {
    TRI_ASSERT(false);
  }
  _flag = false;
  return (! _documents.empty());
  LEAVE_BLOCK;
}

int IndexRangeBlock::initializeCursor (AqlItemBlock* items, size_t pos) {
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

AqlItemBlock* IndexRangeBlock::getSome (size_t atLeast,
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
          || (! initRanges())) {
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
        
        if (! initRanges()) {
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

size_t IndexRangeBlock::skipSome (size_t atLeast,
                                  size_t atMost) {
  
  if (_done) {
    return 0;
  }

  size_t skipped = 0;

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize, atMost);
      if (! ExecutionBlock::getBlock(toFetch, toFetch) 
          || (! initRanges())) {
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
        if (! initRanges()) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents using the primary index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readPrimaryIndex (IndexOrCondition const& ranges) {
  ENTER_BLOCK;
  auto primaryIndex = _collection->documentCollection()->primaryIndex();
 
  for (size_t i = 0; i < ranges.size(); i++) {
    std::string key;

    for (auto const& x : ranges[i]) {
      if (x._attr == std::string(TRI_VOC_ATTRIBUTE_ID)) {
        // lookup by _id

        // we can use lower bound because only equality is supported
        TRI_ASSERT(x.is1ValueRangeInfo());
        auto const json = x._lowConst.bound().json();

        if (TRI_IsStringJson(json)) {
          // _id must be a string
          TRI_voc_cid_t documentCid;
          std::string documentKey;

          // parse _id value
          int errorCode = resolve(json->_value._string.data, documentCid, documentKey);

          if (errorCode == TRI_ERROR_NO_ERROR) {
            bool const isCluster = triagens::arango::ServerState::instance()->isRunningInCluster();

            if (! isCluster && documentCid == _collection->documentCollection()->_info._cid) {
              // only continue lookup if the id value is syntactically correct and
              // refers to "our" collection, using local collection id
              key = documentKey;
            }
            else if (isCluster && documentCid == _collection->documentCollection()->_info._planId) {
            // only continue lookup if the id value is syntactically correct and
            // refers to "our" collection, using cluster collection id
              key = documentKey;
            }
          }
        }
        /*if (! x._lows.empty() || ! x._highs.empty() || x._lowConst.isDefined() || x._highConst.isDefined()) {
          break;
        }*/
      }
      else if (x._attr == std::string(TRI_VOC_ATTRIBUTE_KEY)) {
        // lookup by _key

        // we can use lower bound because only equality is supported
        TRI_ASSERT(x.is1ValueRangeInfo());
        auto const json = x._lowConst.bound().json();
        if (TRI_IsStringJson(json)) {
          key = std::string(json->_value._string.data, json->_value._string.length - 1);
        }

        /*if (! x._lows.empty() || ! x._highs.empty() || x._lowConst.isDefined() || x._highConst.isDefined()) {
          break;
        }*/
      }
    }

    if (! key.empty()) {
      ++_engine->_stats.scannedIndex;

      auto found = static_cast<TRI_doc_mptr_t const*>(primaryIndex->lookupKey(key.c_str()));

      if (found != nullptr) {
        _documents.emplace_back(*found);
      }
    }
  }

  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build search values for edge index lookup
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::getEdgeIndexIterator (IndexAndCondition const& ranges) {
  ENTER_BLOCK;
  
  _edgeNextElement = nullptr;

  if (_edgeIndexIterator != nullptr) {
    delete _edgeIndexIterator;
    _edgeIndexIterator = nullptr;
  }
 
  auto buildIterator = [this] (TRI_edge_direction_e direction, TRI_json_t const* key) -> void {
    TRI_ASSERT(_edgeIndexIterator == nullptr);

    TRI_voc_cid_t documentCid;
    std::string documentKey;

    int errorCode = resolve(key->_value._string.data, documentCid, documentKey);

    if (errorCode == TRI_ERROR_NO_ERROR) {
      _edgeIndexIterator = new TRI_edge_index_iterator_t(direction, documentCid, (TRI_voc_key_t) documentKey.c_str());
    }
  };

  for (auto const& x : ranges) {
    if (x._attr == std::string(TRI_VOC_ATTRIBUTE_FROM)) {
      // we can use lower bound because only equality is supported
      TRI_ASSERT(x.is1ValueRangeInfo());
      auto const json = x._lowConst.bound().json();
      if (TRI_IsStringJson(json)) {
        // no error will be thrown if _from is not a string
        buildIterator(TRI_EDGE_OUT, json);
      }
      break;
    }
    else if (x._attr == std::string(TRI_VOC_ATTRIBUTE_TO)) {
      // we can use lower bound because only equality is supported
      TRI_ASSERT(x.is1ValueRangeInfo());
      auto const json = x._lowConst.bound().json();
      if (TRI_IsStringJson(json)) {
        // no error will be thrown if _to is not a string
        buildIterator(TRI_EDGE_IN, json);
      }
      break;
    }
  }
  
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually read from the edge index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readEdgeIndex (size_t atMost) {
  ENTER_BLOCK;

  if (_edgeIndexIterator == nullptr) {
    return;
  }

  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);
 
  try { 
    size_t nrSent = 0;
    while (nrSent < atMost && _edgeIndexIterator != nullptr) { 
      size_t const n = _documents.size();

      TRI_IF_FAILURE("IndexRangeBlock::readEdgeIndex") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      static_cast<triagens::arango::EdgeIndex*>(idx)->lookup(_edgeIndexIterator, _documents, _edgeNextElement, atMost);
    
      size_t const numRead = _documents.size() - n;

      _engine->_stats.scannedIndex += static_cast<int64_t>(numRead);
      nrSent += numRead;

      if (_edgeNextElement == nullptr) {
        delete _edgeIndexIterator;
        _edgeIndexIterator = nullptr;

        if (++_posInRanges < _condition->size()) {
          getEdgeIndexIterator(_condition->at(_posInRanges));
        }
      }
    }
  }
  catch (...) {
    if (_edgeIndexIterator != nullptr) {
      delete _edgeIndexIterator;
      _edgeIndexIterator = nullptr;
    }
    throw;
  }
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the search values for the hash index lookup
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::destroyHashIndexSearchValues () {
  if (_hashIndexSearchValue._values != nullptr) {
    auto shaper = _collection->documentCollection()->getShaper(); 

    for (size_t i = 0; i < _hashIndexSearchValue._length; ++i) {
      TRI_DestroyShapedJson(shaper->memoryZone(), &_hashIndexSearchValue._values[i]);
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _hashIndexSearchValue._values);
    _hashIndexSearchValue._values = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up search values for the hash index lookup
////////////////////////////////////////////////////////////////////////////////

bool IndexRangeBlock::setupHashIndexSearchValue (IndexAndCondition const& range) { 
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);

  auto hashIndex = static_cast<triagens::arango::HashIndex*>(idx);
  auto const& paths = hashIndex->paths();

  auto shaper = _collection->documentCollection()->getShaper(); 

  size_t const n = paths.size();

  TRI_ASSERT(_hashIndexSearchValue._values == nullptr); // to prevent leak
  _hashIndexSearchValue._length = 0;
    // initialize the whole range of shapes with zeros
  _hashIndexSearchValue._values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, 
      n * sizeof(TRI_shaped_json_t), true));

  if (_hashIndexSearchValue._values == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
    
  _hashIndexSearchValue._length = n;


  for (size_t i = 0; i < n; ++i) {
    TRI_shape_pid_t pid = paths[i][0].first;
    TRI_ASSERT(pid != 0);
   
    char const* name = shaper->attributeNameShapePid(pid);
    std::string const lookFor(name);

    for (auto const& x : range) {
      if (x._attr == lookFor) {    //found attribute
        if (x._lowConst.bound().json() == nullptr) {
          // attribute is empty. this may be case if a function expression is used as a 
          // comparison value, and the function returns an empty list, e.g. x.a IN PASSTHRU([])
          return false;
        }

        auto shaped = TRI_ShapedJsonJson(shaper, x._lowConst.bound().json(), false); 
        // here x->_low->_bound == x->_high->_bound 
        if (shaped == nullptr) {
          return false;
        }

        _hashIndexSearchValue._values[i] = *shaped;
        // free only the pointer, but not the internals
        TRI_Free(shaper->memoryZone(), shaped);
        break; 
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build search values for hash index lookup
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::getHashIndexIterator (IndexAndCondition const& ranges) {
  ENTER_BLOCK;
  
  _hashNextElement = nullptr;
 
  destroyHashIndexSearchValues();
  if (! setupHashIndexSearchValue(ranges)) {
    destroyHashIndexSearchValues();
  }

  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually read from the hash index
////////////////////////////////////////////////////////////////////////////////
 
void IndexRangeBlock::readHashIndex (size_t atMost) {
  ENTER_BLOCK;

  if (_hashIndexSearchValue._values == nullptr) {
    return;
  }

  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);
  
  size_t nrSent = 0;
  while (nrSent < atMost) { 
    size_t const n = _documents.size();

    TRI_IF_FAILURE("IndexRangeBlock::readHashIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    static_cast<triagens::arango::HashIndex*>(idx)->lookup(&_hashIndexSearchValue, _documents, _hashNextElement, atMost);
    size_t const numRead = _documents.size() - n;

    _engine->_stats.scannedIndex += static_cast<int64_t>(numRead);
    nrSent += numRead;

    if (_hashNextElement == nullptr) {
      destroyHashIndexSearchValues();

      if (++_posInRanges < _condition->size()) {
        getHashIndexIterator(_condition->at(_posInRanges));
      }
      if (_hashIndexSearchValue._values == nullptr) {
        _hashNextElement = nullptr;
        break;
      }
    }
  }
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read documents using a skiplist index
////////////////////////////////////////////////////////////////////////////////

// it is only possible to query a skip list using more than one attribute if we
// only have equalities followed by a single arbitrary comparison (i.e x.a == 1
// && x.b == 2 && x.c > 3 && x.c <= 4). Then we do: 
//
//   TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, left, right, nullptr, shaper,
//     2);
//
// where 
//
//   left =  TRI_CreateIndexOperator(TRI_GT_INDEX_OPERATOR, nullptr, nullptr, [1,2,3],
//     shaper, 3)
//
//   right =  TRI_CreateIndexOperator(TRI_LE_INDEX_OPERATOR, nullptr, nullptr, [1,2,4],
//     shaper, 3)
//
// If the final comparison is an equality (x.a == 1 && x.b == 2 && x.c ==3), then 
// we just do:
//
//   TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr, nullptr, [1,2,3],
//     shaper, 3)
//
// It is necessary that values of the attributes are listed in the correct
// order (i.e. <a> must be the first attribute indexed, and <b> must be the
// second). If one of the attributes is not indexed, then it is ignored,
// provided we are querying all the previously indexed attributes (i.e. we
// cannot do (x.c == 1 && x.a == 2) if the index covers <a>, <b>, <c> in this
// order but we can do (x.a == 2)).
//
// If the comparison is not equality, then the values of the parameters 
// (i.e. the 1 in x.c >= 1) cannot be lists or arrays.
//

void IndexRangeBlock::getSkiplistIterator (IndexAndCondition const& ranges) {
  ENTER_BLOCK;
  TRI_ASSERT(_skiplistIterator == nullptr);
  
  auto en = static_cast<IndexRangeNode const*>(getPlanNode());
  auto idx = en->_index->getInternals();
  TRI_ASSERT(idx != nullptr);

  auto shaper = _collection->documentCollection()->getShaper(); 
  TRI_ASSERT(shaper != nullptr);

  TRI_index_operator_t* skiplistOperator = nullptr; 

  Json parameters(Json::Array); 
  size_t i = 0;
  for (; i < ranges.size(); i++) {
    auto const& range = ranges[i];
    // TRI_ASSERT(range.isConstant());
    if (range.is1ValueRangeInfo()) {   // it's an equality . . . 
      parameters(range._lowConst.bound().copy());
    } 
    else {                          // it's not an equality and so the final comparison 
      if (parameters.size() != 0) {
        skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr,
            nullptr, parameters.copy().steal(), shaper, i);
      }
      if (range._lowConst.isDefined()) {
        auto op = range._lowConst.toIndexOperator(false, parameters.copy(), shaper);

        if (skiplistOperator != nullptr) {
          skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
              skiplistOperator, op, nullptr, shaper, 2);
        } 
        else {
          skiplistOperator = op;
        }
      }
      if (range._highConst.isDefined()) {
        auto op = range._highConst.toIndexOperator(true, parameters.copy(), shaper);

        if (skiplistOperator != nullptr) {
          skiplistOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
              skiplistOperator, op, nullptr, shaper, 2);
        } 
        else {
          skiplistOperator = op;
        }
      }
    }
  }

  if (skiplistOperator == nullptr) {      // only have equalities . . .
    if (parameters.size() == 0) {
      // this creates the infinite range (i.e. >= null)
      Json hass(Json::Array);
      hass.add(Json(Json::Null));
      skiplistOperator = TRI_CreateIndexOperator(TRI_GE_INDEX_OPERATOR, nullptr,
          nullptr, hass.steal(), shaper, 1);
    }
    else {
      skiplistOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr,
          nullptr, parameters.steal(), shaper, i);
    }
  }
  
  delete _skiplistIterator;

  _skiplistIterator = static_cast<triagens::arango::SkiplistIndex*>(idx)->lookup(skiplistOperator, en->_reverse);

  if (skiplistOperator != nullptr) {
    delete skiplistOperator;
  }

  if (_skiplistIterator == nullptr) {
    int res = TRI_errno();

    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      return;
    }

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }
  LEAVE_BLOCK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief actually read from the skiplist index
////////////////////////////////////////////////////////////////////////////////

void IndexRangeBlock::readSkiplistIndex (size_t atMost) {
  ENTER_BLOCK;
  if (_skiplistIterator == nullptr) {
    return;
  }

  try {
    size_t nrSent = 0;
    while (nrSent < atMost && _skiplistIterator != nullptr) {
      TRI_index_element_t* indexElement = _skiplistIterator->next();

      if (indexElement == nullptr) {
        delete _skiplistIterator;
        _skiplistIterator = nullptr;
        if (++_posInRanges < _condition->size()) {
          getSkiplistIterator(_condition->at(_sortCoords[_posInRanges]));
        }
      } 
      else {
        TRI_IF_FAILURE("IndexRangeBlock::readSkiplistIndex") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        
        _documents.emplace_back(*(indexElement->document()));
        ++nrSent;
        ++_engine->_stats.scannedIndex;
      }
    }
  }
  catch (...) {
    if (_skiplistIterator != nullptr) {
      delete _skiplistIterator;
      _skiplistIterator = nullptr;
    }
    throw;
  }
  LEAVE_BLOCK;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
