////////////////////////////////////////////////////////////////////////////////
/// @brief hash array implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Dr. Oreste Costa-Panaia
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2004-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "hash-array.h"

#include "Basics/fasthash.h"
#include "Indexes/HashIndex.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        COMPARISON
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

bool TRI_hash_array_t::isEqualKeyElement (TRI_index_search_value_t const* left,
                                          TRI_index_element_t const* right) const {
  TRI_ASSERT_EXPENSIVE(right->document() != nullptr);

  for (size_t j = 0;  j < _numFields;  ++j) {
    TRI_shaped_json_t* leftJson = &left->_values[j];
    TRI_shaped_sub_t* rightSub = &right->subObjects()[j];

    if (leftJson->_sid != rightSub->_sid) {
      return false;
    }

    auto length = leftJson->_data.length;

    char const* rightData;
    size_t rightLength;
    TRI_InspectShapedSub(rightSub, right->document(), rightData, rightLength);

    if (length != rightLength) {
      return false;
    }

    if (length > 0 && memcmp(leftJson->_data.data, rightData, length) != 0) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_hash_array_t::hashKey (TRI_index_search_value_t const* key) const {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < _numFields;  ++j) {
    // ignore the sid for hashing
    hash = fasthash64(key->_values[j]._data.data, key->_values[j]._data.length,
                      hash);
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_hash_array_t::hashElement (TRI_index_element_t const* element) const {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < _numFields;  j++) {
    char const* data;
    size_t length;
    TRI_InspectShapedSub(&element->subObjects()[j], element->document(), data, length);

    // ignore the sid for hashing
    // only hash the data block
    hash = fasthash64(data, length, hash);
  }

  return hash;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
