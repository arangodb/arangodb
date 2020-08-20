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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "v8-vpack.h"

#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/V8PlatformFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

/// @brief maximum array/object nesting depth
static int const MaxLevels = 80;

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VelocyValueType::String into a V8 object
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> ObjectVPackString(v8::Isolate* isolate,
                                                      VPackSlice const& slice) {
  arangodb::velocypack::ValueLength l;
  char const* val = slice.getString(l);
  if (l == 0) {
    return v8::String::Empty(isolate);
  }
  return TRI_V8_PAIR_STRING(isolate, val, l);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VelocyValueType::Object into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectVPackObject(v8::Isolate* isolate, VPackSlice const& slice,
                                               VPackOptions const* options,
                                               VPackSlice const* base) {
  TRI_ASSERT(slice.isObject());
  v8::Handle<v8::Object> object = v8::Object::New(isolate);

  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  TRI_GET_GLOBALS();

  VPackObjectIterator it(slice, true);
  while (it.valid()) {
    arangodb::velocypack::ValueLength l;
    VPackSlice k = it.key(false);

    if (k.isString()) {
      // regular attribute
      char const* p = k.getString(l);
      object
          ->DefineOwnProperty(TRI_IGETC, TRI_V8_PAIR_STRING(isolate, p, l),
                              TRI_VPackToV8(isolate, it.value(), options, &slice))
          .FromMaybe(false);
    } else {
      // optimized code path for translated system attributes
      VPackSlice v = VPackSlice(k.begin() + 1);
      v8::Local<v8::Value> sub;
      if (v.isString()) {
        char const* p = v.getString(l);
        // value of _key, _id, _from, _to, and _rev is ASCII too
        sub = TRI_V8_ASCII_PAIR_STRING(isolate, p, l);
      } else {
        sub = TRI_VPackToV8(isolate, v, options, &slice);
      }

      uint8_t which = static_cast<uint8_t>(k.getUInt()) + VelocyPackHelper::AttributeBase;
      switch (which) {
        case VelocyPackHelper::KeyAttribute: {
          object
              ->DefineOwnProperty(TRI_IGETC,
                                  v8::Local<v8::String>::New(isolate, v8g->_KeyKey), sub)
              .FromMaybe(false);
          break;
        }
        case VelocyPackHelper::RevAttribute: {
          object
              ->DefineOwnProperty(TRI_IGETC,
                                  v8::Local<v8::String>::New(isolate, v8g->_RevKey), sub)
              .FromMaybe(false);
          break;
        }
        case VelocyPackHelper::IdAttribute: {
          object
              ->DefineOwnProperty(TRI_IGETC,
                                  v8::Local<v8::String>::New(isolate, v8g->_IdKey), sub)
              .FromMaybe(false);
          break;
        }
        case VelocyPackHelper::FromAttribute: {
          object
              ->DefineOwnProperty(TRI_IGETC,
                                  v8::Local<v8::String>::New(isolate, v8g->_FromKey), sub)
              .FromMaybe(false);
          break;
        }
        case VelocyPackHelper::ToAttribute: {
          object
              ->DefineOwnProperty(TRI_IGETC,
                                  v8::Local<v8::String>::New(isolate, v8g->_ToKey), sub)
              .FromMaybe(false);
          break;
        }
      }
    }

    if (arangodb::V8PlatformFeature::isOutOfMemory(isolate)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    it.next();
  }

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VelocyValueType::Array into a V8 object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ObjectVPackArray(v8::Isolate* isolate, VPackSlice const& slice,
                                              VPackOptions const* options,
                                              VPackSlice const* base) {
  TRI_ASSERT(slice.isArray());

  VPackArrayIterator it(slice);

  v8::Handle<v8::Array> object = v8::Array::New(isolate, static_cast<int>(it.size()));

  if (object.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  uint32_t j = 0;
  while (it.valid()) {
    v8::Handle<v8::Value> val = TRI_VPackToV8(isolate, it.value(), options, &slice);
    if (!val.IsEmpty()) {
      object->Set(TRI_IGETC, j++, val).FromMaybe(false);
    }
    if (arangodb::V8PlatformFeature::isOutOfMemory(isolate)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    it.next();
  }

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a VPack value into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_VPackToV8(v8::Isolate* isolate, VPackSlice const& slice,
                                    VPackOptions const* options, VPackSlice const* base) {
  switch (slice.type()) {
    case VPackValueType::Null: {
      return v8::Null(isolate);
    }
    case VPackValueType::Bool: {
      return v8::Boolean::New(isolate, slice.getBool());
    }
    case VPackValueType::Double: {
      // convert NaN, +inf & -inf to null
      double value = slice.getDouble();
      if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL ||
          value == -HUGE_VAL) {
        return v8::Null(isolate);
      }
      return v8::Number::New(isolate, slice.getDouble());
    }
    case VPackValueType::Int: {
      int64_t value = slice.getInt();
      if (value >= -2147483648LL && value <= 2147483647LL) {
        // value is within bounds of an int32_t
        return v8::Integer::New(isolate, static_cast<int32_t>(value));
      }
      if (value >= 0 && value <= 4294967295LL) {
        // value is within bounds of a uint32_t
        return v8::Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(value));
      }
      // must use double to avoid truncation
      return v8::Number::New(isolate, static_cast<double>(slice.getInt()));
    }
    case VPackValueType::UInt: {
      uint64_t value = slice.getUInt();
      if (value <= 4294967295ULL) {
        // value is within bounds of a uint32_t
        return v8::Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(value));
      }
      // must use double to avoid truncation
      return v8::Number::New(isolate, static_cast<double>(slice.getUInt()));
    }
    case VPackValueType::SmallInt: {
      return v8::Integer::New(isolate, slice.getNumericValue<int32_t>());
    }
    case VPackValueType::String: {
      return ObjectVPackString(isolate, slice);
    }
    case VPackValueType::Array: {
      return ObjectVPackArray(isolate, slice, options, base);
    }
    case VPackValueType::Object: {
      return ObjectVPackObject(isolate, slice, options, base);
    }
    case VPackValueType::External: {
      // resolve external
      return TRI_VPackToV8(isolate, VPackSlice(reinterpret_cast<uint8_t const*>(slice.getExternal())), options, base);
    }
    case VPackValueType::Custom: {
      if (options == nullptr || options->customTypeHandler == nullptr || base == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Could not extract custom attribute.");
      }
      std::string id = options->customTypeHandler->toString(slice, options, *base);
      return TRI_V8_STD_STRING(isolate, id);
    }
    case VPackValueType::None:
    default: { return v8::Undefined(isolate); }
  }
}

struct BuilderContext {
  BuilderContext(v8::Handle<v8::Context> context, v8::Isolate* isolate,
                 VPackBuilder& builder, bool keepTopLevelOpen)
      : context(context),
        isolate(isolate),
        builder(builder),
        level(0),
        keepTopLevelOpen(keepTopLevelOpen) {}

  v8::Handle<v8::Context> context;
  v8::Isolate* isolate;
  v8::Handle<v8::Value> toJsonKey;
  VPackBuilder& builder;
  int level;
  bool keepTopLevelOpen;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a VPackValue to either an array or an object
////////////////////////////////////////////////////////////////////////////////

template <typename T, bool inObject>
static inline void AddValue(BuilderContext& context,
                            arangodb::velocypack::StringRef const& attributeName, T const& value) {
  if (inObject) {
    context.builder.addUnchecked(attributeName.data(), attributeName.size(), value);
  } else {
    context.builder.add(value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a VPack value
////////////////////////////////////////////////////////////////////////////////

template <bool performAllChecks, bool inObject>
static int V8ToVPack(BuilderContext& context, v8::Handle<v8::Value> const parameter,
                     arangodb::velocypack::StringRef const& attributeName, bool convertFunctionsToNull) {
  if (parameter->IsNullOrUndefined() ||
      (convertFunctionsToNull && parameter->IsFunction())) {
    AddValue<VPackValue, inObject>(context, attributeName, VPackValue(VPackValueType::Null));
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsBoolean()) {
    AddValue<VPackValue, inObject>(context, attributeName,
                                   VPackValue(TRI_ObjectToBoolean(context.isolate, parameter)));
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsNumber()) {
    if (parameter->IsInt32()) {
      AddValue<VPackValue, inObject>(
          context, attributeName,
          VPackValue(parameter->ToInt32(context.context).ToLocalChecked()->Value()));
      return TRI_ERROR_NO_ERROR;
    }

    if (parameter->IsUint32()) {
      AddValue<VPackValue, inObject>(
          context, attributeName,
          VPackValue(parameter->ToUint32(context.context).ToLocalChecked()->Value()));
      return TRI_ERROR_NO_ERROR;
    }
        
    double value = parameter->ToNumber(context.context).ToLocalChecked()->Value();
    if (std::isnan(value) || !std::isfinite(value)) {
      AddValue<VPackValue, inObject>(context, attributeName, VPackValue(VPackValueType::Null));
    } else {
      AddValue<VPackValue, inObject>(
          context, attributeName,
          VPackValue(parameter->ToNumber(context.context).ToLocalChecked()->Value()));
    }
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsString()) {
    v8::String::Utf8Value str(context.isolate,
                              parameter->ToString(context.context).ToLocalChecked());

    if (*str == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    AddValue<VPackValuePair, inObject>(context, attributeName,
                                       VPackValuePair(*str, str.length(),
                                                      VPackValueType::String));
    return TRI_ERROR_NO_ERROR;
  }

  if (parameter->IsArray()) {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(parameter);

    if (++context.level > MaxLevels) {
      // too much recursion - roll back level increase and return null and an error instead
      --context.level;
      AddValue<VPackValue, inObject>(context, attributeName, VPackValue(VPackValueType::Null));
      return TRI_ERROR_BAD_PARAMETER;
    }

    AddValue<VPackValue, inObject>(context, attributeName,
                                   VPackValue(VPackValueType::Array));
    uint32_t const n = array->Length();

    int res = TRI_ERROR_NO_ERROR;
    for (uint32_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> value = array->Get(context.context, i).FromMaybe(v8::Local<v8::Value>());
      if (value->IsUndefined()) {
        // ignore array values which are undefined
        continue;
      }

      res = V8ToVPack<performAllChecks, false>(context, value, arangodb::velocypack::StringRef(),
                                               convertFunctionsToNull);

      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
      
    --context.level;

    if (!context.keepTopLevelOpen || context.level > 0) {
      context.builder.close();
    }
    return res;
  }

  if (parameter->IsObject()) {
    if (performAllChecks) {
      if (parameter->IsBooleanObject()) {
        AddValue<VPackValue, inObject>(context, attributeName,
                                       VPackValue(v8::Handle<v8::BooleanObject>::Cast(parameter)
                                                  ->BooleanValue(context.isolate)));
        return TRI_ERROR_NO_ERROR;
      }

      if (parameter->IsNumberObject()) {
        double value = v8::Handle<v8::NumberObject>::Cast(parameter)->NumberValue(context.context).FromMaybe(0.0);
        if (std::isnan(value) || !std::isfinite(value)) {
          AddValue<VPackValue, inObject>(context, attributeName, VPackValue(VPackValueType::Null));
        } else {
          AddValue<VPackValue, inObject>(context, attributeName,
                                         VPackValue(v8::Handle<v8::NumberObject>::Cast(parameter)
                                                        ->NumberValue(context.context)
                                                        .FromMaybe(0.0)));
        }
        return TRI_ERROR_NO_ERROR;
      }

      if (parameter->IsStringObject()) {
        v8::String::Utf8Value str(context.isolate,
                                  parameter->ToString(context.context).ToLocalChecked());

        if (*str == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        AddValue<VPackValuePair, inObject>(context, attributeName,
                                           VPackValuePair(*str, str.length(),
                                                          VPackValueType::String));
        return TRI_ERROR_NO_ERROR;
      }

      if (parameter->IsRegExp() || parameter->IsFunction() || parameter->IsExternal()) {
        return TRI_ERROR_BAD_PARAMETER;
      }
    }

    v8::Handle<v8::Object> o = parameter->ToObject(context.context).ToLocalChecked();

    if (performAllChecks) {
      // first check if the object has a "toJSON" function
      if (o->Has(context.context, context.toJsonKey).FromMaybe(false)) {
        // call it if yes
        v8::Handle<v8::Value> func = o->Get(context.context, context.toJsonKey).FromMaybe(v8::Local<v8::Value>());
        if (func->IsFunction()) {
          v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(func);

          // assign a dummy entry to the args array even if we don't need it.
          // this prevents "error C2466: cannot allocate an array of constant
          // size 0" in MSVC
          v8::Handle<v8::Value> args[] = {v8::Null(context.isolate)};
          v8::Handle<v8::Value> converted = toJson->Call(context.context, o, 0, args).FromMaybe(v8::Local<v8::Value>());

          if (!converted.IsEmpty()) {
            // return whatever toJSON returned
            return V8ToVPack<performAllChecks, inObject>(context, converted, attributeName,
                                                         convertFunctionsToNull);
          }
        }

        // intentionally falls through
      }
    }

    v8::Handle<v8::Array> names = o->GetOwnPropertyNames(context.context).FromMaybe(v8::Local<v8::Array>());
    uint32_t const n = names->Length();
    
    if (++context.level > MaxLevels) {
      // too much recursion - roll back level increase and return null and an error instead
      --context.level;
      AddValue<VPackValue, inObject>(context, attributeName, VPackValue(VPackValueType::Null));
      return TRI_ERROR_BAD_PARAMETER;
    }

    AddValue<VPackValue, inObject>(context, attributeName,
                                   VPackValue(VPackValueType::Object));

    int res = TRI_ERROR_NO_ERROR;

    for (uint32_t i = 0; i < n; ++i) {
      // process attribute name
      v8::Handle<v8::String> key =
        names->Get(context.context, i).FromMaybe(v8::Local<v8::Value>())->ToString(context.context).ToLocalChecked();
      v8::String::Utf8Value str(context.isolate, key);

      if (*str == nullptr) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        break;
      }

      v8::Handle<v8::Value> value = o->Get(context.context, key).FromMaybe(v8::Handle<v8::Value>());
      if (value->IsUndefined()) {
        // ignore object values which are undefined
        continue;
      }

      res = V8ToVPack<performAllChecks, true>(context, value,
                                              arangodb::velocypack::StringRef(*str, str.length()),
                                              convertFunctionsToNull);

      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
    
    --context.level;

    if (!context.keepTopLevelOpen || context.level > 0) {
      context.builder.close();
    }
    return res;
  }

  return TRI_ERROR_BAD_PARAMETER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to VPack value
////////////////////////////////////////////////////////////////////////////////

int TRI_V8ToVPack(v8::Isolate* isolate, VPackBuilder& builder,
                  v8::Local<v8::Value> const value, bool keepTopLevelOpen,
                  bool convertFunctionsToNull) {
  v8::HandleScope scope(isolate);
  BuilderContext context(isolate->GetCurrentContext(), isolate, builder, keepTopLevelOpen);
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL_STRING(ToJsonKey);
  context.toJsonKey = ToJsonKey;
  return V8ToVPack<true, false>(context, value, arangodb::velocypack::StringRef(), convertFunctionsToNull);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to VPack value, simplified version
/// this function assumes that the V8 object does not contain any cycles and
/// does not contain types such as Function, Date or RegExp
////////////////////////////////////////////////////////////////////////////////

int TRI_V8ToVPackSimple(v8::Isolate* isolate, arangodb::velocypack::Builder& builder,
                        v8::Handle<v8::Value> const value) {
  // a HandleScope must have been created by the caller already
  BuilderContext context(isolate->GetCurrentContext(), isolate, builder, false);
  return V8ToVPack<false, false>(context, value, arangodb::velocypack::StringRef(), false);
}
