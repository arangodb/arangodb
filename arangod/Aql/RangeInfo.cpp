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

//#if 0
#define ENTER_BLOCK try { (void) 0;
#define LEAVE_BLOCK } catch (...) { std::cout << "caught an exception in " << __FUNCTION__ << ", " << __FILE__ << ":" << __LINE__ << "!\n"; throw; }
//#else
//#define ENTER_BLOCK
//#define LEAVE_BLOCK
//#endif

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

////////////////////////////////////////////////////////////////////////////////
/// @brief class RangeInfoMap
////////////////////////////////////////////////////////////////////////////////
        
////////////////////////////////////////////////////////////////////////////////
/// @brief construct RangeInfoMap containing single RangeInfo created from the
/// args
////////////////////////////////////////////////////////////////////////////////

RangeInfoMap::RangeInfoMap (std::string const& var, 
                            std::string const& name, 
                            RangeInfoBound low, 
                            RangeInfoBound high,
                            bool equality) {
    RangeInfoMap(RangeInfo(var, name, low, high, equality));
}

RangeInfoMap::RangeInfoMap (RangeInfo ri) : _ranges() {
    std::unordered_map<std::string, RangeInfo> map;
    map.emplace(make_pair(ri._attr, ri));
    _ranges.emplace(std::make_pair(ri._var, map));
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

void RangeInfoMap::erase (RangeInfo* ri) {
  auto it = find(ri->_var);
  if (it != nullptr) {
    auto it2 = it->find(ri->_attr);
    if (it2 != (*it).end()) {
      it->erase(it2);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone
////////////////////////////////////////////////////////////////////////////////
        
RangeInfoMap* RangeInfoMap::clone () {
  auto rim = new RangeInfoMap();
  
  try { 
    for (auto x: _ranges) {
      for (auto y: x.second) {
        rim->insert(y.second.clone());
      }
    }
  }
  catch (...) {
    delete rim;
    throw;
  }
        
  return rim;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief eraseEmptyOrUndefined remove all empty or undefined RangeInfos for
/// the variable <var> in the RIM
////////////////////////////////////////////////////////////////////////////////

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
/// @brief isValid: are all the range infos for the variable <var> valid?
////////////////////////////////////////////////////////////////////////////////

bool RangeInfoMap::isValid (std::string const& var) {

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

////////////////////////////////////////////////////////////////////////////////
/// @brief attributes: returns a vector of the names of the attributes for the
/// variable var stored in the RIM.
////////////////////////////////////////////////////////////////////////////////

void RangeInfoMap::attributes (std::unordered_set<std::string>& set, 
                               std::string const& var) {
  std::unordered_map<std::string, RangeInfo>* map = find(var);
  if (map != nullptr) {
    for(auto x: *map) {
      set.insert(x.first);
    }
  }
}

std::unordered_set<std::string> RangeInfoMap::variables () {
  std::unordered_set<std::string> vars;
  for(auto x: _ranges) {
    vars.insert(x.first);
  }
  return vars;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief class RangeInfoMapVec
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor: construct RangeInfoMapVec containing a single
/// RangeInfoMap containing a single RangeInfo.
////////////////////////////////////////////////////////////////////////////////

RangeInfoMapVec::RangeInfoMapVec  (RangeInfoMap* rim) :
  _rangeInfoMapVec() {
  
  _rangeInfoMapVec.emplace_back(rim);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RangeInfoMapVec::~RangeInfoMapVec () {
  for (auto x: _rangeInfoMapVec) {
    delete x;
  }
  _rangeInfoMapVec.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief emplace_back: emplace_back RangeInfoMap in vector
////////////////////////////////////////////////////////////////////////////////

void RangeInfoMapVec::emplace_back (RangeInfoMap* rim) {
  _rangeInfoMapVec.emplace_back(rim);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief eraseEmptyOrUndefined remove all empty or undefined RangeInfos for
/// the variable <var> in every RangeInfoMap in the vector 
////////////////////////////////////////////////////////////////////////////////

void RangeInfoMapVec::eraseEmptyOrUndefined(std::string const& var) {
  for (RangeInfoMap* x: _rangeInfoMapVec) {
    x->eraseEmptyOrUndefined(var);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find: this is the same as _rangeInfoMapVec[pos]->find(var), i.e. find
/// the map of RangeInfos for the variable <var>.
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, RangeInfo>* RangeInfoMapVec::find (
                                              std::string const& var, size_t pos) {
  if (pos >= _rangeInfoMapVec.size()) {
    return nullptr;
  }
  return _rangeInfoMapVec[pos]->find(var);
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief isMapped: returns true if <var> is in every RIM in the vector
////////////////////////////////////////////////////////////////////////////////

bool RangeInfoMapVec::isMapped(std::string const& var) {
  for (size_t i = 0; i < _rangeInfoMapVec.size(); i++) {
    if (_rangeInfoMapVec[i]->find(var) == nullptr) {
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validPositions: returns a vector of the positions in the RIM vector
/// that contain valid RangeInfoMap for the variable named var
////////////////////////////////////////////////////////////////////////////////

std::vector<size_t> RangeInfoMapVec::validPositions(std::string const& var) {
  std::vector<size_t> valid;

  for (size_t i = 0; i < _rangeInfoMapVec.size(); i++) {
    if (_rangeInfoMapVec[i]->isValid(var)) {
      valid.push_back(i);
    }
  }
  return valid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attributes: returns the set of names of the attributes for the
/// variable var stored in the RIM vector.
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<std::string> RangeInfoMapVec::attributes (std::string const& var) {
  std::unordered_set<std::string> set;

  for (size_t i = 0; i < size(); i++) {
    _rangeInfoMapVec[i]->attributes(set, var);
  }
  return set;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief differenceRangeInfo: returns the difference of the constant parts of
/// the given RangeInfo and the union of the RangeInfos (for the same var and
/// attr) in the vector. Potentially modifies both the argument and the
/// RangeInfos in the vector.
////////////////////////////////////////////////////////////////////////////////

void RangeInfoMapVec::differenceRangeInfo (RangeInfo& newRi) {
  
  for (auto rim: _rangeInfoMapVec) {
    RangeInfo* oldRi = rim->find(newRi._var, newRi._attr);
    if (oldRi != nullptr) {
      differenceRangeInfos(*oldRi, newRi);
      if (! newRi.isValid() || 
          (newRi._lowConst.bound().isEmpty() && newRi._highConst.bound().isEmpty())){
        break;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief combining range info maps and vectors 
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief orCombineRangeInfoMapVecs: return a new RangeInfoMapVec appending
/// those RIMs in the right arg (which are not identical to an existing RIM) in
/// a copy of the left arg.
////////////////////////////////////////////////////////////////////////////////

RangeInfoMapVec* triagens::aql::orCombineRangeInfoMapVecs (RangeInfoMapVec* lhs, 
                                                           RangeInfoMapVec* rhs) {
 
  if (lhs == nullptr) {
    return rhs;
  }
  
  if (lhs->empty()) {
    delete lhs;
    return rhs;
  }

  if (rhs == nullptr) {
    return lhs;
  }
  
  if (rhs->empty()) {
    delete rhs;
    return lhs;
  }
  
  //avoid inserting overlapping conditions
  for (size_t i = 0; i < rhs->size(); i++) {
    auto rim = new RangeInfoMap();
    try {
      for (auto x: (*rhs)[i]->_ranges) {
        for (auto y: x.second) {
          RangeInfo ri = y.second.clone();
          rim->insert(ri);
        }
      }
      if (! rim->empty()) {
        lhs->emplace_back(rim);
      } 
      else {
        delete rim;
      }
    }
    catch (...) {
      delete rim;
      throw;
    }
  }
  
  delete rhs;
  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineRangeInfoMaps: insert every RangeInfo in the <rhs> in the
/// <lhs> and delete the <rhs>
////////////////////////////////////////////////////////////////////////////////

RangeInfoMap* triagens::aql::andCombineRangeInfoMaps (RangeInfoMap* lhs, RangeInfoMap* rhs) {

  for (auto x: rhs->_ranges) {
    for (auto y: x.second) {
      lhs->insert(y.second.clone());
    }
  }
  delete rhs;
  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineRangeInfoMapVecs: returns a new RangeInfoMapVec by
/// distributing the AND into the ORs in a condition like:
/// (OR condition) AND (OR condition).
///
/// The returned RIMV is new, unless either side is empty or the nullptr, 
/// in which case that side is returned and the other is deleted.
////////////////////////////////////////////////////////////////////////////////

RangeInfoMapVec* triagens::aql::andCombineRangeInfoMapVecs (RangeInfoMapVec* lhs, 
                                                            RangeInfoMapVec* rhs) {
  if (lhs == nullptr || lhs->empty()) {
    if (rhs != nullptr) {
      delete rhs;
    }
    return lhs;
  }

  if (rhs == nullptr || rhs->empty()) {
    if (lhs != nullptr) {
      delete lhs;
    }
    return rhs;
  }

  auto rimv = new RangeInfoMapVec(); // must be a new one!
  try {
    for (size_t i = 0; i < lhs->size(); i++) {
      for (size_t j = 0; j < rhs->size(); j++) {
        rimv->emplace_back(andCombineRangeInfoMaps((*lhs)[i]->clone(), (*rhs)[j]->clone()));
      }
    }
  }
  catch (...) {
    delete rimv;
    throw;
  }
  delete lhs;
  delete rhs;
  return rimv;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison of range infos
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// @brief containmentRangeInfos: check if the constant bounds of a RI are a
// subset of another.  Returns -1 if lhs is a (not necessarily proper) subset of
// rhs, 0 if neither is contained in the other, and, 1 if rhs is contained in
// lhs. Only for range infos with the same variable and attribute
////////////////////////////////////////////////////////////////////////////////

static int containmentRangeInfos (RangeInfo const& lhs, RangeInfo const& rhs) {
  TRI_ASSERT(lhs._var == rhs._var);
  TRI_ASSERT(lhs._attr == rhs._attr);
  
  int LoLo = CompareRangeInfoBound(lhs._lowConst, rhs._lowConst, -1); 
  // -1 if lhs is tighter than rhs, 1 if rhs tighter than lhs
  int HiHi = CompareRangeInfoBound(lhs._highConst, rhs._highConst, 1); 
  // -1 if lhs is tighter than rhs, 1 if rhs tighter than lhs
  // 0 if equal
  if (LoLo == HiHi) {
    return (LoLo == 0 ? 1 : LoLo);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief areDisjointRangeInfos: returns true if the constant parts of lhs and
/// rhs are disjoint and false otherwise.
/// Only for range infos with the same variable and attribute
////////////////////////////////////////////////////////////////////////////////

bool triagens::aql::areDisjointRangeInfos (RangeInfo const& lhs, 
                                           RangeInfo const& rhs) {
  TRI_ASSERT(lhs._var == rhs._var);
  TRI_ASSERT(lhs._attr == rhs._attr);
 
  int HiLo;
  if (lhs._highConst.isDefined() && rhs._lowConst.isDefined()) {
    HiLo = TRI_CompareValuesJson(lhs._highConst.bound().json(), 
        rhs._lowConst.bound().json());
    if ((HiLo == -1) ||
      (HiLo == 0 && (! lhs._highConst.inclusive() ||! rhs._lowConst.inclusive()))) {
      return true;
    }
  } 
  
  //else compare lhs low > rhs high 

  int LoHi;
  if (lhs._lowConst.isDefined() && rhs._highConst.isDefined()) {
    LoHi = TRI_CompareValuesJson(lhs._lowConst.bound().json(), 
        rhs._highConst.bound().json());
    return (LoHi == 1) || 
      (LoHi == 0 && (! lhs._lowConst.inclusive() ||! rhs._highConst.inclusive()));
  } 
  // in this case, either:
  // a) lhs.hi defined and rhs.lo undefined; or
  // b) lhs.hi undefined and rhs.lo defined;
  // 
  // and either:
  //
  // c) lhs.lo defined and rhs.hi undefined
  // d) lhs.lo undefined and rhs.hi defined.
  //
  // a+c) lhs = (x,y) and rhs = (-infty, +infty) -> FALSE
  // a+d) lhs = (-infty, x) and rhs = (-infty, y) -> FALSE
  // b+c) lhs = (x, infty) and rhs = (y, infty) -> FALSE
  // b+d) lhs = (-infty, infty) -> FALSE
  return false; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief differenceRangeInfos: returns the difference of the constant parts of
/// the given RangeInfos. 
///
/// Modifies either lhs or rhs in place, so that the constant parts of lhs 
/// and rhs are disjoint, and the union of the modified lhs and rhs equals the
/// union of the originals.
///
/// Only for range infos with the same variable and attribute
////////////////////////////////////////////////////////////////////////////////

void triagens::aql::differenceRangeInfos (RangeInfo& lhs, RangeInfo& rhs) {
  TRI_ASSERT(lhs._var == lhs._var);
  TRI_ASSERT(lhs._attr == rhs._attr);
  
  if (! (lhs.isConstant() && rhs.isConstant())) {
    return;
  }

  if (! areDisjointRangeInfos(lhs, rhs)) {
    int contained = containmentRangeInfos(lhs, rhs);
    if (contained == -1) { 
      // lhs is a subset of rhs, disregard lhs
      // unassign _lowConst and _highConst
      if (lhs.isConstant()) {
        lhs.invalidate();
      } 
      else {
        RangeInfoBound rib;
        lhs._lowConst.assign(rib);
        lhs._highConst.assign(rib);
      }
    } 
    else if (contained == 1) {
      // rhs is a subset of lhs, disregard rhs
      // unassign _lowConst and _highConst
      if (rhs.isConstant()) {
        rhs.invalidate();
      } 
      else {
        RangeInfoBound rib;
        rhs._lowConst.assign(rib);
        rhs._highConst.assign(rib);
      }
    } 
    else {
      // lhs and rhs have non-empty intersection
      int LoLo = CompareRangeInfoBound(lhs._lowConst, rhs._lowConst, -1);
      if (LoLo == 1) { // replace low bound of new with high bound of old
        rhs._lowConst.assign(lhs._highConst);
        rhs._lowConst.setInclude(! lhs._highConst.inclusive());
      } 
      else { // replace the high bound of the new with the low bound of the old
        rhs._highConst.assign(lhs._lowConst);
        rhs._highConst.setInclude(! lhs._lowConst.inclusive());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison of index "and" conditions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief areDisjointIndexAndConditions: returns true if the arguments describe
/// disjoint sets, and false otherwise.
////////////////////////////////////////////////////////////////////////////////

bool triagens::aql::areDisjointIndexAndConditions (IndexAndCondition& and1, IndexAndCondition& and2) {
  
  for (auto ri1: and1) {
    for (auto ri2: and2) {
      if (ri2._attr == ri1._attr) {
        if (areDisjointRangeInfos(ri1, ri2)) {
          return true;
        }
      }
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief isContainedIndexAndConditions: returns true if the first argument is
/// contained in the second, and false otherwise.
////////////////////////////////////////////////////////////////////////////////

bool triagens::aql::isContainedIndexAndConditions (IndexAndCondition& and1, IndexAndCondition& and2) {
  
  for (auto ri1: and1) {
    bool contained = false;
    for (auto ri2: and2) {
      if (ri2._attr == ri1._attr) {
        if (containmentRangeInfos(ri2, ri1) == 1){
          contained = true;
          break;
        }
      }
    }
    
    if (! contained) {
      return false;
    }
  }    
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief differenceIndexAnd: modifies its args in place 
/// so that the intersection of the sets they describe is empty and their union
/// is the same as if the function was never called. 
////////////////////////////////////////////////////////////////////////////////

void triagens::aql::differenceIndexAnd (IndexAndCondition& and1, IndexAndCondition& and2) {

  if (and1.empty() || and2.empty()) {
    return;
  }

  if (! areDisjointIndexAndConditions(and1, and2)) {
    if (isContainedIndexAndConditions(and1, and2)) { 
      // and1 is a subset of and2, disregard and1
      and1.clear();
    } 
    else if (isContainedIndexAndConditions(and2, and1)) {
      // and2 is a subset of and1, disregard and2
      and2.clear();
    } 
    else {
      // and1 and and2 have non-empty intersection
      for (auto& ri1: and1) {
        for (auto& ri2: and2) {
          if (ri2._attr == ri1._attr && containmentRangeInfos(ri1, ri2) == 0){
            int LoLo = CompareRangeInfoBound(ri1._lowConst, ri2._lowConst, -1);
            if (LoLo == 1) { // replace low bound of new with high bound of old
              ri2._lowConst.assign(ri1._highConst);
              ri2._lowConst.setInclude(! ri1._highConst.inclusive());
            } 
            else { // replace the high bound of the new with the low bound of the old
              ri2._highConst.assign(ri1._lowConst);
              ri2._highConst.setInclude(! ri1._lowConst.inclusive());
            }
          }
        }
      }    
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief differenceIndexAnd: modifies its argument in place so that the index
/// "and" conditions it contains describe disjoint sets. 
////////////////////////////////////////////////////////////////////////////////

void triagens::aql::removeOverlapsIndexOr (IndexOrCondition& ioc) {
  
  // remove invalid  
  for (auto it = ioc.begin(); it < ioc.end(); ) {
    bool invalid = false;
    for (RangeInfo ri: *it) {
      if (! ri.isValid()) {
        invalid = true;
        it = ioc.erase(it);
        break;
      }
    } 
    if (! invalid) { 
      it++;
    }
  }

  for (size_t i = 1; i < ioc.size(); i++) {
    for (size_t j = 0; j < i; j++) {
      differenceIndexAnd(ioc.at(j), ioc.at(i));
    }
  }
  // remove empty 
  for (auto it = ioc.begin(); it < ioc.end(); ) {
    if (it->empty()) {
      it = ioc.erase(it);
    } 
    else {
      it++;
    }
  }
}
