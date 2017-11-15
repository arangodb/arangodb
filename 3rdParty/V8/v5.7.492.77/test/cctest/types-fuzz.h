// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_TEST_CCTEST_TYPES_H_
#define V8_TEST_CCTEST_TYPES_H_

#include "src/base/utils/random-number-generator.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/v8.h"

namespace v8 {
namespace internal {
namespace compiler {

class Types {
 public:
  Types(Zone* zone, Isolate* isolate, v8::base::RandomNumberGenerator* rng)
      : zone_(zone), rng_(rng) {
#define DECLARE_TYPE(name, value) \
  name = Type::name();            \
  types.push_back(name);
    PROPER_BITSET_TYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE

    SignedSmall = Type::SignedSmall();
    UnsignedSmall = Type::UnsignedSmall();

    object_map = isolate->factory()->NewMap(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);

    smi = handle(Smi::FromInt(666), isolate);
    boxed_smi = isolate->factory()->NewHeapNumber(666);
    signed32 = isolate->factory()->NewHeapNumber(0x40000000);
    float1 = isolate->factory()->NewHeapNumber(1.53);
    float2 = isolate->factory()->NewHeapNumber(0.53);
    // float3 is identical to float1 in order to test that OtherNumberConstant
    // types are equal by double value and not by handle pointer value.
    float3 = isolate->factory()->NewHeapNumber(1.53);
    object1 = isolate->factory()->NewJSObjectFromMap(object_map);
    object2 = isolate->factory()->NewJSObjectFromMap(object_map);
    array = isolate->factory()->NewJSArray(20);
    uninitialized = isolate->factory()->uninitialized_value();
    SmiConstant = Type::NewConstant(smi, zone);
    Signed32Constant = Type::NewConstant(signed32, zone);

    ObjectConstant1 = Type::HeapConstant(object1, zone);
    ObjectConstant2 = Type::HeapConstant(object2, zone);
    ArrayConstant = Type::HeapConstant(array, zone);
    UninitializedConstant = Type::HeapConstant(uninitialized, zone);

    values.push_back(smi);
    values.push_back(boxed_smi);
    values.push_back(signed32);
    values.push_back(object1);
    values.push_back(object2);
    values.push_back(array);
    values.push_back(uninitialized);
    values.push_back(float1);
    values.push_back(float2);
    values.push_back(float3);
    for (ValueVector::iterator it = values.begin(); it != values.end(); ++it) {
      types.push_back(Type::NewConstant(*it, zone));
    }

    integers.push_back(isolate->factory()->NewNumber(-V8_INFINITY));
    integers.push_back(isolate->factory()->NewNumber(+V8_INFINITY));
    integers.push_back(isolate->factory()->NewNumber(-rng_->NextInt(10)));
    integers.push_back(isolate->factory()->NewNumber(+rng_->NextInt(10)));
    for (int i = 0; i < 10; ++i) {
      double x = rng_->NextInt();
      integers.push_back(isolate->factory()->NewNumber(x));
      x *= rng_->NextInt();
      if (!IsMinusZero(x)) integers.push_back(isolate->factory()->NewNumber(x));
    }

    Integer = Type::Range(-V8_INFINITY, +V8_INFINITY, zone);

    for (int i = 0; i < 30; ++i) {
      types.push_back(Fuzz());
    }
  }

  Handle<i::Map> object_map;

  Handle<i::Smi> smi;
  Handle<i::HeapNumber> boxed_smi;
  Handle<i::HeapNumber> signed32;
  Handle<i::HeapNumber> float1;
  Handle<i::HeapNumber> float2;
  Handle<i::HeapNumber> float3;
  Handle<i::JSObject> object1;
  Handle<i::JSObject> object2;
  Handle<i::JSArray> array;
  Handle<i::Oddball> uninitialized;

#define DECLARE_TYPE(name, value) Type* name;
  PROPER_BITSET_TYPE_LIST(DECLARE_TYPE)
  #undef DECLARE_TYPE

  Type* SignedSmall;
  Type* UnsignedSmall;

  Type* SmiConstant;
  Type* Signed32Constant;
  Type* ObjectConstant1;
  Type* ObjectConstant2;
  Type* ArrayConstant;
  Type* UninitializedConstant;

  Type* Integer;

  typedef std::vector<Type*> TypeVector;
  typedef std::vector<Handle<i::Object> > ValueVector;

  TypeVector types;
  ValueVector values;
  ValueVector integers;  // "Integer" values used for range limits.

  Type* Of(Handle<i::Object> value) { return Type::Of(value, zone_); }

  Type* NewConstant(Handle<i::Object> value) {
    return Type::NewConstant(value, zone_);
  }

  Type* HeapConstant(Handle<i::HeapObject> value) {
    return Type::HeapConstant(value, zone_);
  }

  Type* Range(double min, double max) { return Type::Range(min, max, zone_); }

  Type* Union(Type* t1, Type* t2) { return Type::Union(t1, t2, zone_); }

  Type* Intersect(Type* t1, Type* t2) { return Type::Intersect(t1, t2, zone_); }

  Type* Random() {
    return types[rng_->NextInt(static_cast<int>(types.size()))];
  }

  Type* Fuzz(int depth = 4) {
    switch (rng_->NextInt(depth == 0 ? 3 : 20)) {
      case 0: {  // bitset
        #define COUNT_BITSET_TYPES(type, value) + 1
        int n = 0 PROPER_BITSET_TYPE_LIST(COUNT_BITSET_TYPES);
        #undef COUNT_BITSET_TYPES
        // Pick a bunch of named bitsets and return their intersection.
        Type* result = Type::Any();
        for (int i = 0, m = 1 + rng_->NextInt(3); i < m; ++i) {
          int j = rng_->NextInt(n);
#define PICK_BITSET_TYPE(type, value)                         \
  if (j-- == 0) {                                             \
    Type* tmp = Type::Intersect(result, Type::type(), zone_); \
    if (tmp->Is(Type::None()) && i != 0) {                    \
      break;                                                  \
    } else {                                                  \
      result = tmp;                                           \
      continue;                                               \
    }                                                         \
  }
          PROPER_BITSET_TYPE_LIST(PICK_BITSET_TYPE)
          #undef PICK_BITSET_TYPE
        }
        return result;
      }
      case 1: {  // constant
        int i = rng_->NextInt(static_cast<int>(values.size()));
        return Type::NewConstant(values[i], zone_);
      }
      case 2: {  // range
        int i = rng_->NextInt(static_cast<int>(integers.size()));
        int j = rng_->NextInt(static_cast<int>(integers.size()));
        double min = integers[i]->Number();
        double max = integers[j]->Number();
        if (min > max) std::swap(min, max);
        return Type::Range(min, max, zone_);
      }
      default: {  // union
        int n = rng_->NextInt(10);
        Type* type = None;
        for (int i = 0; i < n; ++i) {
          Type* operand = Fuzz(depth - 1);
          type = Type::Union(type, operand, zone_);
        }
        return type;
      }
    }
    UNREACHABLE();
  }

  Zone* zone() { return zone_; }

 private:
  Zone* zone_;
  v8::base::RandomNumberGenerator* rng_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif
