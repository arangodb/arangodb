////////////////////////////////////////////////////////////////////////////////
/// @brief read-accesses internals of a document
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
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "DocumentAccessor.h"
#include "Basics/conversions.h"
#include "Basics/Exceptions.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/VocShaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  DocumentAccessor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

DocumentAccessor::DocumentAccessor (triagens::arango::CollectionNameResolver const* resolver,
                                    TRI_document_collection_t* document,
                                    TRI_doc_mptr_t const* mptr)
  : _resolver(resolver),
    _document(document),
    _mptr(mptr),
    _json(),
    _current(nullptr) {
}

DocumentAccessor::~DocumentAccessor () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

bool DocumentAccessor::hasKey (std::string const& attribute) const {
  if (! isObject()) {
    return false;
  }

  return true;
}

bool DocumentAccessor::isObject () const {
  if (_current != nullptr) {
    return TRI_IsObjectJson(_current);
  }

  // ok, must be a document/edge
  return true;
}

bool DocumentAccessor::isArray () const {
  if (_current != nullptr) {
    return TRI_IsArrayJson(_current);
  }

  // ok, must be a document/edge
  return false;
}

size_t DocumentAccessor::length () const {
  if (! isArray()) {
    return 0;
  }
  // ok, we have confirmed this is an array
  TRI_ASSERT(_current != nullptr);
  if (_current != nullptr) {
    return TRI_LengthArrayJson(_current);
  }
  return 0;
}

DocumentAccessor& DocumentAccessor::get (std::string const& name) {
  return get(name.c_str(), name.size());
}

DocumentAccessor& DocumentAccessor::get (char const* name, size_t nameLength) {
  if (_current == nullptr) {
    // a document, we need the access its attributes using special methods
    lookupDocumentAttribute(name, nameLength);
  }
  else {
    // already a JSON
    lookupJsonAttribute(name, nameLength);
  }

  return *this;
}

DocumentAccessor& DocumentAccessor::at (int64_t index) {
  if (isArray()) {
    size_t length = TRI_LengthArrayJson(_current);

    if (index < 0) {
      // a negative position is allowed
      index = static_cast<int64_t>(length) + index; 
    }

    if (index >= 0 && index < static_cast<int64_t>(length)) {
      // only look up the value if it is within array bounds
      TRI_json_t* found = TRI_LookupArrayJson(_current, static_cast<size_t>(index));

      if (found != nullptr) {
        _current = found;
        return *this;
      }
    }
    // fall-through intentional
  }

  setToNull();
  return *this;
}
    
triagens::basics::Json DocumentAccessor::toJson () {
  if (_current == nullptr) {
    return triagens::basics::Json(triagens::basics::Json::Null);
  }

  // should always be true
  if (_current == _json.get()) {
    // _current points at the JSON that we own
    TRI_json_t* value = _json.release();
    setToNull();
    return triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, value);
  }

  TRI_json_t* copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, _current);

  if (copy != nullptr) {
    _json.release();
    return triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, copy);
  }
  // fall-through intentional

  return triagens::basics::Json(triagens::basics::Json::Null);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

void DocumentAccessor::setToNull () {
  // check if already null
  if (! TRI_IsNullJson(_current)) {
    _json.reset(TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
    _current = _json.get();
  }
}

void DocumentAccessor::lookupJsonAttribute (char const* name, size_t nameLength) {
  TRI_ASSERT(_current != nullptr);

  TRI_json_t* value = TRI_LookupObjectJson(_current, name);

  if (value == nullptr) {
    // attribute not found
    setToNull();
  }
  else {
    // found
    _current = value;
  }
}

void DocumentAccessor::lookupDocumentAttribute (char const* name, size_t nameLength) {
  if (*name == '_' && name[1] != '\0') {
    if (name[1] == 'k' && nameLength == 4 && memcmp(name, TRI_VOC_ATTRIBUTE_KEY, nameLength) == 0) {
      // _key value is copied into JSON
      char const* key = TRI_EXTRACT_MARKER_KEY(_mptr);
      if (key == nullptr) {
        setToNull();
        return;
      }
      _json.reset(TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, key, strlen(key)));
      _current = _json.get();
      return;
    }

    if (name[1] == 'i' && nameLength == 3 && memcmp(name, TRI_VOC_ATTRIBUTE_ID, nameLength) == 0) {
      // _id
      char buffer[512]; // big enough for max key length + max collection name length
      size_t pos = _resolver->getCollectionName(&buffer[0], _document->_info._cid);
      buffer[pos++] = '/';
      char const* key = TRI_EXTRACT_MARKER_KEY(_mptr);
      if (key == nullptr) {
        setToNull();
        return;
      }
      size_t len = strlen(key);
      memcpy(&buffer[pos], key, len);
      buffer[pos + len] = '\0';
      _json.reset(TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, &buffer[0], pos + len));
      _current = _json.get();
      return;
    }

    if (name[1] == 'r' && nameLength == 4 && memcmp(name, TRI_VOC_ATTRIBUTE_REV, nameLength) == 0) {
      // _rev
      char buffer[21];
      TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(_mptr);
      size_t len = TRI_StringUInt64InPlace(rid, &buffer[0]);
      _json.reset(TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, &buffer[0], len));
      _current = _json.get();
      return;
    }

    if (name[1] == 'f' && nameLength == 5 && memcmp(name, TRI_VOC_ATTRIBUTE_FROM, nameLength) == 0) {
      // _from
      char buffer[512]; // big enough for max key length + max collection name length
      size_t pos = _resolver->getCollectionNameCluster(&buffer[0], TRI_EXTRACT_MARKER_FROM_CID(_mptr));
      buffer[pos++] = '/';
      char const* key = TRI_EXTRACT_MARKER_FROM_KEY(_mptr);
      if (key == nullptr) {
        setToNull();
        return;
      }
      size_t len = strlen(key);
      memcpy(&buffer[pos], key, len);
      buffer[pos + len] = '\0';
      _json.reset(TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, &buffer[0], pos + len));
      _current = _json.get();
      return;
    }

    if (name[1] == 't' && nameLength == 3 && memcmp(name, TRI_VOC_ATTRIBUTE_TO, nameLength) == 0) {
      // to
      char buffer[512]; // big enough for max key length + max collection name length
      size_t pos = _resolver->getCollectionNameCluster(&buffer[0], TRI_EXTRACT_MARKER_TO_CID(_mptr));
      buffer[pos++] = '/';
      char const* key = TRI_EXTRACT_MARKER_TO_KEY(_mptr);
      if (key == nullptr) {
        setToNull();
        return;
      }
      size_t len = strlen(key);
      memcpy(&buffer[pos], key, len);
      buffer[pos + len] = '\0';
      _json.reset(TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, &buffer[0], pos + len));
      _current = _json.get();
      return;
    }

    // fall-through intentional
  }

  auto shaper = _document->getShaper();

  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(name);

  if (pid == 0) {
    // attribute does not exist
    setToNull();
    return;
  }

  // attribute exists
  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, _mptr->getDataPtr());

  TRI_shaped_json_t json;
  TRI_shape_t const* shape;

  bool ok = shaper->extractShapedJson(&document, 0, pid, &json, &shape);

  if (ok && shape != nullptr) {
    _json.reset(TRI_JsonShapedJson(shaper, &json));
    _current = _json.get();
    return;
  }

  // not found
  setToNull();
}

