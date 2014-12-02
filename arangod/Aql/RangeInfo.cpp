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
/// @brief isIdenticalRangeInfo: check if lhs and rhs are identical!
////////////////////////////////////////////////////////////////////////////////

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
  for (auto x: _ranges) {
    for (auto y: x.second) {
      rim->insert(y.second.clone());
    }
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

////////////////////////////////////////////////////////////////////////////////
/// @brief class RangeInfoMapVec
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor: construct RangeInfoMapVec containing a single
/// RangeInfoMap containing a single RangeInfo.
////////////////////////////////////////////////////////////////////////////////

RangeInfoMapVec::RangeInfoMapVec  (RangeInfoMap* rim) :
  _rangeInfoMapVec() {
  
  if (! rim->empty()){
    _rangeInfoMapVec.emplace_back(rim);
  }
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
/// @brief isIdenticalToExisting: returns true if the input RangeInfo is
/// identical to an existing RangeInfo at any position in the vector. 
////////////////////////////////////////////////////////////////////////////////

bool RangeInfoMapVec::isIdenticalToExisting (RangeInfo x) {

  for (auto rim: _rangeInfoMapVec) {
    RangeInfo* ri = rim->find(x._var, x._attr);
    if (ri != nullptr && isIdenticalRangeInfo(*ri, x)){
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief isMapped: returns true if <var> in every RIM in the vector
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
/// @brief attributes: returns a vector of the names of the attributes for the
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
/// @brief orCombineRangeInfoMapVecs: return a new RangeInfoMapVec appending
/// those RIMs in the right arg (which are not identical to an existing RIM) in
/// a copy of the left arg.
///
/// The return RIMV is new unless one of the arguments is empty.
////////////////////////////////////////////////////////////////////////////////

RangeInfoMapVec* triagens::aql::orCombineRangeInfoMapVecs (RangeInfoMapVec* lhs, 
                                                           RangeInfoMapVec* rhs) {
 
  if (lhs->empty()) {
    return rhs;
  }

  if (rhs->empty()) {
    return lhs;
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
        // take the difference of 
        RangeInfo* ri = rimv->differenceRangeInfo(&y.second);
        if (ri != nullptr) { 
          // if ri is nullptr, then y.second is contained in an existing ri 
          rim->insert(*ri);
        }
      }
    }
    if (! rim->empty()) {
      rimv->emplace_back(rim);
    }
  }

  return rimv;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineRangeInfoMaps: insert every RangeInfo in the right argument
/// in the left argument
////////////////////////////////////////////////////////////////////////////////

RangeInfoMap* triagens::aql::andCombineRangeInfoMaps (RangeInfoMap* lhs, RangeInfoMap* rhs) {

  for (auto x: rhs->_ranges) {
    for (auto y: x.second) {
      lhs->insert(y.second.clone());
    }
  }
  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief andCombineRangeInfoMapVecs: return a new RangeInfoMapVec by
/// distributing the AND into the ORs in a condition like:
/// (OR condition) AND (OR condition).
///
/// The return RIMV is the lhs, unless the it is empty or the nullptr, in which case it is the rhs. 
////////////////////////////////////////////////////////////////////////////////

RangeInfoMapVec* triagens::aql::andCombineRangeInfoMapVecs (RangeInfoMapVec* lhs, 
                                                            RangeInfoMapVec* rhs) {
  if (lhs == nullptr || lhs->empty()) {
    return lhs;
  }

  if (rhs == nullptr || rhs->empty()) {
    return rhs;
  }

  auto rimv = new RangeInfoMapVec();

  for (size_t i = 0; i < lhs->size(); i++) {
    for (size_t j = 0; j < rhs->size(); j++) {
      rimv->emplace_back(andCombineRangeInfoMaps((*lhs)[i]->clone(), (*rhs)[j]->clone()));
    }
  }
  delete lhs;
  delete rhs;
  return rimv;
}

// check if the constant bounds of a RI are a subset of another.
// returns -1 if lhs is a (not necessarily proper) subset of rhs, 0 if neither is
// contained in the other, and, 1 if rhs is contained in lhs. 

static int containmentRangeInfos (RangeInfo* lhs, RangeInfo* rhs) {
  
  int LoLo = CompareRangeInfoBound(lhs->_lowConst, rhs->_lowConst, -1); 
  // -1 if lhs is tighter than rhs, 1 if rhs tighter than lhs
  int HiHi = CompareRangeInfoBound(lhs->_highConst, rhs->_highConst, 1); 
  // -1 if lhs is tighter than rhs, 1 if rhs tighter than lhs
  // 0 if equal
  if (LoLo == HiHi) {
    return (LoLo == 0 ? 1 : LoLo);
  }
  return 0;
}

// returns true if the constant parts of lhs and rhs are disjoint and false
// otherwise

static bool areDisjointRangeInfos (RangeInfo* lhs, RangeInfo* rhs) {
 
  int HiLo;
  if (lhs->_highConst.isDefined() && rhs->_lowConst.isDefined()) {
    HiLo = TRI_CompareValuesJson(lhs->_highConst.bound().json(), 
        rhs->_lowConst.bound().json());
    if ((HiLo == -1) ||
      (HiLo == 0 && (! lhs->_highConst.inclusive() ||! rhs->_lowConst.inclusive()))) {
      return true;
    }
  } 
  
  //else compare lhs low > rhs high 

  int LoHi;
  if (lhs->_lowConst.isDefined() && rhs->_highConst.isDefined()) {
    LoHi = TRI_CompareValuesJson(lhs->_lowConst.bound().json(), 
        rhs->_highConst.bound().json());
    return (LoHi == 1) || 
      (LoHi == 0 && (! lhs->_lowConst.inclusive() ||! rhs->_highConst.inclusive()));
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
/// @brief differenceRangeInfo: returns the difference of the constant parts of
/// the given RangeInfo and the union of the RangeInfos (for the same var and
/// attr) in the vector. Returns the nullptr if it is empty. 
////////////////////////////////////////////////////////////////////////////////

RangeInfo* RangeInfoMapVec::differenceRangeInfo (RangeInfo* newRi) {
  
  for (auto rim: _rangeInfoMapVec) {
    RangeInfo* oldRi = rim->find(newRi->_var, newRi->_attr);
    if (oldRi != nullptr && ! areDisjointRangeInfos(oldRi, newRi)) {
      int contained = containmentRangeInfos(oldRi, newRi);
      if (contained == -1) { 
        // oldRi is a subset of newRi, erase the old RI 
        // continuing finding the difference of the new one and existing RIs
        if (oldRi->isConstant()) {
          rim->erase(oldRi);
        } 
        else {
          // unassign _lowConst and _highConst
          RangeInfoBound rib;
          oldRi->_lowConst.assign(rib);
          oldRi->_highConst.assign(rib);
        }
      } 
      else if (contained == 1) {
        // newRi is a subset of oldRi, disregard new
        if (newRi->isConstant()) {
          return nullptr; // i.e. do nothing on return of this function
        } 
        else {
          // unassign _lowConst and _highConst
          RangeInfoBound rib;
          newRi->_lowConst.assign(rib);
          newRi->_highConst.assign(rib);
          return newRi;
        }
      } 
      else {
        // oldRi and newRi have non-empty intersection
        int LoLo = CompareRangeInfoBound(oldRi->_lowConst, newRi->_lowConst, -1);
        if (LoLo == 1) { // replace low bound of new with high bound of old
          newRi->_lowConst.assign(oldRi->_highConst);
          newRi->_lowConst.setInclude(! oldRi->_highConst.inclusive());
        } 
        else { // replace the high bound of the new with the low bound of the old
          newRi->_highConst.assign(oldRi->_lowConst);
          newRi->_highConst.setInclude(! oldRi->_lowConst.inclusive());
        }
      }
    }
  }
  return newRi;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  differenceIndexOrAndRangeInfo: analogue of differenceRangeInfo
////////////////////////////////////////////////////////////////////////////////

static RangeInfo* differenceIndexOrAndRangeInfo (
    IndexOrCondition orCond, RangeInfo* newRi) {

  for (IndexAndCondition andCond: orCond) {
    for (size_t i = 0; i < andCond.size(); i++) {
      RangeInfo oldRi = andCond[i];
      if (! areDisjointRangeInfos(&oldRi, newRi)) {
        int contained = containmentRangeInfos(&oldRi, newRi);
        if (contained == -1) { 
          // oldRi is a subset of newRi, erase the old RI 
          // continuing finding the difference of the new one and existing RIs
          if (oldRi.isConstant()) {
            oldRi.invalidate();
          } 
          else {
            // unassign _lowConst and _highConst
            RangeInfoBound rib;
            oldRi._lowConst.assign(rib);
            oldRi._highConst.assign(rib);
          }
        } 
        else if (contained == 1) {
          // newRi is a subset of oldRi, disregard new
          if (newRi->isConstant()) {
            return nullptr; // i.e. do nothing on return of this function
          } else {
            // unassign _lowConst and _highConst
            RangeInfoBound rib;
            newRi->_lowConst.assign(rib);
            newRi->_highConst.assign(rib);
            return newRi;
          }
        } 
        else {
          // oldRi and newRi have non-empty intersection
          int LoLo = CompareRangeInfoBound(oldRi._lowConst, newRi->_lowConst, -1);
          if (LoLo == 1) { // replace low bound of new with high bound of old
            newRi->_lowConst.assign(oldRi._highConst);
            newRi->_lowConst.setInclude(! oldRi._highConst.inclusive());
          } 
          else { // replace the high bound of the new with the low bound of the old
            newRi->_highConst.assign(oldRi._lowConst);
            newRi->_highConst.setInclude(! oldRi._lowConst.inclusive());
          }
        }
      }
    }
  }
  return newRi;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief  orCombineIndexOrAndIndexAnd: analogue of orCombineRangeInfoMapVecs
////////////////////////////////////////////////////////////////////////////////

void triagens::aql::orCombineIndexOrAndIndexAnd (
    IndexOrCondition* orCond, IndexAndCondition andCond) {
 
  if (orCond->empty()) {
    orCond->push_back(andCond);
    return;
  }

  if (andCond.empty()) {
    return;
  }

  //avoid inserting overlapping ranges
  IndexAndCondition newAnd;
  for (RangeInfo x: andCond) {
    RangeInfo* ri = differenceIndexOrAndRangeInfo(*orCond, &x);
    if (ri != nullptr) { 
      // if ri is nullptr, then y.second is contained in an existing ri 
      newAnd.emplace_back(*ri);
    }
  }

  if (! newAnd.empty()) {
    orCond->emplace_back(newAnd);
  }
}


