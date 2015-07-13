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
#include "Basics/JsonHelper.h"
#include "ShapedJson/shaped-json.h"
#include "Utils/CollectionExport.h"
#include "VocBase/document-collection.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"

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
                        bool hasCount,
                        bool cached) 
  : Cursor(id, batchSize, extra, ttl, hasCount),
    _vocbase(vocbase),
    _json(json),
    _size(TRI_LengthArrayJson(_json)),
    _cached(cached) {

  TRI_UseVocBase(vocbase);
}
        
JsonCursor::~JsonCursor () {
  freeJson(); 

  TRI_ReleaseVocBase(_vocbase);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the cursor contents into a string buffer
////////////////////////////////////////////////////////////////////////////////
        
void JsonCursor::dump (triagens::basics::StringBuffer& buffer) {
  buffer.appendText("\"result\":[");

  size_t const n = batchSize();

  // reserve 48 bytes per result document by default, but only
  // if the specified batch size does not get out of hand
  // otherwise specifying a very high batch size would make the allocation fail
  // in every case, even if there were much less documents in the collection
  if (n <= 50000) {
    int res = buffer.reserve(n * 48);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  for (size_t i = 0; i < n; ++i) {
    if (! hasNext()) {
      break;
    }

    if (i > 0) {
      buffer.appendChar(',');
    }
    
    auto row = next();
    if (row == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    int res = TRI_StringifyJson(buffer.stringBuffer(), row);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  buffer.appendText("],\"hasMore\":");
  buffer.appendText(hasNext() ? "true" : "false");

  if (hasNext()) {
    // only return cursor id if there are more documents
    buffer.appendText(",\"id\":\"");
    buffer.appendInteger(id());
    buffer.appendText("\"");
  }

  if (hasCount()) {
    buffer.appendText(",\"count\":");
    buffer.appendInteger(static_cast<uint64_t>(count()));
  }

  TRI_json_t const* extraJson = extra();

  if (TRI_IsObjectJson(extraJson)) {
    buffer.appendText(",\"extra\":");
    TRI_StringifyJson(buffer.stringBuffer(), extraJson);
  }

  buffer.appendText(",\"cached\":");
  buffer.appendText(_cached ? "true" : "false");
    
  if (! hasNext()) {
    // mark the cursor as deleted
    this->deleted();
  }
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
// --SECTION--                                                class ExportCursor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

ExportCursor::ExportCursor (TRI_vocbase_t* vocbase,
                            CursorId id,
                            triagens::arango::CollectionExport* ex,
                            size_t batchSize,
                            double ttl, 
                            bool hasCount)
  : Cursor(id, batchSize, nullptr, ttl, hasCount),
    _vocbase(vocbase),
    _ex(ex),
    _size(ex->_documents->size()) {

  TRI_UseVocBase(vocbase);
}
        
ExportCursor::~ExportCursor () {
  delete _ex;
  TRI_ReleaseVocBase(_vocbase);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool ExportCursor::hasNext () {
  if (_ex == nullptr) {
    return false;
  }

  return (_position < _size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element (not implemented)
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* ExportCursor::next () {
  // should not be called directly
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t ExportCursor::count () const {
  return _size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the cursor contents into a string buffer
////////////////////////////////////////////////////////////////////////////////
        
void ExportCursor::dump (triagens::basics::StringBuffer& buffer) {
  TRI_ASSERT(_ex != nullptr);

  TRI_shaper_t* shaper = _ex->_document->getShaper();
  auto const restrictionType = _ex->_restrictions.type;

  buffer.appendText("\"result\":[");

  size_t const n = batchSize();

  for (size_t i = 0; i < n; ++i) {
    if (! hasNext()) {
      break;
    }

    if (i > 0) {
      buffer.appendChar(',');
    }
    
    auto marker = static_cast<TRI_df_marker_t const*>(_ex->_documents->at(_position++));

    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, marker);
    triagens::basics::Json json(shaper->_memoryZone, TRI_JsonShapedJson(shaper, &shaped));

    // append the internal attributes

    // _id, _key, _rev
    char const* key = TRI_EXTRACT_MARKER_KEY(marker);
    std::string id(_ex->_resolver.getCollectionName(_ex->_document->_info._cid));
    id.push_back('/');
    id.append(key);

    json(TRI_VOC_ATTRIBUTE_ID, triagens::basics::Json(id));
    json(TRI_VOC_ATTRIBUTE_REV, triagens::basics::Json(std::to_string(TRI_EXTRACT_MARKER_RID(marker))));
    json(TRI_VOC_ATTRIBUTE_KEY, triagens::basics::Json(key));

    if (TRI_IS_EDGE_MARKER(marker)) {
      // _from
      std::string from(_ex->_resolver.getCollectionNameCluster(TRI_EXTRACT_MARKER_FROM_CID(marker)));
      from.push_back('/');
      from.append(TRI_EXTRACT_MARKER_FROM_KEY(marker));
      json(TRI_VOC_ATTRIBUTE_FROM, triagens::basics::Json(from));
        
      // _to
      std::string to(_ex->_resolver.getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(marker)));
      to.push_back('/');
      to.append(TRI_EXTRACT_MARKER_TO_KEY(marker));
      json(TRI_VOC_ATTRIBUTE_TO, triagens::basics::Json(to));
    }

    if (restrictionType == CollectionExport::Restrictions::RESTRICTION_INCLUDE ||
        restrictionType == CollectionExport::Restrictions::RESTRICTION_EXCLUDE) {
      // only include the specified fields
      // for this we'll modify the JSON that we already have, in place
      // we'll scan through the JSON attributs from left to right and
      // keep all those that we want to keep. we'll overwrite existing
      // other values in the JSON 
      TRI_json_t* obj = json.json();
      TRI_ASSERT(TRI_IsObjectJson(obj));

      size_t const n = TRI_LengthVector(&obj->_value._objects);

      size_t j = 0;
      for (size_t i = 0; i < n; i += 2) {
        auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&obj->_value._objects, i));

        if (! TRI_IsStringJson(key)) {
          continue;
        }

        bool const keyContainedInRestrictions = (_ex->_restrictions.fields.find(key->_value._string.data) != _ex->_restrictions.fields.end());

        if ((restrictionType == CollectionExport::Restrictions::RESTRICTION_INCLUDE && keyContainedInRestrictions) ||
            (restrictionType == CollectionExport::Restrictions::RESTRICTION_EXCLUDE && ! keyContainedInRestrictions)) {
          // include the field
          if (i != j) {
            // steal the key and the value
            void* src = TRI_AddressVector(&obj->_value._objects, i);
            void* dst = TRI_AddressVector(&obj->_value._objects, j);
            memcpy(dst, src, 2 * sizeof(TRI_json_t));
          }
          j += 2;
        }
        else {
          // do not include the field
          // key
          auto src = static_cast<TRI_json_t*>(TRI_AddressVector(&obj->_value._objects, i));
          TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, src);
          // value
          TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, src + 1);
        }
      }

      // finally adjust the length of the patched JSON so the NULL fields at
      // the end will not be dumped
      TRI_SetLengthVector(&obj->_value._objects, j); 
    }
    else {
      // no restrictions
      TRI_ASSERT(restrictionType == CollectionExport::Restrictions::RESTRICTION_NONE);
    }
        
    int res = TRI_StringifyJson(buffer.stringBuffer(), json.json());

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  buffer.appendText("],\"hasMore\":");
  buffer.appendText(hasNext() ? "true" : "false");

  if (hasNext()) {
    // only return cursor id if there are more documents
    buffer.appendText(",\"id\":\"");
    buffer.appendInteger(id());
    buffer.appendText("\"");
  }

  if (hasCount()) {
    buffer.appendText(",\"count\":");
    buffer.appendInteger(static_cast<uint64_t>(count()));
  }

  if (! hasNext()) {
    delete _ex;
    _ex = nullptr;

    // mark the cursor as deleted
    this->deleted();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
