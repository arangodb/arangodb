////////////////////////////////////////////////////////////////////////////////
/// @brief legends for shaped JSON objects to make them self-contained
///
/// @file
/// Code for legends.
///
/// DISCLAIMER
///
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2014-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ShapedJson/Legends.h"

using namespace std;
using namespace triagens;
using namespace triagens::basics;


void JsonLegend::clear () {
  _have_attribute.clear();
  _attribs.clear();
  _att_data.clear();
  _have_shape.clear();
  _shapes.clear();
  _shape_data.clear();
}

bool JsonLegend::addAttributeId (TRI_shape_aid_t aid) {
  unordered_set<TRI_shape_aid_t>::const_iterator it = _have_attribute.find(aid);
  if (it != _have_attribute.end()) {
    return true;
  }

  char const* p = _shaper->lookupAttributeId(_shaper, aid);
  if (0 == p) {
    return false;
  }

  _have_attribute.insert(aid);
  size_t len = strlen(p);
  _attribs.emplace_back(aid, _att_data.length());
  _att_data.appendText(p, len+1);   // including the zero byte
  return true;
}

bool JsonLegend::addShape (TRI_shape_sid_t sid, 
                           char const* data, uint32_t len) {
  // data can be 0, then no data is associated, note that if the shape
  // contains an inhomogeneous list as one of its subobjects, then the
  // shape legend could be incomplete, because the actual shapes of
  // the subobject(s) are held only in the data and not in the shaper.

  TRI_shape_t const* shape = 0;

  // First the trivial cases:
  if (sid < TRI_FirstCustomShapeIdShaper()) {
    shape = TRI_LookupSidBasicShapeShaper(sid);
  }
  else {
    unordered_set<TRI_shape_sid_t>::const_iterator it = _have_shape.find(sid);
    if (it == _have_shape.end()) {
      TRI_shape_t const* shape = _shaper->lookupShapeId(_shaper, sid);
      if (0 == shape) {
        return false;
      }

      _have_shape.insert(sid);
      Shape sh(sid, _shape_data.length(), shape->_size);
      _shapes.push_back(sh);
      _shape_data.appendText( reinterpret_cast<char const*>(shape),
                              shape->_size );
    }
  }

  // Now we have to add all attribute IDs and all shapes used by this
  // one recursively, note that the data of this object is in a
  // consistent state, such that we can call ourselves recursively.
  if (shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    // Handle a homogeneous list with equal size entries:
    // Subobjects have fixed size, so in particular no subobject can
    // contain any inhomogeneous list as one of its subobjects.
    // ...
  }
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    // Handle a homogeneous list:
    // Only one sid, but one of the subobjects could be an
    // inhomogeneous list. We first scan the shape without data, if this
    // goes well, there was no subshape containing an inhomogeneous
    // list! Otherwise, we have to scan all entries of the list.
    // ...
  }
  else if (shape->_type == TRI_SHAPE_LIST) {
    // Handle an inhomogeneous list:
    // We have to scan recursively all entries of the list since they
    // contain sids in the data area.
    // ...
  }
  else if (shape->_type == TRI_SHAPE_ARRAY) {
    // Handle an array ...
    // Distinguish between fixed size subobjects and variable size
    // subobjects. The fixed ones cannot contain inhomogeneous lists.
    // ...
  }

  return true;
}

size_t JsonLegend::getSize () {
  // Add string pool size and shape pool size and add space for header
  // and tables.
  return 0;
}

void JsonLegend::dump (void* buf) {
  // Dump the resulting legend to a given buffer.

}
