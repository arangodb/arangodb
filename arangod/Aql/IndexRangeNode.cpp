////////////////////////////////////////////////////////////////////////////////
/// @brief IndexRangeNode
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

#include "Aql/IndexRangeNode.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/WalkerWorker.h"
#include "Aql/Ast.h"
#include "Basics/StringBuffer.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                         methods of IndexRangeNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for IndexRangeNode
////////////////////////////////////////////////////////////////////////////////

void IndexRangeNode::toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone,
                                   bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));
  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // put together the range info . . .
  triagens::basics::Json ranges(triagens::basics::Json::Array, _ranges.size());

  for (auto const& x : _ranges) {
    triagens::basics::Json range(triagens::basics::Json::Array, x.size());
    for(auto const& y : x) {
      range.add(y.toJson());
    }
    ranges.add(range);
  }

  // Now put info about vocbase and cid in there
  json("database", triagens::basics::Json(_vocbase->_name))
      ("collection", triagens::basics::Json(_collection->getName()))
      ("outVariable", _outVariable->toJson())
      ("ranges", ranges);
 
  json("index", _index->toJson()); 
  json("reverse", triagens::basics::Json(_reverse));

  // And add it:
  nodes(json);
}

ExecutionNode* IndexRangeNode::clone (ExecutionPlan* plan,
                                      bool withDependencies,
                                      bool withProperties) const {
  std::vector<std::vector<RangeInfo>> ranges;
  for (size_t i = 0; i < _ranges.size(); i++){
    ranges.emplace_back(std::vector<RangeInfo>());
    
    for (auto const& x : _ranges.at(i)) {
      ranges.at(i).emplace_back(x);
    }
  }

  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto c = new IndexRangeNode(plan, _id, _vocbase, _collection, 
                              outVariable, _index, ranges, _reverse);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for IndexRangeNode from Json
////////////////////////////////////////////////////////////////////////////////

IndexRangeNode::IndexRangeNode (ExecutionPlan* plan,
                                triagens::basics::Json const& json)
  : ExecutionNode(plan, json),
    _vocbase(plan->getAst()->query()->vocbase()),
    _collection(plan->getAst()->query()->collections()->get(JsonHelper::checkAndGetStringValue(json.json(), "collection"))),
    _outVariable(varFromJson(plan->getAst(), json, "outVariable")),
    _index(nullptr), 
    _ranges(),
    _reverse(false) {

  triagens::basics::Json rangeArrayJson(TRI_UNKNOWN_MEM_ZONE, JsonHelper::checkAndGetArrayValue(json.json(), "ranges"));

  for (size_t i = 0; i < rangeArrayJson.size(); i++) { //loop over the ranges . . .
    _ranges.emplace_back();

    triagens::basics::Json rangeJson(rangeArrayJson.at(static_cast<int>(i)));
    for (size_t j = 0; j < rangeJson.size(); j++) {
      _ranges.at(i).emplace_back(rangeJson.at(static_cast<int>(j)));
    }
  }

  // now the index . . . 
  // TODO the following could be a constructor method for
  // an Index object when these are actually used
  auto index = JsonHelper::checkAndGetObjectValue(json.json(), "index");
  auto iid   = JsonHelper::checkAndGetStringValue(index, "id");

  _index = _collection->getIndex(iid);
  _reverse = JsonHelper::checkAndGetBooleanValue(json.json(), "reverse");

  if (_index == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "index not found");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the pattern matches this node's index
////////////////////////////////////////////////////////////////////////////////

ExecutionNode::IndexMatch IndexRangeNode::matchesIndex (IndexMatchVec const& pattern) const {
  return CompareIndex(this, _index, pattern);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an index range node is a multiple of the cost of
/// its unique dependency
////////////////////////////////////////////////////////////////////////////////
 
double IndexRangeNode::estimateCost (size_t& nrItems) const { 
  static double const EqualityReductionFactor = 100.0;

  size_t incoming = 0;
  double const dependencyCost = _dependencies.at(0)->getCost(incoming);
  size_t docCount = _collection->count();

  TRI_ASSERT(! _ranges.empty());
  
  if (_index->type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
    // always an equality lookup

    // selectivity of primary index is always 1
    nrItems = incoming * _ranges.size();
    return dependencyCost + nrItems;
  }
  
  if (_index->type == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    // always an equality lookup
    
    // check if the index can provide a selectivity estimate
    if (! estimateItemsWithIndexSelectivity(incoming, nrItems)) {
      // use hard-coded heuristic
      nrItems = incoming * _ranges.size() * docCount / static_cast<size_t>(EqualityReductionFactor);
    }
        
    nrItems = (std::max)(nrItems, static_cast<size_t>(1));

    return dependencyCost + nrItems;
  }

  if (_index->type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX) {
    // always an equality lookup

    // check if the index can provide a selectivity estimate
    if (! estimateItemsWithIndexSelectivity(incoming, nrItems)) {
      // use hard-coded heuristic
      if (_index->unique) {
        nrItems = incoming * _ranges.size();
      }
      else {
        double cost = static_cast<double>(docCount) * incoming * _ranges.size();
        // the more attributes are contained in the index, the more specific the lookup will be
        for (size_t i = 0; i < _ranges.at(0).size(); ++i) { 
          cost /= EqualityReductionFactor; 
        }
    
        nrItems = static_cast<size_t>(cost);
      }
    }
        
    nrItems = (std::max)(nrItems, static_cast<size_t>(1));
    // the more attributes an index matches, the better it is
    double matchLengthFactor = _ranges.at(0).size() * 0.01;

    // this is to prefer the hash index over skiplists if everything else is equal
    return dependencyCost + ((static_cast<double>(nrItems) - matchLengthFactor) * 0.9999995);
  }

  if (_index->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    auto const count = _ranges.at(0).size();
    
    if (count == 0) {
      // no ranges? so this is unlimited -> has to be more expensive
      nrItems = incoming * docCount;
      return dependencyCost + nrItems;
    }

    if (_index->unique) {
      bool allEquality = true;
      for (auto const& x : _ranges) {
        // check if we are using all indexed attributes in the query
        if (x.size() != _index->fields.size()) {
          allEquality = false;
          break;
        }

        // check if this is an equality comparison
        if (x.empty() || ! x.back().is1ValueRangeInfo()) {
          allEquality = false;
          break;
        }
      }

      if (allEquality) {
        // unique index, all attributes compared using eq (==) operator
        nrItems = incoming * _ranges.size();
        return dependencyCost + nrItems;
      }
    }

    // build a total cost for the index usage by peeking into all ranges
    double totalCost = 0.0;

    for (auto const& x : _ranges) {
      double cost = static_cast<double>(docCount) * incoming;

      for (auto const& y : x) { //only doing the 1-d case so far
        if (y.is1ValueRangeInfo()) {
          // equality lookup
          cost /= EqualityReductionFactor;
          continue;
        }

        bool hasLowerBound = false;
        bool hasUpperBound = false;

        if (y._lowConst.isDefined() || y._lows.size() > 0) {
          hasLowerBound = true;
        }
        if (y._highConst.isDefined() || y._highs.size() > 0) {
          hasUpperBound = true;
        }

        if (hasLowerBound && hasUpperBound) {
          // both lower and upper bounds defined
          cost /= 10.0;
        }
        else if (hasLowerBound || hasUpperBound) {
          // either only low or high bound defined
          cost /= 2.0;
        }

        // each bound (const and dynamic) counts!
        size_t const numBounds = y._lows.size() + 
                                 y._highs.size() + 
                                 (y._lowConst.isDefined() ? 1 : 0) + 
                                 (y._highConst.isDefined() ? 1 : 0);

        for (size_t j = 0; j < numBounds; ++j) {
          // each dynamic bound again reduces the cost
          cost *= 0.95;
        }
      }

      totalCost += cost;
    }

    totalCost = static_cast<double>((std::max)(static_cast<size_t>(totalCost), static_cast<size_t>(1))); 

    nrItems = static_cast<size_t>(totalCost);

    return dependencyCost + totalCost;
  }

  // no index
  nrItems = incoming * docCount;
  return dependencyCost + nrItems;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

std::vector<Variable const*> IndexRangeNode::getVariablesUsedHere () const {
  std::unordered_set<Variable const*> s;
  // actual work is done by that method  
  getVariablesUsedHere(s); 

  // copy result into vector
  std::vector<Variable const*> v;
  v.reserve(s.size());

  for (auto const& vv : s) {
    v.emplace_back(const_cast<Variable*>(vv));
  }
  return v;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

void IndexRangeNode::getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const {
  for (auto const& x : _ranges) {
    for (RangeInfo const& y : x) {
      for (RangeInfoBound const& z : y._lows) {
        AstNode const* a = z.getExpressionAst(_plan->getAst());
        Ast::getReferencedVariables(a, vars);
      }
      for (RangeInfoBound const& z : y._highs) {
        AstNode const* a = z.getExpressionAst(_plan->getAst());
        Ast::getReferencedVariables(a, vars);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief provide an estimate for the number of items, using the index
/// selectivity info (if present)
////////////////////////////////////////////////////////////////////////////////

bool IndexRangeNode::estimateItemsWithIndexSelectivity (size_t incoming,
                                                        size_t& nrItems) const {
  // check if the index can provide a selectivity estimate
  if (! _index->hasSelectivityEstimate()) {
    return false; 
  }

  // use index selectivity estimate
  double estimate = _index->selectivityEstimate();

  if (estimate <= 0.0) {
    // avoid DIV0
    return false;
  }

  nrItems = static_cast<size_t>(incoming * _ranges.size() * (1.0 / estimate));
  return true;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

