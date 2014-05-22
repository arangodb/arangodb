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

////////////////////////////////////////////////////////////////////////////////
/// Data format of a legend in memory:
/// 
/// Rough overview description:
/// 
///   - attribute-ID table
///   - shape table
///   - attribute-ID string data
///   - padding to achieve 8-byte alignment
///   - shape data
///   - padding to achieve 8-byte alignment
/// 
/// Description of the attribute-ID table (actual binary types in 
/// [square brackets]):
/// 
///   - number of entries [TRI_shape_size_t]
///   - each entry consists of:
///   
///       - attribute ID (aid) [TRI_shape_aid_t]
///       - offset to string value, measured from the beginning of the legend
///         [TRI_shape_size_t]
/// 
/// The entries in the attribute-ID table are sorted by ascending order of 
/// shape IDs to allow for binary search if needed.
///
/// Description of the shape table:
/// 
///   - number of entries [TRI_shape_size_t]
///   - each entry consists of:
/// 
///       - shape ID (sid) [TRI_shape_sid_t]
///       - offset to shape data, measured from the beginning of the legend
///         [TRI_shape_size_t]
///       - size in bytes of the shape data for this shape ID [TRI_shape_size_t]
/// 
/// The entries in the shape table are sorted by ascending order of 
/// shape IDs to allow for binary search if needed.
///
/// The strings for the attribute IDs are stored one after another, each 
/// including a terminating zero byte. At the end of the string data follow
/// zero bytes to pad to a total length that is divisible by 8.
/// 
/// The actual entries of the shape data is stored one after another. Alignment
/// for each entry is automatically given by the length of the shape data. At 
/// the end there is padding to make the length of the total legend divisible
/// by 8.
/// 
/// Note that the builtin shapes are never dumped and that proper legends
/// contain all attribute IDs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all data to build a new legend, keep shaper
////////////////////////////////////////////////////////////////////////////////

void JsonLegend::clear () {
  _have_attribute.clear();
  _attribs.clear();
  _att_data.clear();
  _have_shape.clear();
  _shapes.clear();
  _shape_data.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an attribute ID to the legend
////////////////////////////////////////////////////////////////////////////////

int JsonLegend::addAttributeId (TRI_shape_aid_t aid) {
  unordered_set<TRI_shape_aid_t>::const_iterator it = _have_attribute.find(aid);
  if (it != _have_attribute.end()) {
    return TRI_ERROR_NO_ERROR;
  }

  char const* p = _shaper->lookupAttributeId(_shaper, aid);
  if (0 == p) {
    return TRI_ERROR_AID_NOT_FOUND;
  }

  _have_attribute.insert(aid);
  size_t len = strlen(p);
  _attribs.emplace_back(aid, _att_data.length());
  _att_data.appendText(p, len+1);   // including the zero byte
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a shape to the legend
////////////////////////////////////////////////////////////////////////////////

int JsonLegend::addShape (TRI_shape_sid_t sid, 
                          char const* data, uint32_t len) {
  // data can be 0, then no data is associated, note that if the shape
  // contains an inhomogeneous list as one of its subobjects, then the
  // shape legend could be incomplete, because the actual shapes of
  // the subobject(s) are held only in the data and not in the shaper.
  // In this case this method includes all shapes it can but then
  // returns TRI_ERROR_LEGEND_INCOMPLETE.

  int res = TRI_ERROR_NO_ERROR;

  TRI_shape_t const* shape = 0;

  // First the trivial cases:
  if (sid < TRI_FirstCustomShapeIdShaper()) {
    shape = TRI_LookupSidBasicShapeShaper(sid);
  }
  else {
    shape = _shaper->lookupShapeId(_shaper, sid);
    if (0 == shape) {
      return TRI_ERROR_LEGEND_INCOMPLETE;
    }

    unordered_set<TRI_shape_sid_t>::const_iterator it = _have_shape.find(sid);
    if (it == _have_shape.end()) {
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
    // contain any inhomogeneous list as one of its subobjects,
    // therefore we do not have to hand down actual shaped JSON data.
    TRI_homogeneous_sized_list_shape_t const* shape_spec
      = reinterpret_cast<TRI_homogeneous_sized_list_shape_t const*>
                        (shape);
    res = addShape(shape_spec->_sidEntry, 0, 0);
  }
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    // Handle a homogeneous list:
    // Only one sid, but one of the subobjects could be an
    // inhomogeneous list. We first scan the shape without data, if this
    // goes well, there was no subshape containing an inhomogeneous
    // list! Otherwise, we have to scan all entries of the list.
    TRI_homogeneous_list_shape_t const* shape_spec
      = reinterpret_cast<TRI_homogeneous_list_shape_t const*>
                        (shape);
    res = addShape(shape_spec->_sidEntry, 0, 0);
    if (res == TRI_ERROR_LEGEND_INCOMPLETE) {
      // The subdocuments contain inhomogeneous lists, so we have to
      // scan them all:
      res = TRI_ERROR_NO_ERROR;  // just in case the length is 0
      TRI_shape_length_list_t const* len
        = reinterpret_cast<TRI_shape_length_list_t const*>(data);
      TRI_shape_size_t const* offsets
        = reinterpret_cast<TRI_shape_size_t const*>(len+1);
      TRI_shape_length_list_t i;
      for (i = 0;i < *len;i++) {
        res = addShape(shape_spec->_sidEntry, data + offsets[i],
                                              offsets[i+1]-offsets[i]);
        if (res != TRI_ERROR_NO_ERROR) {
          break;
        }
      }
    }
  }
  else if (shape->_type == TRI_SHAPE_LIST) {
    // Handle an inhomogeneous list:
    // We have to scan recursively all entries of the list since they
    // contain sids in the data area.
    TRI_shape_length_list_t const* len
      = reinterpret_cast<TRI_shape_length_list_t const*>(data);
    TRI_shape_sid_t const* sids
      = reinterpret_cast<TRI_shape_sid_t const*>(len+1);
    TRI_shape_size_t const* offsets
      = reinterpret_cast<TRI_shape_size_t const*>(sids + *len);
    TRI_shape_length_list_t i;
    for (i = 0;i < *len;i++) {
      res = addShape(sids[i], data + offsets[i], offsets[i+1]-offsets[i]);
      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  }
  else if (shape->_type == TRI_SHAPE_ARRAY) {
    // Handle an array:
    // Distinguish between fixed size subobjects and variable size
    // subobjects. The fixed ones cannot contain inhomogeneous lists.
    TRI_array_shape_t const* shape_spec
      = reinterpret_cast<TRI_array_shape_t const*> (shape);
    TRI_shape_sid_t const* sids
      = reinterpret_cast<TRI_shape_sid_t const*>(shape_spec+1);
    TRI_shape_aid_t const* aids
      = reinterpret_cast<TRI_shape_aid_t const*>
        (sids + (shape_spec->_fixedEntries + shape_spec->_variableEntries));
    TRI_shape_size_t const* offsets
      = reinterpret_cast<TRI_shape_size_t const*>
        (aids + (shape_spec->_fixedEntries + shape_spec->_variableEntries));
    uint64_t i;
    for (i = 0; res == TRI_ERROR_NO_ERROR && 
                i < shape_spec->_fixedEntries + shape_spec->_variableEntries;
         i++) {
      res = addAttributeId(aids[i]);
    }
    for (i = 0; res == TRI_ERROR_NO_ERROR && i < shape_spec->_fixedEntries; 
         i++) {
      // Fixed size subdocs cannot have inhomogeneous lists as subdocs:
      res = addShape(sids[i], 0, 0);
    }
    for (i = 0; res == TRI_ERROR_NO_ERROR && i < shape_spec->_variableEntries;
         i++) {
      addShape(sids[i + shape_spec->_fixedEntries],
               data + offsets[i], offsets[i+1] - offsets[i]);
    }
  }

  return res;
}

static inline TRI_shape_size_t roundup8 (TRI_shape_size_t x) {
  return (x + 7) - ((x + 7) & 7);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the total size in bytes of the legend
////////////////////////////////////////////////////////////////////////////////

size_t JsonLegend::getSize () const {
  // Add string pool size and shape pool size and add space for header
  // and tables in bytes.
  return   sizeof(TRI_shape_size_t)                 // number of aids
         + sizeof(AttributeId) * _attribs.size()    // aid entries
         + sizeof(TRI_shape_size_t)                 // number of sids
         + sizeof(Shape) * _shapes.size()           // sid entries
         + roundup8(_att_data.length())             // string data, padded
         + roundup8(_shape_data.length());          // shape data, padded
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the legend to the buffer pointed to by buf
////////////////////////////////////////////////////////////////////////////////

void JsonLegend::dump (void* buf) {
  // Dump the resulting legend to a given buffer.

  // First sort the aids in ascending order:
  sort(_attribs.begin(), _attribs.end(), AttributeComparerObject);

  // Then sort the sids in ascending order:
  sort(_shapes.begin(), _shapes.end(), ShapeComparerObject);

  // Total length of table data to add to offsets:
  TRI_shape_size_t socle =   sizeof(TRI_shape_size_t)
                           + sizeof(AttributeId) * _attribs.size()
                           + sizeof(TRI_shape_size_t)
                           + sizeof(Shape) * _shapes.size();

  // Attribute ID table:
  TRI_shape_size_t* p = reinterpret_cast<TRI_shape_size_t*>(buf);
  TRI_shape_size_t i;
  *p++ = _attribs.size();
  AttributeId* a = reinterpret_cast<AttributeId*>(p);
  for (i = 0; i < _attribs.size(); i++) {
    _attribs[i].offset += socle;
    *a++ = _attribs[i];
    _attribs[i].offset -= socle;
  }

  // Add the length of the string data to socle for second table:
  socle += roundup8(_att_data.length());

  // shape table:
  p = reinterpret_cast<TRI_shape_size_t*>(a);
  *p++ = _shapes.size();
  Shape* s = reinterpret_cast<Shape*>(p);
  for (i = 0; i < _shapes.size(); i++) {
    _shapes[i].offset += socle;
    *s++ = _shapes[i];
    _shapes[i].offset -= socle;
  }

  // Attribute ID string data:
  char* c = reinterpret_cast<char*>(s);
  memcpy(c, _att_data.c_str(), _att_data.length());
  i = roundup8(_att_data.length());
  if (i > _att_data.length()) {
    memset( c + _att_data.length(), 0, i-_att_data.length());
  }
  c += i;

  // Shape data:
  memcpy(c, _shape_data.c_str(), _shape_data.length());
  i = roundup8(_shape_data.length());
  if (i > _shape_data.length()) {
    memset( c + _shape_data.length(), 0, i-_shape_data.length());
  }
}
