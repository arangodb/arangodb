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
#include "Aql/RangeInfo.h"

using namespace triagens::basics;
using namespace triagens::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief 3-way comparison of the tightness of upper or lower RangeInfoBounds.
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

static int CompareRangeInfoBound (RangeInfoBound left, RangeInfoBound right, 
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
/// @brief class RangeInfo
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor from JSON
////////////////////////////////////////////////////////////////////////////////

RangeInfo::RangeInfo (basics::Json const& json) :
  _var(basics::JsonHelper::checkAndGetStringValue(json.json(), "variable")),
  _attr(basics::JsonHelper::checkAndGetStringValue(json.json(), "attr")),
  _valid(basics::JsonHelper::checkAndGetBooleanValue(json.json(), "valid")),
  _defined(true) {

  Json lows = json.get("low");
  if (! lows.isList()) {
    THROW_INTERNAL_ERROR("low attribute must be a list");
  }
  Json highs = json.get("high");
  if (! highs.isList()) {
    THROW_INTERNAL_ERROR("high attribute must be a list");
  }
  // If an exception is thrown from within these loops, then the
  // vectors _low and _high will be destroyed properly, so no 
  // try/catch is needed.
  for (size_t i = 0; i < lows.size(); i++) {
    _low.emplace_back(lows.at(i));
  }
  for (size_t i = 0; i < highs.size(); i++) {
    _high.emplace_back(highs.at(i));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson for a RangeInfo
////////////////////////////////////////////////////////////////////////////////

Json RangeInfo::toJson () {
  Json item(basics::Json::Array);
  item("variable", Json(_var))
      ("attr", Json(_attr));
  Json lowList(Json::List, _low.size());
  for (auto l : _low) {
    if (l.isDefined()) {
      lowList(l.toJson());
    }
  }
  item("low", lowList);
  Json highList(Json::List, _high.size());
  for (auto h : _high) {
    if (h.isDefined()) {
      highList(h.toJson());
    }
  }
  item("high", highList);
  item("valid", Json(_valid));
  return item;
}
        
////////////////////////////////////////////////////////////////////////////////
/// @brief class RangesInfo
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief  insert if there is no range corresponding to variable name <var>,
/// and attribute <name>, and otherwise intersection with existing range
////////////////////////////////////////////////////////////////////////////////

void RangesInfo::insert (RangeInfo newRange) { 
  
  TRI_ASSERT(newRange.isDefined());

  std::unordered_map<std::string, RangeInfo>* oldMap = find(newRange._var);

  if (oldMap == nullptr) {
    std::unordered_map<std::string, RangeInfo> newMap;
    newMap.insert(make_pair(newRange._attr, newRange));
    _ranges.insert(std::make_pair(newRange._var, newMap));
    return;
  }
  
  auto it = oldMap->find(newRange._attr); 
  
  if (it == oldMap->end()) {
    oldMap->insert(make_pair(newRange._attr, newRange));
    return;
  }

  RangeInfo& oldRange((*it).second);

  if (!oldRange._valid) { // intersection of the empty set with any set is empty!
    return;
  }
  
  // this case is not covered by those below . . .
  if (oldRange.is1ValueRangeInfo() && newRange.is1ValueRangeInfo()) {
    if (!TRI_CheckSameValueJson(oldRange._low[0].bound().json(), 
                                newRange._low[0].bound().json())) {
      oldRange._valid = false;
      return;
    }
  }

  if (CompareRangeInfoBound(newRange._low[0], oldRange._low[0], -1) == -1) {
    oldRange._low[0].assign(newRange._low[0]);
  }
  
  if (CompareRangeInfoBound(newRange._high[0], oldRange._high[0], 1) == -1) {
    oldRange._high[0].assign(newRange._high[0]);
  }

  // check the new range bounds are valid
  if (oldRange._low[0].isDefined() && oldRange._high[0].isDefined()) {
    int cmp = TRI_CompareValuesJson(oldRange._low[0].bound().json(), 
            oldRange._high[0].bound().json());
    if (cmp == 1 || (cmp == 0 && 
          !(oldRange._low[0].inclusive() == true && 
            oldRange._high[0].inclusive() == true ))) {
      // range invalid
      oldRange._valid = false;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief  insert if there is no range corresponding to variable name <var>,
/// and attribute <name>, and otherwise intersection with existing range
////////////////////////////////////////////////////////////////////////////////

void RangesInfo::insert (std::string var, std::string name, 
                         RangeInfoBound low, RangeInfoBound high) { 
  insert(RangeInfo(var, name, low, high));
};

