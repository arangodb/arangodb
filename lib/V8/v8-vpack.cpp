////////////////////////////////////////////////////////////////////////////////
/// @brief V8 utility functions
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vpack.h"

#include "Basics/Exceptions.h"
#include "V8/v8-utils.h"

#include <velocypack/velocypack-aliases.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VelocyValueType::String into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectVPackString (v8::Isolate* isolate, VPackSlice const& slice) {
  arangodb::velocypack::ValueLength l;
  char const* val = slice.getString(l);
  return TRI_V8_PAIR_STRING(val, l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VelocyValueType::Object into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectVPackObject (v8::Isolate* isolate, VPackSlice const& slice) {
  v8::Handle<v8::Object> object = v8::Object::New(isolate);
  
  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  VPackObjectIterator it(slice);
  while (it.valid()) {
    v8::Handle<v8::Value> val = TRI_VPackToV8(isolate, it.value());
    if (! val.IsEmpty()) {
      auto k = ObjectVPackString(isolate, it.key()); 
      object->ForceSet(k, val);
    }
    it.next();
  }

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VelocyValueType::Array into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectVPackArray (v8::Isolate* isolate, VPackSlice const& slice) {
  v8::Handle<v8::Array> object = v8::Array::New(isolate, static_cast<int>(slice.length()));
  
  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  uint32_t j = 0;
  VPackArrayIterator it(slice);

  while (it.valid()) {
    v8::Handle<v8::Value> val = TRI_VPackToV8(isolate, it.value());
    if (! val.IsEmpty()) {
      object->Set(j++, val);
    }
    it.next();
  }

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VPack value into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_VPackToV8 (v8::Isolate* isolate,
                                     VPackSlice const& slice) {
  switch (slice.type()) {
    case VPackValueType::Null:
      return v8::Null(isolate); 
    case VPackValueType::Bool:
      return v8::Boolean::New(isolate, slice.getBool());
    case VPackValueType::Double:
      return v8::Number::New(isolate, slice.getDouble());
    case VPackValueType::Int:
      return v8::Number::New(isolate, slice.getInt());
    case VPackValueType::UInt:
      return v8::Number::New(isolate, slice.getUInt());
    case VPackValueType::SmallInt:
      return v8::Number::New(isolate, slice.getSmallInt());
    case VPackValueType::String:
      return ObjectVPackString(isolate, slice);
    case VPackValueType::Object:
      return ObjectVPackObject(isolate, slice);
    case VPackValueType::Array:
      return ObjectVPackArray(isolate, slice);
    case VPackValueType::None:
    default:
      return v8::Undefined(isolate);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a VPackValue to either an array or an object
////////////////////////////////////////////////////////////////////////////////

static void AddValue (VPackBuilder& builder,
                      std::string const& attributeName,
                      bool inObject,
                      VPackValue const& value) {
  if (inObject) {
    builder.add(attributeName, value);
  }
  else {
    builder.add(value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a VPack value
////////////////////////////////////////////////////////////////////////////////

static int V8ToVPack (v8::Isolate* isolate,
                      VPackBuilder& builder,
                      v8::Handle<v8::Value> const parameter,
                      std::set<int>& seenHashes,
                      std::vector<v8::Handle<v8::Object>>& seenObjects,
                      std::string const& attributeName,
                      bool inObject) {
  v8::HandleScope scope(isolate);

  if (parameter->IsNull()) {
    AddValue(builder, attributeName, inObject, VPackValue(VPackValueType::Null));
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsBoolean()) {
    v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean();
    AddValue(builder, attributeName, inObject, VPackValue(booleanParameter->Value()));
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsNumber()) {
    v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
    AddValue(builder, attributeName, inObject, VPackValue(numberParameter->Value()));
    return TRI_ERROR_NO_ERROR;
  }
  
  if (parameter->IsString()) {
    v8::Handle<v8::String> stringParameter = parameter->ToString();
    TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, stringParameter);
    
    if (*str == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    AddValue(builder, attributeName, inObject, VPackValue(*str));
    return TRI_ERROR_NO_ERROR;
  }
  
  if (parameter->IsArray()) {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(parameter);

    AddValue(builder, attributeName, inObject, VPackValue(VPackValueType::Array));
    uint32_t const n = array->Length();

    for (uint32_t i = 0; i < n; ++i) {
      // get address of next element
      int res = V8ToVPack(isolate, builder, array->Get(i), seenHashes, seenObjects, "", false);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }

    builder.close();
    return TRI_ERROR_NO_ERROR;
  }
  
  if (parameter->IsObject()) {
    if (parameter->IsBooleanObject()) {
      AddValue(builder, attributeName, inObject, VPackValue(v8::Handle<v8::BooleanObject>::Cast(parameter)->BooleanValue()));
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsNumberObject()) {
      AddValue(builder, attributeName, inObject, VPackValue(v8::Handle<v8::NumberObject>::Cast(parameter)->NumberValue()));
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsStringObject()) {
      v8::Handle<v8::String> stringParameter(parameter->ToString());
      TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, stringParameter);
      
      if (*str == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    
      AddValue(builder, attributeName, inObject, VPackValue(*str));
      return TRI_ERROR_NO_ERROR;
    }
  
    if (parameter->IsRegExp() || 
        parameter->IsFunction() || 
        parameter->IsExternal()) {
      return TRI_ERROR_BAD_PARAMETER;
    }

    v8::Handle<v8::Object> o = parameter->ToObject();

    // first check if the object has a "toJSON" function
    v8::Handle<v8::String> toJsonString = TRI_V8_PAIR_STRING("toJSON", 6);
    if (o->Has(toJsonString)) {
      // call it if yes
      v8::Handle<v8::Value> func = o->Get(toJsonString);
      if (func->IsFunction()) {
        v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(func);

        v8::Handle<v8::Value> args;
        v8::Handle<v8::Value> converted = toJson->Call(o, 0, &args);

        if (! converted.IsEmpty()) {
          // return whatever toJSON returned
          TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, converted->ToString());

          if (*str == nullptr) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }
    
          // this passes ownership for the utf8 string to the JSON object
          AddValue(builder, attributeName, inObject, VPackValue(*str));
          return TRI_ERROR_NO_ERROR;
        }
      }

      // fall-through intentional
    }

    int hash = o->GetIdentityHash();

    if (seenHashes.find(hash) != seenHashes.end()) {
      // LOG_TRACE("found hash %d", hash);

      for (auto& it : seenObjects) {
        if (parameter->StrictEquals(it)) {
          // object is recursive
          return TRI_ERROR_BAD_PARAMETER;
        }
      }
    }
    else {
      seenHashes.emplace(hash);
    }

    seenObjects.emplace_back(o);

    v8::Handle<v8::Array> names = o->GetOwnPropertyNames();
    uint32_t const n = names->Length();

    AddValue(builder, attributeName, inObject, VPackValue(VPackValueType::Object));

    for (uint32_t i = 0; i < n; ++i) {
      // process attribute name
      v8::Handle<v8::Value> key = names->Get(i);
      TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, key);

      if (*str == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      int res = V8ToVPack(isolate, builder, o->Get(key), seenHashes, seenObjects, *str, true);

      if (res != TRI_ERROR_NO_ERROR) {
        // to mimic behavior of previous ArangoDB versions, we need to silently ignore this error
        // a better solution would be:
        // return res;
      }
    }

    seenObjects.pop_back();
    builder.close();
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_BAD_PARAMETER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to VPack value
////////////////////////////////////////////////////////////////////////////////

VPackBuilder TRI_V8ToVPack (v8::Isolate* isolate,
                            v8::Handle<v8::Value> const parameter) {

  VPackBuilder builder;

  std::set<int> seenHashes;
  std::vector<v8::Handle<v8::Object>> seenObjects;
  int res = V8ToVPack(isolate, builder, parameter, seenHashes, seenObjects, "", false);

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return builder;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
