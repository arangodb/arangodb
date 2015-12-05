////////////////////////////////////////////////////////////////////////////////
/// @brief AQL value
///
/// @file arangod/Aql/AqlValue.cpp
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlValue.h"
#include "Aql/AqlItemBlock.h"
#include "Basics/json-utilities.h"
#include "Utils/ShapedJsonTransformer.h"
#include "V8/v8-conv.h"
#include "V8Server/v8-shape-conv.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "VocBase/VocShaper.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is true
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isTrue () const {
  if (_type == JSON) {
    TRI_json_t* json = _json->json();
    if (TRI_IsBooleanJson(json) && json->_value._boolean) {
      return true;
    }
    else if (TRI_IsNumberJson(json) && json->_value._number != 0.0) {
      return true;
    }
    else if (TRI_IsStringJson(json) && json->_value._string.length != 1) {
      // the trailing NULL byte counts, too...
      return true;
    }
    else if (TRI_IsArrayJson(json) || TRI_IsObjectJson(json)) {
      return true;
    }
  }
  else if (_type == RANGE || _type == DOCVEC) {
    // a range or a docvec is equivalent to an array
    return true;
  }
  else if (_type == EMPTY) {
    return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy, explicit destruction, only when needed
////////////////////////////////////////////////////////////////////////////////

void AqlValue::destroy () {
  switch (_type) {
    case JSON: {
      delete _json;
      _json = nullptr;
      break;
    }
    case DOCVEC: {
      if (_vector != nullptr) {
        for (auto it = _vector->begin(); it != _vector->end(); ++it) {
          delete *it;
        }
        delete _vector;
        _vector = nullptr;
      }
      break;
    }
    case RANGE: {
      delete _range;
      _range = nullptr;
      break;
    }
    case SHAPED: {
      // do nothing here, since data pointers need not be freed
      break;
    }
    case EMPTY: {
      // do nothing
      break;
    }
  }
 
  // to avoid double freeing     
  _type = EMPTY;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name of an AqlValue type
////////////////////////////////////////////////////////////////////////////////
      
std::string AqlValue::getTypeString () const {
  switch (_type) {
    case JSON: 
      return std::string("json (") + std::string(TRI_GetTypeStringJson(_json->json())) + std::string(")");
    case SHAPED: 
      return "shaped";
    case DOCVEC: 
      return "docvec";
    case RANGE: 
      return "range";
    case EMPTY: 
      return "empty";
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone for recursive copying
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::clone () const {
  switch (_type) {
    case JSON: {
      return AqlValue(new Json(_json->copy()));
    }

    case SHAPED: {
      return AqlValue(_marker);
    }

    case DOCVEC: {
      auto c = new std::vector<AqlItemBlock*>;
      try {
        c->reserve(_vector->size());
        for (auto it = _vector->begin(); it != _vector->end(); ++it) {
          c->emplace_back((*it)->slice(0, (*it)->size()));
        }
      }
      catch (...) {
        for (auto x : *c) {
          delete x;
        }
        delete c;
        throw;
      }
      return AqlValue(c);
    }

    case RANGE: {
      return AqlValue(_range->_low, _range->_high);
    }

    case EMPTY: {
      return AqlValue();
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shallow clone
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::shallowClone () const {
  if (_type == JSON) {
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, _json->json(), Json::NOFREE));
  }

  // fallback to regular deep-copying
  return clone();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a string value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isString () const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      return TRI_IsStringJson(json);
    }

    case SHAPED: 
    case DOCVEC: 
    case RANGE: 
    case EMPTY: {
      return false;
    }
  }

  TRI_ASSERT(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a numeric value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isNumber () const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      return TRI_IsNumberJson(json);
    }

    case SHAPED: 
    case DOCVEC: 
    case RANGE: 
    case EMPTY: {
      return false;
    }
  }

  TRI_ASSERT(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a boolean value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isBoolean () const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      return TRI_IsBooleanJson(json);
    }

    case SHAPED: 
    case DOCVEC: 
    case RANGE: 
    case EMPTY: {
      return false;
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains an array value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isArray () const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      return TRI_IsArrayJson(json);
    }

    case SHAPED: {
      return false;
    }

    case DOCVEC: 
    case RANGE: {
      return true;
    }

    case EMPTY: {
      return false;
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains an object value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isObject () const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      return TRI_IsObjectJson(json);
    }

    case SHAPED: {
      return true;
    }

    case DOCVEC: 
    case RANGE: 
    case EMPTY: {
      return false;
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a null value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isNull (bool emptyIsNull) const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      return json == nullptr || json->_type == TRI_JSON_NULL;
    }

    case SHAPED: {
      return false;
    }

    case DOCVEC: 
    case RANGE: {
      return false;
    }

    case EMPTY: {
      return emptyIsNull;
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the array member at position i
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json AqlValue::at (triagens::arango::AqlTransaction* trx, 
                                     size_t i) const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      if (TRI_IsArrayJson(json)) {
        if (i < TRI_LengthArrayJson(json)) {
          return triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, static_cast<TRI_json_t*>(TRI_AtVector(&json->_value._objects, i)), triagens::basics::Json::NOFREE);
        }
      }
      break; // fall-through to exception
    }

    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);
      // calculate the result list length
      size_t offset = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        auto current = (*it);
        size_t const n = current->size();

        if (offset + i < n) {
          auto vecCollection = current->getDocumentCollection(0);

          return current->getValue(i - offset, 0).toJson(trx, vecCollection, true);
        }

        offset += (*it)->size();
      }
      break; // fall-through to exception
    }

    case RANGE: {
      TRI_ASSERT(_range != nullptr);
      return triagens::basics::Json(static_cast<double>(_range->at(i)));
    }
       
    case SHAPED: 
    case EMPTY: {
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of an AqlValue containing an array
////////////////////////////////////////////////////////////////////////////////

size_t AqlValue::arraySize () const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();

      if (TRI_IsArrayJson(json)) {
        return TRI_LengthArrayJson(json);
      }
      return 0;
    }

    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);
      // calculate the result list length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        totalSize += (*it)->size();
      }
      return totalSize;
    }

    case RANGE: {
      TRI_ASSERT(_range != nullptr);
      return _range->size();
    }
       
    case SHAPED: 
    case EMPTY: {
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the numeric value of an AqlValue
////////////////////////////////////////////////////////////////////////////////

int64_t AqlValue::toInt64 () const {
  switch (_type) {
    case JSON: 
      return TRI_ToInt64Json(_json->json());
    case RANGE: {
      size_t rangeSize = _range->size();
      if (rangeSize == 1) {  
        return _range->at(0);
      }
      return 0;
    }
    case DOCVEC: 
    case SHAPED: 
    case EMPTY: 
      // cannot convert these types
      return 0;
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the numeric value of an AqlValue
////////////////////////////////////////////////////////////////////////////////

double AqlValue::toNumber (bool& failed) const {
  switch (_type) {
    case JSON: 
      return TRI_ToDoubleJson(_json->json(), failed);
    case RANGE: {
      size_t rangeSize = _range->size();
      if (rangeSize == 1) {  
        return static_cast<double>(_range->at(0));
      }
      return 0.0;
    }
    case DOCVEC: 
    case SHAPED: 
    case EMPTY: 
      // cannot convert these types
      return 0.0;
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a string representation of the AqlValue
////////////////////////////////////////////////////////////////////////////////

std::string AqlValue::toString () const {
  switch (_type) {
    case JSON: {
      TRI_json_t const* json = _json->json();
      TRI_ASSERT(TRI_IsStringJson(json));
      return std::string(json->_value._string.data, json->_value._string.length - 1);
    }

    case SHAPED: 
    case DOCVEC: 
    case RANGE: 
    case EMPTY: {
      // cannot convert these types
    }
  }

  return std::string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
/// only construct those attributes that are needed in the expression
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> AqlValue::toV8Partial (v8::Isolate* isolate,
                                             triagens::arango::AqlTransaction* trx, 
                                             std::unordered_set<std::string> const& attributes,
                                             TRI_document_collection_t const* document) const {
  TRI_ASSERT_EXPENSIVE(_type == JSON);

  TRI_json_t const* json = _json->json();

  if (TRI_IsObjectJson(json)) {
    size_t const n = TRI_LengthVector(&json->_value._objects);

    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    // only construct those attributes needed
    size_t left = attributes.size();

    // we can only have got here if we had attributes
    TRI_ASSERT(left > 0);

    // iterate over all the object's attributes
    for (size_t i = 0; i < n; i += 2) {
      TRI_json_t const* key = static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, i));

      if (! TRI_IsStringJson(key)) { 
        continue;
      }
   
      // check if we need to render this attribute
      auto it = attributes.find(std::string(key->_value._string.data, key->_value._string.length - 1));

      if (it == attributes.end()) {
        // we can skip the attribute
        continue;
      }

      TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AddressVector(&json->_value._objects, (i + 1)));

      if (value != nullptr) {
        result->ForceSet(TRI_V8_STD_STRING((*it)), TRI_ObjectJson(isolate, value));
      }

      if (--left == 0) {
        // we have rendered all required attributes
        break;
      }
    }

    // return partial object
    return result;
  }

  // fallback    
  return TRI_ObjectJson(isolate, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> AqlValue::toV8 (v8::Isolate* isolate,
                                      triagens::arango::AqlTransaction* trx, 
                                      TRI_document_collection_t const* document) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      return TRI_ObjectJson(isolate, _json->json());
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);
      return TRI_WrapShapedJson<triagens::arango::AqlTransaction>(isolate, *trx, document->_info._cid, _marker);
    }

    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);

      // calculate the result array length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        totalSize += (*it)->size();
      }

      // allocate the result array
      v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(totalSize));
      uint32_t j = 0; // output row count

      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        auto current = (*it);
        size_t const n = current->size();
        auto vecCollection = current->getDocumentCollection(0);
        for (size_t i = 0; i < n; ++i) {
          result->Set(j++, current->getValueReference(i, 0).toV8(isolate, trx, vecCollection));
        }
      }
      return result;
    }

    case RANGE: {
      TRI_ASSERT(_range != nullptr);

      // allocate the buffer for the result
      size_t const n = _range->size();
      v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));
      
      for (uint32_t i = 0; i < n; ++i) {
        // is it safe to use a double here (precision loss)?
        result->Set(i, v8::Number::New(isolate, static_cast<double>(_range->at(static_cast<size_t>(i)))));
      }

      return result;
    }

    case EMPTY: {
      return v8::Null(isolate); // TODO: FIXME decide if we really want this...---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
      return v8::Undefined(isolate);
    }
  }
      
  // should never get here
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson method
////////////////////////////////////////////////////////////////////////////////
      
Json AqlValue::toJson (triagens::arango::AqlTransaction* trx,
                       TRI_document_collection_t const* document, 
                       bool copy) const {
  switch (_type) {
    case JSON: {
      if (copy) {
        return _json->copy();
      }
      return Json(_json->zone(), _json->json(), Json::NOFREE);
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);

      auto shaper = document->getShaper();
      return TRI_ExpandShapedJson(
        shaper,
        trx->resolver(),
        document->_info._cid,
        _marker
      );
    }
          
    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);

      // calculate the result array length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        totalSize += (*it)->size();
      }

      // allocate the result array
      Json json(Json::Array, static_cast<size_t>(totalSize));

      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        auto current = (*it);
        size_t const n = current->size();
        auto vecCollection = current->getDocumentCollection(0);
        for (size_t i = 0; i < n; ++i) {
          json.add(current->getValueReference(i, 0).toJson(trx, vecCollection, true));
        }
      }

      return json;
    }
          
    case RANGE: {
      TRI_ASSERT(_range != nullptr);

      // allocate the buffer for the result
      size_t const n = _range->size();
      Json json(Json::Array, n);

      for (size_t i = 0; i < n; ++i) {
        // is it safe to use a double here (precision loss)?
        json.add(Json(static_cast<double>(_range->at(i))));
      }

      return json;
    }

    case EMPTY: {
      return Json(Json::Null); // TODO FIXME: decide if we really want this...--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
      return triagens::basics::Json();
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the JSON contents
////////////////////////////////////////////////////////////////////////////////
      
uint64_t AqlValue::hash (triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* document) const {
  switch (_type) {
    case JSON: {
      return TRI_FastHashJson(_json->json());
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);

      auto shaper = document->getShaper();
      TRI_shaped_json_t shaped;
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, _marker);
      Json json(shaper->memoryZone(), TRI_JsonShapedJson(shaper, &shaped));

      // append the internal attributes

      // _id, _key, _rev
      char const* key = TRI_EXTRACT_MARKER_KEY(_marker);
      std::string id(trx->resolver()->getCollectionName(document->_info._cid));
      id.push_back('/');
      id.append(key);
      json(TRI_VOC_ATTRIBUTE_ID, Json(id));
      json(TRI_VOC_ATTRIBUTE_REV, Json(std::to_string(TRI_EXTRACT_MARKER_RID(_marker))));
      json(TRI_VOC_ATTRIBUTE_KEY, Json(key));

      if (TRI_IS_EDGE_MARKER(_marker)) {
        // _from
        std::string from(trx->resolver()->getCollectionNameCluster(TRI_EXTRACT_MARKER_FROM_CID(_marker)));
        from.push_back('/');
        from.append(TRI_EXTRACT_MARKER_FROM_KEY(_marker));
        json(TRI_VOC_ATTRIBUTE_FROM, Json(from));
        
        // _to
        std::string to(trx->resolver()->getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(_marker)));
        to.push_back('/');
        to.append(TRI_EXTRACT_MARKER_TO_KEY(_marker));
        json(TRI_VOC_ATTRIBUTE_TO, Json(to));
      }

      return TRI_FastHashJson(json.json());
    }
          
    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);

      // calculate the result array length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        totalSize += (*it)->size();
      }

      // allocate the result array
      Json json(Json::Array, static_cast<size_t>(totalSize));

      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        auto current = (*it);
        size_t const n = current->size();
        auto vecCollection = current->getDocumentCollection(0);

        for (size_t i = 0; i < n; ++i) {
          json.add(current->getValueReference(i, 0).toJson(trx, vecCollection, true));
        }
      }

      return TRI_FastHashJson(json.json());
    }
          
    case RANGE: {
      TRI_ASSERT(_range != nullptr);

      // allocate the buffer for the result
      size_t const n = _range->size();
      Json json(Json::Array, n);

      for (size_t i = 0; i < n; ++i) {
        // is it safe to use a double here (precision loss)?
        json.add(Json(static_cast<double>(_range->at(i))));
      }

      return TRI_FastHashJson(json.json());
    }

    case EMPTY: {
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an attribute value from the AqlValue 
/// this will return an empty Json if the value is not an object
////////////////////////////////////////////////////////////////////////////////

Json AqlValue::extractObjectMember (triagens::arango::AqlTransaction* trx,
                                    TRI_document_collection_t const* document,
                                    char const* name,
                                    bool copy,
                                    triagens::basics::StringBuffer& buffer) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      TRI_json_t const* json = _json->json();

      if (TRI_IsObjectJson(json)) {
        TRI_json_t const* found = TRI_LookupObjectJson(json, name);

        if (found != nullptr) {
          if (copy) {
            // return a copy of the value
            return Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, found), triagens::basics::Json::AUTOFREE);
          }

          // return a pointer to the original value, without asking for its ownership
          return Json(TRI_UNKNOWN_MEM_ZONE, found, triagens::basics::Json::NOFREE);
        }
      }
      
      // attribute does not exist or something went wrong - fall-through to returning null below
      return Json(Json::Null);
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);

      // look for the attribute name in the shape
      if (*name == '_' && name[1] != '\0') {
        if (name[1] == 'k' && strcmp(name, TRI_VOC_ATTRIBUTE_KEY) == 0) {
          // _key value is copied into JSON
          return Json(TRI_UNKNOWN_MEM_ZONE, TRI_EXTRACT_MARKER_KEY(_marker));
        }
        if (name[1] == 'i' && strcmp(name, TRI_VOC_ATTRIBUTE_ID) == 0) {
          // _id
          buffer.reset();
          trx->resolver()->getCollectionName(document->_info._cid, buffer);
          buffer.appendChar('/');
          buffer.appendText(TRI_EXTRACT_MARKER_KEY(_marker));
          return Json(TRI_UNKNOWN_MEM_ZONE, buffer.c_str(), buffer.length());
        }
        if (name[1] == 'r' && strcmp(name, TRI_VOC_ATTRIBUTE_REV) == 0) {
          // _rev
          TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(_marker);
          buffer.reset();
          buffer.appendInteger(rid);
          return Json(TRI_UNKNOWN_MEM_ZONE, buffer.c_str(), buffer.length());
        }
        if (name[1] == 'f' && 
            strcmp(name, TRI_VOC_ATTRIBUTE_FROM) == 0 &&
            (_marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
             _marker->_type == TRI_WAL_MARKER_EDGE)) {
          buffer.reset();
          trx->resolver()->getCollectionNameCluster(TRI_EXTRACT_MARKER_FROM_CID(_marker), buffer);
          buffer.appendChar('/');
          buffer.appendText(TRI_EXTRACT_MARKER_FROM_KEY(_marker));
          return Json(TRI_UNKNOWN_MEM_ZONE, buffer.c_str(), buffer.length());
        }
        if (name[1] == 't' && 
            strcmp(name, TRI_VOC_ATTRIBUTE_TO) == 0 &&
            (_marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
            _marker->_type == TRI_WAL_MARKER_EDGE)) {
          buffer.reset();
          trx->resolver()->getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(_marker), buffer);
          buffer.appendChar('/');
          buffer.appendText(TRI_EXTRACT_MARKER_TO_KEY(_marker));
          return Json(TRI_UNKNOWN_MEM_ZONE, buffer.c_str(), buffer.length());
        }
      }

      auto shaper = document->getShaper();

      TRI_shape_pid_t pid = shaper->lookupAttributePathByName(name);

      if (pid != 0) {
        // attribute exists
        TRI_shaped_json_t document;
        TRI_EXTRACT_SHAPED_JSON_MARKER(document, _marker);

        TRI_shaped_json_t json;
        TRI_shape_t const* shape;

        bool ok = shaper->extractShapedJson(&document, 0, pid, &json, &shape);

        if (ok && shape != nullptr) {
          return Json(TRI_UNKNOWN_MEM_ZONE, TRI_JsonShapedJson(shaper, &json));
        }
      }

      // attribute does not exist or something went wrong - fall-through to returning null
      break;
    }

    case DOCVEC:
    case RANGE:
    case EMPTY: {
      break;
    }
  }

  return Json(Json::Null);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a value from an array AqlValue 
/// this will return null if the value is not an array
/// depending on the last parameter, the return value will either contain a
/// copy of the original value in the array or a reference to it (which must
/// not be freed)
////////////////////////////////////////////////////////////////////////////////

Json AqlValue::extractArrayMember (triagens::arango::AqlTransaction* trx,
                                   TRI_document_collection_t const* document,
                                   int64_t position,
                                   bool copy) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      TRI_json_t const* json = _json->json();

      if (TRI_IsArrayJson(json)) {
        size_t const length = TRI_LengthArrayJson(json);
        if (position < 0) {
          // a negative position is allowed
          position = static_cast<int64_t>(length) + position; 
        }
        
        if (position >= 0 && position < static_cast<int64_t>(length)) {
          // only look up the value if it is within array bounds
          TRI_json_t const* found = TRI_LookupArrayJson(json, static_cast<size_t>(position));

          if (found != nullptr) {
            if (copy) {
              // return a copy of the value
              return Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, found), triagens::basics::Json::AUTOFREE);
            }

            // return a pointer to the original value, without asking for its ownership
            return Json(TRI_UNKNOWN_MEM_ZONE, found, triagens::basics::Json::NOFREE);
          }
        }
      }
      
      // attribute does not exist or something went wrong - fall-through to returning null below
      return Json(Json::Null);
    }

    case RANGE: {
      TRI_ASSERT(_range != nullptr);
      size_t const n = _range->size();

      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }

      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        return Json(static_cast<double>(_range->at(static_cast<size_t>(position))));
      }
      break; // fall-through to returning null 
    }
    
    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);
      size_t const p = static_cast<size_t>(position);

      // calculate the result array length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        if (p < totalSize + (*it)->size()) {
          // found the correct vector
          auto vecCollection = (*it)->getDocumentCollection(0);
          return (*it)->getValueReference(p - totalSize, 0).toJson(trx, vecCollection, copy);
        }
        totalSize += (*it)->size();
      }
      break; // fall-through to returning null
    }

    case SHAPED: 
    case EMPTY: {
      break; // fall-through to returning null
    }
  }

  return Json(Json::Null);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::CreateFromBlocks (triagens::arango::AqlTransaction* trx,
                                     std::vector<AqlItemBlock*> const& src,
                                     std::vector<std::string> const& variableNames) {
  size_t totalSize = 0;

  for (auto it = src.begin(); it != src.end(); ++it) {
    totalSize += (*it)->size();
  }

  std::unique_ptr<Json> json(new Json(Json::Array, totalSize));

  for (auto it = src.begin(); it != src.end(); ++it) {
    auto current = (*it);
    RegisterId const n = current->getNrRegs();
    
    std::vector<std::pair<RegisterId, TRI_document_collection_t const*>> registers;
    for (RegisterId j = 0; j < n; ++j) {
      // temporaries don't have a name and won't be included
      if (variableNames[j][0] != '\0') {
        registers.emplace_back(std::make_pair(j, current->getDocumentCollection(j)));
      }
    }

    for (size_t i = 0; i < current->size(); ++i) {
      Json values(Json::Object, registers.size());

      // only enumerate the registers that are left
      for (auto const& reg : registers) {
        values.set(variableNames[reg.first], current->getValueReference(i, reg.first).toJson(trx, reg.second, true));
      }

      json->add(values);
    }
  }

  return AqlValue(json.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::CreateFromBlocks (triagens::arango::AqlTransaction* trx,
                                     std::vector<AqlItemBlock*> const& src,
                                     triagens::aql::RegisterId expressionRegister) {
  size_t totalSize = 0;

  for (auto it = src.begin(); it != src.end(); ++it) {
    totalSize += (*it)->size();
  }

  std::unique_ptr<Json> json(new Json(Json::Array, totalSize));

  for (auto it = src.begin(); it != src.end(); ++it) {
    auto current = (*it);
    auto document = current->getDocumentCollection(expressionRegister); 

    for (size_t i = 0; i < current->size(); ++i) {
      json->add(current->getValueReference(i, expressionRegister).toJson(trx, document, true));
    }
  }

  return AqlValue(json.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief 3-way comparison for AqlValue objects
////////////////////////////////////////////////////////////////////////////////

int AqlValue::Compare (triagens::arango::AqlTransaction* trx,
                       AqlValue const& left,  
                       TRI_document_collection_t const* leftcoll,
                       AqlValue const& right, 
                       TRI_document_collection_t const* rightcoll,
                       bool compareUtf8) {
  if (left._type != right._type) {
    if (left._type == AqlValue::EMPTY) {
      return -1;
    }

    if (right._type == AqlValue::EMPTY) {
      return 1;
    }

    // JSON against x
    if (left._type == AqlValue::JSON && 
        (right._type == AqlValue::SHAPED ||
         right._type == AqlValue::RANGE ||
         right._type == AqlValue::DOCVEC)) {
        triagens::basics::Json rjson = right.toJson(trx, rightcoll, false);
      return TRI_CompareValuesJson(left._json->json(), rjson.json(), compareUtf8);
    }
    
    // SHAPED against x
    if (left._type == AqlValue::SHAPED) {
      triagens::basics::Json ljson = left.toJson(trx, leftcoll, false);

      if (right._type == AqlValue::JSON) {
        return TRI_CompareValuesJson(ljson.json(), right._json->json(), compareUtf8);
      }
      else if (right._type == AqlValue::RANGE ||
               right._type == AqlValue::DOCVEC) {
        triagens::basics::Json rjson = right.toJson(trx, rightcoll, false);
        return TRI_CompareValuesJson(ljson.json(), rjson.json(), compareUtf8);
      }
    }

    // RANGE against x
    if (left._type == AqlValue::RANGE) {
      triagens::basics::Json ljson = left.toJson(trx, leftcoll, false);

      if (right._type == AqlValue::JSON) {
        return TRI_CompareValuesJson(ljson.json(), right._json->json(), compareUtf8);
      }
      else if (right._type == AqlValue::SHAPED ||
               right._type == AqlValue::DOCVEC) {
        triagens::basics::Json rjson = right.toJson(trx, rightcoll, false);
        return TRI_CompareValuesJson(ljson.json(), rjson.json(), compareUtf8);
      }
    }
    
    // DOCVEC against x
    if (left._type == AqlValue::DOCVEC) {
      triagens::basics::Json ljson = left.toJson(trx, leftcoll, false);

      if (right._type == AqlValue::JSON) {
        return TRI_CompareValuesJson(ljson.json(), right._json->json(), compareUtf8);
      }
      else if (right._type == AqlValue::SHAPED ||
               right._type == AqlValue::RANGE) {
        triagens::basics::Json rjson = right.toJson(trx, rightcoll, false);
        return TRI_CompareValuesJson(ljson.json(), rjson.json(), compareUtf8);
      }
    }

    // No other comparisons are defined
    TRI_ASSERT(false);
  }

  // if we get here, types are equal

  switch (left._type) {
    case AqlValue::EMPTY: {
      return 0;
    }

    case AqlValue::JSON: {
      return TRI_CompareValuesJson(left._json->json(), right._json->json(), compareUtf8);
    }

    case AqlValue::SHAPED: {
      TRI_shaped_json_t l;
      TRI_shaped_json_t r;
      TRI_EXTRACT_SHAPED_JSON_MARKER(l, left._marker);
      TRI_EXTRACT_SHAPED_JSON_MARKER(r, right._marker);
                
      return TRI_CompareShapeTypes(nullptr, nullptr, &l, leftcoll->getShaper(), 
                                   nullptr, nullptr, &r, rightcoll->getShaper());
    }

    case AqlValue::DOCVEC: { 
      // use lexicographic ordering of AqlValues regardless of block,
      // DOCVECs have a single register coming from ReturnNode.
      size_t lblock = 0;
      size_t litem = 0;
      size_t rblock = 0;
      size_t ritem = 0;
      
      while (lblock < left._vector->size() && 
             rblock < right._vector->size()) {
        AqlValue lval = left._vector->at(lblock)->getValue(litem, 0);
        AqlValue rval = right._vector->at(rblock)->getValue(ritem, 0);

        int cmp = Compare(
          trx,
          lval, 
          left._vector->at(lblock)->getDocumentCollection(0),
          rval, 
          right._vector->at(rblock)->getDocumentCollection(0),
          compareUtf8
        );

        if (cmp != 0) {
          return cmp;
        }
        if (++litem == left._vector->size()) {
          litem = 0;
          lblock++;
        }
        if (++ritem == right._vector->size()) {
          ritem = 0;
          rblock++;
        }
      }

      if (lblock == left._vector->size() && 
          rblock == right._vector->size()){
        return 0;
      }

      return (lblock < left._vector->size() ? -1 : 1);
    }

    case AqlValue::RANGE: {
      if (left._range->_low < right._range->_low) {
        return -1;
      } 
      if (left._range->_low > right._range->_low) {
        return 1;
      } 
      if (left._range->_high < left._range->_high) {
        return -1;
      } 
      if (left._range->_high > left._range->_high) {
        return 1;
      }
      return 0;
    }

    default: {
      TRI_ASSERT(false);
      return 0;
    }
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

