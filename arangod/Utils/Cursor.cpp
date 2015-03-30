////////////////////////////////////////////////////////////////////////////////
/// @brief cursor
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Utils/Cursor.h"
#include "Basics/json.h"
#include "VocBase/vocbase.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                      class Cursor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

Cursor::Cursor (CursorId id,
                size_t batchSize,
                TRI_json_t* extra,
                double ttl, 
                bool hasCount) 
  : _id(id),
    _batchSize(batchSize),
    _position(0),
    _extra(extra),
    _ttl(ttl),
    _expires(TRI_microtime() + _ttl),
    _hasCount(hasCount),
    _isDeleted(false),
    _isUsed(false) {

}
        
Cursor::~Cursor () {
  if (_extra != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _extra);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  class JsonCursor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

JsonCursor::JsonCursor (TRI_vocbase_t* vocbase,
                        CursorId id,
                        TRI_json_t* json,
                        size_t batchSize,
                        TRI_json_t* extra,
                        double ttl, 
                        bool hasCount) 
  : Cursor(id, batchSize, extra, ttl, hasCount),
    _vocbase(vocbase),
    _json(json),
    _size(TRI_LengthArrayJson(_json)) {

  TRI_UseVocBase(vocbase);
}
        
JsonCursor::~JsonCursor () {
  TRI_ReleaseVocBase(_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool JsonCursor::hasNext () {
  if (_position < _size) {
    return true;
  }

  freeJson(); 
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* JsonCursor::next () {
  TRI_ASSERT_EXPENSIVE(_json != nullptr);
  TRI_ASSERT_EXPENSIVE(_position < _size);

  return static_cast<TRI_json_t*>(TRI_AtVector(&_json->_value._objects, _position++));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t JsonCursor::count () const {
  return _size;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief free the internals
////////////////////////////////////////////////////////////////////////////////

void JsonCursor::freeJson () {
  if (_json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _json);
    _json = nullptr;
  }

  _isDeleted = true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
