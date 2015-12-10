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

  TRI_ASSERT(mptr != nullptr);
}

DocumentAccessor::DocumentAccessor (TRI_json_t const* json)
  : _resolver(nullptr),
    _document(nullptr),
    _mptr(nullptr),
    _json(),
    _current(json) {

  TRI_ASSERT(_current != nullptr);
}

DocumentAccessor::DocumentAccessor (VPackSlice const& slice)
  : _resolver(nullptr),
    _document(nullptr),
    _mptr(nullptr),
    _json(),
    _current(nullptr) {

  _current = triagens::basics::VelocyPackHelper::velocyPackToJson(slice);
  TRI_ASSERT(_current != nullptr);
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

  if (_current == nullptr) {
    if (attribute.size() > 1 && attribute[0] == '_') {
      if (attribute == TRI_VOC_ATTRIBUTE_ID ||
          attribute == TRI_VOC_ATTRIBUTE_KEY ||
          attribute == TRI_VOC_ATTRIBUTE_REV) {
        return true;
      }
      
      if (TRI_IS_EDGE_MARKER(_mptr) &&
          (attribute == TRI_VOC_ATTRIBUTE_FROM ||
          attribute == TRI_VOC_ATTRIBUTE_TO)) {
        return true;
      }
    }

    auto shaper = _document->getShaper();

    TRI_shape_pid_t pid = shaper->lookupAttributePathByName(attribute.c_str());

    if (pid != 0) {
      return true;
    }

    return false;
  }

  return (TRI_LookupObjectJson(_current, attribute.c_str()) != nullptr);
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
      TRI_json_t const* found = TRI_LookupArrayJson(_current, static_cast<size_t>(index));

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
    // we're still pointing to the original document
    auto shaper = _document->getShaper();

    // fetch document from mptr
    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, _mptr->getDataPtr());
    triagens::basics::Json json(shaper->memoryZone(), TRI_JsonShapedJson(shaper, &shaped));

    // add internal attributes

    // _id, _key, _rev
    char const* key = TRI_EXTRACT_MARKER_KEY(_mptr);
    std::string id(_resolver->getCollectionName(_document->_info._cid));
    id.push_back('/');
    id.append(key);
    json(TRI_VOC_ATTRIBUTE_ID, triagens::basics::Json(id));
    json(TRI_VOC_ATTRIBUTE_REV, triagens::basics::Json(std::to_string(TRI_EXTRACT_MARKER_RID(_mptr))));
    json(TRI_VOC_ATTRIBUTE_KEY, triagens::basics::Json(key));
      
    if (TRI_IS_EDGE_MARKER(_mptr)) {
      // _from
      std::string from(_resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_FROM_CID(_mptr)));
      from.push_back('/');
      from.append(TRI_EXTRACT_MARKER_FROM_KEY(_mptr));
      json(TRI_VOC_ATTRIBUTE_FROM, triagens::basics::Json(from));
        
      // _to
      std::string to(_resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(_mptr)));
      to.push_back('/');
      to.append(TRI_EXTRACT_MARKER_TO_KEY(_mptr));
      json(TRI_VOC_ATTRIBUTE_TO, triagens::basics::Json(to));
    }

    return json;
  }

  if (_current == _json.get()) {
    // _current points at the JSON that we own

    // steal the JSON
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
  if (_current != nullptr && _current->_type == TRI_JSON_NULL) {
    // already null. done!
    return;
  }

  _json.reset(TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
  if (_json.get() == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  _current = _json.get();
  TRI_ASSERT(_current != nullptr);
}

void DocumentAccessor::lookupJsonAttribute (char const* name, size_t nameLength) {
  TRI_ASSERT(_current != nullptr);

  if (! isObject()) {
    setToNull();
    return;
  }

  TRI_json_t const* value = TRI_LookupObjectJson(_current, name);

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

      if (_json.get() == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

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
      if (_json.get() == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      _current = _json.get();
      return;
    }

    if (name[1] == 'r' && nameLength == 4 && memcmp(name, TRI_VOC_ATTRIBUTE_REV, nameLength) == 0) {
      // _rev
      char buffer[21];
      TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(_mptr);
      size_t len = TRI_StringUInt64InPlace(rid, &buffer[0]);
      _json.reset(TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, &buffer[0], len));
      if (_json.get() == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
      _current = _json.get();
      return;
    }

    if (TRI_IS_EDGE_MARKER(_mptr)) {
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
        if (_json.get() == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
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
        if (_json.get() == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
        }
        _current = _json.get();
        return;
      }
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
    if (_json.get() == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    _current = _json.get();
    return;
  }

  // not found
  setToNull();
}

