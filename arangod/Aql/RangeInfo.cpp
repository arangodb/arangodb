////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for RangeInfo
///
/// @file arangod/Aql/RangeInfo.h
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
/// @author not James
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <Basics/Common.h>
#include "Basics/JsonHelper.h"
#include "Aql/RangeInfo.h"

using namespace triagens::basics;
using namespace triagens::aql;
using Json = triagens::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief 3-way comparison of the tightness of upper or lower constant
/// RangeInfoBounds.
/// Returns -1 if <left> is tighter than <right>, 1 if <right> is tighter than
/// <left>, and 0 if the bounds are the same. The argument <lowhigh> should be
/// -1 if we are comparing lower bounds and 1 if we are comparing upper bounds. 
///
/// If <left> or <right> is a undefined, this indicates no bound. 
///
/// If ~ is the comparison and (x,y), (z,t) are RangeInfoBounds (i.e. x,z are
/// Json values, y,t are booleans) that we are comparing as lower bounds, then
/// the following holds:
///
///                 -1  x>z or (x=z, y=false, z=true)  
/// (x,y) ~ (z,t) = 0   x=z, y=t
///                 1   z>x or (x=z, y=true, t=false)
///
/// as upper bounds:
/// 
///                 -1  x<z or (x=z, y=false, z=true)  
/// (x,y) ~ (z,t) = 0   x=z, y=t
///                 1   z<x or (x=z, y=true, t=false)
///
/// For example (x<2) is tighter than (x<3) and (x<=2). The bound (x<2) is
/// represented as (2, false), and (x<=2) is (2, true), and so as upper bounds
/// (2, false) ~ (2, true) = -1 indicating that (2,false)=(x<2) is tighter than
/// (2, true)=(x<=2). 
////////////////////////////////////////////////////////////////////////////////

static int CompareRangeInfoBound (RangeInfoBound const& left, 
                                  RangeInfoBound const& right, 
                                  int lowhigh) {
  if (! left.isDefined()) {
    return (right.isDefined() ? 1 : 0);
  } 
  if (! right.isDefined()) {
    return -1;
  }

  int cmp = TRI_CompareValuesJson(left.bound().json(), right.bound().json());
  if (cmp == 0 && (left.inclusive() != right.inclusive())) {
    return (left.inclusive() ? 1 : -1);
  }
  return cmp * lowhigh;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief class RangeInfoBound
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineLowerBounds, changes the bound in *this and replaces
/// it by the stronger bound of *this and that, interpreting both as lower
/// bounds, this only works for constant bounds.
////////////////////////////////////////////////////////////////////////////////

void RangeInfoBound::andCombineLowerBounds (RangeInfoBound const& that) {
  if (CompareRangeInfoBound(that, *this, -1) == -1) {
    assign(that);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineUpperBounds, changes the bound in *this and replaces
/// it by the stronger bound of *this and that, interpreting both as upper
/// bounds, this only works for constant bounds.
////////////////////////////////////////////////////////////////////////////////

void RangeInfoBound::andCombineUpperBounds (RangeInfoBound const& that) {
  if (CompareRangeInfoBound(that, *this, 1) == -1) {
    assign(that);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getExpressionAst, looks up or computes (if necessary) an AST
/// for the variable bound, return nullptr for a constant bound, the new
/// (if needed) nodes are registered with the ast
////////////////////////////////////////////////////////////////////////////////

AstNode const* RangeInfoBound::getExpressionAst (Ast* ast) const {
  if (_expressionAst != nullptr) {
    return _expressionAst;
  }
  if (_isConstant) {
    return nullptr;
  }
  _expressionAst = new AstNode(ast, _bound);
  return _expressionAst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief class RangeInfo
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor from JSON
////////////////////////////////////////////////////////////////////////////////

RangeInfo::RangeInfo (triagens::basics::Json const& json) 
  : _var(basics::JsonHelper::checkAndGetStringValue(json.json(), "variable")),
    _attr(basics::JsonHelper::checkAndGetStringValue(json.json(), "attr")),
    _valid(basics::JsonHelper::checkAndGetBooleanValue(json.json(), "valid")),
    _defined(true),
    _equality(basics::JsonHelper::checkAndGetBooleanValue(json.json(), "equality")) {

  triagens::basics::Json jsonLowList = json.get("lows");
  if (! jsonLowList.isList()) {
    THROW_INTERNAL_ERROR("low attribute must be a list");
  }
  triagens::basics::Json jsonHighList = json.get("highs");
  if (! jsonHighList.isList()) {
    THROW_INTERNAL_ERROR("high attribute must be a list");
  }
  // If an exception is thrown from within these loops, then the
  // vectors _low and _high will be destroyed properly, so no 
  // try/catch is needed.
  for (size_t i = 0; i < jsonLowList.size(); i++) {
    _lows.emplace_back(jsonLowList.at(static_cast<int>(i)));
  }
  for (size_t i = 0; i < jsonHighList.size(); i++) {
    _highs.emplace_back(jsonHighList.at(static_cast<int>(i)));
  }
  _lowConst.assign(json.get("lowConst"));
  _highConst.assign(json.get("highConst"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson for a RangeInfo
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json RangeInfo::toJson () const {
  triagens::basics::Json item(basics::Json::Array);
  item("variable", triagens::basics::Json(_var))
      ("attr", triagens::basics::Json(_attr))
      ("lowConst", _lowConst.toJson())
      ("highConst", _highConst.toJson());
  triagens::basics::Json lowList(triagens::basics::Json::List, _lows.size());
  for (auto l : _lows) {
    lowList(l.toJson());
  }
  item("lows", lowList);
  triagens::basics::Json highList(triagens::basics::Json::List, _highs.size());
  for (auto h : _highs) {
    highList(h.toJson());
  }
  item("highs", highList);
  item("valid", triagens::basics::Json(_valid));
  item("equality", triagens::basics::Json(_equality));
  return item;
}
        
////////////////////////////////////////////////////////////////////////////////
/// @brief class RangeInfoMap
////////////////////////////////////////////////////////////////////////////////
        
RangeInfoMap::RangeInfoMap (std::string const& var, 
                            std::string const& name, 
                            RangeInfoBound low, 
                            RangeInfoBound high,
                            bool equality)
  : _ranges() {
    RangeInfo ri(var, name, low, high, equality);
    std::unordered_map<std::string, RangeInfo> map;
    map.emplace(make_pair(name, ri));
    _ranges.emplace(std::make_pair(var, map));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  insert if there is no range corresponding to variable name <var>,
/// and attribute <name>, and otherwise intersection with existing range
////////////////////////////////////////////////////////////////////////////////

void RangeInfoMap::insert (RangeInfo newRange) { 
  TRI_ASSERT(newRange.isDefined());

  std::unordered_map<std::string, RangeInfo>* oldMap = find(newRange._var);

  if (oldMap == nullptr) {
    std::unordered_map<std::string, RangeInfo> newMap;
    newMap.emplace(make_pair(newRange._attr, newRange));
    _ranges.emplace(std::make_pair(newRange._var, newMap));
    return;
  }
  
  auto it = oldMap->find(newRange._attr); 
  
  if (it == oldMap->end()) {
    oldMap->emplace(make_pair(newRange._attr, newRange));
    return;
  }

  RangeInfo& oldRange((*it).second);

  oldRange.fuse(newRange);
}

////////////////////////////////////////////////////////////////////////////////
///// @brief fuse, fuse two ranges, must be for the same variable and attribute
////////////////////////////////////////////////////////////////////////////////

void RangeInfo::fuse (RangeInfo const& that) {
  TRI_ASSERT(_var == that._var);
  TRI_ASSERT(_attr == that._attr);
    
  if (! isValid()) { 
    // intersection of the empty set with any set is empty!
    return;
  }
  
  if (! that.isValid()) {
    // intersection of the empty set with any set is empty!
    invalidate();
    return;
  }

  // The following is a corner case for constant bounds:
  if (_equality && that._equality && 
      _lowConst.isDefined() && that._lowConst.isDefined()) {
    if (! TRI_CheckSameValueJson(_lowConst.bound().json(),
                                 that._lowConst.bound().json())) {
      invalidate();
      return;
    }
  }

  // First sort out the constant low bounds:
  _lowConst.andCombineLowerBounds(that._lowConst);
  // Simply append the variable ones:
  for (auto l : that._lows) {
    _lows.emplace_back(l);
  }

  // Sort out the constant high bounds:
  _highConst.andCombineUpperBounds(that._highConst);
  // Simply append the variable ones:
  for (auto h : that._highs) {
    _highs.emplace_back(h);
  }
    
  // If either range knows that it can be at most one value, then this is
  // enough:
  _equality |= that._equality;

  // check the new constant range bounds are valid:
  if (_lowConst.isDefined() && _highConst.isDefined()) {
    int cmp = TRI_CompareValuesJson(_lowConst.bound().json(), 
                                    _highConst.bound().json());
    if (cmp == 1) {
      invalidate();
      return;
    }
    if (cmp == 0) {
      if (! (_lowConst.inclusive() && _highConst.inclusive()) ) {
        // range invalid
        invalidate();
      }
      else {
        _equality = true;  // Can only be at most one value
      }
    }
  }
}

static bool isIdenticalRangeInfo (RangeInfo lhs, RangeInfo rhs) {
  
  if (CompareRangeInfoBound(lhs._lowConst, rhs._lowConst, -1) != 0) {
    return false;
  }
  if (CompareRangeInfoBound(lhs._highConst, rhs._highConst, 1) != 0) {
    return false;
  }

  if (lhs._lows.size() != rhs._lows.size()) {
    return false;
  }

  if (lhs._highs.size() != rhs._highs.size()) {
    return false;
  }
  
  auto lhsIt = lhs._lows.begin();
  auto rhsIt = rhs._lows.begin();
  while (lhsIt != lhs._lows.end()) {
    if (TRI_CompareValuesJson(lhsIt->bound().json(), rhsIt->bound().json()) != 0) {
      return false;
    }
    lhsIt++;
    rhsIt++;
  }

  lhsIt = lhs._highs.begin();
  rhsIt = rhs._highs.begin();
  while (lhsIt != lhs._highs.end()) {
    if (TRI_CompareValuesJson(lhsIt->bound().json(), rhsIt->bound().json()) != 0) {
      return false;
    }
    lhsIt++;
    rhsIt++;
  }
  return true;
}

bool RangeInfoMap::isValid (std::string const& var) {
  // are all the range infos valid?

  std::unordered_map<std::string, RangeInfo>* map = find(var);
  if (map != nullptr) {
    for(auto x: *map) {
      if (! x.second.isValid()) {
        return false;
      }
    }
    return true;
  }
  return false;
}

std::unordered_set<std::string> RangeInfoMap::attributes (std::string const& var) {
  std::unordered_set<std::string> attrs;
  std::unordered_map<std::string, RangeInfo>* map = find(var);
  if (map != nullptr) {
    for(auto x: *map) {
      attrs.insert(x.first);
    }
  }
  return attrs;
}

RangeInfoMap* RangeInfoMap::clone () {
  auto rim = new RangeInfoMap();
  for (auto x: _ranges) {
    for (auto y: x.second) {
      rim->insert(y.second.clone());
    }
  }
  return rim;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  insert if there is no range corresponding to variable name <var>,
/// and attribute <name>, and otherwise intersection with existing range
////////////////////////////////////////////////////////////////////////////////

void RangeInfoMap::insert (std::string const& var, 
                           std::string const& name, 
                           RangeInfoBound low, 
                           RangeInfoBound high,
                           bool equality) { 
  insert(RangeInfo(var, name, low, high, equality));
}


void RangeInfoMap::eraseEmptyOrUndefined(std::string const& var) {
  
  std::unordered_map<std::string, RangeInfo>* map = find(var);
  if (map != nullptr) {
    for (auto it = map->begin(); it != map->end(); /* no hoisting */ ) {
      if (it->second._lows.empty() &&
          it->second._highs.empty() &&
          ! it->second._lowConst.isDefined() &&
          ! it->second._highConst.isDefined()) {
        it = map->erase(it);
      }
      else {
        it++;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

/*RangeInfoMapVec::RangeInfoMapVec  (std::string const& var, 
                                   std::string const& name, 
                                   RangeInfoBound low, 
                                   RangeInfoBound high,
                                   bool equality) : 
  _rangeInfoMapVec() {
  
  _rangeInfoMapVec.emplace_back(new RangeInfoMap(var, name, low, high, equality));
}*/

RangeInfoMapVec::RangeInfoMapVec  (RangeInfoMap* rim) :
  _rangeInfoMapVec() {
  
  if (! rim->empty()){
    _rangeInfoMapVec.emplace_back(rim);
  }
}

void RangeInfoMapVec::emplace_back (RangeInfoMap* rim) {
  _rangeInfoMapVec.emplace_back(rim);
}

std::unordered_set<std::string> RangeInfoMapVec::attributes (std::string const& var) {
  return _rangeInfoMapVec[0]->attributes(var);
}

std::vector<size_t> RangeInfoMapVec::validPositions(std::string const& var) {
  std::vector<size_t> valid;

  for (size_t i = 0; i < _rangeInfoMapVec.size(); i++) {
    if (_rangeInfoMapVec[i]->isValid(var)) {
      valid.push_back(i);
    }
  }
  return valid;
}

void RangeInfoMapVec::eraseEmptyOrUndefined(std::string const& var) {
  for (RangeInfoMap* x: _rangeInfoMapVec) {
    x->eraseEmptyOrUndefined(var);
  }
}

std::unordered_map<std::string, RangeInfo>* RangeInfoMapVec::find (
                                              std::string const& var, size_t pos) {
  if (pos >= _rangeInfoMapVec.size()) {
    return nullptr;
  }
  return _rangeInfoMapVec[pos]->find(var);
} 

void RangeInfoMapVec::insertAnd (std::string const& var, 
                                 std::string const& name, 
                                 RangeInfoBound low, 
                                 RangeInfoBound high,
                                 bool equality) { 
  insertAnd(RangeInfo(var, name, low, high, equality));
}

// var.attr > 1 and var.attr < 10

void RangeInfoMapVec::insertAnd (RangeInfo range) {
  
  if (_rangeInfoMapVec.empty()) {
    _rangeInfoMapVec.emplace_back(new RangeInfoMap());
  }
  for (size_t i = 0; i < _rangeInfoMapVec.size(); i++) {
    _rangeInfoMapVec[i]->insert(range);
  }

}

bool RangeInfoMapVec::isIdenticalToExisting (RangeInfo x) {

  for (auto rim: _rangeInfoMapVec) {
    RangeInfo* ri = rim->find(x._var, x._attr);
    if (ri != nullptr && isIdenticalRangeInfo(*ri, x)){
      return true;
    }
  }
  return false;
}

RangeInfoMap* triagens::aql::andCombineRangeInfoMaps (RangeInfoMap* lhs, RangeInfoMap* rhs) {
 
  RangeInfoMap* rim = lhs->clone();

  for (auto x: rhs->_ranges) {
    for (auto y: x.second) {
      rim->insert(y.second.clone());
    }
  }
  return rim;
}

RangeInfoMapVec* triagens::aql::orCombineRangeInfoMapVecs (RangeInfoMapVec* lhs, 
                                                           RangeInfoMapVec* rhs) {
 
  if (lhs->empty()) {
    return rhs; //TODO copy?
  }


  if (rhs->empty()) {
    return lhs; //TODO copy?
  }

  auto rimv = new RangeInfoMapVec();

  // this lhs already doesn't contain duplicate conditions
  for (size_t i = 0; i < lhs->size(); i++) {
    rimv->emplace_back((*lhs)[i]->clone());
  }

  //avoid inserting identical conditions
  for (size_t i = 0; i < rhs->size(); i++) {
    auto rim = new RangeInfoMap();
    for (auto x: (*rhs)[i]->_ranges) {
      for (auto y: x.second) {
        if (! rimv->isIdenticalToExisting(y.second)) {
          rim->insert(y.second);
        }
      }
    }
    if (! rim->empty()) {
      rimv->emplace_back(rim);
    }
  }

  return rimv;
}

// distributes AND into OR

RangeInfoMapVec* triagens::aql::andCombineRangeInfoMapVecs (RangeInfoMapVec* lhs, 
                                                            RangeInfoMapVec* rhs) {
  if (lhs->empty()) {
    return lhs;
  }

  if (rhs->empty()) {
    return rhs;
  }

  auto rimv = new RangeInfoMapVec();

  for (size_t i = 0; i < lhs->size(); i++) {
    for (size_t j = 0; j < rhs->size(); j++) {
      rimv->emplace_back(andCombineRangeInfoMaps((*lhs)[i]->clone(), (*rhs)[j]->clone()));
    }
  }
  return rimv;
}

