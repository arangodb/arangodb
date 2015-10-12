////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// @file arangod/Aql/ExecutionNode.cpp
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
/// @author Michael Hackstein
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/IndexNode.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Ast.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                              methods of IndexNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for IndexNode
////////////////////////////////////////////////////////////////////////////////

void IndexNode::toJsonHelper (triagens::basics::Json& nodes,
                              TRI_memory_zone_t* zone,
                              bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));
  // call base class method

  if (json.isEmpty()) {
    return;
  }
 
  // Now put info about vocbase and cid in there
  json("database",    triagens::basics::Json(_vocbase->_name))
      ("collection",  triagens::basics::Json(_collection->getName()))
      ("outVariable", _outVariable->toJson());

  triagens::basics::Json indexes(triagens::basics::Json::Array, _indexes.size());
  for (auto& index : _indexes) {
    indexes.add(index->toJson());
  }

  json("indexes",   indexes); 
  json("condition", _condition->toJson(TRI_UNKNOWN_MEM_ZONE, verbose)); 
  json("reverse",   triagens::basics::Json(_reverse));

  // And add it:
  nodes(json);
}

ExecutionNode* IndexNode::clone (ExecutionPlan* plan,
                                 bool withDependencies,
                                 bool withProperties) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto c = new IndexNode(plan, _id, _vocbase, _collection, 
                         outVariable, _indexes, _condition->clone(), _reverse);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for IndexNode from Json
////////////////////////////////////////////////////////////////////////////////

IndexNode::IndexNode (ExecutionPlan* plan,
                      triagens::basics::Json const& json)
  : ExecutionNode(plan, json),
    _vocbase(plan->getAst()->query()->vocbase()),
    _collection(plan->getAst()->query()->collections()->get(JsonHelper::checkAndGetStringValue(json.json(), "collection"))),
    _outVariable(varFromJson(plan->getAst(), json, "outVariable")),
    _indexes(),
    _condition(nullptr),
    _reverse(JsonHelper::checkAndGetBooleanValue(json.json(), "reverse")) { 

  auto indexes = JsonHelper::checkAndGetArrayValue(json.json(), "indexes");

  TRI_ASSERT(TRI_IsArrayJson(indexes));
  size_t length = TRI_LengthArrayJson(indexes);
  _indexes.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    auto iid  = JsonHelper::checkAndGetStringValue(TRI_LookupArrayJson(indexes, i), "id");
    auto index = _collection->getIndex(iid);

    if (index == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "index not found");
    }

    _indexes.emplace_back(index);
  }

  TRI_json_t const* condition = JsonHelper::checkAndGetObjectValue(json.json(), "condition");

  triagens::basics::Json conditionJson(TRI_UNKNOWN_MEM_ZONE, condition, triagens::basics::Json::NOFREE);
  _condition = Condition::fromJson(plan, conditionJson);

  TRI_ASSERT(_condition != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the IndexNode
////////////////////////////////////////////////////////////////////////////////

IndexNode::~IndexNode () {
  delete _condition;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an index node is a multiple of the cost of
/// its unique dependency
////////////////////////////////////////////////////////////////////////////////
 
double IndexNode::estimateCost (size_t& nrItems) const {
  nrItems = 1;
  return 1.0;
  /* 
  static double const EqualityReductionFactor = 100.0;

  size_t incoming = 0;
  double const dependencyCost = _dependencies.at(0)->getCost(incoming);
  size_t docCount = _collection->count();

  auto estimateItemsWithIndexSelectivity = [] (triagens::aql::Index const* index,
                                               size_t incoming,
                                               size_t& nrItems) const {
    // check if the index can provide a selectivity estimate
    if (! index->hasSelectivityEstimate()) {
      return false; 
    }

    // use index selectivity estimate
    double estimate = index->selectivityEstimate();

    if (estimate <= 0.0) {
      // avoid DIV0
      return false;
    }

    nrItems = static_cast<size_t>(incoming * _ranges.size() * (1.0 / estimate));
    return true;
  };

  auto estimateIndexCost = [&] (triagens::aql::Index const* idx) -> double {
    if (idx->type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
      // always an equality lookup

      // selectivity of primary index is always 1
      nrItems = incoming * _ranges.size();
      return dependencyCost + nrItems;
    }
  
    if (idx->type == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      // always an equality lookup
    
      // check if the index can provide a selectivity estimate
      if (! estimateItemsWithIndexSelectivity(idx, incoming, nrItems)) {
        // use hard-coded heuristic
        nrItems = incoming * _ranges.size() * docCount / static_cast<size_t>(EqualityReductionFactor);
      }
        
      nrItems = (std::max)(nrItems, static_cast<size_t>(1));

      return dependencyCost + nrItems;
    }

    if (idx->type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX) {
      // always an equality lookup

      // check if the index can provide a selectivity estimate
      if (! estimateItemsWithIndexSelectivity(idx, incoming, nrItems)) {
        // use hard-coded heuristic
        if (idx->unique) {
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

    if (idx->type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
      auto const count = _ranges.at(0).size();
      
      if (count == 0) {
        // no ranges? so this is unlimited -> has to be more expensive
        nrItems = incoming * docCount;
        return dependencyCost + nrItems;
      }

      if (idx->unique) {
        bool allEquality = true;
        for (auto const& x : _ranges) {
          // check if we are using all indexed attributes in the query
          if (x.size() != idx->fields().size()) {
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
  };
  */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

std::vector<Variable const*> IndexNode::getVariablesUsedHere () const {
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

void IndexNode::getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const {
  Ast::getReferencedVariables(_condition->root(), vars);

  vars.erase(_outVariable);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

