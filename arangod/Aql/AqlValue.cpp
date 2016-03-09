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

#include "AqlValue.h"
#include "Aql/AqlItemBlock.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/AqlTransaction.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"
#include "VocBase/document-collection.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

#if 0
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
#endif

#if 0
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

#endif

#if 0
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
#endif

#if 0
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
#endif


#if 0
at:
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
#endif

#if 0
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
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a V8 value as input for the expression execution in V8
////////////////////////////////////////////////////////////////////////////////

#if 0
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
#endif

#if 0
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
#endif

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
  destroyQuick();
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
    destroyQuick();
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
    destroyQuick();
    move(other);
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the value
////////////////////////////////////////////////////////////////////////////////

uint64_t AqlValue$::hash() const {
  switch (type()) {
    case INTERNAL:
    case EXTERNAL:
    case REFERENCE:
    case REFERENCE_STICKY: {
      return slice().hash();
    }

    case RANGE: {
      // build the range values temporarily
      Range* r = range();
      size_t const n = r->size();

      VPackBuilder builder;
      builder.openArray();

      for (size_t i = 0; i < n; ++i) {
        builder.add(VPackValue(r->at(i)));
      }

      builder.close();
      return builder.slice().hash();
    }
  }

  return 0;
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
  if (type() == RANGE) {
    return false;
  }
  return slice().isNone();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the value contains a null value
////////////////////////////////////////////////////////////////////////////////

bool AqlValue$::isNull(bool emptyIsNull) const {
  if (type() == RANGE) {
    // special handling for ranges
    return false;
  }

  if (slice().isNone()) {
    return emptyIsNull;
  }

  return slice().isNull();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the value is a number
//////////////////////////////////////////////////////////////////////////////
 
bool AqlValue$::isNumber() const {
  if (type() == RANGE) {
    // special handling for ranges
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
                       TRI_VPackToV8(isolate, it.value, trx->transactionContext()->getVPackOptions()));

      if (--left == 0) {
        // we have rendered all required attributes
        break;
      }
    }

    // return partial object
    return result;
  }

  // fallback
  return TRI_VPackToV8(isolate, s, trx->transactionContext()->getVPackOptions());
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
  return TRI_VPackToV8(isolate, slice(), trx->transactionContext()->getVPackOptions());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief convert a value to VelocyPack, appending to a build
//////////////////////////////////////////////////////////////////////////////

void AqlValue$::toVelocyPack(arangodb::AqlTransaction* trx,
                             VPackBuilder& builder) const {
  if (type() == RANGE) {
    builder.openArray();
    
    Range* r = range();  
    size_t const n = r->size();
    for (size_t i = 0; i < n; ++i) {
      builder.add(VPackValue(r->at(i)));
    }
    builder.close();
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief convert a value to VelocyPack
//////////////////////////////////////////////////////////////////////////////

AqlValue$ AqlValue$::toVelocyPack(arangodb::AqlTransaction* trx) const {
  if (type() == RANGE) {
    VPackBuilder builder;
    builder.openArray();
    
    Range* r = range();  
    size_t const n = r->size();
    for (size_t i = 0; i < n; ++i) {
      builder.add(VPackValue(r->at(i)));
    }
    builder.close();
    return AqlValue$(builder);
  }
  return *this;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief clone a value
//////////////////////////////////////////////////////////////////////////////

AqlValue$ AqlValue$::clone() const {
  AqlValue$ temp(*this);
  return temp;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidates/resets a value to None
//////////////////////////////////////////////////////////////////////////////

void AqlValue$::invalidate() {
  VPackSlice empty; // None slice
  memcpy(_data.internal, empty.begin(), empty.byteSize());
  setType(AqlValueType::INTERNAL);
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
      // other's value can stay in place
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

void AqlValue$::destroyQuick() {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

AqlValue$ AqlValue$::CreateFromBlocks(
    arangodb::AqlTransaction* trx, std::vector<AqlItemBlock*> const& src,
    std::vector<std::string> const& variableNames) {

  VPackBuilder builder;
  builder.openArray();

  for (auto const& current : src) {
    RegisterId const n = current->getNrRegs();

    std::vector<RegisterId> registers;
    for (RegisterId j = 0; j < n; ++j) {
      // temporaries don't have a name and won't be included
      if (variableNames[j][0] != '\0') {
        registers.emplace_back(j);
      }
    }

    for (size_t i = 0; i < current->size(); ++i) {
      builder.openObject();

      // only enumerate the registers that are left
      for (auto const& reg : registers) {
        builder.add(variableNames[reg], current->getValueReference(i, reg).toVelocyPack(trx).slice());
      }

      builder.close();
    }
  }

  builder.close();

  return AqlValue$(builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from a vector of AqlItemBlock*s
////////////////////////////////////////////////////////////////////////////////

AqlValue$ AqlValue$::CreateFromBlocks(
    arangodb::AqlTransaction* trx, std::vector<AqlItemBlock*> const& src,
    arangodb::aql::RegisterId expressionRegister) {

  VPackBuilder builder;
  builder.openArray();

  for (auto const& current : src) {
    for (size_t i = 0; i < current->size(); ++i) {
      builder.add(current->getValueReference(i, expressionRegister).toVelocyPack(trx).slice());
    }
  }

  builder.close();

  return AqlValue$(builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief 3-way comparison for AqlValue objects
////////////////////////////////////////////////////////////////////////////////

int AqlValue$::Compare(arangodb::AqlTransaction* trx, AqlValue$ const& left,
                       AqlValue$ const& right,
                       bool compareUtf8) {
  AqlValue$::AqlValueType const leftType = left.type();
  AqlValue$::AqlValueType const rightType = right.type();

  if (leftType != rightType) {
    return arangodb::basics::VelocyPackHelper::compare(left.toVelocyPack(trx).slice(), right.toVelocyPack(trx).slice(), compareUtf8);
  }

  // if we get here, types are equal

  switch (leftType) {
    case AqlValue$::AqlValueType::INTERNAL:
    case AqlValue$::AqlValueType::EXTERNAL:
    case AqlValue$::AqlValueType::REFERENCE:
    case AqlValue$::AqlValueType::REFERENCE_STICKY: {
      return arangodb::basics::VelocyPackHelper::compare(left.slice(), right.slice(), compareUtf8);
    }
    case AqlValue$::AqlValueType::RANGE: {
      if (left.range()->_low < right.range()->_low) {
        return -1;
      }
      if (left.range()->_low > right.range()->_low) {
        return 1;
      }
      if (left.range()->_high < left.range()->_high) {
        return -1;
      }
      if (left.range()->_high > left.range()->_high) {
        return 1;
      }
      return 0;
    }
  }

  return 0;
}

