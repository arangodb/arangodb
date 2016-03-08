////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlValue.h"
#include "Aql/AqlItemBlock.h"
#include "Basics/json-utilities.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/AqlTransaction.h"
#include "Utils/ShapedJsonTransformer.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-shape-conv.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using Json = arangodb::basics::Json;
using JsonHelper = arangodb::basics::JsonHelper;

AqlValue::AqlValue(AqlValue$ const& other) : _json(nullptr), _type(JSON) {
  _json = new Json(TRI_UNKNOWN_MEM_ZONE, arangodb::basics::VelocyPackHelper::velocyPackToJson(other.slice()));
  TRI_ASSERT(_json != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a quick method to decide whether a value is true
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isTrue() const {
  if (_type == JSON) {
    TRI_ASSERT(_json != nullptr);
    TRI_json_t* json = _json->json();
    if (TRI_IsBooleanJson(json) && json->_value._boolean) {
      return true;
    } else if (TRI_IsNumberJson(json) && json->_value._number != 0.0) {
      return true;
    } else if (TRI_IsStringJson(json) && json->_value._string.length != 1) {
      // the trailing NULL byte counts, too...
      return true;
    } else if (TRI_IsArrayJson(json) || TRI_IsObjectJson(json)) {
      return true;
    }
  } else if (_type == RANGE || _type == DOCVEC) {
    // a range or a docvec is equivalent to an array
    return true;
  } else if (_type == EMPTY) {
    return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy, explicit destruction, only when needed
////////////////////////////////////////////////////////////////////////////////

void AqlValue::destroy() {
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

std::string AqlValue::getTypeString() const {
  switch (_type) {
    case JSON:
      TRI_ASSERT(_json != nullptr);
      return std::string("json (") +
             std::string(TRI_GetTypeStringJson(_json->json())) +
             std::string(")");
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

AqlValue AqlValue::clone() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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
      } catch (...) {
        for (auto& x : *c) {
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

AqlValue AqlValue::shallowClone() const {
  if (_type == JSON) {
    TRI_ASSERT(_json != nullptr);
    return AqlValue(
        new Json(TRI_UNKNOWN_MEM_ZONE, _json->json(), Json::NOFREE));
  }

  // fallback to regular deep-copying
  return clone();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the AqlValue contains a string value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue::isString() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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

bool AqlValue::isNumber() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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

bool AqlValue::isBoolean() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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

bool AqlValue::isArray() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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

bool AqlValue::isObject() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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

bool AqlValue::isNull(bool emptyIsNull) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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

arangodb::basics::Json AqlValue::at(arangodb::AqlTransaction* trx,
                                    size_t i) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      TRI_json_t const* json = _json->json();
      if (TRI_IsArrayJson(json)) {
        if (i < TRI_LengthArrayJson(json)) {
          return arangodb::basics::Json(
              TRI_UNKNOWN_MEM_ZONE,
              static_cast<TRI_json_t*>(TRI_AtVector(&json->_value._objects, i)),
              arangodb::basics::Json::NOFREE);
        }
      }
      break;  // fall-through to exception
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

          return current->getValue(i - offset, 0)
              .toJson(trx, vecCollection, true);
        }

        offset += (*it)->size();
      }
      break;  // fall-through to exception
    }

    case RANGE: {
      TRI_ASSERT(_range != nullptr);
      return arangodb::basics::Json(static_cast<double>(_range->at(i)));
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

size_t AqlValue::arraySize() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
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

int64_t AqlValue::toInt64() const {
  switch (_type) {
    case JSON:
      TRI_ASSERT(_json != nullptr);
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

double AqlValue::toNumber(bool& failed) const {
  switch (_type) {
    case JSON:
      TRI_ASSERT(_json != nullptr);
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

std::string AqlValue::toString() const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      TRI_json_t const* json = _json->json();
      TRI_ASSERT(TRI_IsStringJson(json));
      return std::string(json->_value._string.data,
                         json->_value._string.length - 1);
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

v8::Handle<v8::Value> AqlValue::toV8Partial(
    v8::Isolate* isolate, arangodb::AqlTransaction* trx,
    std::unordered_set<std::string> const& attributes,
    TRI_document_collection_t const* document) const {
  TRI_ASSERT(_type == JSON);
  TRI_ASSERT(_json != nullptr);

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
      TRI_json_t const* key = static_cast<TRI_json_t const*>(
          TRI_AddressVector(&json->_value._objects, i));

      if (!TRI_IsStringJson(key)) {
        continue;
      }

      // check if we need to render this attribute
      auto it = attributes.find(std::string(key->_value._string.data,
                                            key->_value._string.length - 1));

      if (it == attributes.end()) {
        // we can skip the attribute
        continue;
      }

      TRI_json_t const* value = static_cast<TRI_json_t const*>(
          TRI_AddressVector(&json->_value._objects, (i + 1)));

      if (value != nullptr) {
        result->ForceSet(TRI_V8_STD_STRING((*it)),
                         TRI_ObjectJson(isolate, value));
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

v8::Handle<v8::Value> AqlValue::toV8(
    v8::Isolate* isolate, arangodb::AqlTransaction* trx,
    TRI_document_collection_t const* document) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      return TRI_ObjectJson(isolate, _json->json());
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);
      return TRI_WrapShapedJson<arangodb::AqlTransaction>(
          isolate, *trx, document->_info.id(), _marker);
    }

    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);

      // calculate the result array length
      size_t totalSize = 0;
      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        totalSize += (*it)->size();
      }

      // allocate the result array
      v8::Handle<v8::Array> result =
          v8::Array::New(isolate, static_cast<int>(totalSize));
      uint32_t j = 0;  // output row count

      for (auto it = _vector->begin(); it != _vector->end(); ++it) {
        auto current = (*it);
        size_t const n = current->size();
        auto vecCollection = current->getDocumentCollection(0);
        for (size_t i = 0; i < n; ++i) {
          result->Set(j++, current->getValueReference(i, 0)
                               .toV8(isolate, trx, vecCollection));
        }
      }
      return result;
    }

    case RANGE: {
      TRI_ASSERT(_range != nullptr);

      // allocate the buffer for the result
      size_t const n = _range->size();
      v8::Handle<v8::Array> result =
          v8::Array::New(isolate, static_cast<int>(n));

      for (uint32_t i = 0; i < n; ++i) {
        // is it safe to use a double here (precision loss)?
        result->Set(i, v8::Number::New(isolate, static_cast<double>(_range->at(
                                                    static_cast<size_t>(i)))));
      }

      return result;
    }

    case EMPTY: {
      return v8::Null(isolate);
    }
  }

  // should never get here
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson method
////////////////////////////////////////////////////////////////////////////////

Json AqlValue::toJson(arangodb::AqlTransaction* trx,
                      TRI_document_collection_t const* document,
                      bool copy) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      if (copy) {
        return _json->copy();
      }
      return Json(_json->zone(), _json->json(), Json::NOFREE);
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);

      auto shaper = document->getShaper();
      return TRI_ExpandShapedJson(shaper, trx->resolver(), document->_info.id(),
                                  _marker);
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
          json.add(current->getValueReference(i, 0)
                       .toJson(trx, vecCollection, true));
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
      return Json(Json::Null);
    }
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////
  
AqlValue$::AqlValue$() {
  invalidate(*this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

AqlValue$::AqlValue$(VPackBuilder const& data) {
  TRI_ASSERT(data.isClosed());
  VPackValueLength length = data.size();
  if (length < 16) {
    // Use internal
    memcpy(_data.internal, data.data(), length);
    setType(AqlValueType::INTERNAL);
  } else {
    // Use external
    _data.external = new VPackBuffer<uint8_t>(length);
    memcpy(_data.external->data(), data.data(), length);
    setType(AqlValueType::EXTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

AqlValue$::AqlValue$(VPackBuilder const* data) : AqlValue$(*data) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

AqlValue$::AqlValue$(VPackSlice const& data) {
  VPackValueLength length = data.byteSize();
  if (length < 16) {
    // Use internal
    memcpy(_data.internal, data.begin(), length);
    setType(AqlValueType::INTERNAL);
  } else {
    // Use external
    _data.external = new VPackBuffer<uint8_t>(length);
    memcpy(_data.external->data(), data.begin(), length);
    setType(AqlValueType::EXTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

AqlValue$::AqlValue$(VPackSlice const& data, AqlValueType type) {
  if (type == AqlValueType::REFERENCE ||
      type == AqlValueType::REFERENCE_STICKY) {
    // simply copy the slice
    _data.reference = data.begin();
    setType(type);
    return;
  }

  // everything else as usual
  VPackValueLength length = data.byteSize();
  if (length < 16) {
    // Use internal
    memcpy(_data.internal, data.begin(), length);
    setType(AqlValueType::INTERNAL);
  } else {
    // Use external
    _data.external = new VPackBuffer<uint8_t>(length);
    memcpy(_data.external->data(), data.begin(), length);
    setType(AqlValueType::EXTERNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with range value
////////////////////////////////////////////////////////////////////////////////
  
AqlValue$::AqlValue$(int64_t low, int64_t high) {
  Range* range = new Range(low, high);

  _data.internal[0] = 0xf4; // custom type for range
  _data.internal[1] = sizeof(Range*);
  memcpy(&_data.internal[2], &range, sizeof(Range*));
  setType(AqlValueType::RANGE);
}
   
AqlValue$::AqlValue$(bool value) 
    : AqlValue$(value ? arangodb::basics::VelocyPackHelper::TrueValue() : 
       arangodb::basics::VelocyPackHelper::FalseValue()) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////
  
AqlValue$::~AqlValue$() {
  destroy();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy
////////////////////////////////////////////////////////////////////////////////

AqlValue$::AqlValue$(AqlValue$ const& other) {
  copy(other);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy
////////////////////////////////////////////////////////////////////////////////

AqlValue$& AqlValue$::operator=(AqlValue$ const& other) {
  if (this != &other) {
    destroy();
    copy(other);
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move
////////////////////////////////////////////////////////////////////////////////

AqlValue$::AqlValue$(AqlValue$&& other) {
  move(other);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move
////////////////////////////////////////////////////////////////////////////////

AqlValue$& AqlValue$::operator=(AqlValue$&& other) {
  if (this != &other) {
    destroy();
    move(other);
  }
  return *this;
}

// Temporary constructor to transform an old AqlValue to a new VPackBased
// AqlValue
AqlValue$::AqlValue$(AqlValue const& other, arangodb::AqlTransaction* trx,
                     TRI_document_collection_t const* document) {
  VPackBuilder builder;
  switch (other._type) {
    case AqlValue::JSON: {
      TRI_ASSERT(other._json != nullptr);
      // TODO: Internal is still JSON. We always copy.
      int res = arangodb::basics::JsonHelper::toVelocyPack(other._json->json(), builder);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
      break;
    }
    case AqlValue::SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(other._marker != nullptr);
      auto shaper = document->getShaper();
      Json tmp = TRI_ExpandShapedJson(shaper, trx->resolver(),
                                      document->_info.id(), other._marker);
      int res = arangodb::basics::JsonHelper::toVelocyPack(tmp.json(), builder);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
      break;
    }
    case AqlValue::DOCVEC: {
      TRI_ASSERT(other._vector != nullptr);
      try {
        VPackArrayBuilder b(&builder);
        for (auto const& current : *other._vector) {
          size_t const n = current->size();
          auto vecCollection = current->getDocumentCollection(0);
          for (size_t i = 0; i < n; ++i) {
            current->getValueReference(i, 0)
                .toVelocyPack(trx, vecCollection, builder);
          }
        }
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      break;
    }
    case AqlValue::RANGE: {
      // TODO Has to be replaced by VPackCustom Type
      TRI_ASSERT(other._range != nullptr);
      Range* range = new Range(other._range->_low, other._range->_high);

      _data.internal[0] = 0xf4; // custom type for range
      _data.internal[1] = sizeof(Range*);
      memcpy(&_data.internal[2], &range, sizeof(Range*));
      _data.internal[15] = AqlValueType::RANGE; 
      break;
    }
    case AqlValue::EMPTY: {
      builder.add(VPackValue(VPackValueType::Null));
      break;
    }
    default: {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }
  VPackValueLength length = builder.size();
  if (length < 16) {
    // Small enough for local
    // copy memory from the builder into the internal data.
    memcpy(_data.internal, builder.data(), length);
    setType(AqlValueType::INTERNAL);
  } else {
    // We need a large external buffer
    // TODO: Replace by SlimBuffer
    _data.external = new VPackBuffer<uint8_t>(length);
    memcpy(_data.external->data(), builder.data(), length);
    setType(AqlValueType::EXTERNAL);
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return a slice for the contained value
//////////////////////////////////////////////////////////////////////////////

VPackSlice AqlValue$::slice() const {
  switch (type()) {
    case EXTERNAL:
      return VPackSlice(_data.external->data());
    case REFERENCE:
    case REFERENCE_STICKY:
      return VPackSlice(_data.reference);
    case INTERNAL:
    case RANGE: 
      return VPackSlice(_data.internal);
  }
  return VPackSlice(); // we should not get here
}

//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the value is empty / none
//////////////////////////////////////////////////////////////////////////////
 
bool AqlValue$::isNone() const {
  return slice().isNone();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the value is a number
//////////////////////////////////////////////////////////////////////////////
 
bool AqlValue$::isNumber() const {
  if (type() == RANGE) {
    return false;
  }
  return slice().isNumber();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the value is an object
//////////////////////////////////////////////////////////////////////////////
 
bool AqlValue$::isObject() const {
  if (type() == RANGE) {
    // special handling for ranges
    return true;
  }
  // all other cases
  return slice().isObject();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the value is an array (note: this treats ranges
/// as arrays, too!)
//////////////////////////////////////////////////////////////////////////////
 
bool AqlValue$::isArray() const {
  if (type() == RANGE) {
    // special handling for ranges
    return true;
  }
  // all other cases
  return slice().isArray();
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief get the (array) length (note: this treats ranges as arrays, too!)
//////////////////////////////////////////////////////////////////////////////

size_t AqlValue$::length() const {
  if (type() == RANGE) {
    // special handling for ranges
    return range()->size();
  }
  // all other cases
  VPackSlice const s = slice();
  if (s.isArray()) {
    return s.length();
  }
  return 0;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief get the (array) element at position 
//////////////////////////////////////////////////////////////////////////////

AqlValue$ AqlValue$::at(int64_t position) const {
  if (type() == RANGE) {
    // special handling for ranges
    Range* r = range();
    size_t const n = r->size();

    if (position < 0) {
      // a negative position is allowed
      position = static_cast<int64_t>(n) + position;
    }

    if (position >= 0 && position < static_cast<int64_t>(n)) {
      // only look up the value if it is within array bounds
      VPackBuilder builder;
      builder.add(VPackValue(r->at(static_cast<size_t>(position))));
      return AqlValue$(builder);
    }
    // fall-through intentional
  }
  else {
    VPackSlice const s = slice();
    if (s.isArray()) {
      size_t const n = static_cast<size_t>(s.length());
      if (position < 0) {
        // a negative position is allowed
        position = static_cast<int64_t>(n) + position;
      }

      if (position >= 0 && position < static_cast<int64_t>(n)) {
        // only look up the value if it is within array bounds
        if (type() == AqlValueType::REFERENCE_STICKY) {
          return AqlValue$(s.at(position), AqlValueType::REFERENCE_STICKY);
        }
        return AqlValue$(s.at(position));
      }
    }
  }

  // default is to return null
  return AqlValue$(arangodb::basics::VelocyPackHelper::NullValue(), REFERENCE_STICKY);
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief get the (object) element by name
//////////////////////////////////////////////////////////////////////////////
  
AqlValue$ AqlValue$::get(std::string const& name) const {
  VPackSlice const s = slice();
  if (s.isObject()) {
    VPackSlice const value = s.get(name);
    if (!value.isNone()) {
      if (type() == AqlValueType::REFERENCE_STICKY) {
        return AqlValue$(value, AqlValueType::REFERENCE_STICKY);
      }
      return AqlValue$(value);
    }
  }
  
  // default is to return null
  return AqlValue$(arangodb::basics::VelocyPackHelper::NullValue(), REFERENCE_STICKY);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get the (object) element(s) by name
//////////////////////////////////////////////////////////////////////////////
  
AqlValue$ AqlValue$::get(std::vector<char const*> const& names) const {
  VPackSlice const s = slice();
  if (s.isObject()) {
    VPackSlice const value = s.get(names);
    if (!value.isNone()) {
      if (type() == AqlValueType::REFERENCE_STICKY) {
        return AqlValue$(value, AqlValueType::REFERENCE_STICKY);
      }
      return AqlValue$(value);
    }
  }
  
  // default is to return null
  return AqlValue$(arangodb::basics::VelocyPackHelper::NullValue(), REFERENCE_STICKY);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get the numeric value of an AqlValue
//////////////////////////////////////////////////////////////////////////////
 
double AqlValue$::toDouble() const {
  if (type() == RANGE) {
    // special handling for ranges
    Range* r = range();
    size_t rangeSize = r->size();
  
    if (rangeSize == 1) {
      return static_cast<double>(r->at(0));
    }
    return 0.0;
  }

  VPackSlice const s = slice();

  if (s.isBoolean()) {
    return s.getBoolean() ? 1.0 : 0.0;
  }
  if (s.isNumber()) {
    return s.getNumber<double>();
  }
  if (s.isString()) {
    try {
      return std::stod(s.copyString());
    } catch (...) {
      // conversion failed
    }
    return 0.0;
  }
  if (s.isArray()) {
    if (s.length() == 1) {
      AqlValue$ tmp(s.at(0));
      return tmp.toDouble();
    }
    return 0.0;
  }

  return 0.0;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get the numeric value of an AqlValue
//////////////////////////////////////////////////////////////////////////////

int64_t AqlValue$::toInt64() const {
  if (type() == RANGE) {
    // special handling for ranges
    Range* r = range();
    size_t rangeSize = r->size();
  
    if (rangeSize == 1) {
      return static_cast<int64_t>(r->at(0));
    }
    return 0.0;
  }
  
  VPackSlice const s = slice();
  
  if (s.isBoolean()) {
    return s.getBoolean() ? 1 : 0;
  }
  if (s.isNumber()) {
    return s.getNumber<int64_t>();
  }
  if (s.isString()) {
    try {
      return static_cast<int64_t>(std::stoll(s.copyString()));
    } catch (...) {
      // conversion failed
    }
    return 0;
  }
  if (s.isArray()) {
    if (s.length() == 1) {
      AqlValue$ tmp(s.at(0));
      return tmp.toInt64();
    }
    return 0;
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the contained value evaluates to true
//////////////////////////////////////////////////////////////////////////////

bool AqlValue$::isTrue() const {
  if (type() == RANGE) {
    return true;
  }
  VPackSlice const s = slice();

  if (s.isBoolean()) {
    return s.getBoolean();
  } 
  if (s.isNumber()) {
    return (s.getNumber<double>() != 0.0);
  } 
  if (s.isString()) {
    VPackValueLength length;
    s.getString(length);
    return length > 0;
  } 
  if (s.isArray() || s.isObject()) {
    return true;
  }

  // all other cases, include Null and None
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
/// only construct those attributes that are needed in the expression
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> AqlValue$::toV8Partial(
    v8::Isolate* isolate, arangodb::AqlTransaction* trx,
    std::unordered_set<std::string> const& attributes) const {
  VPackSlice const s = slice();

  if (isObject()) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    // only construct those attributes needed
    size_t left = attributes.size();

    // we can only have got here if we had attributes
    TRI_ASSERT(left > 0);

    // iterate over all the object's attributes
    for (auto const& it : VPackObjectIterator(s)) {
      // check if we need to render this attribute
      auto it2 = attributes.find(it.key.copyString());

      if (it2 == attributes.end()) {
        // we can skip the attribute
        continue;
      }

      // TODO: do we need a customTypeHandler here?
      result->ForceSet(TRI_V8_STD_STRING((*it2)),
                       TRI_VPackToV8(isolate, it.value));

      if (--left == 0) {
        // we have rendered all required attributes
        break;
      }
    }

    // return partial object
    return result;
  }

  // fallback
  // TODO: do we need a customTypeHandler here?
  return TRI_VPackToV8(isolate, s);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> AqlValue$::toV8(
    v8::Isolate* isolate, arangodb::AqlTransaction* trx) const {
  if (type() == RANGE) {
    Range* r = range();
    size_t const n = r->size();

    v8::Handle<v8::Array> result =
          v8::Array::New(isolate, static_cast<int>(n));

    for (uint32_t i = 0; i < n; ++i) {
      // is it safe to use a double here (precision loss)?
      result->Set(i, v8::Number::New(isolate, 
        static_cast<double>(r->at(static_cast<size_t>(i)))));
    }

    return result;
  }

  // all other types go here
  return TRI_VPackToV8(isolate, slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief sets the value type
//////////////////////////////////////////////////////////////////////////////

void AqlValue$::setType(AqlValueType type) {
  _data.internal[15] = type;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief helper function for copy construction/assignment
//////////////////////////////////////////////////////////////////////////////

void AqlValue$::copy(AqlValue$ const& other) {
  setType(other.type());
  VPackValueLength length = other.slice().byteSize();
  switch (other.type()) {
    case EXTERNAL: {
      _data.external = new VPackBuffer<uint8_t>(length);
      memcpy(_data.external->data(), other._data.external->data(), length);
      break;
    }
    case REFERENCE: 
    case REFERENCE_STICKY: {
      _data.reference = other._data.reference;
      break;
    }
    case INTERNAL: {
      memcpy(_data.internal, other._data.internal, length);
      break;
    }
    case RANGE: {
      Range* range = new Range(other.range()->_low, other.range()->_high);

      _data.internal[0] = 0xf4; // custom type for range
      _data.internal[1] = sizeof(Range*);
      memcpy(&_data.internal[2], &range, sizeof(Range*));
      break; 
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief helper function for move construction/assignment
//////////////////////////////////////////////////////////////////////////////

void AqlValue$::move(AqlValue$& other) {
  setType(other.type());
  switch (other.type()) {
    case EXTERNAL: {
      // steal the other's external value
      _data.external = other._data.external;

      // overwrite the other's value
      invalidate(other);
      break;
    }
    case REFERENCE: 
    case REFERENCE_STICKY: {
      // reuse the other's external value
      _data.reference = other._data.reference;
      break;
    }
    case INTERNAL: {
      VPackValueLength length = other.slice().byteSize();
      memcpy(_data.internal, other._data.internal, length);
      break;
    }
    case RANGE: {
      // copy the range pointer over to us
      VPackValueLength length = other.slice().byteSize();
      memcpy(_data.internal, other._data.internal, length);
      
      // overwrite the other's value
      invalidate(other);
      break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief destroy the value's internals
//////////////////////////////////////////////////////////////////////////////

void AqlValue$::destroy() {
  switch (type()) {
    case EXTERNAL: {
      if (_data.external != nullptr) {
        delete _data.external;
      }
      break;
    }
    case REFERENCE:
    case REFERENCE_STICKY:
    case INTERNAL: {
      // NOTHING TO DO
      break;
    }
    case RANGE: {
      delete range();
      break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidates/resets a value to None
//////////////////////////////////////////////////////////////////////////////

void AqlValue$::invalidate(AqlValue$& other) {
  VPackSlice empty; // None slice
  memcpy(other._data.internal, empty.begin(), empty.byteSize());
  setType(AqlValueType::INTERNAL);
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief get pointer of the included Range object
//////////////////////////////////////////////////////////////////////////////

Range* AqlValue$::range() const {
  TRI_ASSERT(type() == RANGE);
  Range* range = nullptr;
  memcpy(&range, &_data.internal[2], sizeof(Range*));
  return range;
}


void AqlValue::toVelocyPack(arangodb::AqlTransaction* trx,
                            TRI_document_collection_t const* document,
                            VPackBuilder& builder) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      // TODO: Internal is still JSON. We always copy.
      int res = arangodb::basics::JsonHelper::toVelocyPack(_json->json(), builder);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
      break;
    }
    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);
      auto shaper = document->getShaper();
      Json tmp = TRI_ExpandShapedJson(shaper, trx->resolver(),
                                      document->_info.id(), _marker);
      int res = arangodb::basics::JsonHelper::toVelocyPack(tmp.json(), builder);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
      break;
    }
    case DOCVEC: {
      TRI_ASSERT(_vector != nullptr);
      try {
        VPackArrayBuilder b(&builder);
        for (auto const& current : *_vector) {
          size_t const n = current->size();
          auto vecCollection = current->getDocumentCollection(0);
          for (size_t i = 0; i < n; ++i) {
            current->getValueReference(i, 0)
                .toVelocyPack(trx, vecCollection, builder);
          }
        }
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      break;
    }
    case RANGE: {
      // TODO Has to be replaced by VPackCustom Type
      TRI_ASSERT(_range != nullptr);
      try {
        VPackArrayBuilder b(&builder);
        size_t const n = _range->size();
        for (size_t i = 0; i < n; ++i) {
          builder.add(VPackValue(_range->at(i)));
        }
      } catch (...) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      break;
    }
    case EMPTY: {
      builder.add(VPackValue(VPackValueType::Null));
      break;
    }
    default: {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the JSON contents
////////////////////////////////////////////////////////////////////////////////

uint64_t AqlValue::hash(arangodb::AqlTransaction* trx,
                        TRI_document_collection_t const* document) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      return TRI_FastHashJson(_json->json());
    }

    case SHAPED: {
      TRI_ASSERT(document != nullptr);
      TRI_ASSERT(_marker != nullptr);

      auto shaper = document->getShaper();
      TRI_shaped_json_t shaped;
      TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, _marker);

      auto v = TRI_JsonShapedJson(shaper, &shaped);

      if (v == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      Json json(shaper->memoryZone(), v);

      // append the internal attributes

      // _id, _key, _rev
      char const* key = TRI_EXTRACT_MARKER_KEY(_marker);
      std::string id(trx->resolver()->getCollectionName(document->_info.id()));
      id.push_back('/');
      id.append(key);
      json(TRI_VOC_ATTRIBUTE_ID, Json(id));
      json(TRI_VOC_ATTRIBUTE_REV,
           Json(std::to_string(TRI_EXTRACT_MARKER_RID(_marker))));
      json(TRI_VOC_ATTRIBUTE_KEY, Json(key));

#if 0 
      // TODO
      if (TRI_IS_EDGE_MARKER(_marker)) {
        // _from
        std::string from(trx->resolver()->getCollectionNameCluster(
            TRI_EXTRACT_MARKER_FROM_CID(_marker)));
        from.push_back('/');
        from.append(TRI_EXTRACT_MARKER_FROM_KEY(_marker));
        json(TRI_VOC_ATTRIBUTE_FROM, Json(from));

        // _to
        std::string to(trx->resolver()->getCollectionNameCluster(
            TRI_EXTRACT_MARKER_TO_CID(_marker)));
        to.push_back('/');
        to.append(TRI_EXTRACT_MARKER_TO_KEY(_marker));
        json(TRI_VOC_ATTRIBUTE_TO, Json(to));
      }
#endif      

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
          json.add(current->getValueReference(i, 0)
                       .toJson(trx, vecCollection, true));
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

Json AqlValue::extractObjectMember(
    arangodb::AqlTransaction* trx, TRI_document_collection_t const* document,
    char const* name, bool copy, arangodb::basics::StringBuffer& buffer) const {
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_json != nullptr);
      TRI_json_t const* json = _json->json();

      if (TRI_IsObjectJson(json)) {
        TRI_json_t const* found = TRI_LookupObjectJson(json, name);

        if (found != nullptr) {
          if (copy) {
            auto c = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, found);

            if (c == nullptr) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
            }

            // return a copy of the value
            return Json(TRI_UNKNOWN_MEM_ZONE, c,
                        arangodb::basics::Json::AUTOFREE);
          }

          // return a pointer to the original value, without asking for its
          // ownership
          return Json(TRI_UNKNOWN_MEM_ZONE, found,
                      arangodb::basics::Json::NOFREE);
        }
      }

      // attribute does not exist or something went wrong - fall-through to
      // returning null below
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
          trx->resolver()->getCollectionName(document->_info.id(), buffer);
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
#if 0 
        // TODO       
        if (name[1] == 'f' && strcmp(name, TRI_VOC_ATTRIBUTE_FROM) == 0 &&
            (_marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
             _marker->_type == TRI_WAL_MARKER_EDGE)) {
          buffer.reset();
          trx->resolver()->getCollectionNameCluster(
              TRI_EXTRACT_MARKER_FROM_CID(_marker), buffer);
          buffer.appendChar('/');
          buffer.appendText(TRI_EXTRACT_MARKER_FROM_KEY(_marker));
          return Json(TRI_UNKNOWN_MEM_ZONE, buffer.c_str(), buffer.length());
        }
        if (name[1] == 't' && strcmp(name, TRI_VOC_ATTRIBUTE_TO) == 0 &&
            (_marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
             _marker->_type == TRI_WAL_MARKER_EDGE)) {
          buffer.reset();
          trx->resolver()->getCollectionNameCluster(
              TRI_EXTRACT_MARKER_TO_CID(_marker), buffer);
          buffer.appendChar('/');
          buffer.appendText(TRI_EXTRACT_MARKER_TO_KEY(_marker));
          return Json(TRI_UNKNOWN_MEM_ZONE, buffer.c_str(), buffer.length());
        }
#endif
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
          auto v = TRI_JsonShapedJson(shaper, &json);

          if (v == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }

          return Json(TRI_UNKNOWN_MEM_ZONE, v);
        }
      }

      // attribute does not exist or something went wrong - fall-through to
      // returning null
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

Json AqlValue::extractArrayMember(arangodb::AqlTransaction* trx,
                                  TRI_document_collection_t const* document,
                                  int64_t position, bool copy) const {
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
          TRI_json_t const* found =
              TRI_LookupArrayJson(json, static_cast<size_t>(position));

          if (found != nullptr) {
            if (copy) {
              auto c = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, found);

              if (c == nullptr) {
                THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
              }
              // return a copy of the value
              return Json(TRI_UNKNOWN_MEM_ZONE, c,
                          arangodb::basics::Json::AUTOFREE);
            }

            // return a pointer to the original value, without asking for its
            // ownership
            return Json(TRI_UNKNOWN_MEM_ZONE, found,
                        arangodb::basics::Json::NOFREE);
          }
        }
      }

      // attribute does not exist or something went wrong - fall-through to
      // returning null below
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
        return Json(
            static_cast<double>(_range->at(static_cast<size_t>(position))));
      }
      break;  // fall-through to returning null
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
          return (*it)
              ->getValueReference(p - totalSize, 0)
              .toJson(trx, vecCollection, copy);
        }
        totalSize += (*it)->size();
      }
      break;  // fall-through to returning null
    }

    case SHAPED:
    case EMPTY: {
      break;  // fall-through to returning null
    }
  }

  return Json(Json::Null);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::CreateFromBlocks(
    arangodb::AqlTransaction* trx, std::vector<AqlItemBlock*> const& src,
    std::vector<std::string> const& variableNames) {
  size_t totalSize = 0;

  for (auto it = src.begin(); it != src.end(); ++it) {
    totalSize += (*it)->size();
  }

  auto json = std::make_unique<Json>(Json::Array, totalSize);

  for (auto it = src.begin(); it != src.end(); ++it) {
    auto current = (*it);
    RegisterId const n = current->getNrRegs();

    std::vector<std::pair<RegisterId, TRI_document_collection_t const*>>
        registers;
    for (RegisterId j = 0; j < n; ++j) {
      // temporaries don't have a name and won't be included
      if (variableNames[j][0] != '\0') {
        registers.emplace_back(
            std::make_pair(j, current->getDocumentCollection(j)));
      }
    }

    for (size_t i = 0; i < current->size(); ++i) {
      Json values(Json::Object, registers.size());

      // only enumerate the registers that are left
      for (auto const& reg : registers) {
        values.set(variableNames[reg.first],
                   current->getValueReference(i, reg.first)
                       .toJson(trx, reg.second, true));
      }

      json->add(values);
    }
  }

  return AqlValue(json.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

AqlValue AqlValue::CreateFromBlocks(
    arangodb::AqlTransaction* trx, std::vector<AqlItemBlock*> const& src,
    arangodb::aql::RegisterId expressionRegister) {
  size_t totalSize = 0;

  for (auto it = src.begin(); it != src.end(); ++it) {
    totalSize += (*it)->size();
  }

  auto json = std::make_unique<Json>(Json::Array, totalSize);

  for (auto it = src.begin(); it != src.end(); ++it) {
    auto current = (*it);
    auto document = current->getDocumentCollection(expressionRegister);

    for (size_t i = 0; i < current->size(); ++i) {
      json->add(current->getValueReference(i, expressionRegister)
                    .toJson(trx, document, true));
    }
  }

  return AqlValue(json.release());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief 3-way comparison for AqlValue objects
////////////////////////////////////////////////////////////////////////////////

int AqlValue::Compare(arangodb::AqlTransaction* trx, AqlValue const& left,
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
        (right._type == AqlValue::SHAPED || right._type == AqlValue::RANGE ||
         right._type == AqlValue::DOCVEC)) {
      arangodb::basics::Json rjson = right.toJson(trx, rightcoll, false);
      return TRI_CompareValuesJson(left._json->json(), rjson.json(),
                                   compareUtf8);
    }

    // SHAPED against x
    if (left._type == AqlValue::SHAPED) {
      arangodb::basics::Json ljson = left.toJson(trx, leftcoll, false);

      if (right._type == AqlValue::JSON) {
        return TRI_CompareValuesJson(ljson.json(), right._json->json(),
                                     compareUtf8);
      } else if (right._type == AqlValue::RANGE ||
                 right._type == AqlValue::DOCVEC) {
        arangodb::basics::Json rjson = right.toJson(trx, rightcoll, false);
        return TRI_CompareValuesJson(ljson.json(), rjson.json(), compareUtf8);
      }
    }

    // RANGE against x
    if (left._type == AqlValue::RANGE) {
      arangodb::basics::Json ljson = left.toJson(trx, leftcoll, false);

      if (right._type == AqlValue::JSON) {
        return TRI_CompareValuesJson(ljson.json(), right._json->json(),
                                     compareUtf8);
      } else if (right._type == AqlValue::SHAPED ||
                 right._type == AqlValue::DOCVEC) {
        arangodb::basics::Json rjson = right.toJson(trx, rightcoll, false);
        return TRI_CompareValuesJson(ljson.json(), rjson.json(), compareUtf8);
      }
    }

    // DOCVEC against x
    if (left._type == AqlValue::DOCVEC) {
      arangodb::basics::Json ljson = left.toJson(trx, leftcoll, false);

      if (right._type == AqlValue::JSON) {
        return TRI_CompareValuesJson(ljson.json(), right._json->json(),
                                     compareUtf8);
      } else if (right._type == AqlValue::SHAPED ||
                 right._type == AqlValue::RANGE) {
        arangodb::basics::Json rjson = right.toJson(trx, rightcoll, false);
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
      TRI_ASSERT(left._json != nullptr);
      TRI_ASSERT(right._json != nullptr);
      return TRI_CompareValuesJson(left._json->json(), right._json->json(),
                                   compareUtf8);
    }

    case AqlValue::SHAPED: {
      TRI_shaped_json_t l;
      TRI_shaped_json_t r;
      TRI_EXTRACT_SHAPED_JSON_MARKER(l, left._marker);
      TRI_EXTRACT_SHAPED_JSON_MARKER(r, right._marker);

      return TRI_CompareShapeTypes(nullptr, nullptr, &l, leftcoll->getShaper(),
                                   nullptr, nullptr, &r,
                                   rightcoll->getShaper());
    }

    case AqlValue::DOCVEC: {
      // use lexicographic ordering of AqlValues regardless of block,
      // DOCVECs have a single register coming from ReturnNode.
      size_t lblock = 0;
      size_t litem = 0;
      size_t rblock = 0;
      size_t ritem = 0;

      while (lblock < left._vector->size() && rblock < right._vector->size()) {
        AqlValue lval = left._vector->at(lblock)->getValue(litem, 0);
        AqlValue rval = right._vector->at(rblock)->getValue(ritem, 0);

        int cmp = Compare(
            trx, lval, left._vector->at(lblock)->getDocumentCollection(0), rval,
            right._vector->at(rblock)->getDocumentCollection(0), compareUtf8);

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

      if (lblock == left._vector->size() && rblock == right._vector->size()) {
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
