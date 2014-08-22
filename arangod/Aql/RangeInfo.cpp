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
/// If <left> or <right> is a nullptr, this indicates no bound. 
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

static int CompareRangeInfoBound (RangeInfoBound const* left, 
    RangeInfoBound const* right, int lowhigh) {
  if (left == nullptr) {
    return (right == nullptr ? 0 : 1);
  } 
  if (right == nullptr) {
    return -1;
  }

  int cmp = TRI_CompareValuesJson(left->_bound.json(), right->_bound.json());
  if (cmp == 0 && (left->_include != right->_include)) {
    return (left->_include?1:-1);
  }
  return cmp * lowhigh;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief  insert if there is no range corresponding to variable name <var>,
/// and attribute <name>, and otherwise intersection with existing range
////////////////////////////////////////////////////////////////////////////////

void RangesInfo::insert (std::string var, std::string name, 
                         RangeInfoBound* low, RangeInfoBound* high) { 
  auto oldMap = find(var);
  auto newRange = new RangeInfo(low, high);

  if (oldMap == nullptr) {
    // TODO add exception . . .
    auto newMap = std::unordered_map<std::string, RangeInfo*>();
    newMap.insert(make_pair(name, newRange));
    _ranges.insert(std::make_pair(var, newMap));
    return;
  }
  
  auto oldRange = find(var, name); //TODO improve, using oldMap
  
  if (oldRange == nullptr) {
    // TODO add exception . . .
    oldMap->insert(make_pair(name, newRange));
    return;
  }

  if (!oldRange->_valid) { // intersection of the empty set with any set is empty!
    return;
  }
  
  //this case is not covered by those below . . .
  if (oldRange->is1ValueRangeInfo() && newRange->is1ValueRangeInfo()) {
    if (!TRI_CheckSameValueJson(oldRange->_low->_bound.json(), 
        newRange->_low->_bound.json())) {
      oldRange->_valid = false;
      return;
    }
  }

  if (CompareRangeInfoBound(newRange->_low, oldRange->_low, -1) == -1) {
    oldRange->_low = newRange->_low;
  }
  
  if (CompareRangeInfoBound(newRange->_high, oldRange->_high, 1) == -1) {
    oldRange->_high = newRange->_high;
  }

  // check the new range bounds are valid
  if (oldRange->_low != nullptr && oldRange->_high != nullptr) {
    int cmp = TRI_CompareValuesJson(oldRange->_low->_bound.json(), 
            oldRange->_high->_bound.json());
    if (cmp == 1 || (cmp == 0 && 
          !(oldRange->_low->_include == true && oldRange->_high->_include == true ))) {
      // range invalid
      oldRange->_valid = false;
    }
  }
};
