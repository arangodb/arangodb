// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "src/objects-inl.h"

#include "src/accessors.h"
#include "src/allocation-site-scopes.h"
#include "src/api-arguments-inl.h"
#include "src/api-natives.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/bits.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/builtins/builtins.h"
#include "src/code-stubs.h"
#include "src/compiler.h"
#include "src/counters-inl.h"
#include "src/counters.h"
#include "src/date.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/execution.h"
#include "src/field-index-inl.h"
#include "src/field-index.h"
#include "src/field-type.h"
#include "src/frames-inl.h"
#include "src/globals.h"
#include "src/ic/ic.h"
#include "src/identity-map.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/keys.h"
#include "src/log.h"
#include "src/lookup-inl.h"
#include "src/macro-assembler.h"
#include "src/map-updater.h"
#include "src/messages.h"
#include "src/objects-body-descriptors-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/code-inl.h"
#include "src/objects/compilation-cache-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-collection-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-date-time-format.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-generator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format.h"
#include "src/objects/js-locale.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segmenter.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/literal-objects-inl.h"
#include "src/objects/map.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/microtask-queue-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/parsing/preparsed-scope-data.h"
#include "src/property-descriptor.h"
#include "src/prototype.h"
#include "src/regexp/jsregexp.h"
#include "src/safepoint-table.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/snapshot.h"
#include "src/source-position-table.h"
#include "src/string-builder-inl.h"
#include "src/string-search.h"
#include "src/string-stream.h"
#include "src/unicode-cache-inl.h"
#include "src/unicode-decoder.h"
#include "src/utils-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/zone.h"

#ifdef ENABLE_DISASSEMBLER
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/eh-frame.h"
#endif

namespace v8 {
namespace internal {

bool ComparisonResultToBool(Operation op, ComparisonResult result) {
  switch (op) {
    case Operation::kLessThan:
      return result == ComparisonResult::kLessThan;
    case Operation::kLessThanOrEqual:
      return result == ComparisonResult::kLessThan ||
             result == ComparisonResult::kEqual;
    case Operation::kGreaterThan:
      return result == ComparisonResult::kGreaterThan;
    case Operation::kGreaterThanOrEqual:
      return result == ComparisonResult::kGreaterThan ||
             result == ComparisonResult::kEqual;
    default:
      break;
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, InstanceType instance_type) {
  switch (instance_type) {
#define WRITE_TYPE(TYPE) \
  case TYPE:             \
    return os << #TYPE;
    INSTANCE_TYPE_LIST(WRITE_TYPE)
#undef WRITE_TYPE
  }
  UNREACHABLE();
}

Handle<FieldType> Object::OptimalType(Isolate* isolate,
                                      Representation representation) {
  if (representation.IsNone()) return FieldType::None(isolate);
  if (FLAG_track_field_types) {
    if (representation.IsHeapObject() && IsHeapObject()) {
      // We can track only JavaScript objects with stable maps.
      Handle<Map> map(HeapObject::cast(this)->map(), isolate);
      if (map->is_stable() && map->IsJSReceiverMap()) {
        return FieldType::Class(map, isolate);
      }
    }
  }
  return FieldType::Any(isolate);
}

MaybeHandle<JSReceiver> Object::ToObject(Isolate* isolate,
                                         Handle<Object> object,
                                         Handle<Context> native_context,
                                         const char* method_name) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  Handle<JSFunction> constructor;
  if (object->IsSmi()) {
    constructor = handle(native_context->number_function(), isolate);
  } else {
    int constructor_function_index =
        Handle<HeapObject>::cast(object)->map()->GetConstructorFunctionIndex();
    if (constructor_function_index == Map::kNoConstructorFunctionIndex) {
      if (method_name != nullptr) {
        THROW_NEW_ERROR(
            isolate,
            NewTypeError(
                MessageTemplate::kCalledOnNullOrUndefined,
                isolate->factory()->NewStringFromAsciiChecked(method_name)),
            JSReceiver);
      }
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kUndefinedOrNullToObject),
                      JSReceiver);
    }
    constructor = handle(
        JSFunction::cast(native_context->get(constructor_function_index)),
        isolate);
  }
  Handle<JSObject> result = isolate->factory()->NewJSObject(constructor);
  Handle<JSValue>::cast(result)->set_value(*object);
  return result;
}

// ES6 section 9.2.1.2, OrdinaryCallBindThis for sloppy callee.
// static
MaybeHandle<JSReceiver> Object::ConvertReceiver(Isolate* isolate,
                                                Handle<Object> object) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  if (object->IsNullOrUndefined(isolate)) {
    return isolate->global_proxy();
  }
  return Object::ToObject(isolate, object);
}

// static
MaybeHandle<Object> Object::ConvertToNumberOrNumeric(Isolate* isolate,
                                                     Handle<Object> input,
                                                     Conversion mode) {
  while (true) {
    if (input->IsNumber()) {
      return input;
    }
    if (input->IsString()) {
      return String::ToNumber(isolate, Handle<String>::cast(input));
    }
    if (input->IsOddball()) {
      return Oddball::ToNumber(isolate, Handle<Oddball>::cast(input));
    }
    if (input->IsSymbol()) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kSymbolToNumber),
                      Object);
    }
    if (input->IsBigInt()) {
      if (mode == Conversion::kToNumeric) return input;
      DCHECK_EQ(mode, Conversion::kToNumber);
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kBigIntToNumber),
                      Object);
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input, JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
                                                ToPrimitiveHint::kNumber),
        Object);
  }
}

// static
MaybeHandle<Object> Object::ConvertToInteger(Isolate* isolate,
                                             Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input,
      ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumber), Object);
  if (input->IsSmi()) return input;
  return isolate->factory()->NewNumber(DoubleToInteger(input->Number()));
}

// static
MaybeHandle<Object> Object::ConvertToInt32(Isolate* isolate,
                                           Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input,
      ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumber), Object);
  if (input->IsSmi()) return input;
  return isolate->factory()->NewNumberFromInt(DoubleToInt32(input->Number()));
}

// static
MaybeHandle<Object> Object::ConvertToUint32(Isolate* isolate,
                                            Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input,
      ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumber), Object);
  if (input->IsSmi()) return handle(Smi::cast(*input)->ToUint32Smi(), isolate);
  return isolate->factory()->NewNumberFromUint(DoubleToUint32(input->Number()));
}

// static
MaybeHandle<Name> Object::ConvertToName(Isolate* isolate,
                                        Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input, Object::ToPrimitive(input, ToPrimitiveHint::kString),
      Name);
  if (input->IsName()) return Handle<Name>::cast(input);
  return ToString(isolate, input);
}

// ES6 7.1.14
// static
MaybeHandle<Object> Object::ConvertToPropertyKey(Isolate* isolate,
                                                 Handle<Object> value) {
  // 1. Let key be ToPrimitive(argument, hint String).
  MaybeHandle<Object> maybe_key =
      Object::ToPrimitive(value, ToPrimitiveHint::kString);
  // 2. ReturnIfAbrupt(key).
  Handle<Object> key;
  if (!maybe_key.ToHandle(&key)) return key;
  // 3. If Type(key) is Symbol, then return key.
  if (key->IsSymbol()) return key;
  // 4. Return ToString(key).
  // Extending spec'ed behavior, we'd be happy to return an element index.
  if (key->IsSmi()) return key;
  if (key->IsHeapNumber()) {
    uint32_t uint_value;
    if (value->ToArrayLength(&uint_value) &&
        uint_value <= static_cast<uint32_t>(Smi::kMaxValue)) {
      return handle(Smi::FromInt(static_cast<int>(uint_value)), isolate);
    }
  }
  return Object::ToString(isolate, key);
}

// static
MaybeHandle<String> Object::ConvertToString(Isolate* isolate,
                                            Handle<Object> input) {
  while (true) {
    if (input->IsOddball()) {
      return handle(Handle<Oddball>::cast(input)->to_string(), isolate);
    }
    if (input->IsNumber()) {
      return isolate->factory()->NumberToString(input);
    }
    if (input->IsSymbol()) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kSymbolToString),
                      String);
    }
    if (input->IsBigInt()) {
      return BigInt::ToString(isolate, Handle<BigInt>::cast(input));
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input, JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
                                                ToPrimitiveHint::kString),
        String);
    // The previous isString() check happened in Object::ToString and thus we
    // put it at the end of the loop in this helper.
    if (input->IsString()) {
      return Handle<String>::cast(input);
    }
  }
}

namespace {

bool IsErrorObject(Isolate* isolate, Handle<Object> object) {
  if (!object->IsJSReceiver()) return false;
  Handle<Symbol> symbol = isolate->factory()->stack_trace_symbol();
  return JSReceiver::HasOwnProperty(Handle<JSReceiver>::cast(object), symbol)
      .FromMaybe(false);
}

Handle<String> AsStringOrEmpty(Isolate* isolate, Handle<Object> object) {
  return object->IsString() ? Handle<String>::cast(object)
                            : isolate->factory()->empty_string();
}

Handle<String> NoSideEffectsErrorToString(Isolate* isolate,
                                          Handle<Object> input) {
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(input);

  Handle<Name> name_key = isolate->factory()->name_string();
  Handle<Object> name = JSReceiver::GetDataProperty(receiver, name_key);
  Handle<String> name_str = AsStringOrEmpty(isolate, name);

  Handle<Name> msg_key = isolate->factory()->message_string();
  Handle<Object> msg = JSReceiver::GetDataProperty(receiver, msg_key);
  Handle<String> msg_str = AsStringOrEmpty(isolate, msg);

  if (name_str->length() == 0) return msg_str;
  if (msg_str->length() == 0) return name_str;

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(name_str);
  builder.AppendCString(": ");
  builder.AppendString(msg_str);

  return builder.Finish().ToHandleChecked();
}

}  // namespace

// static
Handle<String> Object::NoSideEffectsToString(Isolate* isolate,
                                             Handle<Object> input) {
  DisallowJavascriptExecution no_js(isolate);

  if (input->IsString() || input->IsNumber() || input->IsOddball()) {
    return Object::ToString(isolate, input).ToHandleChecked();
  } else if (input->IsBigInt()) {
    MaybeHandle<String> maybe_string =
        BigInt::ToString(isolate, Handle<BigInt>::cast(input), 10, kDontThrow);
    Handle<String> result;
    if (maybe_string.ToHandle(&result)) return result;
    // BigInt-to-String conversion can fail on 32-bit platforms where
    // String::kMaxLength is too small to fit this BigInt.
    return isolate->factory()->NewStringFromStaticChars(
        "<a very large BigInt>");
  } else if (input->IsFunction()) {
    // -- F u n c t i o n
    Handle<String> fun_str;
    if (input->IsJSBoundFunction()) {
      fun_str = JSBoundFunction::ToString(Handle<JSBoundFunction>::cast(input));
    } else {
      DCHECK(input->IsJSFunction());
      fun_str = JSFunction::ToString(Handle<JSFunction>::cast(input));
    }

    if (fun_str->length() > 128) {
      IncrementalStringBuilder builder(isolate);
      builder.AppendString(isolate->factory()->NewSubString(fun_str, 0, 111));
      builder.AppendCString("...<omitted>...");
      builder.AppendString(isolate->factory()->NewSubString(
          fun_str, fun_str->length() - 2, fun_str->length()));

      return builder.Finish().ToHandleChecked();
    }
    return fun_str;
  } else if (input->IsSymbol()) {
    // -- S y m b o l
    Handle<Symbol> symbol = Handle<Symbol>::cast(input);

    IncrementalStringBuilder builder(isolate);
    builder.AppendCString("Symbol(");
    if (symbol->name()->IsString()) {
      builder.AppendString(handle(String::cast(symbol->name()), isolate));
    }
    builder.AppendCharacter(')');

    return builder.Finish().ToHandleChecked();
  } else if (input->IsJSReceiver()) {
    // -- J S R e c e i v e r
    Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(input);
    Handle<Object> to_string = JSReceiver::GetDataProperty(
        receiver, isolate->factory()->toString_string());

    if (IsErrorObject(isolate, input) ||
        *to_string == *isolate->error_to_string()) {
      // When internally formatting error objects, use a side-effects-free
      // version of Error.prototype.toString independent of the actually
      // installed toString method.
      return NoSideEffectsErrorToString(isolate, input);
    } else if (*to_string == *isolate->object_to_string()) {
      Handle<Object> ctor = JSReceiver::GetDataProperty(
          receiver, isolate->factory()->constructor_string());
      if (ctor->IsFunction()) {
        Handle<String> ctor_name;
        if (ctor->IsJSBoundFunction()) {
          ctor_name = JSBoundFunction::GetName(
                          isolate, Handle<JSBoundFunction>::cast(ctor))
                          .ToHandleChecked();
        } else if (ctor->IsJSFunction()) {
          Handle<Object> ctor_name_obj =
              JSFunction::GetName(isolate, Handle<JSFunction>::cast(ctor));
          ctor_name = AsStringOrEmpty(isolate, ctor_name_obj);
        }

        if (ctor_name->length() != 0) {
          IncrementalStringBuilder builder(isolate);
          builder.AppendCString("#<");
          builder.AppendString(ctor_name);
          builder.AppendCString(">");

          return builder.Finish().ToHandleChecked();
        }
      }
    }
  }

  // At this point, input is either none of the above or a JSReceiver.

  Handle<JSReceiver> receiver;
  if (input->IsJSReceiver()) {
    receiver = Handle<JSReceiver>::cast(input);
  } else {
    // This is the only case where Object::ToObject throws.
    DCHECK(!input->IsSmi());
    int constructor_function_index =
        Handle<HeapObject>::cast(input)->map()->GetConstructorFunctionIndex();
    if (constructor_function_index == Map::kNoConstructorFunctionIndex) {
      return isolate->factory()->NewStringFromAsciiChecked("[object Unknown]");
    }

    receiver = Object::ToObject(isolate, input, isolate->native_context())
                   .ToHandleChecked();
  }

  Handle<String> builtin_tag = handle(receiver->class_name(), isolate);
  Handle<Object> tag_obj = JSReceiver::GetDataProperty(
      receiver, isolate->factory()->to_string_tag_symbol());
  Handle<String> tag =
      tag_obj->IsString() ? Handle<String>::cast(tag_obj) : builtin_tag;

  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("[object ");
  builder.AppendString(tag);
  builder.AppendCString("]");

  return builder.Finish().ToHandleChecked();
}

// static
MaybeHandle<Object> Object::ConvertToLength(Isolate* isolate,
                                            Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(isolate, input), Object);
  if (input->IsSmi()) {
    int value = std::max(Smi::ToInt(*input), 0);
    return handle(Smi::FromInt(value), isolate);
  }
  double len = DoubleToInteger(input->Number());
  if (len <= 0.0) {
    return handle(Smi::kZero, isolate);
  } else if (len >= kMaxSafeInteger) {
    len = kMaxSafeInteger;
  }
  return isolate->factory()->NewNumber(len);
}

// static
MaybeHandle<Object> Object::ConvertToIndex(
    Isolate* isolate, Handle<Object> input,
    MessageTemplate::Template error_index) {
  if (input->IsUndefined(isolate)) return handle(Smi::kZero, isolate);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(isolate, input), Object);
  if (input->IsSmi() && Smi::ToInt(*input) >= 0) return input;
  double len = DoubleToInteger(input->Number()) + 0.0;
  auto js_len = isolate->factory()->NewNumber(len);
  if (len < 0.0 || len > kMaxSafeInteger) {
    THROW_NEW_ERROR(isolate, NewRangeError(error_index, js_len), Object);
  }
  return js_len;
}

bool Object::BooleanValue(Isolate* isolate) {
  if (IsSmi()) return Smi::ToInt(this) != 0;
  DCHECK(IsHeapObject());
  if (IsBoolean()) return IsTrue(isolate);
  if (IsNullOrUndefined(isolate)) return false;
  if (IsUndetectable()) return false;  // Undetectable object is false.
  if (IsString()) return String::cast(this)->length() != 0;
  if (IsHeapNumber()) return DoubleToBoolean(HeapNumber::cast(this)->value());
  if (IsBigInt()) return BigInt::cast(this)->ToBoolean();
  return true;
}


namespace {

// TODO(bmeurer): Maybe we should introduce a marker interface Number,
// where we put all these methods at some point?
ComparisonResult NumberCompare(double x, double y) {
  if (std::isnan(x) || std::isnan(y)) {
    return ComparisonResult::kUndefined;
  } else if (x < y) {
    return ComparisonResult::kLessThan;
  } else if (x > y) {
    return ComparisonResult::kGreaterThan;
  } else {
    return ComparisonResult::kEqual;
  }
}

bool NumberEquals(double x, double y) {
  // Must check explicitly for NaN's on Windows, but -0 works fine.
  if (std::isnan(x)) return false;
  if (std::isnan(y)) return false;
  return x == y;
}

bool NumberEquals(const Object* x, const Object* y) {
  return NumberEquals(x->Number(), y->Number());
}

bool NumberEquals(Handle<Object> x, Handle<Object> y) {
  return NumberEquals(*x, *y);
}

ComparisonResult Reverse(ComparisonResult result) {
  if (result == ComparisonResult::kLessThan) {
    return ComparisonResult::kGreaterThan;
  }
  if (result == ComparisonResult::kGreaterThan) {
    return ComparisonResult::kLessThan;
  }
  return result;
}

}  // anonymous namespace

// static
Maybe<ComparisonResult> Object::Compare(Isolate* isolate, Handle<Object> x,
                                        Handle<Object> y) {
  // ES6 section 7.2.11 Abstract Relational Comparison step 3 and 4.
  if (!Object::ToPrimitive(x, ToPrimitiveHint::kNumber).ToHandle(&x) ||
      !Object::ToPrimitive(y, ToPrimitiveHint::kNumber).ToHandle(&y)) {
    return Nothing<ComparisonResult>();
  }
  if (x->IsString() && y->IsString()) {
    // ES6 section 7.2.11 Abstract Relational Comparison step 5.
    return Just(String::Compare(isolate, Handle<String>::cast(x),
                                Handle<String>::cast(y)));
  }
  if (x->IsBigInt() && y->IsString()) {
    return Just(BigInt::CompareToString(isolate, Handle<BigInt>::cast(x),
                                        Handle<String>::cast(y)));
  }
  if (x->IsString() && y->IsBigInt()) {
    return Just(Reverse(BigInt::CompareToString(
        isolate, Handle<BigInt>::cast(y), Handle<String>::cast(x))));
  }
  // ES6 section 7.2.11 Abstract Relational Comparison step 6.
  if (!Object::ToNumeric(isolate, x).ToHandle(&x) ||
      !Object::ToNumeric(isolate, y).ToHandle(&y)) {
    return Nothing<ComparisonResult>();
  }

  bool x_is_number = x->IsNumber();
  bool y_is_number = y->IsNumber();
  if (x_is_number && y_is_number) {
    return Just(NumberCompare(x->Number(), y->Number()));
  } else if (!x_is_number && !y_is_number) {
    return Just(BigInt::CompareToBigInt(Handle<BigInt>::cast(x),
                                        Handle<BigInt>::cast(y)));
  } else if (x_is_number) {
    return Just(Reverse(BigInt::CompareToNumber(Handle<BigInt>::cast(y), x)));
  } else {
    return Just(BigInt::CompareToNumber(Handle<BigInt>::cast(x), y));
  }
}


// static
Maybe<bool> Object::Equals(Isolate* isolate, Handle<Object> x,
                           Handle<Object> y) {
  // This is the generic version of Abstract Equality Comparison. Must be in
  // sync with CodeStubAssembler::Equal.
  while (true) {
    if (x->IsNumber()) {
      if (y->IsNumber()) {
        return Just(NumberEquals(x, y));
      } else if (y->IsBoolean()) {
        return Just(NumberEquals(*x, Handle<Oddball>::cast(y)->to_number()));
      } else if (y->IsString()) {
        return Just(NumberEquals(
            x, String::ToNumber(isolate, Handle<String>::cast(y))));
      } else if (y->IsBigInt()) {
        return Just(BigInt::EqualToNumber(Handle<BigInt>::cast(y), x));
      } else if (y->IsJSReceiver()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsString()) {
      if (y->IsString()) {
        return Just(String::Equals(isolate, Handle<String>::cast(x),
                                   Handle<String>::cast(y)));
      } else if (y->IsNumber()) {
        x = String::ToNumber(isolate, Handle<String>::cast(x));
        return Just(NumberEquals(x, y));
      } else if (y->IsBoolean()) {
        x = String::ToNumber(isolate, Handle<String>::cast(x));
        return Just(NumberEquals(*x, Handle<Oddball>::cast(y)->to_number()));
      } else if (y->IsBigInt()) {
        return Just(BigInt::EqualToString(isolate, Handle<BigInt>::cast(y),
                                          Handle<String>::cast(x)));
      } else if (y->IsJSReceiver()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsBoolean()) {
      if (y->IsOddball()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsNumber()) {
        return Just(NumberEquals(Handle<Oddball>::cast(x)->to_number(), *y));
      } else if (y->IsString()) {
        y = String::ToNumber(isolate, Handle<String>::cast(y));
        return Just(NumberEquals(Handle<Oddball>::cast(x)->to_number(), *y));
      } else if (y->IsBigInt()) {
        x = Oddball::ToNumber(isolate, Handle<Oddball>::cast(x));
        return Just(BigInt::EqualToNumber(Handle<BigInt>::cast(y), x));
      } else if (y->IsJSReceiver()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
        x = Oddball::ToNumber(isolate, Handle<Oddball>::cast(x));
      } else {
        return Just(false);
      }
    } else if (x->IsSymbol()) {
      if (y->IsSymbol()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsJSReceiver()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsBigInt()) {
      if (y->IsBigInt()) {
        return Just(BigInt::EqualToBigInt(BigInt::cast(*x), BigInt::cast(*y)));
      }
      return Equals(isolate, y, x);
    } else if (x->IsJSReceiver()) {
      if (y->IsJSReceiver()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsUndetectable()) {
        return Just(x->IsUndetectable());
      } else if (y->IsBoolean()) {
        y = Oddball::ToNumber(isolate, Handle<Oddball>::cast(y));
      } else if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(x))
                      .ToHandle(&x)) {
        return Nothing<bool>();
      }
    } else {
      return Just(x->IsUndetectable() && y->IsUndetectable());
    }
  }
}


bool Object::StrictEquals(Object* that) {
  if (this->IsNumber()) {
    if (!that->IsNumber()) return false;
    return NumberEquals(this, that);
  } else if (this->IsString()) {
    if (!that->IsString()) return false;
    return String::cast(this)->Equals(String::cast(that));
  } else if (this->IsBigInt()) {
    if (!that->IsBigInt()) return false;
    return BigInt::EqualToBigInt(BigInt::cast(this), BigInt::cast(that));
  }
  return this == that;
}


// static
Handle<String> Object::TypeOf(Isolate* isolate, Handle<Object> object) {
  if (object->IsNumber()) return isolate->factory()->number_string();
  if (object->IsOddball())
    return handle(Oddball::cast(*object)->type_of(), isolate);
  if (object->IsUndetectable()) {
    return isolate->factory()->undefined_string();
  }
  if (object->IsString()) return isolate->factory()->string_string();
  if (object->IsSymbol()) return isolate->factory()->symbol_string();
  if (object->IsBigInt()) return isolate->factory()->bigint_string();
  if (object->IsCallable()) return isolate->factory()->function_string();
  return isolate->factory()->object_string();
}


// static
MaybeHandle<Object> Object::Add(Isolate* isolate, Handle<Object> lhs,
                                Handle<Object> rhs) {
  if (lhs->IsNumber() && rhs->IsNumber()) {
    return isolate->factory()->NewNumber(lhs->Number() + rhs->Number());
  } else if (lhs->IsString() && rhs->IsString()) {
    return isolate->factory()->NewConsString(Handle<String>::cast(lhs),
                                             Handle<String>::cast(rhs));
  }
  ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToPrimitive(lhs), Object);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToPrimitive(rhs), Object);
  if (lhs->IsString() || rhs->IsString()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToString(isolate, rhs),
                               Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToString(isolate, lhs),
                               Object);
    return isolate->factory()->NewConsString(Handle<String>::cast(lhs),
                                             Handle<String>::cast(rhs));
  }
  ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(isolate, rhs),
                             Object);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(isolate, lhs),
                             Object);
  return isolate->factory()->NewNumber(lhs->Number() + rhs->Number());
}


// static
MaybeHandle<Object> Object::OrdinaryHasInstance(Isolate* isolate,
                                                Handle<Object> callable,
                                                Handle<Object> object) {
  // The {callable} must have a [[Call]] internal method.
  if (!callable->IsCallable()) return isolate->factory()->false_value();

  // Check if {callable} is a bound function, and if so retrieve its
  // [[BoundTargetFunction]] and use that instead of {callable}.
  if (callable->IsJSBoundFunction()) {
    Handle<Object> bound_callable(
        Handle<JSBoundFunction>::cast(callable)->bound_target_function(),
        isolate);
    return Object::InstanceOf(isolate, object, bound_callable);
  }

  // If {object} is not a receiver, return false.
  if (!object->IsJSReceiver()) return isolate->factory()->false_value();

  // Get the "prototype" of {callable}; raise an error if it's not a receiver.
  Handle<Object> prototype;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, prototype,
      Object::GetProperty(isolate, callable,
                          isolate->factory()->prototype_string()),
      Object);
  if (!prototype->IsJSReceiver()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kInstanceofNonobjectProto, prototype),
        Object);
  }

  // Return whether or not {prototype} is in the prototype chain of {object}.
  Maybe<bool> result = JSReceiver::HasInPrototypeChain(
      isolate, Handle<JSReceiver>::cast(object), prototype);
  if (result.IsNothing()) return MaybeHandle<Object>();
  return isolate->factory()->ToBoolean(result.FromJust());
}

// static
MaybeHandle<Object> Object::InstanceOf(Isolate* isolate, Handle<Object> object,
                                       Handle<Object> callable) {
  // The {callable} must be a receiver.
  if (!callable->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kNonObjectInInstanceOfCheck),
                    Object);
  }

  // Lookup the @@hasInstance method on {callable}.
  Handle<Object> inst_of_handler;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, inst_of_handler,
      JSReceiver::GetMethod(Handle<JSReceiver>::cast(callable),
                            isolate->factory()->has_instance_symbol()),
      Object);
  if (!inst_of_handler->IsUndefined(isolate)) {
    // Call the {inst_of_handler} on the {callable}.
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, inst_of_handler, callable, 1, &object),
        Object);
    return isolate->factory()->ToBoolean(result->BooleanValue(isolate));
  }

  // The {callable} must have a [[Call]] internal method.
  if (!callable->IsCallable()) {
    THROW_NEW_ERROR(
        isolate, NewTypeError(MessageTemplate::kNonCallableInInstanceOfCheck),
        Object);
  }

  // Fall back to OrdinaryHasInstance with {callable} and {object}.
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      JSReceiver::OrdinaryHasInstance(isolate, callable, object), Object);
  return result;
}

// static
MaybeHandle<Object> Object::GetMethod(Handle<JSReceiver> receiver,
                                      Handle<Name> name) {
  Handle<Object> func;
  Isolate* isolate = receiver->GetIsolate();
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, func, JSReceiver::GetProperty(isolate, receiver, name), Object);
  if (func->IsNullOrUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }
  if (!func->IsCallable()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kPropertyNotFunction,
                                          func, name, receiver),
                    Object);
  }
  return func;
}

namespace {

MaybeHandle<FixedArray> CreateListFromArrayLikeFastPath(
    Isolate* isolate, Handle<Object> object, ElementTypes element_types) {
  if (element_types == ElementTypes::kAll) {
    if (object->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(object);
      uint32_t length;
      if (!array->HasArrayPrototype(isolate) ||
          !array->length()->ToUint32(&length) || !array->HasFastElements() ||
          !JSObject::PrototypeHasNoElements(isolate, *array)) {
        return MaybeHandle<FixedArray>();
      }
      return array->GetElementsAccessor()->CreateListFromArrayLike(
          isolate, array, length);
    } else if (object->IsJSTypedArray()) {
      Handle<JSTypedArray> array = Handle<JSTypedArray>::cast(object);
      size_t length = array->length_value();
      if (array->WasNeutered() ||
          length > static_cast<size_t>(FixedArray::kMaxLength)) {
        return MaybeHandle<FixedArray>();
      }
      return array->GetElementsAccessor()->CreateListFromArrayLike(
          isolate, array, static_cast<uint32_t>(length));
    }
  }
  return MaybeHandle<FixedArray>();
}

}  // namespace

// static
MaybeHandle<FixedArray> Object::CreateListFromArrayLike(
    Isolate* isolate, Handle<Object> object, ElementTypes element_types) {
  // Fast-path for JSArray and JSTypedArray.
  MaybeHandle<FixedArray> fast_result =
      CreateListFromArrayLikeFastPath(isolate, object, element_types);
  if (!fast_result.is_null()) return fast_result;
  // 1. ReturnIfAbrupt(object).
  // 2. (default elementTypes -- not applicable.)
  // 3. If Type(obj) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "CreateListFromArrayLike")),
                    FixedArray);
  }

  // 4. Let len be ? ToLength(? Get(obj, "length")).
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);
  Handle<Object> raw_length_number;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, raw_length_number,
                             Object::GetLengthFromArrayLike(isolate, receiver),
                             FixedArray);
  uint32_t len;
  if (!raw_length_number->ToUint32(&len) ||
      len > static_cast<uint32_t>(FixedArray::kMaxLength)) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kInvalidArrayLength),
                    FixedArray);
  }
  // 5. Let list be an empty List.
  Handle<FixedArray> list = isolate->factory()->NewFixedArray(len);
  // 6. Let index be 0.
  // 7. Repeat while index < len:
  for (uint32_t index = 0; index < len; ++index) {
    // 7a. Let indexName be ToString(index).
    // 7b. Let next be ? Get(obj, indexName).
    Handle<Object> next;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, next,
                               JSReceiver::GetElement(isolate, receiver, index),
                               FixedArray);
    switch (element_types) {
      case ElementTypes::kAll:
        // Nothing to do.
        break;
      case ElementTypes::kStringAndSymbol: {
        // 7c. If Type(next) is not an element of elementTypes, throw a
        //     TypeError exception.
        if (!next->IsName()) {
          THROW_NEW_ERROR(isolate,
                          NewTypeError(MessageTemplate::kNotPropertyName, next),
                          FixedArray);
        }
        // 7d. Append next as the last element of list.
        // Internalize on the fly so we can use pointer identity later.
        next = isolate->factory()->InternalizeName(Handle<Name>::cast(next));
        break;
      }
    }
    list->set(index, *next);
    // 7e. Set index to index + 1. (See loop header.)
  }
  // 8. Return list.
  return list;
}


// static
MaybeHandle<Object> Object::GetLengthFromArrayLike(Isolate* isolate,
                                                   Handle<JSReceiver> object) {
  Handle<Object> val;
  Handle<Name> key = isolate->factory()->length_string();
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, val, JSReceiver::GetProperty(isolate, object, key), Object);
  return Object::ToLength(isolate, val);
}

// static
Maybe<bool> JSReceiver::HasProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::HasProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                    it->GetName());
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (result.IsNothing()) return Nothing<bool>();
        if (result.FromJust() != ABSENT) return Just(true);
        break;
      }
      case LookupIterator::ACCESS_CHECK: {
        if (it->HasAccess()) break;
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
        if (result.IsNothing()) return Nothing<bool>();
        return Just(result.FromJust() != ABSENT);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        // TypedArray out-of-bounds access.
        return Just(false);
      case LookupIterator::ACCESSOR:
      case LookupIterator::DATA:
        return Just(true);
    }
  }
  return Just(false);
}

// static
Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       Handle<Name> name) {
  if (object->IsJSModuleNamespace()) {
    PropertyDescriptor desc;
    return JSReceiver::GetOwnPropertyDescriptor(object->GetIsolate(), object,
                                                name, &desc);
  }

  if (object->IsJSObject()) {  // Shortcut.
    LookupIterator it = LookupIterator::PropertyOrElement(
        object->GetIsolate(), object, name, object, LookupIterator::OWN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, name);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}

// static
MaybeHandle<Object> Object::GetProperty(LookupIterator* it,
                                        OnNonExistent on_non_existent) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY: {
        bool was_found;
        MaybeHandle<Object> result =
            JSProxy::GetProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                 it->GetName(), it->GetReceiver(), &was_found);
        if (!was_found) it->NotFound();
        return result;
      }
      case LookupIterator::INTERCEPTOR: {
        bool done;
        Handle<Object> result;
        ASSIGN_RETURN_ON_EXCEPTION(
            it->isolate(), result,
            JSObject::GetPropertyWithInterceptor(it, &done), Object);
        if (done) return result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        return JSObject::GetPropertyWithFailedAccessCheck(it);
      case LookupIterator::ACCESSOR:
        return GetPropertyWithAccessor(it);
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::DATA:
        return it->GetDataValue();
    }
  }

  if (on_non_existent == OnNonExistent::kThrowReferenceError) {
    THROW_NEW_ERROR(it->isolate(),
                    NewReferenceError(MessageTemplate::kNotDefined, it->name()),
                    Object);
  }
  return it->isolate()->factory()->undefined_value();
}


// static
MaybeHandle<Object> JSProxy::GetProperty(Isolate* isolate,
                                         Handle<JSProxy> proxy,
                                         Handle<Name> name,
                                         Handle<Object> receiver,
                                         bool* was_found) {
  *was_found = true;

  DCHECK(!name->IsPrivate());
  STACK_CHECK(isolate, MaybeHandle<Object>());
  Handle<Name> trap_name = isolate->factory()->get_string();
  // 1. Assert: IsPropertyKey(P) is true.
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyRevoked, trap_name),
                    Object);
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 6. Let trap be ? GetMethod(handler, "get").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name), Object);
  // 7. If trap is undefined, then
  if (trap->IsUndefined(isolate)) {
    // 7.a Return target.[[Get]](P, Receiver).
    LookupIterator it =
        LookupIterator::PropertyOrElement(isolate, receiver, name, target);
    MaybeHandle<Object> result = Object::GetProperty(&it);
    *was_found = it.IsFound();
    return result;
  }
  // 8. Let trapResult be ? Call(trap, handler, «target, P, Receiver»).
  Handle<Object> trap_result;
  Handle<Object> args[] = {target, name, receiver};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args), Object);

  MaybeHandle<Object> result =
      JSProxy::CheckGetSetTrapResult(isolate, name, target, trap_result, kGet);
  if (result.is_null()) {
    return result;
  }

  // 11. Return trap_result
  return trap_result;
}

// static
MaybeHandle<Object> JSProxy::CheckGetSetTrapResult(Isolate* isolate,
                                                   Handle<Name> name,
                                                   Handle<JSReceiver> target,
                                                   Handle<Object> trap_result,
                                                   AccessKind access_kind) {
  // 9. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> target_found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN_NULL(target_found);
  // 10. If targetDesc is not undefined, then
  if (target_found.FromJust()) {
    // 10.a. If IsDataDescriptor(targetDesc) and targetDesc.[[Configurable]] is
    //       false and targetDesc.[[Writable]] is false, then
    // 10.a.i. If SameValue(trapResult, targetDesc.[[Value]]) is false,
    //        throw a TypeError exception.
    bool inconsistent = PropertyDescriptor::IsDataDescriptor(&target_desc) &&
                        !target_desc.configurable() &&
                        !target_desc.writable() &&
                        !trap_result->SameValue(*target_desc.value());
    if (inconsistent) {
      if (access_kind == kGet) {
        THROW_NEW_ERROR(
            isolate,
            NewTypeError(MessageTemplate::kProxyGetNonConfigurableData, name,
                         target_desc.value(), trap_result),
            Object);
      } else {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kProxySetFrozenData, name));
        return MaybeHandle<Object>();
      }
    }
    // 10.b. If IsAccessorDescriptor(targetDesc) and targetDesc.[[Configurable]]
    //       is false and targetDesc.[[Get]] is undefined, then
    // 10.b.i. If trapResult is not undefined, throw a TypeError exception.
    if (access_kind == kGet) {
      inconsistent = PropertyDescriptor::IsAccessorDescriptor(&target_desc) &&
                     !target_desc.configurable() &&
                     target_desc.get()->IsUndefined(isolate) &&
                     !trap_result->IsUndefined(isolate);
    } else {
      inconsistent = PropertyDescriptor::IsAccessorDescriptor(&target_desc) &&
                     !target_desc.configurable() &&
                     target_desc.set()->IsUndefined(isolate);
    }
    if (inconsistent) {
      if (access_kind == kGet) {
        THROW_NEW_ERROR(
            isolate,
            NewTypeError(MessageTemplate::kProxyGetNonConfigurableAccessor,
                         name, trap_result),
            Object);
      } else {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kProxySetFrozenAccessor, name));
        return MaybeHandle<Object>();
      }
    }
  }
  return isolate->factory()->undefined_value();
}


Handle<Object> JSReceiver::GetDataProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::INTERCEPTOR:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        // Support calling this method without an active context, but refuse
        // access to access-checked objects in that case.
        if (it->isolate()->context() != nullptr && it->HasAccess()) continue;
        V8_FALLTHROUGH;
      case LookupIterator::JSPROXY:
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::ACCESSOR:
        // TODO(verwaest): For now this doesn't call into AccessorInfo, since
        // clients don't need it. Update once relevant.
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::DATA:
        return it->GetDataValue();
    }
  }
  return it->isolate()->factory()->undefined_value();
}


bool Object::ToInt32(int32_t* value) {
  if (IsSmi()) {
    *value = Smi::ToInt(this);
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(this)->value();
    // Check range before conversion to avoid undefined behavior.
    if (num >= kMinInt && num <= kMaxInt && FastI2D(FastD2I(num)) == num) {
      *value = FastD2I(num);
      return true;
    }
  }
  return false;
}

Handle<SharedFunctionInfo> FunctionTemplateInfo::GetOrCreateSharedFunctionInfo(
    Isolate* isolate, Handle<FunctionTemplateInfo> info,
    MaybeHandle<Name> maybe_name) {
  Object* current_info = info->shared_function_info();
  if (current_info->IsSharedFunctionInfo()) {
    return handle(SharedFunctionInfo::cast(current_info), isolate);
  }
  Handle<Name> name;
  Handle<String> name_string;
  if (maybe_name.ToHandle(&name) && name->IsString()) {
    name_string = Handle<String>::cast(name);
  } else if (info->class_name()->IsString()) {
    name_string = handle(String::cast(info->class_name()), isolate);
  } else {
    name_string = isolate->factory()->empty_string();
  }
  FunctionKind function_kind;
  if (info->remove_prototype()) {
    function_kind = kConciseMethod;
  } else {
    function_kind = kNormalFunction;
  }
  Handle<SharedFunctionInfo> result =
      isolate->factory()->NewSharedFunctionInfoForApiFunction(name_string, info,
                                                              function_kind);

  result->set_length(info->length());
  result->DontAdaptArguments();
  DCHECK(result->IsApiFunction());

  info->set_shared_function_info(*result);
  return result;
}

bool FunctionTemplateInfo::IsTemplateFor(Map* map) {
  // There is a constraint on the object; check.
  if (!map->IsJSObjectMap()) return false;
  // Fetch the constructor function of the object.
  Object* cons_obj = map->GetConstructor();
  Object* type;
  if (cons_obj->IsJSFunction()) {
    JSFunction* fun = JSFunction::cast(cons_obj);
    type = fun->shared()->function_data();
  } else if (cons_obj->IsFunctionTemplateInfo()) {
    type = FunctionTemplateInfo::cast(cons_obj);
  } else {
    return false;
  }
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  while (type->IsFunctionTemplateInfo()) {
    if (type == this) return true;
    type = FunctionTemplateInfo::cast(type)->parent_template();
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}


// static
Handle<TemplateList> TemplateList::New(Isolate* isolate, int size) {
  Handle<FixedArray> list =
      isolate->factory()->NewFixedArray(kLengthIndex + size);
  list->set(kLengthIndex, Smi::kZero);
  return Handle<TemplateList>::cast(list);
}

// static
Handle<TemplateList> TemplateList::Add(Isolate* isolate,
                                       Handle<TemplateList> list,
                                       Handle<i::Object> value) {
  STATIC_ASSERT(kFirstElementIndex == 1);
  int index = list->length() + 1;
  Handle<i::FixedArray> fixed_array = Handle<FixedArray>::cast(list);
  fixed_array = FixedArray::SetAndGrow(isolate, fixed_array, index, value);
  fixed_array->set(kLengthIndex, Smi::FromInt(index));
  return Handle<TemplateList>::cast(fixed_array);
}

// static
MaybeHandle<JSObject> JSObject::New(Handle<JSFunction> constructor,
                                    Handle<JSReceiver> new_target,
                                    Handle<AllocationSite> site) {
  // If called through new, new.target can be:
  // - a subclass of constructor,
  // - a proxy wrapper around constructor, or
  // - the constructor itself.
  // If called through Reflect.construct, it's guaranteed to be a constructor.
  Isolate* const isolate = constructor->GetIsolate();
  DCHECK(constructor->IsConstructor());
  DCHECK(new_target->IsConstructor());
  DCHECK(!constructor->has_initial_map() ||
         constructor->initial_map()->instance_type() != JS_FUNCTION_TYPE);

  Handle<Map> initial_map;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, initial_map,
      JSFunction::GetDerivedMap(isolate, constructor, new_target), JSObject);
  Handle<JSObject> result =
      isolate->factory()->NewJSObjectFromMap(initial_map, NOT_TENURED, site);
  if (initial_map->is_dictionary_map()) {
    Handle<NameDictionary> dictionary =
        NameDictionary::New(isolate, NameDictionary::kInitialCapacity);
    result->SetProperties(*dictionary);
  }
  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();
  return result;
}

// 9.1.12 ObjectCreate ( proto [ , internalSlotsList ] )
// Notice: This is NOT 19.1.2.2 Object.create ( O, Properties )
MaybeHandle<JSObject> JSObject::ObjectCreate(Isolate* isolate,
                                             Handle<Object> prototype) {
  // Generate the map with the specified {prototype} based on the Object
  // function's initial map from the current native context.
  // TODO(bmeurer): Use a dedicated cache for Object.create; think about
  // slack tracking for Object.create.
  Handle<Map> map =
      Map::GetObjectCreateMap(isolate, Handle<HeapObject>::cast(prototype));

  // Actually allocate the object.
  Handle<JSObject> object;
  if (map->is_dictionary_map()) {
    object = isolate->factory()->NewSlowJSObjectFromMap(map);
  } else {
    object = isolate->factory()->NewJSObjectFromMap(map);
  }
  return object;
}

void JSObject::EnsureWritableFastElements(Handle<JSObject> object) {
  DCHECK(object->HasSmiOrObjectElements() ||
         object->HasFastStringWrapperElements());
  FixedArray* raw_elems = FixedArray::cast(object->elements());
  Heap* heap = object->GetHeap();
  if (raw_elems->map() != ReadOnlyRoots(heap).fixed_cow_array_map()) return;
  Isolate* isolate = heap->isolate();
  Handle<FixedArray> elems(raw_elems, isolate);
  Handle<FixedArray> writable_elems = isolate->factory()->CopyFixedArrayWithMap(
      elems, isolate->factory()->fixed_array_map());
  object->set_elements(*writable_elems);
  isolate->counters()->cow_arrays_converted()->Increment();
}

int JSObject::GetHeaderSize(InstanceType type,
                            bool function_has_prototype_slot) {
  switch (type) {
    case JS_OBJECT_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_GENERATOR_OBJECT_TYPE:
      return JSGeneratorObject::kSize;
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
      return JSAsyncGeneratorObject::kSize;
    case JS_GLOBAL_PROXY_TYPE:
      return JSGlobalProxy::kSize;
    case JS_GLOBAL_OBJECT_TYPE:
      return JSGlobalObject::kSize;
    case JS_BOUND_FUNCTION_TYPE:
      return JSBoundFunction::kSize;
    case JS_FUNCTION_TYPE:
      return JSFunction::GetHeaderSize(function_has_prototype_slot);
    case JS_VALUE_TYPE:
      return JSValue::kSize;
    case JS_DATE_TYPE:
      return JSDate::kSize;
    case JS_ARRAY_TYPE:
      return JSArray::kSize;
    case JS_ARRAY_BUFFER_TYPE:
      return JSArrayBuffer::kSize;
    case JS_ARRAY_ITERATOR_TYPE:
      return JSArrayIterator::kSize;
    case JS_TYPED_ARRAY_TYPE:
      return JSTypedArray::kSize;
    case JS_DATA_VIEW_TYPE:
      return JSDataView::kSize;
    case JS_SET_TYPE:
      return JSSet::kSize;
    case JS_MAP_TYPE:
      return JSMap::kSize;
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      return JSSetIterator::kSize;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      return JSMapIterator::kSize;
    case JS_WEAK_MAP_TYPE:
      return JSWeakMap::kSize;
    case JS_WEAK_SET_TYPE:
      return JSWeakSet::kSize;
    case JS_PROMISE_TYPE:
      return JSPromise::kSize;
    case JS_REGEXP_TYPE:
      return JSRegExp::kSize;
    case JS_REGEXP_STRING_ITERATOR_TYPE:
      return JSRegExpStringIterator::kSize;
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_MESSAGE_OBJECT_TYPE:
      return JSMessageObject::kSize;
    case JS_ARGUMENTS_TYPE:
      return JSObject::kHeaderSize;
    case JS_ERROR_TYPE:
      return JSObject::kHeaderSize;
    case JS_STRING_ITERATOR_TYPE:
      return JSStringIterator::kSize;
    case JS_MODULE_NAMESPACE_TYPE:
      return JSModuleNamespace::kHeaderSize;
#ifdef V8_INTL_SUPPORT
    case JS_INTL_V8_BREAK_ITERATOR_TYPE:
      return JSV8BreakIterator::kSize;
    case JS_INTL_COLLATOR_TYPE:
      return JSCollator::kSize;
    case JS_INTL_DATE_TIME_FORMAT_TYPE:
      return JSDateTimeFormat::kSize;
    case JS_INTL_LIST_FORMAT_TYPE:
      return JSListFormat::kSize;
    case JS_INTL_LOCALE_TYPE:
      return JSLocale::kSize;
    case JS_INTL_NUMBER_FORMAT_TYPE:
      return JSNumberFormat::kSize;
    case JS_INTL_PLURAL_RULES_TYPE:
      return JSPluralRules::kSize;
    case JS_INTL_RELATIVE_TIME_FORMAT_TYPE:
      return JSRelativeTimeFormat::kSize;
    case JS_INTL_SEGMENTER_TYPE:
      return JSSegmenter::kSize;
#endif  // V8_INTL_SUPPORT
    case WASM_GLOBAL_TYPE:
      return WasmGlobalObject::kSize;
    case WASM_INSTANCE_TYPE:
      return WasmInstanceObject::kSize;
    case WASM_MEMORY_TYPE:
      return WasmMemoryObject::kSize;
    case WASM_MODULE_TYPE:
      return WasmModuleObject::kSize;
    case WASM_TABLE_TYPE:
      return WasmTableObject::kSize;
    default:
      UNREACHABLE();
  }
}

// ES6 9.5.1
// static
MaybeHandle<Object> JSProxy::GetPrototype(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> trap_name = isolate->factory()->getPrototypeOf_string();

  STACK_CHECK(isolate, MaybeHandle<Object>());

  // 1. Let handler be the value of the [[ProxyHandler]] internal slot.
  // 2. If handler is null, throw a TypeError exception.
  // 3. Assert: Type(handler) is Object.
  // 4. Let target be the value of the [[ProxyTarget]] internal slot.
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyRevoked, trap_name),
                    Object);
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  // 5. Let trap be ? GetMethod(handler, "getPrototypeOf").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, trap, GetMethod(handler, trap_name),
                             Object);
  // 6. If trap is undefined, then return target.[[GetPrototypeOf]]().
  if (trap->IsUndefined(isolate)) {
    return JSReceiver::GetPrototype(isolate, target);
  }
  // 7. Let handlerProto be ? Call(trap, handler, «target»).
  Handle<Object> argv[] = {target};
  Handle<Object> handler_proto;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, handler_proto,
      Execution::Call(isolate, trap, handler, arraysize(argv), argv), Object);
  // 8. If Type(handlerProto) is neither Object nor Null, throw a TypeError.
  if (!(handler_proto->IsJSReceiver() || handler_proto->IsNull(isolate))) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyGetPrototypeOfInvalid),
                    Object);
  }
  // 9. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> is_extensible = JSReceiver::IsExtensible(target);
  MAYBE_RETURN_NULL(is_extensible);
  // 10. If extensibleTarget is true, return handlerProto.
  if (is_extensible.FromJust()) return handler_proto;
  // 11. Let targetProto be ? target.[[GetPrototypeOf]]().
  Handle<Object> target_proto;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, target_proto,
                             JSReceiver::GetPrototype(isolate, target), Object);
  // 12. If SameValue(handlerProto, targetProto) is false, throw a TypeError.
  if (!handler_proto->SameValue(*target_proto)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kProxyGetPrototypeOfNonExtensible),
        Object);
  }
  // 13. Return handlerProto.
  return handler_proto;
}

MaybeHandle<Object> Object::GetPropertyWithAccessor(LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<Object> structure = it->GetAccessors();
  Handle<Object> receiver = it->GetReceiver();
  // In case of global IC, the receiver is the global object. Replace by the
  // global proxy.
  if (receiver->IsJSGlobalObject()) {
    receiver = handle(JSGlobalObject::cast(*receiver)->global_proxy(), isolate);
  }

  // We should never get here to initialize a const with the hole value since a
  // const declaration would conflict with the getter.
  DCHECK(!structure->IsForeign());

  // API style callbacks.
  Handle<JSObject> holder = it->GetHolder<JSObject>();
  if (structure->IsAccessorInfo()) {
    Handle<Name> name = it->GetName();
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                                   name, receiver),
                      Object);
    }

    if (!info->has_getter()) return isolate->factory()->undefined_value();

    if (info->is_sloppy() && !receiver->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, receiver,
                                 Object::ConvertReceiver(isolate, receiver),
                                 Object);
    }

    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   kDontThrow);
    Handle<Object> result = args.CallAccessorGetter(info, name);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (result.is_null()) return isolate->factory()->undefined_value();
    Handle<Object> reboxed_result = handle(*result, isolate);
    if (info->replace_on_access() && receiver->IsJSReceiver()) {
      RETURN_ON_EXCEPTION(isolate,
                          Accessors::ReplaceAccessorWithDataProperty(
                              receiver, holder, name, result),
                          Object);
    }
    return reboxed_result;
  }

  // AccessorPair with 'cached' private property.
  if (it->TryLookupCachedProperty()) {
    return Object::GetProperty(it);
  }

  // Regular accessor.
  Handle<Object> getter(AccessorPair::cast(*structure)->getter(), isolate);
  if (getter->IsFunctionTemplateInfo()) {
    SaveContext save(isolate);
    isolate->set_context(*holder->GetCreationContext());
    return Builtins::InvokeApiFunction(
        isolate, false, Handle<FunctionTemplateInfo>::cast(getter), receiver, 0,
        nullptr, isolate->factory()->undefined_value());
  } else if (getter->IsCallable()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return Object::GetPropertyWithDefinedGetter(
        receiver, Handle<JSReceiver>::cast(getter));
  }
  // Getter is not a function.
  return isolate->factory()->undefined_value();
}

// static
Address AccessorInfo::redirect(Address address, AccessorComponent component) {
  ApiFunction fun(address);
  DCHECK_EQ(ACCESSOR_GETTER, component);
  ExternalReference::Type type = ExternalReference::DIRECT_GETTER_CALL;
  return ExternalReference::Create(&fun, type).address();
}

Address AccessorInfo::redirected_getter() const {
  Address accessor = v8::ToCData<Address>(getter());
  if (accessor == kNullAddress) return kNullAddress;
  return redirect(accessor, ACCESSOR_GETTER);
}

Address CallHandlerInfo::redirected_callback() const {
  Address address = v8::ToCData<Address>(callback());
  ApiFunction fun(address);
  ExternalReference::Type type = ExternalReference::DIRECT_API_CALL;
  return ExternalReference::Create(&fun, type).address();
}

bool AccessorInfo::IsCompatibleReceiverMap(Handle<AccessorInfo> info,
                                           Handle<Map> map) {
  if (!info->HasExpectedReceiverType()) return true;
  if (!map->IsJSObjectMap()) return false;
  return FunctionTemplateInfo::cast(info->expected_receiver_type())
      ->IsTemplateFor(*map);
}

Maybe<bool> Object::SetPropertyWithAccessor(LookupIterator* it,
                                            Handle<Object> value,
                                            ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  Handle<Object> structure = it->GetAccessors();
  Handle<Object> receiver = it->GetReceiver();
  // In case of global IC, the receiver is the global object. Replace by the
  // global proxy.
  if (receiver->IsJSGlobalObject()) {
    receiver = handle(JSGlobalObject::cast(*receiver)->global_proxy(), isolate);
  }

  // We should never get here to initialize a const with the hole value since a
  // const declaration would conflict with the setter.
  DCHECK(!structure->IsForeign());

  // API style callbacks.
  Handle<JSObject> holder = it->GetHolder<JSObject>();
  if (structure->IsAccessorInfo()) {
    Handle<Name> name = it->GetName();
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kIncompatibleMethodReceiver, name, receiver));
      return Nothing<bool>();
    }

    if (!info->has_setter()) {
      // TODO(verwaest): We should not get here anymore once all AccessorInfos
      // are marked as special_data_property. They cannot both be writable and
      // not have a setter.
      return Just(true);
    }

    if (info->is_sloppy() && !receiver->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, receiver, Object::ConvertReceiver(isolate, receiver),
          Nothing<bool>());
    }

    // The actual type of setter callback is either
    // v8::AccessorNameSetterCallback or
    // i::Accesors::AccessorNameBooleanSetterCallback, depending on whether the
    // AccessorInfo was created by the API or internally (see accessors.cc).
    // Here we handle both cases using GenericNamedPropertySetterCallback and
    // its Call method.
    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   should_throw);
    Handle<Object> result = args.CallAccessorSetter(info, name, value);
    // In the case of AccessorNameSetterCallback, we know that the result value
    // cannot have been set, so the result of Call will be null.  In the case of
    // AccessorNameBooleanSetterCallback, the result will either be null
    // (signalling an exception) or a boolean Oddball.
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    if (result.is_null()) return Just(true);
    DCHECK(result->BooleanValue(isolate) || should_throw == kDontThrow);
    return Just(result->BooleanValue(isolate));
  }

  // Regular accessor.
  Handle<Object> setter(AccessorPair::cast(*structure)->setter(), isolate);
  if (setter->IsFunctionTemplateInfo()) {
    SaveContext save(isolate);
    isolate->set_context(*holder->GetCreationContext());
    Handle<Object> argv[] = {value};
    RETURN_ON_EXCEPTION_VALUE(
        isolate, Builtins::InvokeApiFunction(
                     isolate, false, Handle<FunctionTemplateInfo>::cast(setter),
                     receiver, arraysize(argv), argv,
                     isolate->factory()->undefined_value()),
        Nothing<bool>());
    return Just(true);
  } else if (setter->IsCallable()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return SetPropertyWithDefinedSetter(
        receiver, Handle<JSReceiver>::cast(setter), value, should_throw);
  }

  RETURN_FAILURE(isolate, should_throw,
                 NewTypeError(MessageTemplate::kNoSetterInCallback,
                              it->GetName(), it->GetHolder<JSObject>()));
}


MaybeHandle<Object> Object::GetPropertyWithDefinedGetter(
    Handle<Object> receiver,
    Handle<JSReceiver> getter) {
  Isolate* isolate = getter->GetIsolate();

  // Platforms with simulators like arm/arm64 expose a funny issue. If the
  // simulator has a separate JS stack pointer from the C++ stack pointer, it
  // can miss C++ stack overflows in the stack guard at the start of JavaScript
  // functions. It would be very expensive to check the C++ stack pointer at
  // that location. The best solution seems to be to break the impasse by
  // adding checks at possible recursion points. What's more, we don't put
  // this stack check behind the USE_SIMULATOR define in order to keep
  // behavior the same between hardware and simulators.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    isolate->StackOverflow();
    return MaybeHandle<Object>();
  }

  return Execution::Call(isolate, getter, receiver, 0, nullptr);
}


Maybe<bool> Object::SetPropertyWithDefinedSetter(Handle<Object> receiver,
                                                 Handle<JSReceiver> setter,
                                                 Handle<Object> value,
                                                 ShouldThrow should_throw) {
  Isolate* isolate = setter->GetIsolate();

  Handle<Object> argv[] = { value };
  RETURN_ON_EXCEPTION_VALUE(isolate, Execution::Call(isolate, setter, receiver,
                                                     arraysize(argv), argv),
                            Nothing<bool>());
  return Just(true);
}


// static
bool JSObject::AllCanRead(LookupIterator* it) {
  // Skip current iteration, it's in state ACCESS_CHECK or INTERCEPTOR, both of
  // which have already been checked.
  DCHECK(it->state() == LookupIterator::ACCESS_CHECK ||
         it->state() == LookupIterator::INTERCEPTOR);
  for (it->Next(); it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      auto accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors)->all_can_read()) return true;
      }
    } else if (it->state() == LookupIterator::INTERCEPTOR) {
      if (it->GetInterceptor()->all_can_read()) return true;
    } else if (it->state() == LookupIterator::JSPROXY) {
      // Stop lookupiterating. And no, AllCanNotRead.
      return false;
    }
  }
  return false;
}

namespace {

MaybeHandle<Object> GetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor, bool* done) {
  *done = false;
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->getter()->IsUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, receiver, Object::ConvertReceiver(isolate, receiver), Object);
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, kDontThrow);

  if (it->IsElement()) {
    result = args.CallIndexedGetter(interceptor, it->index());
  } else {
    result = args.CallNamedGetter(interceptor, it->name());
  }

  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (result.is_null()) return isolate->factory()->undefined_value();
  *done = true;
  // Rebox handle before return
  return handle(*result, isolate);
}

Maybe<PropertyAttributes> GetPropertyAttributesWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);
  HandleScope scope(isolate);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  DCHECK_IMPLIES(!it->IsElement() && it->name()->IsSymbol(),
                 interceptor->can_intercept_symbols());
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<PropertyAttributes>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, kDontThrow);
  if (!interceptor->query()->IsUndefined(isolate)) {
    Handle<Object> result;
    if (it->IsElement()) {
      result = args.CallIndexedQuery(interceptor, it->index());
    } else {
      result = args.CallNamedQuery(interceptor, it->name());
    }
    if (!result.is_null()) {
      int32_t value;
      CHECK(result->ToInt32(&value));
      return Just(static_cast<PropertyAttributes>(value));
    }
  } else if (!interceptor->getter()->IsUndefined(isolate)) {
    // TODO(verwaest): Use GetPropertyWithInterceptor?
    Handle<Object> result;
    if (it->IsElement()) {
      result = args.CallIndexedGetter(interceptor, it->index());
    } else {
      result = args.CallNamedGetter(interceptor, it->name());
    }
    if (!result.is_null()) return Just(DONT_ENUM);
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}

Maybe<bool> SetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    ShouldThrow should_throw, Handle<Object> value) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->setter()->IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  if (it->IsElement()) {
    // TODO(neis): In the future, we may want to actually return the
    // interceptor's result, which then should be a boolean.
    result = !args.CallIndexedSetter(interceptor, it->index(), value).is_null();
  } else {
    result = !args.CallNamedSetter(interceptor, it->name(), value).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

Maybe<bool> DefinePropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    ShouldThrow should_throw, PropertyDescriptor& desc) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->definer()->IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  std::unique_ptr<v8::PropertyDescriptor> descriptor(
      new v8::PropertyDescriptor());
  if (PropertyDescriptor::IsAccessorDescriptor(&desc)) {
    descriptor.reset(new v8::PropertyDescriptor(
        v8::Utils::ToLocal(desc.get()), v8::Utils::ToLocal(desc.set())));
  } else if (PropertyDescriptor::IsDataDescriptor(&desc)) {
    if (desc.has_writable()) {
      descriptor.reset(new v8::PropertyDescriptor(
          v8::Utils::ToLocal(desc.value()), desc.writable()));
    } else {
      descriptor.reset(
          new v8::PropertyDescriptor(v8::Utils::ToLocal(desc.value())));
    }
  }
  if (desc.has_enumerable()) {
    descriptor->set_enumerable(desc.enumerable());
  }
  if (desc.has_configurable()) {
    descriptor->set_configurable(desc.configurable());
  }

  if (it->IsElement()) {
    result = !args.CallIndexedDefiner(interceptor, it->index(), *descriptor)
                  .is_null();
  } else {
    result =
        !args.CallNamedDefiner(interceptor, it->name(), *descriptor).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

}  // namespace

MaybeHandle<Object> JSObject::GetPropertyWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return GetPropertyWithAccessor(it);
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      bool done;
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                                 GetPropertyWithInterceptor(it, &done), Object);
      if (done) return result;
    }

  } else {
    Handle<Object> result;
    bool done;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        GetPropertyWithInterceptorInternal(it, interceptor, &done), Object);
    if (done) return result;
  }

  // Cross-Origin [[Get]] of Well-Known Symbols does not throw, and returns
  // undefined.
  Handle<Name> name = it->GetName();
  if (name->IsSymbol() && Symbol::cast(*name)->is_well_known_symbol()) {
    return it->factory()->undefined_value();
  }

  isolate->ReportFailedAccessCheck(checked);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  return it->factory()->undefined_value();
}


Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return Just(it->property_attributes());
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      auto result = GetPropertyAttributesWithInterceptor(it);
      if (isolate->has_scheduled_exception()) break;
      if (result.IsJust() && result.FromJust() != ABSENT) return result;
    }
  } else {
    Maybe<PropertyAttributes> result =
        GetPropertyAttributesWithInterceptorInternal(it, interceptor);
    if (isolate->has_pending_exception()) return Nothing<PropertyAttributes>();
    if (result.FromMaybe(ABSENT) != ABSENT) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}


// static
bool JSObject::AllCanWrite(LookupIterator* it) {
  for (; it->IsFound() && it->state() != LookupIterator::JSPROXY; it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      Handle<Object> accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors)->all_can_write()) return true;
      }
    }
  }
  return false;
}


Maybe<bool> JSObject::SetPropertyWithFailedAccessCheck(
    LookupIterator* it, Handle<Object> value, ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    if (AllCanWrite(it)) {
      return SetPropertyWithAccessor(it, value, should_throw);
    }
  } else {
    Maybe<bool> result = SetPropertyWithInterceptorInternal(
        it, interceptor, should_throw, value);
    if (isolate->has_pending_exception()) return Nothing<bool>();
    if (result.IsJust()) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  return Just(true);
}


void JSObject::SetNormalizedProperty(Handle<JSObject> object,
                                     Handle<Name> name,
                                     Handle<Object> value,
                                     PropertyDetails details) {
  DCHECK(!object->HasFastProperties());
  DCHECK(name->IsUniqueName());
  Isolate* isolate = object->GetIsolate();

  uint32_t hash = name->Hash();

  if (object->IsJSGlobalObject()) {
    Handle<JSGlobalObject> global_obj = Handle<JSGlobalObject>::cast(object);
    Handle<GlobalDictionary> dictionary(global_obj->global_dictionary(),
                                        isolate);
    int entry = dictionary->FindEntry(ReadOnlyRoots(isolate), name, hash);

    if (entry == GlobalDictionary::kNotFound) {
      DCHECK_IMPLIES(global_obj->map()->is_prototype_map(),
                     Map::IsPrototypeChainInvalidated(global_obj->map()));
      auto cell = isolate->factory()->NewPropertyCell(name);
      cell->set_value(*value);
      auto cell_type = value->IsUndefined(isolate)
                           ? PropertyCellType::kUndefined
                           : PropertyCellType::kConstant;
      details = details.set_cell_type(cell_type);
      value = cell;
      dictionary =
          GlobalDictionary::Add(isolate, dictionary, name, value, details);
      global_obj->set_global_dictionary(*dictionary);
    } else {
      Handle<PropertyCell> cell = PropertyCell::PrepareForValue(
          isolate, dictionary, entry, value, details);
      cell->set_value(*value);
    }
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);

    int entry = dictionary->FindEntry(isolate, name);
    if (entry == NameDictionary::kNotFound) {
      DCHECK_IMPLIES(object->map()->is_prototype_map(),
                     Map::IsPrototypeChainInvalidated(object->map()));
      dictionary =
          NameDictionary::Add(isolate, dictionary, name, value, details);
      object->SetProperties(*dictionary);
    } else {
      PropertyDetails original_details = dictionary->DetailsAt(entry);
      int enumeration_index = original_details.dictionary_index();
      DCHECK_GT(enumeration_index, 0);
      details = details.set_index(enumeration_index);
      dictionary->SetEntry(isolate, entry, *name, *value, details);
    }
  }
}

// static
Maybe<bool> JSReceiver::HasInPrototypeChain(Isolate* isolate,
                                            Handle<JSReceiver> object,
                                            Handle<Object> proto) {
  PrototypeIterator iter(isolate, object, kStartAtReceiver);
  while (true) {
    if (!iter.AdvanceFollowingProxies()) return Nothing<bool>();
    if (iter.IsAtEnd()) return Just(false);
    if (PrototypeIterator::GetCurrent(iter).is_identical_to(proto)) {
      return Just(true);
    }
  }
}

namespace {

bool HasExcludedProperty(
    const ScopedVector<Handle<Object>>* excluded_properties,
    Handle<Object> search_element) {
  // TODO(gsathya): Change this to be a hashtable.
  for (int i = 0; i < excluded_properties->length(); i++) {
    if (search_element->SameValue(*excluded_properties->at(i))) {
      return true;
    }
  }

  return false;
}

V8_WARN_UNUSED_RESULT Maybe<bool> FastAssign(
    Handle<JSReceiver> target, Handle<Object> source,
    const ScopedVector<Handle<Object>>* excluded_properties, bool use_set) {
  // Non-empty strings are the only non-JSReceivers that need to be handled
  // explicitly by Object.assign.
  if (!source->IsJSReceiver()) {
    return Just(!source->IsString() || String::cast(*source)->length() == 0);
  }

  // If the target is deprecated, the object will be updated on first store. If
  // the source for that store equals the target, this will invalidate the
  // cached representation of the source. Preventively upgrade the target.
  // Do this on each iteration since any property load could cause deprecation.
  if (target->map()->is_deprecated()) {
    JSObject::MigrateInstance(Handle<JSObject>::cast(target));
  }

  Isolate* isolate = target->GetIsolate();
  Handle<Map> map(JSReceiver::cast(*source)->map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> from = Handle<JSObject>::cast(source);
  if (from->elements() != ReadOnlyRoots(isolate).empty_fixed_array()) {
    return Just(false);
  }

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int length = map->NumberOfOwnDescriptors();

  bool stable = true;

  for (int i = 0; i < length; i++) {
    Handle<Name> next_key(descriptors->GetKey(i), isolate);
    Handle<Object> prop_value;
    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetStrongValue(i), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex index = FieldIndex::ForDescriptor(*map, i);
          prop_value = JSObject::FastPropertyAt(from, representation, index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value,
            JSReceiver::GetProperty(isolate, from, next_key), Nothing<bool>());
        stable = from->map() == *map;
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(from, next_key, from,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }

    if (use_set) {
      LookupIterator it(target, next_key, target);
      Maybe<bool> result = Object::SetProperty(
          &it, prop_value, LanguageMode::kStrict, StoreOrigin::kNamed);
      if (result.IsNothing()) return result;
      if (stable) stable = from->map() == *map;
    } else {
      if (excluded_properties != nullptr &&
          HasExcludedProperty(excluded_properties, next_key)) {
        continue;
      }

      // 4a ii 2. Perform ? CreateDataProperty(target, nextKey, propValue).
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, target, next_key, &success, LookupIterator::OWN);
      CHECK(success);
      CHECK(JSObject::CreateDataProperty(&it, prop_value, kThrowOnError)
                .FromJust());
    }
  }

  return Just(true);
}
}  // namespace

// static
Maybe<bool> JSReceiver::SetOrCopyDataProperties(
    Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
    const ScopedVector<Handle<Object>>* excluded_properties, bool use_set) {
  Maybe<bool> fast_assign =
      FastAssign(target, source, excluded_properties, use_set);
  if (fast_assign.IsNothing()) return Nothing<bool>();
  if (fast_assign.FromJust()) return Just(true);

  Handle<JSReceiver> from = Object::ToObject(isolate, source).ToHandleChecked();
  // 3b. Let keys be ? from.[[OwnPropertyKeys]]().
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(from, KeyCollectionMode::kOwnOnly, ALL_PROPERTIES,
                              GetKeysConversion::kKeepNumbers),
      Nothing<bool>());

  // 4. Repeat for each element nextKey of keys in List order,
  for (int j = 0; j < keys->length(); ++j) {
    Handle<Object> next_key(keys->get(j), isolate);
    // 4a i. Let desc be ? from.[[GetOwnProperty]](nextKey).
    PropertyDescriptor desc;
    Maybe<bool> found =
        JSReceiver::GetOwnPropertyDescriptor(isolate, from, next_key, &desc);
    if (found.IsNothing()) return Nothing<bool>();
    // 4a ii. If desc is not undefined and desc.[[Enumerable]] is true, then
    if (found.FromJust() && desc.enumerable()) {
      // 4a ii 1. Let propValue be ? Get(from, nextKey).
      Handle<Object> prop_value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value,
          Runtime::GetObjectProperty(isolate, from, next_key), Nothing<bool>());

      if (use_set) {
        // 4c ii 2. Let status be ? Set(to, nextKey, propValue, true).
        Handle<Object> status;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, status,
            Runtime::SetObjectProperty(isolate, target, next_key, prop_value,
                                       LanguageMode::kStrict,
                                       StoreOrigin::kMaybeKeyed),
            Nothing<bool>());
      } else {
        if (excluded_properties != nullptr &&
            HasExcludedProperty(excluded_properties, next_key)) {
          continue;
        }

        // 4a ii 2. Perform ! CreateDataProperty(target, nextKey, propValue).
        bool success;
        LookupIterator it = LookupIterator::PropertyOrElement(
            isolate, target, next_key, &success, LookupIterator::OWN);
        CHECK(success);
        CHECK(JSObject::CreateDataProperty(&it, prop_value, kThrowOnError)
                  .FromJust());
      }
    }
  }

  return Just(true);
}

Map* Object::GetPrototypeChainRootMap(Isolate* isolate) const {
  DisallowHeapAllocation no_alloc;
  if (IsSmi()) {
    Context* native_context = isolate->context()->native_context();
    return native_context->number_function()->initial_map();
  }

  const HeapObject* heap_object = HeapObject::cast(this);
  return heap_object->map()->GetPrototypeChainRootMap(isolate);
}

Map* Map::GetPrototypeChainRootMap(Isolate* isolate) const {
  DisallowHeapAllocation no_alloc;
  if (IsJSReceiverMap()) {
    return const_cast<Map*>(this);
  }
  int constructor_function_index = GetConstructorFunctionIndex();
  if (constructor_function_index != Map::kNoConstructorFunctionIndex) {
    Context* native_context = isolate->context()->native_context();
    JSFunction* constructor_function =
        JSFunction::cast(native_context->get(constructor_function_index));
    return constructor_function->initial_map();
  }
  return ReadOnlyRoots(isolate).null_value()->map();
}

// static
Smi* Object::GetOrCreateHash(Isolate* isolate, Object* key) {
  DisallowHeapAllocation no_gc;
  return key->GetOrCreateHash(isolate);
}

Smi* Object::GetOrCreateHash(Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  Object* hash = Object::GetSimpleHash(this);
  if (hash->IsSmi()) return Smi::cast(hash);

  DCHECK(IsJSReceiver());
  return JSReceiver::cast(this)->GetOrCreateIdentityHash(isolate);
}


bool Object::SameValue(Object* other) {
  if (other == this) return true;

  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    // SameValue(NaN, NaN) is true.
    if (this_value != other_value) {
      return std::isnan(this_value) && std::isnan(other_value);
    }
    // SameValue(0.0, -0.0) is false.
    return (std::signbit(this_value) == std::signbit(other_value));
  }
  if (IsString() && other->IsString()) {
    return String::cast(this)->Equals(String::cast(other));
  }
  if (IsBigInt() && other->IsBigInt()) {
    return BigInt::EqualToBigInt(BigInt::cast(this), BigInt::cast(other));
  }
  return false;
}


bool Object::SameValueZero(Object* other) {
  if (other == this) return true;

  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    // +0 == -0 is true
    return this_value == other_value ||
           (std::isnan(this_value) && std::isnan(other_value));
  }
  if (IsString() && other->IsString()) {
    return String::cast(this)->Equals(String::cast(other));
  }
  if (IsBigInt() && other->IsBigInt()) {
    return BigInt::EqualToBigInt(BigInt::cast(this), BigInt::cast(other));
  }
  return false;
}


MaybeHandle<Object> Object::ArraySpeciesConstructor(
    Isolate* isolate, Handle<Object> original_array) {
  Handle<Object> default_species = isolate->array_function();
  if (original_array->IsJSArray() &&
      Handle<JSArray>::cast(original_array)->HasArrayPrototype(isolate) &&
      isolate->IsArraySpeciesLookupChainIntact()) {
    return default_species;
  }
  Handle<Object> constructor = isolate->factory()->undefined_value();
  Maybe<bool> is_array = Object::IsArray(original_array);
  MAYBE_RETURN_NULL(is_array);
  if (is_array.FromJust()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, constructor,
        Object::GetProperty(isolate, original_array,
                            isolate->factory()->constructor_string()),
        Object);
    if (constructor->IsConstructor()) {
      Handle<Context> constructor_context;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, constructor_context,
          JSReceiver::GetFunctionRealm(Handle<JSReceiver>::cast(constructor)),
          Object);
      if (*constructor_context != *isolate->native_context() &&
          *constructor == constructor_context->array_function()) {
        constructor = isolate->factory()->undefined_value();
      }
    }
    if (constructor->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, constructor,
          JSReceiver::GetProperty(isolate,
                                  Handle<JSReceiver>::cast(constructor),
                                  isolate->factory()->species_symbol()),
          Object);
      if (constructor->IsNull(isolate)) {
        constructor = isolate->factory()->undefined_value();
      }
    }
  }
  if (constructor->IsUndefined(isolate)) {
    return default_species;
  } else {
    if (!constructor->IsConstructor()) {
      THROW_NEW_ERROR(isolate,
          NewTypeError(MessageTemplate::kSpeciesNotConstructor),
          Object);
    }
    return constructor;
  }
}

// ES6 section 7.3.20 SpeciesConstructor ( O, defaultConstructor )
V8_WARN_UNUSED_RESULT MaybeHandle<Object> Object::SpeciesConstructor(
    Isolate* isolate, Handle<JSReceiver> recv,
    Handle<JSFunction> default_ctor) {
  Handle<Object> ctor_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ctor_obj,
      JSObject::GetProperty(isolate, recv,
                            isolate->factory()->constructor_string()),
      Object);

  if (ctor_obj->IsUndefined(isolate)) return default_ctor;

  if (!ctor_obj->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kConstructorNotReceiver),
                    Object);
  }

  Handle<JSReceiver> ctor = Handle<JSReceiver>::cast(ctor_obj);

  Handle<Object> species;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, species,
      JSObject::GetProperty(isolate, ctor,
                            isolate->factory()->species_symbol()),
      Object);

  if (species->IsNullOrUndefined(isolate)) {
    return default_ctor;
  }

  if (species->IsConstructor()) return species;

  THROW_NEW_ERROR(
      isolate, NewTypeError(MessageTemplate::kSpeciesNotConstructor), Object);
}

bool Object::IterationHasObservableEffects() {
  // Check that this object is an array.
  if (!IsJSArray()) return true;
  JSArray* array = JSArray::cast(this);
  Isolate* isolate = array->GetIsolate();

#ifdef V8_ENABLE_FORCE_SLOW_PATH
  if (isolate->force_slow_path()) return true;
#endif

  // Check that we have the original ArrayPrototype.
  if (!array->map()->prototype()->IsJSObject()) return true;
  JSObject* array_proto = JSObject::cast(array->map()->prototype());
  if (!isolate->is_initial_array_prototype(array_proto)) return true;

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  if (!isolate->IsArrayIteratorLookupChainIntact()) return true;

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  ElementsKind array_kind = array->GetElementsKind();
  if (IsFastPackedElementsKind(array_kind)) return false;

  // For FastHoley kinds, an element access on a hole would cause a lookup on
  // the prototype. This could have different results if the prototype has been
  // changed.
  if (IsHoleyElementsKind(array_kind) &&
      isolate->IsNoElementsProtectorIntact()) {
    return false;
  }
  return true;
}

void Object::ShortPrint(FILE* out) {
  OFStream os(out);
  os << Brief(this);
}


void Object::ShortPrint(StringStream* accumulator) {
  std::ostringstream os;
  os << Brief(this);
  accumulator->Add(os.str().c_str());
}


void Object::ShortPrint(std::ostream& os) { os << Brief(this); }

void MaybeObject::ShortPrint(FILE* out) {
  OFStream os(out);
  os << Brief(this);
}

void MaybeObject::ShortPrint(StringStream* accumulator) {
  std::ostringstream os;
  os << Brief(this);
  accumulator->Add(os.str().c_str());
}

void MaybeObject::ShortPrint(std::ostream& os) { os << Brief(this); }

Brief::Brief(const Object* v)
    : value(MaybeObject::FromObject(const_cast<Object*>(v))) {}

std::ostream& operator<<(std::ostream& os, const Brief& v) {
  // TODO(marja): const-correct HeapObjectShortPrint.
  MaybeObject* maybe_object = const_cast<MaybeObject*>(v.value);
  Smi* smi;
  HeapObject* heap_object;
  if (maybe_object->ToSmi(&smi)) {
    smi->SmiPrint(os);
  } else if (maybe_object->IsCleared()) {
    os << "[cleared]";
  } else if (maybe_object->GetHeapObjectIfWeak(&heap_object)) {
    os << "[weak] ";
    heap_object->HeapObjectShortPrint(os);
  } else if (maybe_object->GetHeapObjectIfStrong(&heap_object)) {
    heap_object->HeapObjectShortPrint(os);
  } else {
    UNREACHABLE();
  }
  return os;
}

void Smi::SmiPrint(std::ostream& os) const {  // NOLINT
  os << value();
}

Handle<String> String::SlowFlatten(Isolate* isolate, Handle<ConsString> cons,
                                   PretenureFlag pretenure) {
  DCHECK_NE(cons->second()->length(), 0);

  // TurboFan can create cons strings with empty first parts.
  while (cons->first()->length() == 0) {
    // We do not want to call this function recursively. Therefore we call
    // String::Flatten only in those cases where String::SlowFlatten is not
    // called again.
    if (cons->second()->IsConsString() && !cons->second()->IsFlat()) {
      cons = handle(ConsString::cast(cons->second()), isolate);
    } else {
      return String::Flatten(isolate, handle(cons->second(), isolate));
    }
  }

  DCHECK(AllowHeapAllocation::IsAllowed());
  int length = cons->length();
  PretenureFlag tenure = Heap::InNewSpace(*cons) ? pretenure : TENURED;
  Handle<SeqString> result;
  if (cons->IsOneByteRepresentation()) {
    Handle<SeqOneByteString> flat = isolate->factory()->NewRawOneByteString(
        length, tenure).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    WriteToFlat(*cons, flat->GetChars(), 0, length);
    result = flat;
  } else {
    Handle<SeqTwoByteString> flat = isolate->factory()->NewRawTwoByteString(
        length, tenure).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    WriteToFlat(*cons, flat->GetChars(), 0, length);
    result = flat;
  }
  cons->set_first(isolate, *result);
  cons->set_second(isolate, ReadOnlyRoots(isolate).empty_string());
  DCHECK(result->IsFlat());
  return result;
}



bool String::MakeExternal(v8::String::ExternalStringResource* resource) {
  DisallowHeapAllocation no_allocation;
  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(this->SupportsExternalization());
  DCHECK(resource->IsCacheable());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    ScopedVector<uc16> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    DCHECK_EQ(0, memcmp(smart_chars.start(), resource->data(),
                        resource->length() * sizeof(smart_chars[0])));
  }
#endif  // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kUncachedSize) return false;
  Isolate* isolate;
  // Read-only strings cannot be made external, since that would mutate the
  // string.
  if (!Isolate::FromWritableHeapObject(this, &isolate)) return false;
  Heap* heap = isolate->heap();
  bool is_one_byte = this->IsOneByteRepresentation();
  bool is_internalized = this->IsInternalizedString();
  bool has_pointers = StringShape(this).IsIndirect();
  if (has_pointers) {
    heap->NotifyObjectLayoutChange(this, size, no_allocation);
  }
  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if the space the existing
  // string occupies is too small for a regular external string.  Instead, we
  // resort to an uncached external string instead, omitting the field caching
  // the address of the backing store.  When we encounter uncached external
  // strings in generated code, we need to bailout to runtime.
  Map* new_map;
  ReadOnlyRoots roots(heap);
  if (size < ExternalString::kSize) {
    if (is_internalized) {
      if (is_one_byte) {
        new_map =
            roots
                .uncached_external_internalized_string_with_one_byte_data_map();
      } else {
        new_map = roots.uncached_external_internalized_string_map();
      }
    } else {
      new_map = is_one_byte
                    ? roots.uncached_external_string_with_one_byte_data_map()
                    : roots.uncached_external_string_map();
    }
  } else {
    new_map =
        is_internalized
            ? (is_one_byte
                   ? roots.external_internalized_string_with_one_byte_data_map()
                   : roots.external_internalized_string_map())
            : (is_one_byte ? roots.external_string_with_one_byte_data_map()
                           : roots.external_string_map());
  }

  // Byte size of the external String object.
  int new_size = this->SizeFromMap(new_map);
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size,
                             ClearRecordedSlots::kNo);
  if (has_pointers) {
    heap->ClearRecordedSlotRange(this->address(), this->address() + new_size);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalTwoByteString* self = ExternalTwoByteString::cast(this);
  self->SetResource(isolate, resource);
  heap->RegisterExternalString(this);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.
  return true;
}


bool String::MakeExternal(v8::String::ExternalOneByteStringResource* resource) {
  DisallowHeapAllocation no_allocation;
  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(this->SupportsExternalization());
  DCHECK(resource->IsCacheable());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    if (this->IsTwoByteRepresentation()) {
      ScopedVector<uint16_t> smart_chars(this->length());
      String::WriteToFlat(this, smart_chars.start(), 0, this->length());
      DCHECK(String::IsOneByte(smart_chars.start(), this->length()));
    }
    ScopedVector<char> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    DCHECK_EQ(0, memcmp(smart_chars.start(), resource->data(),
                        resource->length() * sizeof(smart_chars[0])));
  }
#endif  // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kUncachedSize) return false;
  Isolate* isolate;
  // Read-only strings cannot be made external, since that would mutate the
  // string.
  if (!Isolate::FromWritableHeapObject(this, &isolate)) return false;
  Heap* heap = isolate->heap();
  bool is_internalized = this->IsInternalizedString();
  bool has_pointers = StringShape(this).IsIndirect();

  if (has_pointers) {
    heap->NotifyObjectLayoutChange(this, size, no_allocation);
  }

  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if the space the existing
  // string occupies is too small for a regular external string.  Instead, we
  // resort to an uncached external string instead, omitting the field caching
  // the address of the backing store.  When we encounter uncached external
  // strings in generated code, we need to bailout to runtime.
  Map* new_map;
  ReadOnlyRoots roots(heap);
  if (size < ExternalString::kSize) {
    new_map = is_internalized
                  ? roots.uncached_external_one_byte_internalized_string_map()
                  : roots.uncached_external_one_byte_string_map();
  } else {
    new_map = is_internalized
                  ? roots.external_one_byte_internalized_string_map()
                  : roots.external_one_byte_string_map();
  }

  // Byte size of the external String object.
  int new_size = this->SizeFromMap(new_map);
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size,
                             ClearRecordedSlots::kNo);
  if (has_pointers) {
    heap->ClearRecordedSlotRange(this->address(), this->address() + new_size);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalOneByteString* self = ExternalOneByteString::cast(this);
  self->SetResource(isolate, resource);
  heap->RegisterExternalString(this);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.
  return true;
}

bool String::SupportsExternalization() {
  if (this->IsThinString()) {
    return i::ThinString::cast(this)->actual()->SupportsExternalization();
  }

  Isolate* isolate;
  // RO_SPACE strings cannot be externalized.
  if (!Isolate::FromWritableHeapObject(this, &isolate)) {
    return false;
  }

  // Already an external string.
  if (StringShape(this).IsExternal()) {
    return false;
  }

  return !isolate->heap()->IsInGCPostProcessing();
}

void String::StringShortPrint(StringStream* accumulator, bool show_details) {
  int len = length();
  if (len > kMaxShortPrintLength) {
    accumulator->Add("<Very long string[%u]>", len);
    return;
  }

  if (!LooksValid()) {
    accumulator->Add("<Invalid String>");
    return;
  }

  StringCharacterStream stream(this);

  bool truncated = false;
  if (len > kMaxShortPrintLength) {
    len = kMaxShortPrintLength;
    truncated = true;
  }
  bool one_byte = true;
  for (int i = 0; i < len; i++) {
    uint16_t c = stream.GetNext();

    if (c < 32 || c >= 127) {
      one_byte = false;
    }
  }
  stream.Reset(this);
  if (one_byte) {
    if (show_details) accumulator->Add("<String[%u]: ", length());
    for (int i = 0; i < len; i++) {
      accumulator->Put(static_cast<char>(stream.GetNext()));
    }
    if (show_details) accumulator->Put('>');
  } else {
    // Backslash indicates that the string contains control
    // characters and that backslashes are therefore escaped.
    if (show_details) accumulator->Add("<String[%u]\\: ", length());
    for (int i = 0; i < len; i++) {
      uint16_t c = stream.GetNext();
      if (c == '\n') {
        accumulator->Add("\\n");
      } else if (c == '\r') {
        accumulator->Add("\\r");
      } else if (c == '\\') {
        accumulator->Add("\\\\");
      } else if (c < 32 || c > 126) {
        accumulator->Add("\\x%02x", c);
      } else {
        accumulator->Put(static_cast<char>(c));
      }
    }
    if (truncated) {
      accumulator->Put('.');
      accumulator->Put('.');
      accumulator->Put('.');
    }
    if (show_details) accumulator->Put('>');
  }
  return;
}


void String::PrintUC16(std::ostream& os, int start, int end) {  // NOLINT
  if (end < 0) end = length();
  StringCharacterStream stream(this, start);
  for (int i = start; i < end && stream.HasMore(); i++) {
    os << AsUC16(stream.GetNext());
  }
}


void JSObject::JSObjectShortPrint(StringStream* accumulator) {
  switch (map()->instance_type()) {
    case JS_ARRAY_TYPE: {
      double length = JSArray::cast(this)->length()->IsUndefined()
                          ? 0
                          : JSArray::cast(this)->length()->Number();
      accumulator->Add("<JSArray[%u]>", static_cast<uint32_t>(length));
      break;
    }
    case JS_BOUND_FUNCTION_TYPE: {
      JSBoundFunction* bound_function = JSBoundFunction::cast(this);
      accumulator->Add("<JSBoundFunction");
      accumulator->Add(
          " (BoundTargetFunction %p)>",
          reinterpret_cast<void*>(bound_function->bound_target_function()));
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JSWeakMap>");
      break;
    }
    case JS_WEAK_SET_TYPE: {
      accumulator->Add("<JSWeakSet>");
      break;
    }
    case JS_REGEXP_TYPE: {
      accumulator->Add("<JSRegExp");
      JSRegExp* regexp = JSRegExp::cast(this);
      if (regexp->source()->IsString()) {
        accumulator->Add(" ");
        String::cast(regexp->source())->StringShortPrint(accumulator);
      }
      accumulator->Add(">");

      break;
    }
    case JS_FUNCTION_TYPE: {
      JSFunction* function = JSFunction::cast(this);
      Object* fun_name = function->shared()->DebugName();
      bool printed = false;
      if (fun_name->IsString()) {
        String* str = String::cast(fun_name);
        if (str->length() > 0) {
          accumulator->Add("<JSFunction ");
          accumulator->Put(str);
          printed = true;
        }
      }
      if (!printed) {
        accumulator->Add("<JSFunction");
      }
      if (FLAG_trace_file_names) {
        Object* source_name =
            Script::cast(function->shared()->script())->name();
        if (source_name->IsString()) {
          String* str = String::cast(source_name);
          if (str->length() > 0) {
            accumulator->Add(" <");
            accumulator->Put(str);
            accumulator->Add(">");
          }
        }
      }
      accumulator->Add(" (sfi = %p)",
                       reinterpret_cast<void*>(function->shared()));
      accumulator->Put('>');
      break;
    }
    case JS_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JSGenerator>");
      break;
    }
    case JS_ASYNC_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JS AsyncGenerator>");
      break;
    }

    // All other JSObjects are rather similar to each other (JSObject,
    // JSGlobalProxy, JSGlobalObject, JSUndetectable, JSValue).
    default: {
      Map* map_of_this = map();
      Heap* heap = GetHeap();
      Object* constructor = map_of_this->GetConstructor();
      bool printed = false;
      if (constructor->IsHeapObject() &&
          !heap->Contains(HeapObject::cast(constructor))) {
        accumulator->Add("!!!INVALID CONSTRUCTOR!!!");
      } else {
        bool global_object = IsJSGlobalProxy();
        if (constructor->IsJSFunction()) {
          if (!heap->Contains(JSFunction::cast(constructor)->shared())) {
            accumulator->Add("!!!INVALID SHARED ON CONSTRUCTOR!!!");
          } else {
            String* constructor_name =
                JSFunction::cast(constructor)->shared()->Name();
            if (constructor_name->length() > 0) {
              accumulator->Add(global_object ? "<GlobalObject " : "<");
              accumulator->Put(constructor_name);
              accumulator->Add(
                  " %smap = %p",
                  map_of_this->is_deprecated() ? "deprecated-" : "",
                  map_of_this);
              printed = true;
            }
          }
        } else if (constructor->IsFunctionTemplateInfo()) {
          accumulator->Add(global_object ? "<RemoteObject>" : "<RemoteObject>");
          printed = true;
        }
        if (!printed) {
          accumulator->Add("<JS%sObject", global_object ? "Global " : "");
        }
      }
      if (IsJSValue()) {
        accumulator->Add(" value = ");
        JSValue::cast(this)->value()->ShortPrint(accumulator);
      }
      accumulator->Put('>');
      break;
    }
  }
}


void JSObject::PrintElementsTransition(
    FILE* file, Handle<JSObject> object,
    ElementsKind from_kind, Handle<FixedArrayBase> from_elements,
    ElementsKind to_kind, Handle<FixedArrayBase> to_elements) {
  if (from_kind != to_kind) {
    OFStream os(file);
    os << "elements transition [" << ElementsKindToString(from_kind) << " -> "
       << ElementsKindToString(to_kind) << "] in ";
    JavaScriptFrame::PrintTop(object->GetIsolate(), file, false, true);
    PrintF(file, " for ");
    object->ShortPrint(file);
    PrintF(file, " from ");
    from_elements->ShortPrint(file);
    PrintF(file, " to ");
    to_elements->ShortPrint(file);
    PrintF(file, "\n");
  }
}


// static
MaybeHandle<JSFunction> Map::GetConstructorFunction(
    Handle<Map> map, Handle<Context> native_context) {
  if (map->IsPrimitiveMap()) {
    int const constructor_function_index = map->GetConstructorFunctionIndex();
    if (constructor_function_index != kNoConstructorFunctionIndex) {
      return handle(
          JSFunction::cast(native_context->get(constructor_function_index)),
          native_context->GetIsolate());
    }
  }
  return MaybeHandle<JSFunction>();
}

void Map::PrintReconfiguration(Isolate* isolate, FILE* file, int modify_index,
                               PropertyKind kind,
                               PropertyAttributes attributes) {
  OFStream os(file);
  os << "[reconfiguring]";
  Name* name = instance_descriptors()->GetKey(modify_index);
  if (name->IsString()) {
    String::cast(name)->PrintOn(file);
  } else {
    os << "{symbol " << static_cast<void*>(name) << "}";
  }
  os << ": " << (kind == kData ? "kData" : "ACCESSORS") << ", attrs: ";
  os << attributes << " [";
  JavaScriptFrame::PrintTop(isolate, file, false, true);
  os << "]\n";
}

VisitorId Map::GetVisitorId(Map* map) {
  STATIC_ASSERT(kVisitorIdCount <= 256);

  const int instance_type = map->instance_type();
  const bool has_unboxed_fields =
      FLAG_unbox_double_fields && !map->HasFastPointerLayout();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    switch (instance_type & kStringRepresentationMask) {
      case kSeqStringTag:
        if ((instance_type & kStringEncodingMask) == kOneByteStringTag) {
          return kVisitSeqOneByteString;
        } else {
          return kVisitSeqTwoByteString;
        }

      case kConsStringTag:
        if (IsShortcutCandidate(instance_type)) {
          return kVisitShortcutCandidate;
        } else {
          return kVisitConsString;
        }

      case kSlicedStringTag:
        return kVisitSlicedString;

      case kExternalStringTag:
        return kVisitDataObject;

      case kThinStringTag:
        return kVisitThinString;
    }
    UNREACHABLE();
  }

  switch (instance_type) {
    case BYTE_ARRAY_TYPE:
      return kVisitByteArray;

    case BYTECODE_ARRAY_TYPE:
      return kVisitBytecodeArray;

    case FREE_SPACE_TYPE:
      return kVisitFreeSpace;

    case FIXED_ARRAY_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case SCOPE_INFO_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case AWAIT_CONTEXT_TYPE:
    case BLOCK_CONTEXT_TYPE:
    case CATCH_CONTEXT_TYPE:
    case DEBUG_EVALUATE_CONTEXT_TYPE:
    case EVAL_CONTEXT_TYPE:
    case FUNCTION_CONTEXT_TYPE:
    case MODULE_CONTEXT_TYPE:
    case NATIVE_CONTEXT_TYPE:
    case SCRIPT_CONTEXT_TYPE:
    case WITH_CONTEXT_TYPE:
      return kVisitFixedArray;

    case EPHEMERON_HASH_TABLE_TYPE:
      return kVisitEphemeronHashTable;

    case WEAK_FIXED_ARRAY_TYPE:
    case WEAK_ARRAY_LIST_TYPE:
    case DESCRIPTOR_ARRAY_TYPE:
      return kVisitWeakArray;

    case FIXED_DOUBLE_ARRAY_TYPE:
      return kVisitFixedDoubleArray;

    case PROPERTY_ARRAY_TYPE:
      return kVisitPropertyArray;

    case FEEDBACK_CELL_TYPE:
      return kVisitFeedbackCell;

    case FEEDBACK_VECTOR_TYPE:
      return kVisitFeedbackVector;

    case ODDBALL_TYPE:
      return kVisitOddball;

    case MAP_TYPE:
      return kVisitMap;

    case CODE_TYPE:
      return kVisitCode;

    case CELL_TYPE:
      return kVisitCell;

    case PROPERTY_CELL_TYPE:
      return kVisitPropertyCell;

    case TRANSITION_ARRAY_TYPE:
      return kVisitTransitionArray;

    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      return kVisitJSWeakCollection;

    case CALL_HANDLER_INFO_TYPE:
      return kVisitStruct;

    case SHARED_FUNCTION_INFO_TYPE:
      return kVisitSharedFunctionInfo;

    case JS_PROXY_TYPE:
      return kVisitStruct;

    case SYMBOL_TYPE:
      return kVisitSymbol;

    case JS_ARRAY_BUFFER_TYPE:
      return kVisitJSArrayBuffer;

    case JS_DATA_VIEW_TYPE:
      return kVisitJSDataView;

    case JS_TYPED_ARRAY_TYPE:
      return kVisitJSTypedArray;

    case SMALL_ORDERED_HASH_MAP_TYPE:
      return kVisitSmallOrderedHashMap;

    case SMALL_ORDERED_HASH_SET_TYPE:
      return kVisitSmallOrderedHashSet;

    case CODE_DATA_CONTAINER_TYPE:
      return kVisitCodeDataContainer;

    case WASM_INSTANCE_TYPE:
      return kVisitWasmInstanceObject;

    case PRE_PARSED_SCOPE_DATA_TYPE:
      return kVisitPreParsedScopeData;

    case UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE:
      return kVisitUncompiledDataWithoutPreParsedScope;

    case UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE:
      return kVisitUncompiledDataWithPreParsedScope;

    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_MODULE_NAMESPACE_TYPE:
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_ARRAY_ITERATOR_TYPE:
    case JS_ARRAY_TYPE:
    case JS_FUNCTION_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
    case JS_STRING_ITERATOR_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REGEXP_TYPE:
    case JS_REGEXP_STRING_ITERATOR_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_INTL_V8_BREAK_ITERATOR_TYPE:
    case JS_INTL_COLLATOR_TYPE:
    case JS_INTL_DATE_TIME_FORMAT_TYPE:
    case JS_INTL_LIST_FORMAT_TYPE:
    case JS_INTL_LOCALE_TYPE:
    case JS_INTL_NUMBER_FORMAT_TYPE:
    case JS_INTL_PLURAL_RULES_TYPE:
    case JS_INTL_RELATIVE_TIME_FORMAT_TYPE:
    case JS_INTL_SEGMENTER_TYPE:
#endif  // V8_INTL_SUPPORT
    case WASM_EXCEPTION_TYPE:
    case WASM_GLOBAL_TYPE:
    case WASM_MEMORY_TYPE:
    case WASM_MODULE_TYPE:
    case WASM_TABLE_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
      return has_unboxed_fields ? kVisitJSObject : kVisitJSObjectFast;
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
      return kVisitJSApiObject;

    case FILLER_TYPE:
    case FOREIGN_TYPE:
    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
    case FEEDBACK_METADATA_TYPE:
      return kVisitDataObject;

    case BIGINT_TYPE:
      return kVisitBigInt;

    case FIXED_UINT8_ARRAY_TYPE:
    case FIXED_INT8_ARRAY_TYPE:
    case FIXED_UINT16_ARRAY_TYPE:
    case FIXED_INT16_ARRAY_TYPE:
    case FIXED_UINT32_ARRAY_TYPE:
    case FIXED_INT32_ARRAY_TYPE:
    case FIXED_FLOAT32_ARRAY_TYPE:
    case FIXED_UINT8_CLAMPED_ARRAY_TYPE:
    case FIXED_BIGUINT64_ARRAY_TYPE:
    case FIXED_BIGINT64_ARRAY_TYPE:
      return kVisitFixedTypedArrayBase;

    case FIXED_FLOAT64_ARRAY_TYPE:
      return kVisitFixedFloat64Array;

    case ALLOCATION_SITE_TYPE:
      return kVisitAllocationSite;

#define MAKE_STRUCT_CASE(TYPE, Name, name) case TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      if (instance_type == PROTOTYPE_INFO_TYPE) {
        return kVisitPrototypeInfo;
      }
      return kVisitStruct;

    case LOAD_HANDLER_TYPE:
    case STORE_HANDLER_TYPE:
      return kVisitDataHandler;

    default:
      UNREACHABLE();
  }
}

void Map::PrintGeneralization(
    Isolate* isolate, FILE* file, const char* reason, int modify_index,
    int split, int descriptors, bool descriptor_to_field,
    Representation old_representation, Representation new_representation,
    MaybeHandle<FieldType> old_field_type, MaybeHandle<Object> old_value,
    MaybeHandle<FieldType> new_field_type, MaybeHandle<Object> new_value) {
  OFStream os(file);
  os << "[generalizing]";
  Name* name = instance_descriptors()->GetKey(modify_index);
  if (name->IsString()) {
    String::cast(name)->PrintOn(file);
  } else {
    os << "{symbol " << static_cast<void*>(name) << "}";
  }
  os << ":";
  if (descriptor_to_field) {
    os << "c";
  } else {
    os << old_representation.Mnemonic() << "{";
    if (old_field_type.is_null()) {
      os << Brief(*(old_value.ToHandleChecked()));
    } else {
      old_field_type.ToHandleChecked()->PrintTo(os);
    }
    os << "}";
  }
  os << "->" << new_representation.Mnemonic() << "{";
  if (new_field_type.is_null()) {
    os << Brief(*(new_value.ToHandleChecked()));
  } else {
    new_field_type.ToHandleChecked()->PrintTo(os);
  }
  os << "} (";
  if (strlen(reason) > 0) {
    os << reason;
  } else {
    os << "+" << (descriptors - split) << " maps";
  }
  os << ") [";
  JavaScriptFrame::PrintTop(isolate, file, false, true);
  os << "]\n";
}


void JSObject::PrintInstanceMigration(FILE* file,
                                      Map* original_map,
                                      Map* new_map) {
  if (new_map->is_dictionary_map()) {
    PrintF(file, "[migrating to slow]\n");
    return;
  }
  PrintF(file, "[migrating]");
  DescriptorArray* o = original_map->instance_descriptors();
  DescriptorArray* n = new_map->instance_descriptors();
  for (int i = 0; i < original_map->NumberOfOwnDescriptors(); i++) {
    Representation o_r = o->GetDetails(i).representation();
    Representation n_r = n->GetDetails(i).representation();
    if (!o_r.Equals(n_r)) {
      String::cast(o->GetKey(i))->PrintOn(file);
      PrintF(file, ":%s->%s ", o_r.Mnemonic(), n_r.Mnemonic());
    } else if (o->GetDetails(i).location() == kDescriptor &&
               n->GetDetails(i).location() == kField) {
      Name* name = o->GetKey(i);
      if (name->IsString()) {
        String::cast(name)->PrintOn(file);
      } else {
        PrintF(file, "{symbol %p}", static_cast<void*>(name));
      }
      PrintF(file, " ");
    }
  }
  if (original_map->elements_kind() != new_map->elements_kind()) {
    PrintF(file, "elements_kind[%i->%i]", original_map->elements_kind(),
           new_map->elements_kind());
  }
  PrintF(file, "\n");
}

bool JSObject::IsUnmodifiedApiObject(Object** o) {
  Object* object = *o;
  if (object->IsSmi()) return false;
  HeapObject* heap_object = HeapObject::cast(object);
  if (!object->IsJSObject()) return false;
  JSObject* js_object = JSObject::cast(object);
  if (!js_object->IsDroppableApiWrapper()) return false;
  Object* maybe_constructor = js_object->map()->GetConstructor();
  if (!maybe_constructor->IsJSFunction()) return false;
  JSFunction* constructor = JSFunction::cast(maybe_constructor);
  if (js_object->elements()->length() != 0) return false;

  return constructor->initial_map() == heap_object->map();
}

void HeapObject::HeapObjectShortPrint(std::ostream& os) {  // NOLINT
  os << AsHex(reinterpret_cast<Address>(this), kPointerHexDigits, true) << " ";

  if (IsString()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(this)->StringShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  if (IsJSObject()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    JSObject::cast(this)->JSObjectShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  switch (map()->instance_type()) {
    case MAP_TYPE: {
      os << "<Map";
      Map* mapInstance = Map::cast(this);
      if (mapInstance->IsJSObjectMap()) {
        os << "(" << ElementsKindToString(mapInstance->elements_kind()) << ")";
      } else if (mapInstance->instance_size() != kVariableSizeSentinel) {
        os << "[" << mapInstance->instance_size() << "]";
      }
      os << ">";
    } break;
    case AWAIT_CONTEXT_TYPE: {
      os << "<AwaitContext generator= ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      Context::cast(this)->extension()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case BLOCK_CONTEXT_TYPE:
      os << "<BlockContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case CATCH_CONTEXT_TYPE:
      os << "<CatchContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case DEBUG_EVALUATE_CONTEXT_TYPE:
      os << "<DebugEvaluateContext[" << FixedArray::cast(this)->length()
         << "]>";
      break;
    case EVAL_CONTEXT_TYPE:
      os << "<EvalContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case FUNCTION_CONTEXT_TYPE:
      os << "<FunctionContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case MODULE_CONTEXT_TYPE:
      os << "<ModuleContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case NATIVE_CONTEXT_TYPE:
      os << "<NativeContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case SCRIPT_CONTEXT_TYPE:
      os << "<ScriptContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case WITH_CONTEXT_TYPE:
      os << "<WithContext[" << FixedArray::cast(this)->length() << "]>";
      break;
    case SCRIPT_CONTEXT_TABLE_TYPE:
      os << "<ScriptContextTable[" << FixedArray::cast(this)->length() << "]>";
      break;
    case HASH_TABLE_TYPE:
      os << "<HashTable[" << FixedArray::cast(this)->length() << "]>";
      break;
    case ORDERED_HASH_MAP_TYPE:
      os << "<OrderedHashMap[" << FixedArray::cast(this)->length() << "]>";
      break;
    case ORDERED_HASH_SET_TYPE:
      os << "<OrderedHashSet[" << FixedArray::cast(this)->length() << "]>";
      break;
    case NAME_DICTIONARY_TYPE:
      os << "<NameDictionary[" << FixedArray::cast(this)->length() << "]>";
      break;
    case GLOBAL_DICTIONARY_TYPE:
      os << "<GlobalDictionary[" << FixedArray::cast(this)->length() << "]>";
      break;
    case NUMBER_DICTIONARY_TYPE:
      os << "<NumberDictionary[" << FixedArray::cast(this)->length() << "]>";
      break;
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
      os << "<SimpleNumberDictionary[" << FixedArray::cast(this)->length()
         << "]>";
      break;
    case STRING_TABLE_TYPE:
      os << "<StringTable[" << FixedArray::cast(this)->length() << "]>";
      break;
    case FIXED_ARRAY_TYPE:
      os << "<FixedArray[" << FixedArray::cast(this)->length() << "]>";
      break;
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
      os << "<ObjectBoilerplateDescription[" << FixedArray::cast(this)->length()
         << "]>";
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      os << "<FixedDoubleArray[" << FixedDoubleArray::cast(this)->length()
         << "]>";
      break;
    case BYTE_ARRAY_TYPE:
      os << "<ByteArray[" << ByteArray::cast(this)->length() << "]>";
      break;
    case BYTECODE_ARRAY_TYPE:
      os << "<BytecodeArray[" << BytecodeArray::cast(this)->length() << "]>";
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      os << "<DescriptorArray[" << DescriptorArray::cast(this)->length()
         << "]>";
      break;
    case TRANSITION_ARRAY_TYPE:
      os << "<TransitionArray[" << TransitionArray::cast(this)->length()
         << "]>";
      break;
    case PROPERTY_ARRAY_TYPE:
      os << "<PropertyArray[" << PropertyArray::cast(this)->length() << "]>";
      break;
    case FEEDBACK_CELL_TYPE: {
      {
        ReadOnlyRoots roots = GetReadOnlyRoots();
        os << "<FeedbackCell[";
        if (map() == roots.no_closures_cell_map()) {
          os << "no closures";
        } else if (map() == roots.one_closure_cell_map()) {
          os << "one closure";
        } else if (map() == roots.many_closures_cell_map()) {
          os << "many closures";
        } else {
          os << "!!!INVALID MAP!!!";
        }
        os << "]>";
      }
      break;
    }
    case FEEDBACK_VECTOR_TYPE:
      os << "<FeedbackVector[" << FeedbackVector::cast(this)->length() << "]>";
      break;
    case FREE_SPACE_TYPE:
      os << "<FreeSpace[" << FreeSpace::cast(this)->size() << "]>";
      break;
#define TYPED_ARRAY_SHORT_PRINT(Type, type, TYPE, ctype)                      \
  case FIXED_##TYPE##_ARRAY_TYPE:                                             \
    os << "<Fixed" #Type "Array[" << Fixed##Type##Array::cast(this)->length() \
       << "]>";                                                               \
    break;

      TYPED_ARRAYS(TYPED_ARRAY_SHORT_PRINT)
#undef TYPED_ARRAY_SHORT_PRINT

    case PRE_PARSED_SCOPE_DATA_TYPE: {
      PreParsedScopeData* data = PreParsedScopeData::cast(this);
      os << "<PreParsedScopeData[" << data->length() << "]>";
      break;
    }

    case UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE: {
      UncompiledDataWithoutPreParsedScope* data =
          UncompiledDataWithoutPreParsedScope::cast(this);
      os << "<UncompiledDataWithoutPreParsedScope (" << data->start_position()
         << ", " << data->end_position() << ")]>";
      break;
    }

    case UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE: {
      UncompiledDataWithPreParsedScope* data =
          UncompiledDataWithPreParsedScope::cast(this);
      os << "<UncompiledDataWithPreParsedScope (" << data->start_position()
         << ", " << data->end_position()
         << ") preparsed=" << Brief(data->pre_parsed_scope_data()) << ">";
      break;
    }

    case SHARED_FUNCTION_INFO_TYPE: {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(this);
      std::unique_ptr<char[]> debug_name = shared->DebugName()->ToCString();
      if (debug_name[0] != 0) {
        os << "<SharedFunctionInfo " << debug_name.get() << ">";
      } else {
        os << "<SharedFunctionInfo>";
      }
      break;
    }
    case JS_MESSAGE_OBJECT_TYPE:
      os << "<JSMessageObject>";
      break;
#define MAKE_STRUCT_CASE(TYPE, Name, name)   \
  case TYPE:                                 \
    os << "<" #Name;                         \
    Name::cast(this)->BriefPrintDetails(os); \
    os << ">";                               \
    break;
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    case ALLOCATION_SITE_TYPE: {
      os << "<AllocationSite";
      AllocationSite::cast(this)->BriefPrintDetails(os);
      os << ">";
      break;
    }
    case SCOPE_INFO_TYPE: {
      ScopeInfo* scope = ScopeInfo::cast(this);
      os << "<ScopeInfo";
      if (scope->length()) os << " " << scope->scope_type() << " ";
      os << "[" << scope->length() << "]>";
      break;
    }
    case CODE_TYPE: {
      Code* code = Code::cast(this);
      os << "<Code " << Code::Kind2String(code->kind());
      if (code->is_stub()) {
        os << " " << CodeStub::MajorName(CodeStub::GetMajorKey(code));
      } else if (code->is_builtin()) {
        os << " " << Builtins::name(code->builtin_index());
      }
      os << ">";
      break;
    }
    case ODDBALL_TYPE: {
      if (IsUndefined()) {
        os << "<undefined>";
      } else if (IsTheHole()) {
        os << "<the_hole>";
      } else if (IsNull()) {
        os << "<null>";
      } else if (IsTrue()) {
        os << "<true>";
      } else if (IsFalse()) {
        os << "<false>";
      } else {
        os << "<Odd Oddball: ";
        os << Oddball::cast(this)->to_string()->ToCString().get();
        os << ">";
      }
      break;
    }
    case SYMBOL_TYPE: {
      Symbol* symbol = Symbol::cast(this);
      symbol->SymbolShortPrint(os);
      break;
    }
    case HEAP_NUMBER_TYPE: {
      os << "<HeapNumber ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << ">";
      break;
    }
    case MUTABLE_HEAP_NUMBER_TYPE: {
      os << "<MutableHeapNumber ";
      MutableHeapNumber::cast(this)->MutableHeapNumberPrint(os);
      os << '>';
      break;
    }
    case BIGINT_TYPE: {
      os << "<BigInt ";
      BigInt::cast(this)->BigIntShortPrint(os);
      os << ">";
      break;
    }
    case JS_PROXY_TYPE:
      os << "<JSProxy>";
      break;
    case FOREIGN_TYPE:
      os << "<Foreign>";
      break;
    case CELL_TYPE: {
      os << "<Cell value= ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      Cell::cast(this)->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case PROPERTY_CELL_TYPE: {
      PropertyCell* cell = PropertyCell::cast(this);
      os << "<PropertyCell name=";
      cell->name()->ShortPrint(os);
      os << " value=";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      cell->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case CALL_HANDLER_INFO_TYPE: {
      CallHandlerInfo* info = CallHandlerInfo::cast(this);
      os << "<CallHandlerInfo ";
      os << "callback= " << Brief(info->callback());
      os << ", js_callback= " << Brief(info->js_callback());
      os << ", data= " << Brief(info->data());
      if (info->IsSideEffectFreeCallHandlerInfo()) {
        os << ", side_effect_free= true>";
      } else {
        os << ", side_effect_free= false>";
      }
      break;
    }
    default:
      os << "<Other heap object (" << map()->instance_type() << ")>";
      break;
  }
}

void Struct::BriefPrintDetails(std::ostream& os) {}

void Tuple2::BriefPrintDetails(std::ostream& os) {
  os << " " << Brief(value1()) << ", " << Brief(value2());
}

void Tuple3::BriefPrintDetails(std::ostream& os) {
  os << " " << Brief(value1()) << ", " << Brief(value2()) << ", "
     << Brief(value3());
}

void ArrayBoilerplateDescription::BriefPrintDetails(std::ostream& os) {
  os << " " << elements_kind() << ", " << Brief(constant_elements());
}

void CallableTask::BriefPrintDetails(std::ostream& os) {
  os << " callable=" << Brief(callable());
}

void HeapObject::Iterate(ObjectVisitor* v) { IterateFast<ObjectVisitor>(v); }


void HeapObject::IterateBody(ObjectVisitor* v) {
  Map* m = map();
  IterateBodyFast<ObjectVisitor>(m, SizeFromMap(m), v);
}

void HeapObject::IterateBody(Map* map, int object_size, ObjectVisitor* v) {
  IterateBodyFast<ObjectVisitor>(map, object_size, v);
}


struct CallIsValidSlot {
  template <typename BodyDescriptor>
  static bool apply(Map* map, HeapObject* obj, int offset, int) {
    return BodyDescriptor::IsValidSlot(map, obj, offset);
  }
};

bool HeapObject::IsValidSlot(Map* map, int offset) {
  DCHECK_NE(0, offset);
  return BodyDescriptorApply<CallIsValidSlot, bool>(map->instance_type(), map,
                                                    this, offset, 0);
}

String* JSReceiver::class_name() {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  if (IsFunction()) return roots.Function_string();
  if (IsJSArgumentsObject()) return roots.Arguments_string();
  if (IsJSArray()) return roots.Array_string();
  if (IsJSArrayBuffer()) {
    if (JSArrayBuffer::cast(this)->is_shared()) {
      return roots.SharedArrayBuffer_string();
    }
    return roots.ArrayBuffer_string();
  }
  if (IsJSArrayIterator()) return roots.ArrayIterator_string();
  if (IsJSDate()) return roots.Date_string();
  if (IsJSError()) return roots.Error_string();
  if (IsJSGeneratorObject()) return roots.Generator_string();
  if (IsJSMap()) return roots.Map_string();
  if (IsJSMapIterator()) return roots.MapIterator_string();
  if (IsJSProxy()) {
    return map()->is_callable() ? roots.Function_string()
                                : roots.Object_string();
  }
  if (IsJSRegExp()) return roots.RegExp_string();
  if (IsJSSet()) return roots.Set_string();
  if (IsJSSetIterator()) return roots.SetIterator_string();
  if (IsJSTypedArray()) {
#define SWITCH_KIND(Type, type, TYPE, ctype)       \
  if (map()->elements_kind() == TYPE##_ELEMENTS) { \
    return roots.Type##Array_string();             \
  }
    TYPED_ARRAYS(SWITCH_KIND)
#undef SWITCH_KIND
  }
  if (IsJSValue()) {
    Object* value = JSValue::cast(this)->value();
    if (value->IsBoolean()) return roots.Boolean_string();
    if (value->IsString()) return roots.String_string();
    if (value->IsNumber()) return roots.Number_string();
    if (value->IsBigInt()) return roots.BigInt_string();
    if (value->IsSymbol()) return roots.Symbol_string();
    if (value->IsScript()) return roots.Script_string();
    UNREACHABLE();
  }
  if (IsJSWeakMap()) return roots.WeakMap_string();
  if (IsJSWeakSet()) return roots.WeakSet_string();
  if (IsJSGlobalProxy()) return roots.global_string();

  Object* maybe_constructor = map()->GetConstructor();
  if (maybe_constructor->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(maybe_constructor);
    if (constructor->shared()->IsApiFunction()) {
      maybe_constructor = constructor->shared()->get_api_func_data();
    }
  }

  if (maybe_constructor->IsFunctionTemplateInfo()) {
    FunctionTemplateInfo* info = FunctionTemplateInfo::cast(maybe_constructor);
    if (info->class_name()->IsString()) return String::cast(info->class_name());
  }

  return roots.Object_string();
}

bool HeapObject::CanBeRehashed() const {
  DCHECK(NeedsRehashing());
  switch (map()->instance_type()) {
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
      // TODO(yangguo): actually support rehashing OrderedHash{Map,Set}.
      return false;
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
      return true;
    case DESCRIPTOR_ARRAY_TYPE:
      return true;
    case TRANSITION_ARRAY_TYPE:
      return true;
    case SMALL_ORDERED_HASH_MAP_TYPE:
      return SmallOrderedHashMap::cast(this)->NumberOfElements() == 0;
    case SMALL_ORDERED_HASH_SET_TYPE:
      return SmallOrderedHashMap::cast(this)->NumberOfElements() == 0;
    default:
      return false;
  }
  return false;
}

void HeapObject::RehashBasedOnMap(Isolate* isolate) {
  switch (map()->instance_type()) {
    case HASH_TABLE_TYPE:
      UNREACHABLE();
      break;
    case NAME_DICTIONARY_TYPE:
      NameDictionary::cast(this)->Rehash(isolate);
      break;
    case GLOBAL_DICTIONARY_TYPE:
      GlobalDictionary::cast(this)->Rehash(isolate);
      break;
    case NUMBER_DICTIONARY_TYPE:
      NumberDictionary::cast(this)->Rehash(isolate);
      break;
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
      SimpleNumberDictionary::cast(this)->Rehash(isolate);
      break;
    case STRING_TABLE_TYPE:
      StringTable::cast(this)->Rehash(isolate);
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      DCHECK_LE(1, DescriptorArray::cast(this)->number_of_descriptors());
      DescriptorArray::cast(this)->Sort();
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(this)->Sort();
      break;
    case SMALL_ORDERED_HASH_MAP_TYPE:
      DCHECK_EQ(0, SmallOrderedHashMap::cast(this)->NumberOfElements());
      break;
    case SMALL_ORDERED_HASH_SET_TYPE:
      DCHECK_EQ(0, SmallOrderedHashSet::cast(this)->NumberOfElements());
      break;
    default:
      break;
  }
}

namespace {
std::pair<MaybeHandle<JSFunction>, Handle<String>> GetConstructorHelper(
    Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();

  // If the object was instantiated simply with base == new.target, the
  // constructor on the map provides the most accurate name.
  // Don't provide the info for prototypes, since their constructors are
  // reclaimed and replaced by Object in OptimizeAsPrototype.
  if (!receiver->IsJSProxy() && receiver->map()->new_target_is_base() &&
      !receiver->map()->is_prototype_map()) {
    Object* maybe_constructor = receiver->map()->GetConstructor();
    if (maybe_constructor->IsJSFunction()) {
      JSFunction* constructor = JSFunction::cast(maybe_constructor);
      String* name = constructor->shared()->DebugName();
      if (name->length() != 0 &&
          !name->Equals(ReadOnlyRoots(isolate).Object_string())) {
        return std::make_pair(handle(constructor, isolate),
                              handle(name, isolate));
      }
    } else if (maybe_constructor->IsFunctionTemplateInfo()) {
      FunctionTemplateInfo* info =
          FunctionTemplateInfo::cast(maybe_constructor);
      if (info->class_name()->IsString()) {
        return std::make_pair(
            MaybeHandle<JSFunction>(),
            handle(String::cast(info->class_name()), isolate));
      }
    }
  }

  Handle<Object> maybe_tag = JSReceiver::GetDataProperty(
      receiver, isolate->factory()->to_string_tag_symbol());
  if (maybe_tag->IsString())
    return std::make_pair(MaybeHandle<JSFunction>(),
                          Handle<String>::cast(maybe_tag));

  PrototypeIterator iter(isolate, receiver);
  if (iter.IsAtEnd()) {
    return std::make_pair(MaybeHandle<JSFunction>(),
                          handle(receiver->class_name(), isolate));
  }

  Handle<JSReceiver> start = PrototypeIterator::GetCurrent<JSReceiver>(iter);
  LookupIterator it(receiver, isolate->factory()->constructor_string(), start,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Handle<Object> maybe_constructor = JSReceiver::GetDataProperty(&it);
  if (maybe_constructor->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(*maybe_constructor);
    String* name = constructor->shared()->DebugName();

    if (name->length() != 0 &&
        !name->Equals(ReadOnlyRoots(isolate).Object_string())) {
      return std::make_pair(handle(constructor, isolate),
                            handle(name, isolate));
    }
  }

  return std::make_pair(MaybeHandle<JSFunction>(),
                        handle(receiver->class_name(), isolate));
}
}  // anonymous namespace

// static
MaybeHandle<JSFunction> JSReceiver::GetConstructor(
    Handle<JSReceiver> receiver) {
  return GetConstructorHelper(receiver).first;
}

// static
Handle<String> JSReceiver::GetConstructorName(Handle<JSReceiver> receiver) {
  return GetConstructorHelper(receiver).second;
}

Handle<Context> JSReceiver::GetCreationContext() {
  JSReceiver* receiver = this;
  // Externals are JSObjects with null as a constructor.
  DCHECK(!receiver->IsExternal(GetIsolate()));
  Object* constructor = receiver->map()->GetConstructor();
  JSFunction* function;
  if (constructor->IsJSFunction()) {
    function = JSFunction::cast(constructor);
  } else if (constructor->IsFunctionTemplateInfo()) {
    // Remote objects don't have a creation context.
    return Handle<Context>::null();
  } else {
    // Functions have null as a constructor,
    // but any JSFunction knows its context immediately.
    CHECK(receiver->IsJSFunction());
    function = JSFunction::cast(receiver);
  }

  return function->has_context()
             ? Handle<Context>(function->context()->native_context(),
                               receiver->GetIsolate())
             : Handle<Context>::null();
}

// static
MaybeObjectHandle Map::WrapFieldType(Isolate* isolate, Handle<FieldType> type) {
  if (type->IsClass()) {
    return MaybeObjectHandle::Weak(type->AsClass(), isolate);
  }
  return MaybeObjectHandle(type);
}

// static
FieldType* Map::UnwrapFieldType(MaybeObject* wrapped_type) {
  if (wrapped_type->IsCleared()) {
    return FieldType::None();
  }
  HeapObject* heap_object;
  if (wrapped_type->GetHeapObjectIfWeak(&heap_object)) {
    return FieldType::cast(heap_object);
  }
  return wrapped_type->cast<FieldType>();
}

MaybeHandle<Map> Map::CopyWithField(Isolate* isolate, Handle<Map> map,
                                    Handle<Name> name, Handle<FieldType> type,
                                    PropertyAttributes attributes,
                                    PropertyConstness constness,
                                    Representation representation,
                                    TransitionFlag flag) {
  DCHECK(DescriptorArray::kNotFound ==
         map->instance_descriptors()->Search(
             *name, map->NumberOfOwnDescriptors()));

  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  // Compute the new index for new field.
  int index = map->NextFreePropertyIndex();

  if (map->instance_type() == JS_CONTEXT_EXTENSION_OBJECT_TYPE) {
    constness = PropertyConstness::kMutable;
    representation = Representation::Tagged();
    type = FieldType::Any(isolate);
  } else {
    Map::GeneralizeIfCanHaveTransitionableFastElementsKind(
        isolate, map->instance_type(), &constness, &representation, &type);
  }

  MaybeObjectHandle wrapped_type = WrapFieldType(isolate, type);

  DCHECK_IMPLIES(!FLAG_track_constant_fields,
                 constness == PropertyConstness::kMutable);
  Descriptor d = Descriptor::DataField(name, index, attributes, constness,
                                       representation, wrapped_type);
  Handle<Map> new_map = Map::CopyAddDescriptor(isolate, map, &d, flag);
  new_map->AccountAddedPropertyField();
  return new_map;
}

MaybeHandle<Map> Map::CopyWithConstant(Isolate* isolate, Handle<Map> map,
                                       Handle<Name> name,
                                       Handle<Object> constant,
                                       PropertyAttributes attributes,
                                       TransitionFlag flag) {
  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  if (FLAG_track_constant_fields) {
    Representation representation = constant->OptimalRepresentation();
    Handle<FieldType> type = constant->OptimalType(isolate, representation);
    return CopyWithField(isolate, map, name, type, attributes,
                         PropertyConstness::kConst, representation, flag);
  } else {
    // Allocate new instance descriptors with (name, constant) added.
    Descriptor d =
        Descriptor::DataConstant(isolate, name, 0, constant, attributes);
    Handle<Map> new_map = Map::CopyAddDescriptor(isolate, map, &d, flag);
    return new_map;
  }
}

const char* Representation::Mnemonic() const {
  switch (kind_) {
    case kNone: return "v";
    case kTagged: return "t";
    case kSmi: return "s";
    case kDouble: return "d";
    case kInteger32: return "i";
    case kHeapObject: return "h";
    case kExternal: return "x";
    default:
      UNREACHABLE();
  }
}

bool Map::TransitionRemovesTaggedField(Map* target) const {
  int inobject = NumberOfFields();
  int target_inobject = target->NumberOfFields();
  for (int i = target_inobject; i < inobject; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(this, i);
    if (!IsUnboxedDoubleField(index)) return true;
  }
  return false;
}

bool Map::TransitionChangesTaggedFieldToUntaggedField(Map* target) const {
  int inobject = NumberOfFields();
  int target_inobject = target->NumberOfFields();
  int limit = Min(inobject, target_inobject);
  for (int i = 0; i < limit; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(target, i);
    if (!IsUnboxedDoubleField(index) && target->IsUnboxedDoubleField(index)) {
      return true;
    }
  }
  return false;
}

bool Map::TransitionRequiresSynchronizationWithGC(Map* target) const {
  return TransitionRemovesTaggedField(target) ||
         TransitionChangesTaggedFieldToUntaggedField(target);
}

bool Map::InstancesNeedRewriting(Map* target) const {
  int target_number_of_fields = target->NumberOfFields();
  int target_inobject = target->GetInObjectProperties();
  int target_unused = target->UnusedPropertyFields();
  int old_number_of_fields;

  return InstancesNeedRewriting(target, target_number_of_fields,
                                target_inobject, target_unused,
                                &old_number_of_fields);
}

bool Map::InstancesNeedRewriting(Map* target, int target_number_of_fields,
                                 int target_inobject, int target_unused,
                                 int* old_number_of_fields) const {
  // If fields were added (or removed), rewrite the instance.
  *old_number_of_fields = NumberOfFields();
  DCHECK(target_number_of_fields >= *old_number_of_fields);
  if (target_number_of_fields != *old_number_of_fields) return true;

  // If smi descriptors were replaced by double descriptors, rewrite.
  DescriptorArray* old_desc = instance_descriptors();
  DescriptorArray* new_desc = target->instance_descriptors();
  int limit = NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    if (new_desc->GetDetails(i).representation().IsDouble() !=
        old_desc->GetDetails(i).representation().IsDouble()) {
      return true;
    }
  }

  // If no fields were added, and no inobject properties were removed, setting
  // the map is sufficient.
  if (target_inobject == GetInObjectProperties()) return false;
  // In-object slack tracking may have reduced the object size of the new map.
  // In that case, succeed if all existing fields were inobject, and they still
  // fit within the new inobject size.
  DCHECK(target_inobject < GetInObjectProperties());
  if (target_number_of_fields <= target_inobject) {
    DCHECK(target_number_of_fields + target_unused == target_inobject);
    return false;
  }
  // Otherwise, properties will need to be moved to the backing store.
  return true;
}


// static
void JSObject::UpdatePrototypeUserRegistration(Handle<Map> old_map,
                                               Handle<Map> new_map,
                                               Isolate* isolate) {
  DCHECK(old_map->is_prototype_map());
  DCHECK(new_map->is_prototype_map());
  bool was_registered = JSObject::UnregisterPrototypeUser(old_map, isolate);
  new_map->set_prototype_info(old_map->prototype_info());
  old_map->set_prototype_info(Smi::kZero);
  if (FLAG_trace_prototype_users) {
    PrintF("Moving prototype_info %p from map %p to map %p.\n",
           reinterpret_cast<void*>(new_map->prototype_info()),
           reinterpret_cast<void*>(*old_map),
           reinterpret_cast<void*>(*new_map));
  }
  if (was_registered) {
    if (new_map->prototype_info()->IsPrototypeInfo()) {
      // The new map isn't registered with its prototype yet; reflect this fact
      // in the PrototypeInfo it just inherited from the old map.
      PrototypeInfo::cast(new_map->prototype_info())
          ->set_registry_slot(PrototypeInfo::UNREGISTERED);
    }
    JSObject::LazyRegisterPrototypeUser(new_map, isolate);
  }
}

namespace {
// To migrate a fast instance to a fast map:
// - First check whether the instance needs to be rewritten. If not, simply
//   change the map.
// - Otherwise, allocate a fixed array large enough to hold all fields, in
//   addition to unused space.
// - Copy all existing properties in, in the following order: backing store
//   properties, unused fields, inobject properties.
// - If all allocation succeeded, commit the state atomically:
//   * Copy inobject properties from the backing store back into the object.
//   * Trim the difference in instance size of the object. This also cleanly
//     frees inobject properties that moved to the backing store.
//   * If there are properties left in the backing store, trim of the space used
//     to temporarily store the inobject properties.
//   * If there are properties left in the backing store, install the backing
//     store.
void MigrateFastToFast(Handle<JSObject> object, Handle<Map> new_map) {
  Isolate* isolate = object->GetIsolate();
  Handle<Map> old_map(object->map(), isolate);
  // In case of a regular transition.
  if (new_map->GetBackPointer() == *old_map) {
    // If the map does not add named properties, simply set the map.
    if (old_map->NumberOfOwnDescriptors() ==
        new_map->NumberOfOwnDescriptors()) {
      object->synchronized_set_map(*new_map);
      return;
    }

    PropertyDetails details = new_map->GetLastDescriptorDetails();
    int target_index = details.field_index() - new_map->GetInObjectProperties();
    int property_array_length = object->property_array()->length();
    bool have_space = old_map->UnusedPropertyFields() > 0 ||
                      (details.location() == kField && target_index >= 0 &&
                       property_array_length > target_index);
    // Either new_map adds an kDescriptor property, or a kField property for
    // which there is still space, and which does not require a mutable double
    // box (an out-of-object double).
    if (details.location() == kDescriptor ||
        (have_space && ((FLAG_unbox_double_fields && target_index < 0) ||
                        !details.representation().IsDouble()))) {
      object->synchronized_set_map(*new_map);
      return;
    }

    // If there is still space in the object, we need to allocate a mutable
    // double box.
    if (have_space) {
      FieldIndex index =
          FieldIndex::ForDescriptor(*new_map, new_map->LastAdded());
      DCHECK(details.representation().IsDouble());
      DCHECK(!new_map->IsUnboxedDoubleField(index));
      auto value = isolate->factory()->NewMutableHeapNumberWithHoleNaN();
      object->RawFastPropertyAtPut(index, *value);
      object->synchronized_set_map(*new_map);
      return;
    }

    // This migration is a transition from a map that has run out of property
    // space. Extend the backing store.
    int grow_by = new_map->UnusedPropertyFields() + 1;
    Handle<PropertyArray> old_storage(object->property_array(), isolate);
    Handle<PropertyArray> new_storage =
        isolate->factory()->CopyPropertyArrayAndGrow(old_storage, grow_by);

    // Properly initialize newly added property.
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewMutableHeapNumberWithHoleNaN();
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    DCHECK_EQ(kField, details.location());
    DCHECK_EQ(kData, details.kind());
    DCHECK_GE(target_index, 0);  // Must be a backing store index.
    new_storage->set(target_index, *value);

    // From here on we cannot fail and we shouldn't GC anymore.
    DisallowHeapAllocation no_allocation;

    // Set the new property value and do the map transition.
    object->SetProperties(*new_storage);
    object->synchronized_set_map(*new_map);
    return;
  }

  int old_number_of_fields;
  int number_of_fields = new_map->NumberOfFields();
  int inobject = new_map->GetInObjectProperties();
  int unused = new_map->UnusedPropertyFields();

  // Nothing to do if no functions were converted to fields and no smis were
  // converted to doubles.
  if (!old_map->InstancesNeedRewriting(*new_map, number_of_fields, inobject,
                                       unused, &old_number_of_fields)) {
    object->synchronized_set_map(*new_map);
    return;
  }

  int total_size = number_of_fields + unused;
  int external = total_size - inobject;
  Handle<PropertyArray> array = isolate->factory()->NewPropertyArray(external);

  // We use this array to temporarily store the inobject properties.
  Handle<FixedArray> inobject_props =
      isolate->factory()->NewFixedArray(inobject);

  Handle<DescriptorArray> old_descriptors(old_map->instance_descriptors(),
                                          isolate);
  Handle<DescriptorArray> new_descriptors(new_map->instance_descriptors(),
                                          isolate);
  int old_nof = old_map->NumberOfOwnDescriptors();
  int new_nof = new_map->NumberOfOwnDescriptors();

  // This method only supports generalizing instances to at least the same
  // number of properties.
  DCHECK(old_nof <= new_nof);

  for (int i = 0; i < old_nof; i++) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Representation old_representation = old_details.representation();
    Representation representation = details.representation();
    Handle<Object> value;
    if (old_details.location() == kDescriptor) {
      if (old_details.kind() == kAccessor) {
        // In case of kAccessor -> kData property reconfiguration, the property
        // must already be prepared for data of certain type.
        DCHECK(!details.representation().IsNone());
        if (details.representation().IsDouble()) {
          value = isolate->factory()->NewMutableHeapNumberWithHoleNaN();
        } else {
          value = isolate->factory()->uninitialized_value();
        }
      } else {
        DCHECK_EQ(kData, old_details.kind());
        value = handle(old_descriptors->GetStrongValue(i), isolate);
        DCHECK(!old_representation.IsDouble() && !representation.IsDouble());
      }
    } else {
      DCHECK_EQ(kField, old_details.location());
      FieldIndex index = FieldIndex::ForDescriptor(*old_map, i);
      if (object->IsUnboxedDoubleField(index)) {
        uint64_t old_bits = object->RawFastDoublePropertyAsBitsAt(index);
        if (representation.IsDouble()) {
          value = isolate->factory()->NewMutableHeapNumberFromBits(old_bits);
        } else {
          value = isolate->factory()->NewHeapNumberFromBits(old_bits);
        }
      } else {
        value = handle(object->RawFastPropertyAt(index), isolate);
        if (!old_representation.IsDouble() && representation.IsDouble()) {
          DCHECK_IMPLIES(old_representation.IsNone(),
                         value->IsUninitialized(isolate));
          value = Object::NewStorageFor(isolate, value, representation);
        } else if (old_representation.IsDouble() &&
                   !representation.IsDouble()) {
          value = Object::WrapForRead(isolate, value, old_representation);
        }
      }
    }
    DCHECK(!(representation.IsDouble() && value->IsSmi()));
    int target_index = new_descriptors->GetFieldIndex(i);
    if (target_index < inobject) {
      inobject_props->set(target_index, *value);
    } else {
      array->set(target_index - inobject, *value);
    }
  }

  for (int i = old_nof; i < new_nof; i++) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewMutableHeapNumberWithHoleNaN();
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    int target_index = new_descriptors->GetFieldIndex(i);
    if (target_index < inobject) {
      inobject_props->set(target_index, *value);
    } else {
      array->set(target_index - inobject, *value);
    }
  }

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  Heap* heap = isolate->heap();

  int old_instance_size = old_map->instance_size();

  heap->NotifyObjectLayoutChange(*object, old_instance_size, no_allocation);

  // Copy (real) inobject properties. If necessary, stop at number_of_fields to
  // avoid overwriting |one_pointer_filler_map|.
  int limit = Min(inobject, number_of_fields);
  for (int i = 0; i < limit; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
    Object* value = inobject_props->get(i);
    // Can't use JSObject::FastPropertyAtPut() because proper map was not set
    // yet.
    if (new_map->IsUnboxedDoubleField(index)) {
      DCHECK(value->IsMutableHeapNumber());
      // Ensure that all bits of the double value are preserved.
      object->RawFastDoublePropertyAsBitsAtPut(
          index, MutableHeapNumber::cast(value)->value_as_bits());
      if (i < old_number_of_fields && !old_map->IsUnboxedDoubleField(index)) {
        // Transition from tagged to untagged slot.
        heap->ClearRecordedSlot(*object,
                                HeapObject::RawField(*object, index.offset()));
      } else {
#ifdef DEBUG
        heap->VerifyClearedSlot(*object,
                                HeapObject::RawField(*object, index.offset()));
#endif
      }
    } else {
      object->RawFastPropertyAtPut(index, value);
    }
  }

  object->SetProperties(*array);

  // Create filler object past the new instance size.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_instance_size - new_instance_size;
  DCHECK_GE(instance_size_delta, 0);

  if (instance_size_delta > 0) {
    Address address = object->address();
    heap->CreateFillerObjectAt(address + new_instance_size, instance_size_delta,
                               ClearRecordedSlots::kYes);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);
}

void MigrateFastToSlow(Handle<JSObject> object, Handle<Map> new_map,
                       int expected_additional_properties) {
  // The global object is always normalized.
  DCHECK(!object->IsJSGlobalObject());
  // JSGlobalProxy must never be normalized
  DCHECK(!object->IsJSGlobalProxy());

  DCHECK_IMPLIES(new_map->is_prototype_map(),
                 Map::IsPrototypeChainInvalidated(*new_map));

  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Map> map(object->map(), isolate);

  // Allocate new content.
  int real_size = map->NumberOfOwnDescriptors();
  int property_count = real_size;
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    // Make space for two more properties.
    property_count += NameDictionary::kInitialCapacity;
  }
  Handle<NameDictionary> dictionary =
      NameDictionary::New(isolate, property_count);

  Handle<DescriptorArray> descs(map->instance_descriptors(), isolate);
  for (int i = 0; i < real_size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    Handle<Name> key(descs->GetKey(i), isolate);
    Handle<Object> value;
    if (details.location() == kField) {
      FieldIndex index = FieldIndex::ForDescriptor(*map, i);
      if (details.kind() == kData) {
        if (object->IsUnboxedDoubleField(index)) {
          double old_value = object->RawFastDoublePropertyAt(index);
          value = isolate->factory()->NewHeapNumber(old_value);
        } else {
          value = handle(object->RawFastPropertyAt(index), isolate);
          if (details.representation().IsDouble()) {
            DCHECK(value->IsMutableHeapNumber());
            double old_value = Handle<MutableHeapNumber>::cast(value)->value();
            value = isolate->factory()->NewHeapNumber(old_value);
          }
        }
      } else {
        DCHECK_EQ(kAccessor, details.kind());
        value = handle(object->RawFastPropertyAt(index), isolate);
      }

    } else {
      DCHECK_EQ(kDescriptor, details.location());
      value = handle(descs->GetStrongValue(i), isolate);
    }
    DCHECK(!value.is_null());
    PropertyDetails d(details.kind(), details.attributes(),
                      PropertyCellType::kNoCell);
    dictionary = NameDictionary::Add(isolate, dictionary, key, value, d);
  }

  // Copy the next enumeration index from instance descriptor.
  dictionary->SetNextEnumerationIndex(real_size + 1);

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  Heap* heap = isolate->heap();
  int old_instance_size = map->instance_size();
  heap->NotifyObjectLayoutChange(*object, old_instance_size, no_allocation);

  // Resize the object in the heap if necessary.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_instance_size - new_instance_size;
  DCHECK_GE(instance_size_delta, 0);

  if (instance_size_delta > 0) {
    heap->CreateFillerObjectAt(object->address() + new_instance_size,
                               instance_size_delta, ClearRecordedSlots::kYes);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);

  object->SetProperties(*dictionary);

  // Ensure that in-object space of slow-mode object does not contain random
  // garbage.
  int inobject_properties = new_map->GetInObjectProperties();
  if (inobject_properties) {
    Heap* heap = isolate->heap();
    heap->ClearRecordedSlotRange(
        object->address() + map->GetInObjectPropertyOffset(0),
        object->address() + new_instance_size);

    for (int i = 0; i < inobject_properties; i++) {
      FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
      object->RawFastPropertyAtPut(index, Smi::kZero);
    }
  }

  isolate->counters()->props_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    StdoutStream os;
    os << "Object properties have been normalized:\n";
    object->Print(os);
  }
#endif
}

}  // namespace

// static
void JSObject::NotifyMapChange(Handle<Map> old_map, Handle<Map> new_map,
                               Isolate* isolate) {
  if (!old_map->is_prototype_map()) return;

  InvalidatePrototypeChains(*old_map);

  // If the map was registered with its prototype before, ensure that it
  // registers with its new prototype now. This preserves the invariant that
  // when a map on a prototype chain is registered with its prototype, then
  // all prototypes further up the chain are also registered with their
  // respective prototypes.
  UpdatePrototypeUserRegistration(old_map, new_map, isolate);
}

void JSObject::MigrateToMap(Handle<JSObject> object, Handle<Map> new_map,
                            int expected_additional_properties) {
  if (object->map() == *new_map) return;
  Handle<Map> old_map(object->map(), object->GetIsolate());
  NotifyMapChange(old_map, new_map, object->GetIsolate());

  if (old_map->is_dictionary_map()) {
    // For slow-to-fast migrations JSObject::MigrateSlowToFast()
    // must be used instead.
    CHECK(new_map->is_dictionary_map());

    // Slow-to-slow migration is trivial.
    object->synchronized_set_map(*new_map);
  } else if (!new_map->is_dictionary_map()) {
    MigrateFastToFast(object, new_map);
    if (old_map->is_prototype_map()) {
      DCHECK(!old_map->is_stable());
      DCHECK(new_map->is_stable());
      DCHECK(new_map->owns_descriptors());
      DCHECK(old_map->owns_descriptors());
      // Transfer ownership to the new map. Keep the descriptor pointer of the
      // old map intact because the concurrent marker might be iterating the
      // object with the old map.
      old_map->set_owns_descriptors(false);
      DCHECK(old_map->is_abandoned_prototype_map());
      // Ensure that no transition was inserted for prototype migrations.
      DCHECK_EQ(0, TransitionsAccessor(object->GetIsolate(), old_map)
                       .NumberOfTransitions());
      DCHECK(new_map->GetBackPointer()->IsUndefined());
      DCHECK(object->map() != *old_map);
    }
  } else {
    MigrateFastToSlow(object, new_map, expected_additional_properties);
  }

  // Careful: Don't allocate here!
  // For some callers of this method, |object| might be in an inconsistent
  // state now: the new map might have a new elements_kind, but the object's
  // elements pointer hasn't been updated yet. Callers will fix this, but in
  // the meantime, (indirectly) calling JSObjectVerify() must be avoided.
  // When adding code here, add a DisallowHeapAllocation too.
}

void JSObject::ForceSetPrototype(Handle<JSObject> object,
                                 Handle<Object> proto) {
  // object.__proto__ = proto;
  Handle<Map> old_map = Handle<Map>(object->map(), object->GetIsolate());
  Handle<Map> new_map =
      Map::Copy(object->GetIsolate(), old_map, "ForceSetPrototype");
  Map::SetPrototype(object->GetIsolate(), new_map, proto);
  JSObject::MigrateToMap(object, new_map);
}

int Map::NumberOfFields() const {
  DescriptorArray* descriptors = instance_descriptors();
  int result = 0;
  for (int i = 0; i < NumberOfOwnDescriptors(); i++) {
    if (descriptors->GetDetails(i).location() == kField) result++;
  }
  return result;
}

bool Map::HasOutOfObjectProperties() const {
  return GetInObjectProperties() < NumberOfFields();
}

void DescriptorArray::GeneralizeAllFields() {
  int length = number_of_descriptors();
  for (int i = 0; i < length; i++) {
    PropertyDetails details = GetDetails(i);
    details = details.CopyWithRepresentation(Representation::Tagged());
    if (details.location() == kField) {
      DCHECK_EQ(kData, details.kind());
      details = details.CopyWithConstness(PropertyConstness::kMutable);
      SetValue(i, FieldType::Any());
    }
    set(ToDetailsIndex(i), MaybeObject::FromObject(details.AsSmi()));
  }
}

Handle<Map> Map::CopyGeneralizeAllFields(Isolate* isolate, Handle<Map> map,
                                         ElementsKind elements_kind,
                                         int modify_index, PropertyKind kind,
                                         PropertyAttributes attributes,
                                         const char* reason) {
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(), isolate);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> descriptors = DescriptorArray::CopyUpTo(
      isolate, old_descriptors, number_of_own_descriptors);
  descriptors->GeneralizeAllFields();

  Handle<LayoutDescriptor> new_layout_descriptor(
      LayoutDescriptor::FastPointerLayout(), isolate);
  Handle<Map> new_map = CopyReplaceDescriptors(
      isolate, map, descriptors, new_layout_descriptor, OMIT_TRANSITION,
      MaybeHandle<Name>(), reason, SPECIAL_TRANSITION);

  // Unless the instance is being migrated, ensure that modify_index is a field.
  if (modify_index >= 0) {
    PropertyDetails details = descriptors->GetDetails(modify_index);
    if (details.constness() != PropertyConstness::kMutable ||
        details.location() != kField || details.attributes() != attributes) {
      int field_index = details.location() == kField
                            ? details.field_index()
                            : new_map->NumberOfFields();
      Descriptor d = Descriptor::DataField(
          isolate, handle(descriptors->GetKey(modify_index), isolate),
          field_index, attributes, Representation::Tagged());
      descriptors->Replace(modify_index, &d);
      if (details.location() != kField) {
        new_map->AccountAddedPropertyField();
      }
    } else {
      DCHECK(details.attributes() == attributes);
    }

    if (FLAG_trace_generalization) {
      MaybeHandle<FieldType> field_type = FieldType::None(isolate);
      if (details.location() == kField) {
        field_type = handle(
            map->instance_descriptors()->GetFieldType(modify_index), isolate);
      }
      map->PrintGeneralization(
          isolate, stdout, reason, modify_index,
          new_map->NumberOfOwnDescriptors(), new_map->NumberOfOwnDescriptors(),
          details.location() == kDescriptor, details.representation(),
          Representation::Tagged(), field_type, MaybeHandle<Object>(),
          FieldType::Any(isolate), MaybeHandle<Object>());
    }
  }
  new_map->set_elements_kind(elements_kind);
  return new_map;
}

void Map::DeprecateTransitionTree(Isolate* isolate) {
  if (is_deprecated()) return;
  DisallowHeapAllocation no_gc;
  TransitionsAccessor transitions(isolate, this, &no_gc);
  int num_transitions = transitions.NumberOfTransitions();
  for (int i = 0; i < num_transitions; ++i) {
    transitions.GetTarget(i)->DeprecateTransitionTree(isolate);
  }
  DCHECK(!constructor_or_backpointer()->IsFunctionTemplateInfo());
  set_is_deprecated(true);
  if (FLAG_trace_maps) {
    LOG(isolate, MapEvent("Deprecate", this, nullptr));
  }
  dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kTransitionGroup);
  NotifyLeafMapLayoutChange(isolate);
}


// Installs |new_descriptors| over the current instance_descriptors to ensure
// proper sharing of descriptor arrays.
void Map::ReplaceDescriptors(Isolate* isolate, DescriptorArray* new_descriptors,
                             LayoutDescriptor* new_layout_descriptor) {
  // Don't overwrite the empty descriptor array or initial map's descriptors.
  if (NumberOfOwnDescriptors() == 0 || GetBackPointer()->IsUndefined(isolate)) {
    return;
  }

  DescriptorArray* to_replace = instance_descriptors();
  // Replace descriptors by new_descriptors in all maps that share it. The old
  // descriptors will not be trimmed in the mark-compactor, we need to mark
  // all its elements.
  MarkingBarrierForElements(isolate->heap(), to_replace);
  Map* current = this;
  while (current->instance_descriptors() == to_replace) {
    Object* next = current->GetBackPointer();
    if (next->IsUndefined(isolate)) break;  // Stop overwriting at initial map.
    current->SetEnumLength(kInvalidEnumCacheSentinel);
    current->UpdateDescriptors(new_descriptors, new_layout_descriptor);
    current = Map::cast(next);
  }
  set_owns_descriptors(false);
}

Map* Map::FindRootMap(Isolate* isolate) const {
  const Map* result = this;
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined(isolate)) {
      // Initial map always owns descriptors and doesn't have unused entries
      // in the descriptor array.
      DCHECK(result->owns_descriptors());
      DCHECK_EQ(result->NumberOfOwnDescriptors(),
                result->instance_descriptors()->number_of_descriptors());
      return const_cast<Map*>(result);
    }
    result = Map::cast(back);
  }
}

Map* Map::FindFieldOwner(Isolate* isolate, int descriptor) const {
  DisallowHeapAllocation no_allocation;
  DCHECK_EQ(kField, instance_descriptors()->GetDetails(descriptor).location());
  const Map* result = this;
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined(isolate)) break;
    const Map* parent = Map::cast(back);
    if (parent->NumberOfOwnDescriptors() <= descriptor) break;
    result = parent;
  }
  return const_cast<Map*>(result);
}

void Map::UpdateFieldType(Isolate* isolate, int descriptor, Handle<Name> name,
                          PropertyConstness new_constness,
                          Representation new_representation,
                          const MaybeObjectHandle& new_wrapped_type) {
  DCHECK(new_wrapped_type->IsSmi() || new_wrapped_type->IsWeak());
  // We store raw pointers in the queue, so no allocations are allowed.
  DisallowHeapAllocation no_allocation;
  PropertyDetails details = instance_descriptors()->GetDetails(descriptor);
  if (details.location() != kField) return;
  DCHECK_EQ(kData, details.kind());

  Zone zone(isolate->allocator(), ZONE_NAME);
  ZoneQueue<Map*> backlog(&zone);
  backlog.push(this);

  while (!backlog.empty()) {
    Map* current = backlog.front();
    backlog.pop();

    TransitionsAccessor transitions(isolate, current, &no_allocation);
    int num_transitions = transitions.NumberOfTransitions();
    for (int i = 0; i < num_transitions; ++i) {
      Map* target = transitions.GetTarget(i);
      backlog.push(target);
    }
    DescriptorArray* descriptors = current->instance_descriptors();
    PropertyDetails details = descriptors->GetDetails(descriptor);

    // Currently constness change implies map change.
    DCHECK_IMPLIES(new_constness != details.constness(),
                   FLAG_modify_map_inplace);

    // It is allowed to change representation here only from None to something.
    DCHECK(details.representation().Equals(new_representation) ||
           details.representation().IsNone());

    // Skip if already updated the shared descriptor.
    if ((FLAG_modify_map_inplace && new_constness != details.constness()) ||
        descriptors->GetFieldType(descriptor) != *new_wrapped_type.object()) {
      DCHECK_IMPLIES(!FLAG_track_constant_fields,
                     new_constness == PropertyConstness::kMutable);
      Descriptor d = Descriptor::DataField(
          name, descriptors->GetFieldIndex(descriptor), details.attributes(),
          new_constness, new_representation, new_wrapped_type);
      descriptors->Replace(descriptor, &d);
    }
  }
}

bool FieldTypeIsCleared(Representation rep, FieldType* type) {
  return type->IsNone() && rep.IsHeapObject();
}


// static
Handle<FieldType> Map::GeneralizeFieldType(Representation rep1,
                                           Handle<FieldType> type1,
                                           Representation rep2,
                                           Handle<FieldType> type2,
                                           Isolate* isolate) {
  // Cleared field types need special treatment. They represent lost knowledge,
  // so we must be conservative, so their generalization with any other type
  // is "Any".
  if (FieldTypeIsCleared(rep1, *type1) || FieldTypeIsCleared(rep2, *type2)) {
    return FieldType::Any(isolate);
  }
  if (type1->NowIs(type2)) return type2;
  if (type2->NowIs(type1)) return type1;
  return FieldType::Any(isolate);
}

// static
void Map::GeneralizeField(Isolate* isolate, Handle<Map> map, int modify_index,
                          PropertyConstness new_constness,
                          Representation new_representation,
                          Handle<FieldType> new_field_type) {
  // Check if we actually need to generalize the field type at all.
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(), isolate);
  PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
  PropertyConstness old_constness = old_details.constness();
  Representation old_representation = old_details.representation();
  Handle<FieldType> old_field_type(old_descriptors->GetFieldType(modify_index),
                                   isolate);

  // Return if the current map is general enough to hold requested constness and
  // representation/field type.
  if (((FLAG_modify_map_inplace &&
        IsGeneralizableTo(new_constness, old_constness)) ||
       (!FLAG_modify_map_inplace && (old_constness == new_constness))) &&
      old_representation.Equals(new_representation) &&
      !FieldTypeIsCleared(new_representation, *new_field_type) &&
      // Checking old_field_type for being cleared is not necessary because
      // the NowIs check below would fail anyway in that case.
      new_field_type->NowIs(old_field_type)) {
    DCHECK(GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate)
               ->NowIs(old_field_type));
    return;
  }

  // Determine the field owner.
  Handle<Map> field_owner(map->FindFieldOwner(isolate, modify_index), isolate);
  Handle<DescriptorArray> descriptors(field_owner->instance_descriptors(),
                                      isolate);
  DCHECK_EQ(*old_field_type, descriptors->GetFieldType(modify_index));

  new_field_type =
      Map::GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate);
  if (FLAG_modify_map_inplace) {
    new_constness = GeneralizeConstness(old_constness, new_constness);
  }

  PropertyDetails details = descriptors->GetDetails(modify_index);
  Handle<Name> name(descriptors->GetKey(modify_index), isolate);

  MaybeObjectHandle wrapped_type(WrapFieldType(isolate, new_field_type));
  field_owner->UpdateFieldType(isolate, modify_index, name, new_constness,
                               new_representation, wrapped_type);
  field_owner->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kFieldOwnerGroup);

  if (FLAG_trace_generalization) {
    map->PrintGeneralization(
        isolate, stdout, "field type generalization", modify_index,
        map->NumberOfOwnDescriptors(), map->NumberOfOwnDescriptors(), false,
        details.representation(), details.representation(), old_field_type,
        MaybeHandle<Object>(), new_field_type, MaybeHandle<Object>());
  }
}

// TODO(ishell): remove.
// static
Handle<Map> Map::ReconfigureProperty(Isolate* isolate, Handle<Map> map,
                                     int modify_index, PropertyKind new_kind,
                                     PropertyAttributes new_attributes,
                                     Representation new_representation,
                                     Handle<FieldType> new_field_type) {
  DCHECK_EQ(kData, new_kind);  // Only kData case is supported.
  MapUpdater mu(isolate, map);
  return mu.ReconfigureToDataField(modify_index, new_attributes,
                                   PropertyConstness::kConst,
                                   new_representation, new_field_type);
}

// TODO(ishell): remove.
// static
Handle<Map> Map::ReconfigureElementsKind(Isolate* isolate, Handle<Map> map,
                                         ElementsKind new_elements_kind) {
  MapUpdater mu(isolate, map);
  return mu.ReconfigureElementsKind(new_elements_kind);
}

// static
MaybeHandle<Map> Map::TryUpdate(Isolate* isolate, Handle<Map> old_map) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(isolate);

  if (!old_map->is_deprecated()) return old_map;

  // Check the state of the root map.
  Map* root_map = old_map->FindRootMap(isolate);
  if (root_map->is_deprecated()) {
    JSFunction* constructor = JSFunction::cast(root_map->GetConstructor());
    DCHECK(constructor->has_initial_map());
    DCHECK(constructor->initial_map()->is_dictionary_map());
    if (constructor->initial_map()->elements_kind() !=
        old_map->elements_kind()) {
      return MaybeHandle<Map>();
    }
    return handle(constructor->initial_map(), constructor->GetIsolate());
  }
  if (!old_map->EquivalentToForTransition(root_map)) return MaybeHandle<Map>();

  ElementsKind from_kind = root_map->elements_kind();
  ElementsKind to_kind = old_map->elements_kind();
  if (from_kind != to_kind) {
    // Try to follow existing elements kind transitions.
    root_map = root_map->LookupElementsTransitionMap(isolate, to_kind);
    if (root_map == nullptr) return MaybeHandle<Map>();
    // From here on, use the map with correct elements kind as root map.
  }
  Map* new_map = root_map->TryReplayPropertyTransitions(isolate, *old_map);
  if (new_map == nullptr) return MaybeHandle<Map>();
  return handle(new_map, isolate);
}

Map* Map::TryReplayPropertyTransitions(Isolate* isolate, Map* old_map) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(isolate);

  int root_nof = NumberOfOwnDescriptors();

  int old_nof = old_map->NumberOfOwnDescriptors();
  DescriptorArray* old_descriptors = old_map->instance_descriptors();

  Map* new_map = this;
  for (int i = root_nof; i < old_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Map* transition =
        TransitionsAccessor(isolate, new_map, &no_allocation)
            .SearchTransition(old_descriptors->GetKey(i), old_details.kind(),
                              old_details.attributes());
    if (transition == nullptr) return nullptr;
    new_map = transition;
    DescriptorArray* new_descriptors = new_map->instance_descriptors();

    PropertyDetails new_details = new_descriptors->GetDetails(i);
    DCHECK_EQ(old_details.kind(), new_details.kind());
    DCHECK_EQ(old_details.attributes(), new_details.attributes());
    if (!IsGeneralizableTo(old_details.constness(), new_details.constness())) {
      return nullptr;
    }
    DCHECK(IsGeneralizableTo(old_details.location(), new_details.location()));
    if (!old_details.representation().fits_into(new_details.representation())) {
      return nullptr;
    }
    if (new_details.location() == kField) {
      if (new_details.kind() == kData) {
        FieldType* new_type = new_descriptors->GetFieldType(i);
        // Cleared field types need special treatment. They represent lost
        // knowledge, so we must first generalize the new_type to "Any".
        if (FieldTypeIsCleared(new_details.representation(), new_type)) {
          return nullptr;
        }
        DCHECK_EQ(kData, old_details.kind());
        if (old_details.location() == kField) {
          FieldType* old_type = old_descriptors->GetFieldType(i);
          if (FieldTypeIsCleared(old_details.representation(), old_type) ||
              !old_type->NowIs(new_type)) {
            return nullptr;
          }
        } else {
          DCHECK_EQ(kDescriptor, old_details.location());
          DCHECK(!FLAG_track_constant_fields);
          Object* old_value = old_descriptors->GetStrongValue(i);
          if (!new_type->NowContains(old_value)) {
            return nullptr;
          }
        }

      } else {
        DCHECK_EQ(kAccessor, new_details.kind());
#ifdef DEBUG
        FieldType* new_type = new_descriptors->GetFieldType(i);
        DCHECK(new_type->IsAny());
#endif
        UNREACHABLE();
      }
    } else {
      DCHECK_EQ(kDescriptor, new_details.location());
      if (old_details.location() == kField ||
          old_descriptors->GetStrongValue(i) !=
              new_descriptors->GetStrongValue(i)) {
        return nullptr;
      }
    }
  }
  if (new_map->NumberOfOwnDescriptors() != old_nof) return nullptr;
  return new_map;
}


// static
Handle<Map> Map::Update(Isolate* isolate, Handle<Map> map) {
  if (!map->is_deprecated()) return map;
  MapUpdater mu(isolate, map);
  return mu.Update();
}

Maybe<bool> JSObject::SetPropertyWithInterceptor(LookupIterator* it,
                                                 ShouldThrow should_throw,
                                                 Handle<Object> value) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return SetPropertyWithInterceptorInternal(it, it->GetInterceptor(),
                                            should_throw, value);
}

MaybeHandle<Object> Object::SetProperty(Isolate* isolate, Handle<Object> object,
                                        Handle<Name> name, Handle<Object> value,
                                        LanguageMode language_mode,
                                        StoreOrigin store_origin) {
  LookupIterator it(isolate, object, name);
  MAYBE_RETURN_NULL(SetProperty(&it, value, language_mode, store_origin));
  return value;
}

Maybe<bool> Object::SetPropertyInternal(LookupIterator* it,
                                        Handle<Object> value,
                                        LanguageMode language_mode,
                                        StoreOrigin store_origin, bool* found) {
  it->UpdateProtector();
  DCHECK(it->IsFound());
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? kDontThrow : kThrowOnError;

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(it->isolate());

  do {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        // Check whether it makes sense to reuse the lookup iterator. Here it
        // might still call into setters up the prototype chain.
        return JSObject::SetPropertyWithFailedAccessCheck(it, value,
                                                          should_throw);

      case LookupIterator::JSPROXY:
        return JSProxy::SetProperty(it->GetHolder<JSProxy>(), it->GetName(),
                                    value, it->GetReceiver(), language_mode);

      case LookupIterator::INTERCEPTOR: {
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
        } else {
          Maybe<PropertyAttributes> maybe_attributes =
              JSObject::GetPropertyAttributesWithInterceptor(it);
          if (maybe_attributes.IsNothing()) return Nothing<bool>();
          if ((maybe_attributes.FromJust() & READ_ONLY) != 0) {
            return WriteToReadOnlyProperty(it, value, should_throw);
          }
          if (maybe_attributes.FromJust() == ABSENT) break;
          *found = false;
          return Nothing<bool>();
        }
        break;
      }

      case LookupIterator::ACCESSOR: {
        if (it->IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, should_throw);
        }
        Handle<Object> accessors = it->GetAccessors();
        if (accessors->IsAccessorInfo() &&
            !it->HolderIsReceiverOrHiddenPrototype() &&
            AccessorInfo::cast(*accessors)->is_special_data_property()) {
          *found = false;
          return Nothing<bool>();
        }
        return SetPropertyWithAccessor(it, value, should_throw);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC: {
        // IntegerIndexedElementSet converts value to a Number/BigInt prior to
        // the bounds check. The bounds check has already happened here, but
        // perform the possibly effectful ToNumber (or ToBigInt) operation
        // anyways.
        auto holder = it->GetHolder<JSTypedArray>();
        Handle<Object> throwaway_value;
        if (holder->type() == kExternalBigInt64Array ||
            holder->type() == kExternalBigUint64Array) {
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              it->isolate(), throwaway_value,
              BigInt::FromObject(it->isolate(), value), Nothing<bool>());
        } else {
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              it->isolate(), throwaway_value,
              Object::ToNumber(it->isolate(), value), Nothing<bool>());
        }

        // FIXME: Throw a TypeError if the holder is neutered here
        // (IntegerIndexedElementSpec step 5).

        // TODO(verwaest): Per spec, we should return false here (steps 6-9
        // in IntegerIndexedElementSpec), resulting in an exception being thrown
        // on OOB accesses in strict code. Historically, v8 has not done made
        // this change due to uncertainty about web compat. (v8:4901)
        return Just(true);
      }

      case LookupIterator::DATA:
        if (it->IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, should_throw);
        }
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          return SetDataProperty(it, value);
        }
        V8_FALLTHROUGH;
      case LookupIterator::TRANSITION:
        *found = false;
        return Nothing<bool>();
    }
    it->Next();
  } while (it->IsFound());

  *found = false;
  return Nothing<bool>();
}

Maybe<bool> Object::SetProperty(LookupIterator* it, Handle<Object> value,
                                LanguageMode language_mode,
                                StoreOrigin store_origin) {
  if (it->IsFound()) {
    bool found = true;
    Maybe<bool> result =
        SetPropertyInternal(it, value, language_mode, store_origin, &found);
    if (found) return result;
  }

  // If the receiver is the JSGlobalObject, the store was contextual. In case
  // the property did not exist yet on the global object itself, we have to
  // throw a reference error in strict mode.  In sloppy mode, we continue.
  if (is_strict(language_mode) && it->GetReceiver()->IsJSGlobalObject()) {
    it->isolate()->Throw(*it->isolate()->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, it->name()));
    return Nothing<bool>();
  }

  ShouldThrow should_throw =
      is_sloppy(language_mode) ? kDontThrow : kThrowOnError;
  return AddDataProperty(it, value, NONE, should_throw, store_origin);
}

Maybe<bool> Object::SetSuperProperty(LookupIterator* it, Handle<Object> value,
                                     LanguageMode language_mode,
                                     StoreOrigin store_origin) {
  Isolate* isolate = it->isolate();

  if (it->IsFound()) {
    bool found = true;
    Maybe<bool> result =
        SetPropertyInternal(it, value, language_mode, store_origin, &found);
    if (found) return result;
  }

  it->UpdateProtector();

  // The property either doesn't exist on the holder or exists there as a data
  // property.

  ShouldThrow should_throw =
      is_sloppy(language_mode) ? kDontThrow : kThrowOnError;

  if (!it->GetReceiver()->IsJSReceiver()) {
    return WriteToReadOnlyProperty(it, value, should_throw);
  }
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());

  LookupIterator::Configuration c = LookupIterator::OWN;
  LookupIterator own_lookup =
      it->IsElement() ? LookupIterator(isolate, receiver, it->index(), c)
                      : LookupIterator(isolate, receiver, it->name(), c);

  for (; own_lookup.IsFound(); own_lookup.Next()) {
    switch (own_lookup.state()) {
      case LookupIterator::ACCESS_CHECK:
        if (!own_lookup.HasAccess()) {
          return JSObject::SetPropertyWithFailedAccessCheck(&own_lookup, value,
                                                            should_throw);
        }
        break;

      case LookupIterator::ACCESSOR:
        if (own_lookup.GetAccessors()->IsAccessorInfo()) {
          if (own_lookup.IsReadOnly()) {
            return WriteToReadOnlyProperty(&own_lookup, value, should_throw);
          }
          return JSObject::SetPropertyWithAccessor(&own_lookup, value,
                                                   should_throw);
        }
        V8_FALLTHROUGH;
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return RedefineIncompatibleProperty(isolate, it->GetName(), value,
                                            should_throw);

      case LookupIterator::DATA: {
        if (own_lookup.IsReadOnly()) {
          return WriteToReadOnlyProperty(&own_lookup, value, should_throw);
        }
        return SetDataProperty(&own_lookup, value);
      }

      case LookupIterator::INTERCEPTOR:
      case LookupIterator::JSPROXY: {
        PropertyDescriptor desc;
        Maybe<bool> owned =
            JSReceiver::GetOwnPropertyDescriptor(&own_lookup, &desc);
        MAYBE_RETURN(owned, Nothing<bool>());
        if (!owned.FromJust()) {
          return JSReceiver::CreateDataProperty(&own_lookup, value,
                                                should_throw);
        }
        if (PropertyDescriptor::IsAccessorDescriptor(&desc) ||
            !desc.writable()) {
          return RedefineIncompatibleProperty(isolate, it->GetName(), value,
                                              should_throw);
        }

        PropertyDescriptor value_desc;
        value_desc.set_value(value);
        return JSReceiver::DefineOwnProperty(isolate, receiver, it->GetName(),
                                             &value_desc, should_throw);
      }

      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
    }
  }

  return AddDataProperty(&own_lookup, value, NONE, should_throw, store_origin);
}

Maybe<bool> Object::CannotCreateProperty(Isolate* isolate,
                                         Handle<Object> receiver,
                                         Handle<Object> name,
                                         Handle<Object> value,
                                         ShouldThrow should_throw) {
  RETURN_FAILURE(
      isolate, should_throw,
      NewTypeError(MessageTemplate::kStrictCannotCreateProperty, name,
                   Object::TypeOf(isolate, receiver), receiver));
}


Maybe<bool> Object::WriteToReadOnlyProperty(LookupIterator* it,
                                            Handle<Object> value,
                                            ShouldThrow should_throw) {
  return WriteToReadOnlyProperty(it->isolate(), it->GetReceiver(),
                                 it->GetName(), value, should_throw);
}


Maybe<bool> Object::WriteToReadOnlyProperty(Isolate* isolate,
                                            Handle<Object> receiver,
                                            Handle<Object> name,
                                            Handle<Object> value,
                                            ShouldThrow should_throw) {
  RETURN_FAILURE(isolate, should_throw,
                 NewTypeError(MessageTemplate::kStrictReadOnlyProperty, name,
                              Object::TypeOf(isolate, receiver), receiver));
}


Maybe<bool> Object::RedefineIncompatibleProperty(Isolate* isolate,
                                                 Handle<Object> name,
                                                 Handle<Object> value,
                                                 ShouldThrow should_throw) {
  RETURN_FAILURE(isolate, should_throw,
                 NewTypeError(MessageTemplate::kRedefineDisallowed, name));
}


Maybe<bool> Object::SetDataProperty(LookupIterator* it, Handle<Object> value) {
  DCHECK_IMPLIES(it->GetReceiver()->IsJSProxy(),
                 it->GetName()->IsPrivateField());
  DCHECK_IMPLIES(!it->IsElement() && it->GetName()->IsPrivateField(),
                 it->state() == LookupIterator::DATA);
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());

  // Store on the holder which may be hidden behind the receiver.
  DCHECK(it->HolderIsReceiverOrHiddenPrototype());

  Handle<Object> to_assign = value;
  // Convert the incoming value to a number for storing into typed arrays.
  if (it->IsElement() && receiver->IsJSObject() &&
      JSObject::cast(*receiver)->HasFixedTypedArrayElements()) {
    ElementsKind elements_kind = JSObject::cast(*receiver)->GetElementsKind();
    if (elements_kind == BIGINT64_ELEMENTS ||
        elements_kind == BIGUINT64_ELEMENTS) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(it->isolate(), to_assign,
                                       BigInt::FromObject(it->isolate(), value),
                                       Nothing<bool>());
      // We have to recheck the length. However, it can only change if the
      // underlying buffer was neutered, so just check that.
      if (Handle<JSArrayBufferView>::cast(receiver)->WasNeutered()) {
        return Just(true);
        // TODO(neis): According to the spec, this should throw a TypeError.
      }
    } else if (!value->IsNumber() && !value->IsUndefined(it->isolate())) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(it->isolate(), to_assign,
                                       Object::ToNumber(it->isolate(), value),
                                       Nothing<bool>());
      // We have to recheck the length. However, it can only change if the
      // underlying buffer was neutered, so just check that.
      if (Handle<JSArrayBufferView>::cast(receiver)->WasNeutered()) {
        return Just(true);
        // TODO(neis): According to the spec, this should throw a TypeError.
      }
    }
  }

  // Possibly migrate to the most up-to-date map that will be able to store
  // |value| under it->name().
  it->PrepareForDataProperty(to_assign);

  // Write the property value.
  it->WriteDataValue(to_assign, false);

#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    receiver->HeapObjectVerify(it->isolate());
  }
#endif
  return Just(true);
}

Maybe<bool> Object::AddDataProperty(LookupIterator* it, Handle<Object> value,
                                    PropertyAttributes attributes,
                                    ShouldThrow should_throw,
                                    StoreOrigin store_origin) {
  if (!it->GetReceiver()->IsJSReceiver()) {
    return CannotCreateProperty(it->isolate(), it->GetReceiver(), it->GetName(),
                                value, should_throw);
  }

  // Private symbols should be installed on JSProxy using
  // JSProxy::SetPrivateSymbol.
  if (it->GetReceiver()->IsJSProxy() && it->GetName()->IsPrivate() &&
      !it->GetName()->IsPrivateField()) {
    RETURN_FAILURE(it->isolate(), should_throw,
                   NewTypeError(MessageTemplate::kProxyPrivate));
  }

  DCHECK_NE(LookupIterator::INTEGER_INDEXED_EXOTIC, it->state());

  Handle<JSReceiver> receiver = it->GetStoreTarget<JSReceiver>();
  DCHECK_IMPLIES(receiver->IsJSProxy(), it->GetName()->IsPrivateField());
  DCHECK_IMPLIES(receiver->IsJSProxy(),
                 it->state() == LookupIterator::NOT_FOUND);

  // If the receiver is a JSGlobalProxy, store on the prototype (JSGlobalObject)
  // instead. If the prototype is Null, the proxy is detached.
  if (receiver->IsJSGlobalProxy()) return Just(true);

  Isolate* isolate = it->isolate();

  if (it->ExtendingNonExtensible(receiver)) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kObjectNotExtensible, it->GetName()));
  }

  if (it->IsElement()) {
    if (receiver->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(receiver);
      if (JSArray::WouldChangeReadOnlyLength(array, it->index())) {
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kStrictReadOnlyProperty,
                                    isolate->factory()->length_string(),
                                    Object::TypeOf(isolate, array), array));
      }

      if (FLAG_trace_external_array_abuse &&
          array->HasFixedTypedArrayElements()) {
        CheckArrayAbuse(array, "typed elements write", it->index(), true);
      }

      if (FLAG_trace_js_array_abuse && !array->HasFixedTypedArrayElements()) {
        CheckArrayAbuse(array, "elements write", it->index(), false);
      }
    }

    Handle<JSObject> receiver_obj = Handle<JSObject>::cast(receiver);
    JSObject::AddDataElement(receiver_obj, it->index(), value, attributes);
    JSObject::ValidateElements(*receiver_obj);
    return Just(true);
  } else {
    it->UpdateProtector();
    // Migrate to the most up-to-date map that will be able to store |value|
    // under it->name() with |attributes|.
    it->PrepareTransitionToDataProperty(receiver, value, attributes,
                                        store_origin);
    DCHECK_EQ(LookupIterator::TRANSITION, it->state());
    it->ApplyTransitionToDataProperty(receiver);

    // Write the property value.
    it->WriteDataValue(value, true);

#if VERIFY_HEAP
    if (FLAG_verify_heap) {
      receiver->HeapObjectVerify(isolate);
    }
#endif
  }

  return Just(true);
}

void Map::EnsureDescriptorSlack(Isolate* isolate, Handle<Map> map, int slack) {
  // Only supports adding slack to owned descriptors.
  DCHECK(map->owns_descriptors());

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int old_size = map->NumberOfOwnDescriptors();
  if (slack <= descriptors->NumberOfSlackDescriptors()) return;

  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::CopyUpTo(isolate, descriptors, old_size, slack);

  DisallowHeapAllocation no_allocation;
  // The descriptors are still the same, so keep the layout descriptor.
  LayoutDescriptor* layout_descriptor = map->GetLayoutDescriptor();

  if (old_size == 0) {
    map->UpdateDescriptors(*new_descriptors, layout_descriptor);
    return;
  }

  // If the source descriptors had an enum cache we copy it. This ensures
  // that the maps to which we push the new descriptor array back can rely
  // on a cache always being available once it is set. If the map has more
  // enumerated descriptors than available in the original cache, the cache
  // will be lazily replaced by the extended cache when needed.
  new_descriptors->CopyEnumCacheFrom(*descriptors);

  // Replace descriptors by new_descriptors in all maps that share it. The old
  // descriptors will not be trimmed in the mark-compactor, we need to mark
  // all its elements.
  MarkingBarrierForElements(isolate->heap(), *descriptors);

  Map* current = *map;
  while (current->instance_descriptors() == *descriptors) {
    Object* next = current->GetBackPointer();
    if (next->IsUndefined(isolate)) break;  // Stop overwriting at initial map.
    current->UpdateDescriptors(*new_descriptors, layout_descriptor);
    current = Map::cast(next);
  }
  map->UpdateDescriptors(*new_descriptors, layout_descriptor);
}

// static
Handle<Map> Map::GetObjectCreateMap(Isolate* isolate,
                                    Handle<HeapObject> prototype) {
  Handle<Map> map(isolate->native_context()->object_function()->initial_map(),
                  isolate);
  if (map->prototype() == *prototype) return map;
  if (prototype->IsNull(isolate)) {
    return isolate->slow_object_with_null_prototype_map();
  }
  if (prototype->IsJSObject()) {
    Handle<JSObject> js_prototype = Handle<JSObject>::cast(prototype);
    if (!js_prototype->map()->is_prototype_map()) {
      JSObject::OptimizeAsPrototype(js_prototype);
    }
    Handle<PrototypeInfo> info =
        Map::GetOrCreatePrototypeInfo(js_prototype, isolate);
    // TODO(verwaest): Use inobject slack tracking for this map.
    if (info->HasObjectCreateMap()) {
      map = handle(info->ObjectCreateMap(), isolate);
    } else {
      map = Map::CopyInitialMap(isolate, map);
      Map::SetPrototype(isolate, map, prototype);
      PrototypeInfo::SetObjectCreateMap(info, map);
    }
    return map;
  }

  return Map::TransitionToPrototype(isolate, map, prototype);
}

// static
MaybeHandle<Map> Map::TryGetObjectCreateMap(Isolate* isolate,
                                            Handle<HeapObject> prototype) {
  Handle<Map> map(isolate->native_context()->object_function()->initial_map(),
                  isolate);
  if (map->prototype() == *prototype) return map;
  if (prototype->IsNull(isolate)) {
    return isolate->slow_object_with_null_prototype_map();
  }
  if (!prototype->IsJSObject()) return MaybeHandle<Map>();
  Handle<JSObject> js_prototype = Handle<JSObject>::cast(prototype);
  if (!js_prototype->map()->is_prototype_map()) return MaybeHandle<Map>();
  Handle<PrototypeInfo> info =
      Map::GetOrCreatePrototypeInfo(js_prototype, isolate);
  if (!info->HasObjectCreateMap()) return MaybeHandle<Map>();
  return handle(info->ObjectCreateMap(), isolate);
}

template <class T>
static int AppendUniqueCallbacks(Isolate* isolate,
                                 Handle<TemplateList> callbacks,
                                 Handle<typename T::Array> array,
                                 int valid_descriptors) {
  int nof_callbacks = callbacks->length();

  // Fill in new callback descriptors.  Process the callbacks from
  // back to front so that the last callback with a given name takes
  // precedence over previously added callbacks with that name.
  for (int i = nof_callbacks - 1; i >= 0; i--) {
    Handle<AccessorInfo> entry(AccessorInfo::cast(callbacks->get(i)), isolate);
    Handle<Name> key(Name::cast(entry->name()), isolate);
    DCHECK(key->IsUniqueName());
    // Check if a descriptor with this name already exists before writing.
    if (!T::Contains(key, entry, valid_descriptors, array)) {
      T::Insert(key, entry, valid_descriptors, array);
      valid_descriptors++;
    }
  }

  return valid_descriptors;
}

struct FixedArrayAppender {
  typedef FixedArray Array;
  static bool Contains(Handle<Name> key,
                       Handle<AccessorInfo> entry,
                       int valid_descriptors,
                       Handle<FixedArray> array) {
    for (int i = 0; i < valid_descriptors; i++) {
      if (*key == AccessorInfo::cast(array->get(i))->name()) return true;
    }
    return false;
  }
  static void Insert(Handle<Name> key,
                     Handle<AccessorInfo> entry,
                     int valid_descriptors,
                     Handle<FixedArray> array) {
    DisallowHeapAllocation no_gc;
    array->set(valid_descriptors, *entry);
  }
};

int AccessorInfo::AppendUnique(Isolate* isolate, Handle<Object> descriptors,
                               Handle<FixedArray> array,
                               int valid_descriptors) {
  Handle<TemplateList> callbacks = Handle<TemplateList>::cast(descriptors);
  DCHECK_GE(array->length(), callbacks->length() + valid_descriptors);
  return AppendUniqueCallbacks<FixedArrayAppender>(isolate, callbacks, array,
                                                   valid_descriptors);
}

static bool ContainsMap(MapHandles const& maps, Map* map) {
  DCHECK_NOT_NULL(map);
  for (Handle<Map> current : maps) {
    if (!current.is_null() && *current == map) return true;
  }
  return false;
}

Map* Map::FindElementsKindTransitionedMap(Isolate* isolate,
                                          MapHandles const& candidates) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(isolate);

  if (is_prototype_map()) return nullptr;

  ElementsKind kind = elements_kind();
  bool packed = IsFastPackedElementsKind(kind);

  Map* transition = nullptr;
  if (IsTransitionableFastElementsKind(kind)) {
    // Check the state of the root map.
    Map* root_map = FindRootMap(isolate);
    if (!EquivalentToForElementsKindTransition(root_map)) return nullptr;
    root_map = root_map->LookupElementsTransitionMap(isolate, kind);
    DCHECK_NOT_NULL(root_map);
    // Starting from the next existing elements kind transition try to
    // replay the property transitions that does not involve instance rewriting
    // (ElementsTransitionAndStoreStub does not support that).
    for (root_map = root_map->ElementsTransitionMap();
         root_map != nullptr && root_map->has_fast_elements();
         root_map = root_map->ElementsTransitionMap()) {
      Map* current = root_map->TryReplayPropertyTransitions(isolate, this);
      if (current == nullptr) continue;
      if (InstancesNeedRewriting(current)) continue;

      if (ContainsMap(candidates, current) &&
          (packed || !IsFastPackedElementsKind(current->elements_kind()))) {
        transition = current;
        packed = packed && IsFastPackedElementsKind(current->elements_kind());
      }
    }
  }
  return transition;
}

static Map* FindClosestElementsTransition(Isolate* isolate, Map* map,
                                          ElementsKind to_kind) {
  // Ensure we are requested to search elements kind transition "near the root".
  DCHECK_EQ(map->FindRootMap(isolate)->NumberOfOwnDescriptors(),
            map->NumberOfOwnDescriptors());
  Map* current_map = map;

  ElementsKind kind = map->elements_kind();
  while (kind != to_kind) {
    Map* next_map = current_map->ElementsTransitionMap();
    if (next_map == nullptr) return current_map;
    kind = next_map->elements_kind();
    current_map = next_map;
  }

  DCHECK_EQ(to_kind, current_map->elements_kind());
  return current_map;
}

Map* Map::LookupElementsTransitionMap(Isolate* isolate, ElementsKind to_kind) {
  Map* to_map = FindClosestElementsTransition(isolate, this, to_kind);
  if (to_map->elements_kind() == to_kind) return to_map;
  return nullptr;
}

bool Map::IsMapInArrayPrototypeChain(Isolate* isolate) const {
  if (isolate->initial_array_prototype()->map() == this) {
    return true;
  }

  if (isolate->initial_object_prototype()->map() == this) {
    return true;
  }

  return false;
}

static Handle<Map> AddMissingElementsTransitions(Isolate* isolate,
                                                 Handle<Map> map,
                                                 ElementsKind to_kind) {
  DCHECK(IsTransitionElementsKind(map->elements_kind()));

  Handle<Map> current_map = map;

  ElementsKind kind = map->elements_kind();
  TransitionFlag flag;
  if (map->is_prototype_map()) {
    flag = OMIT_TRANSITION;
  } else {
    flag = INSERT_TRANSITION;
    if (IsFastElementsKind(kind)) {
      while (kind != to_kind && !IsTerminalElementsKind(kind)) {
        kind = GetNextTransitionElementsKind(kind);
        current_map = Map::CopyAsElementsKind(isolate, current_map, kind, flag);
      }
    }
  }

  // In case we are exiting the fast elements kind system, just add the map in
  // the end.
  if (kind != to_kind) {
    current_map = Map::CopyAsElementsKind(isolate, current_map, to_kind, flag);
  }

  DCHECK(current_map->elements_kind() == to_kind);
  return current_map;
}

Handle<Map> Map::TransitionElementsTo(Isolate* isolate, Handle<Map> map,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = map->elements_kind();
  if (from_kind == to_kind) return map;

  Context* native_context = isolate->context()->native_context();
  if (from_kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS) {
    if (*map == native_context->fast_aliased_arguments_map()) {
      DCHECK_EQ(SLOW_SLOPPY_ARGUMENTS_ELEMENTS, to_kind);
      return handle(native_context->slow_aliased_arguments_map(), isolate);
    }
  } else if (from_kind == SLOW_SLOPPY_ARGUMENTS_ELEMENTS) {
    if (*map == native_context->slow_aliased_arguments_map()) {
      DCHECK_EQ(FAST_SLOPPY_ARGUMENTS_ELEMENTS, to_kind);
      return handle(native_context->fast_aliased_arguments_map(), isolate);
    }
  } else if (IsFastElementsKind(from_kind) && IsFastElementsKind(to_kind)) {
    // Reuse map transitions for JSArrays.
    DisallowHeapAllocation no_gc;
    if (native_context->GetInitialJSArrayMap(from_kind) == *map) {
      Object* maybe_transitioned_map =
          native_context->get(Context::ArrayMapIndex(to_kind));
      if (maybe_transitioned_map->IsMap()) {
        return handle(Map::cast(maybe_transitioned_map), isolate);
      }
    }
  }

  DCHECK(!map->IsUndefined(isolate));
  // Check if we can go back in the elements kind transition chain.
  if (IsHoleyElementsKind(from_kind) &&
      to_kind == GetPackedElementsKind(from_kind) &&
      map->GetBackPointer()->IsMap() &&
      Map::cast(map->GetBackPointer())->elements_kind() == to_kind) {
    return handle(Map::cast(map->GetBackPointer()), isolate);
  }

  bool allow_store_transition = IsTransitionElementsKind(from_kind);
  // Only store fast element maps in ascending generality.
  if (IsFastElementsKind(to_kind)) {
    allow_store_transition =
        allow_store_transition && IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind);
  }

  if (!allow_store_transition) {
    return Map::CopyAsElementsKind(isolate, map, to_kind, OMIT_TRANSITION);
  }

  return Map::ReconfigureElementsKind(isolate, map, to_kind);
}


// static
Handle<Map> Map::AsElementsKind(Isolate* isolate, Handle<Map> map,
                                ElementsKind kind) {
  Handle<Map> closest_map(FindClosestElementsTransition(isolate, *map, kind),
                          isolate);

  if (closest_map->elements_kind() == kind) {
    return closest_map;
  }

  return AddMissingElementsTransitions(isolate, closest_map, kind);
}


Handle<Map> JSObject::GetElementsTransitionMap(Handle<JSObject> object,
                                               ElementsKind to_kind) {
  Handle<Map> map(object->map(), object->GetIsolate());
  return Map::TransitionElementsTo(object->GetIsolate(), map, to_kind);
}


void JSProxy::Revoke(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  // ES#sec-proxy-revocation-functions
  if (!proxy->IsRevoked()) {
    // 5. Set p.[[ProxyTarget]] to null.
    proxy->set_target(ReadOnlyRoots(isolate).null_value());
    // 6. Set p.[[ProxyHandler]] to null.
    proxy->set_handler(ReadOnlyRoots(isolate).null_value());
  }
  DCHECK(proxy->IsRevoked());
}

// static
Maybe<bool> JSProxy::IsArray(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<JSReceiver> object = Handle<JSReceiver>::cast(proxy);
  for (int i = 0; i < JSProxy::kMaxIterationLimit; i++) {
    Handle<JSProxy> proxy = Handle<JSProxy>::cast(object);
    if (proxy->IsRevoked()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyRevoked,
          isolate->factory()->NewStringFromAsciiChecked("IsArray")));
      return Nothing<bool>();
    }
    object = handle(JSReceiver::cast(proxy->target()), isolate);
    if (object->IsJSArray()) return Just(true);
    if (!object->IsJSProxy()) return Just(false);
  }

  // Too deep recursion, throw a RangeError.
  isolate->StackOverflow();
  return Nothing<bool>();
}

Maybe<bool> JSProxy::HasProperty(Isolate* isolate, Handle<JSProxy> proxy,
                                 Handle<Name> name) {
  DCHECK(!name->IsPrivate());
  STACK_CHECK(isolate, Nothing<bool>());
  // 1. (Assert)
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, isolate->factory()->has_string()));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 6. Let trap be ? GetMethod(handler, "has").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(Handle<JSReceiver>::cast(handler),
                                       isolate->factory()->has_string()),
      Nothing<bool>());
  // 7. If trap is undefined, then
  if (trap->IsUndefined(isolate)) {
    // 7a. Return target.[[HasProperty]](P).
    return JSReceiver::HasProperty(target, name);
  }
  // 8. Let booleanTrapResult be ToBoolean(? Call(trap, handler, «target, P»)).
  Handle<Object> trap_result_obj;
  Handle<Object> args[] = {target, name};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result_obj,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  bool boolean_trap_result = trap_result_obj->BooleanValue(isolate);
  // 9. If booleanTrapResult is false, then:
  if (!boolean_trap_result) {
    MAYBE_RETURN(JSProxy::CheckHasTrap(isolate, name, target), Nothing<bool>());
  }
  // 10. Return booleanTrapResult.
  return Just(boolean_trap_result);
}

Maybe<bool> JSProxy::CheckHasTrap(Isolate* isolate, Handle<Name> name,
                                  Handle<JSReceiver> target) {
  // 9a. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> target_found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN(target_found, Nothing<bool>());
  // 9b. If targetDesc is not undefined, then:
  if (target_found.FromJust()) {
    // 9b i. If targetDesc.[[Configurable]] is false, throw a TypeError
    //       exception.
    if (!target_desc.configurable()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyHasNonConfigurable, name));
      return Nothing<bool>();
    }
    // 9b ii. Let extensibleTarget be ? IsExtensible(target).
    Maybe<bool> extensible_target = JSReceiver::IsExtensible(target);
    MAYBE_RETURN(extensible_target, Nothing<bool>());
    // 9b iii. If extensibleTarget is false, throw a TypeError exception.
    if (!extensible_target.FromJust()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyHasNonExtensible, name));
      return Nothing<bool>();
    }
  }
  return Just(true);
}

Maybe<bool> JSProxy::SetProperty(Handle<JSProxy> proxy, Handle<Name> name,
                                 Handle<Object> value, Handle<Object> receiver,
                                 LanguageMode language_mode) {
  DCHECK(!name->IsPrivate());
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(isolate, Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->set_string();
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? kDontThrow : kThrowOnError;

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined(isolate)) {
    LookupIterator it =
        LookupIterator::PropertyOrElement(isolate, receiver, name, target);
    return Object::SetSuperProperty(&it, value, language_mode,
                                    StoreOrigin::kMaybeKeyed);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target, name, value, receiver};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  if (!trap_result->BooleanValue(isolate)) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, name));
  }

  MaybeHandle<Object> result =
      JSProxy::CheckGetSetTrapResult(isolate, name, target, value, kSet);

  if (result.is_null()) {
    return Nothing<bool>();
  }
  return Just(true);
}


Maybe<bool> JSProxy::DeletePropertyOrElement(Handle<JSProxy> proxy,
                                             Handle<Name> name,
                                             LanguageMode language_mode) {
  DCHECK(!name->IsPrivate());
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? kDontThrow : kThrowOnError;
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(isolate, Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->deleteProperty_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined(isolate)) {
    return JSReceiver::DeletePropertyOrElement(target, name, language_mode);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target, name};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  if (!trap_result->BooleanValue(isolate)) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, name));
  }

  // Enforce the invariant.
  PropertyDescriptor target_desc;
  Maybe<bool> owned =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN(owned, Nothing<bool>());
  if (owned.FromJust() && !target_desc.configurable()) {
    isolate->Throw(*factory->NewTypeError(
        MessageTemplate::kProxyDeletePropertyNonConfigurable, name));
    return Nothing<bool>();
  }
  return Just(true);
}


// static
MaybeHandle<JSProxy> JSProxy::New(Isolate* isolate, Handle<Object> target,
                                  Handle<Object> handler) {
  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kProxyNonObject),
                    JSProxy);
  }
  if (target->IsJSProxy() && JSProxy::cast(*target)->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyHandlerOrTargetRevoked),
                    JSProxy);
  }
  if (!handler->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kProxyNonObject),
                    JSProxy);
  }
  if (handler->IsJSProxy() && JSProxy::cast(*handler)->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyHandlerOrTargetRevoked),
                    JSProxy);
  }
  return isolate->factory()->NewJSProxy(Handle<JSReceiver>::cast(target),
                                        Handle<JSReceiver>::cast(handler));
}


// static
MaybeHandle<Context> JSProxy::GetFunctionRealm(Handle<JSProxy> proxy) {
  DCHECK(proxy->map()->is_constructor());
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(proxy->GetIsolate(),
                    NewTypeError(MessageTemplate::kProxyRevoked), Context);
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()),
                            proxy->GetIsolate());
  return JSReceiver::GetFunctionRealm(target);
}


// static
MaybeHandle<Context> JSBoundFunction::GetFunctionRealm(
    Handle<JSBoundFunction> function) {
  DCHECK(function->map()->is_constructor());
  return JSReceiver::GetFunctionRealm(
      handle(function->bound_target_function(), function->GetIsolate()));
}

// static
MaybeHandle<String> JSBoundFunction::GetName(Isolate* isolate,
                                             Handle<JSBoundFunction> function) {
  Handle<String> prefix = isolate->factory()->bound__string();
  Handle<String> target_name = prefix;
  Factory* factory = isolate->factory();
  // Concatenate the "bound " up to the last non-bound target.
  while (function->bound_target_function()->IsJSBoundFunction()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, target_name,
                               factory->NewConsString(prefix, target_name),
                               String);
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
  }
  if (function->bound_target_function()->IsJSFunction()) {
    Handle<JSFunction> target(
        JSFunction::cast(function->bound_target_function()), isolate);
    Handle<Object> name = JSFunction::GetName(isolate, target);
    if (!name->IsString()) return target_name;
    return factory->NewConsString(target_name, Handle<String>::cast(name));
  }
  // This will omit the proper target name for bound JSProxies.
  return target_name;
}

// static
Maybe<int> JSBoundFunction::GetLength(Isolate* isolate,
                                      Handle<JSBoundFunction> function) {
  int nof_bound_arguments = function->bound_arguments()->length();
  while (function->bound_target_function()->IsJSBoundFunction()) {
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
    // Make sure we never overflow {nof_bound_arguments}, the number of
    // arguments of a function is strictly limited by the max length of an
    // JSAarray, Smi::kMaxValue is thus a reasonably good overestimate.
    int length = function->bound_arguments()->length();
    if (V8_LIKELY(Smi::kMaxValue - nof_bound_arguments > length)) {
      nof_bound_arguments += length;
    } else {
      nof_bound_arguments = Smi::kMaxValue;
    }
  }
  // All non JSFunction targets get a direct property and don't use this
  // accessor.
  Handle<JSFunction> target(JSFunction::cast(function->bound_target_function()),
                            isolate);
  Maybe<int> target_length = JSFunction::GetLength(isolate, target);
  if (target_length.IsNothing()) return target_length;

  int length = Max(0, target_length.FromJust() - nof_bound_arguments);
  return Just(length);
}

// static
Handle<Object> JSFunction::GetName(Isolate* isolate,
                                   Handle<JSFunction> function) {
  if (function->shared()->name_should_print_as_anonymous()) {
    return isolate->factory()->anonymous_string();
  }
  return handle(function->shared()->Name(), isolate);
}

// static
Maybe<int> JSFunction::GetLength(Isolate* isolate,
                                 Handle<JSFunction> function) {
  int length = 0;
  if (function->shared()->is_compiled()) {
    length = function->shared()->GetLength();
  } else {
    // If the function isn't compiled yet, the length is not computed
    // correctly yet. Compile it now and return the right length.
    if (Compiler::Compile(function, Compiler::KEEP_EXCEPTION)) {
      length = function->shared()->GetLength();
    }
    if (isolate->has_pending_exception()) return Nothing<int>();
  }
  DCHECK_GE(length, 0);
  return Just(length);
}

// static
Handle<Context> JSFunction::GetFunctionRealm(Handle<JSFunction> function) {
  DCHECK(function->map()->is_constructor());
  return handle(function->context()->native_context(), function->GetIsolate());
}


// static
MaybeHandle<Context> JSObject::GetFunctionRealm(Handle<JSObject> object) {
  DCHECK(object->map()->is_constructor());
  DCHECK(!object->IsJSFunction());
  return object->GetCreationContext();
}


// static
MaybeHandle<Context> JSReceiver::GetFunctionRealm(Handle<JSReceiver> receiver) {
  if (receiver->IsJSProxy()) {
    return JSProxy::GetFunctionRealm(Handle<JSProxy>::cast(receiver));
  }

  if (receiver->IsJSFunction()) {
    return JSFunction::GetFunctionRealm(Handle<JSFunction>::cast(receiver));
  }

  if (receiver->IsJSBoundFunction()) {
    return JSBoundFunction::GetFunctionRealm(
        Handle<JSBoundFunction>::cast(receiver));
  }

  return JSObject::GetFunctionRealm(Handle<JSObject>::cast(receiver));
}


Maybe<PropertyAttributes> JSProxy::GetPropertyAttributes(LookupIterator* it) {
  PropertyDescriptor desc;
  Maybe<bool> found = JSProxy::GetOwnPropertyDescriptor(
      it->isolate(), it->GetHolder<JSProxy>(), it->GetName(), &desc);
  MAYBE_RETURN(found, Nothing<PropertyAttributes>());
  if (!found.FromJust()) return Just(ABSENT);
  return Just(desc.ToAttributes());
}


void JSObject::AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map) {
  DCHECK(object->map()->GetInObjectProperties() ==
         map->GetInObjectProperties());
  ElementsKind obj_kind = object->map()->elements_kind();
  ElementsKind map_kind = map->elements_kind();
  if (map_kind != obj_kind) {
    ElementsKind to_kind = GetMoreGeneralElementsKind(map_kind, obj_kind);
    if (IsDictionaryElementsKind(obj_kind)) {
      to_kind = obj_kind;
    }
    if (IsDictionaryElementsKind(to_kind)) {
      NormalizeElements(object);
    } else {
      TransitionElementsKind(object, to_kind);
    }
    map = Map::ReconfigureElementsKind(object->GetIsolate(), map, to_kind);
  }
  int number_of_fields = map->NumberOfFields();
  int inobject = map->GetInObjectProperties();
  int unused = map->UnusedPropertyFields();
  int total_size = number_of_fields + unused;
  int external = total_size - inobject;
  // Allocate mutable double boxes if necessary. It is always necessary if we
  // have external properties, but is also necessary if we only have inobject
  // properties but don't unbox double fields.
  if (!FLAG_unbox_double_fields || external > 0) {
    Isolate* isolate = object->GetIsolate();

    Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
    Handle<FixedArray> storage;
    if (!FLAG_unbox_double_fields) {
      storage = isolate->factory()->NewFixedArray(inobject);
    }

    Handle<PropertyArray> array =
        isolate->factory()->NewPropertyArray(external);

    for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
      PropertyDetails details = descriptors->GetDetails(i);
      Representation representation = details.representation();
      if (!representation.IsDouble()) continue;
      FieldIndex index = FieldIndex::ForDescriptor(*map, i);
      if (map->IsUnboxedDoubleField(index)) continue;
      auto box = isolate->factory()->NewMutableHeapNumberWithHoleNaN();
      if (index.is_inobject()) {
        storage->set(index.property_index(), *box);
      } else {
        array->set(index.outobject_array_index(), *box);
      }
    }

    object->SetProperties(*array);

    if (!FLAG_unbox_double_fields) {
      for (int i = 0; i < inobject; i++) {
        FieldIndex index = FieldIndex::ForPropertyIndex(*map, i);
        Object* value = storage->get(i);
        object->RawFastPropertyAtPut(index, value);
      }
    }
  }
  object->synchronized_set_map(*map);
}


void JSObject::MigrateInstance(Handle<JSObject> object) {
  Handle<Map> original_map(object->map(), object->GetIsolate());
  Handle<Map> map = Map::Update(object->GetIsolate(), original_map);
  map->set_is_migration_target(true);
  MigrateToMap(object, map);
  if (FLAG_trace_migration) {
    object->PrintInstanceMigration(stdout, *original_map, *map);
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify(object->GetIsolate());
  }
#endif
}


// static
bool JSObject::TryMigrateInstance(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  DisallowDeoptimization no_deoptimization(isolate);
  Handle<Map> original_map(object->map(), isolate);
  Handle<Map> new_map;
  if (!Map::TryUpdate(isolate, original_map).ToHandle(&new_map)) {
    return false;
  }
  JSObject::MigrateToMap(object, new_map);
  if (FLAG_trace_migration && *original_map != object->map()) {
    object->PrintInstanceMigration(stdout, *original_map, object->map());
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify(isolate);
  }
#endif
  return true;
}

void JSObject::AddProperty(Isolate* isolate, Handle<JSObject> object,
                           Handle<Name> name, Handle<Object> value,
                           PropertyAttributes attributes) {
  LookupIterator it(isolate, object, name, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
#ifdef DEBUG
  uint32_t index;
  DCHECK(!object->IsJSProxy());
  DCHECK(!name->AsArrayIndex(&index));
  Maybe<PropertyAttributes> maybe = GetPropertyAttributes(&it);
  DCHECK(maybe.IsJust());
  DCHECK(!it.IsFound());
  DCHECK(object->map()->is_extensible() || name->IsPrivate());
#endif
  CHECK(AddDataProperty(&it, value, attributes, kThrowOnError,
                        StoreOrigin::kNamed)
            .IsJust());
}


// Reconfigures a property to a data property with attributes, even if it is not
// reconfigurable.
// Requires a LookupIterator that does not look at the prototype chain beyond
// hidden prototypes.
MaybeHandle<Object> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    AccessorInfoHandling handling) {
  MAYBE_RETURN_NULL(DefineOwnPropertyIgnoreAttributes(it, value, attributes,
                                                      kThrowOnError, handling));
  return value;
}


Maybe<bool> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    ShouldThrow should_throw, AccessorInfoHandling handling) {
  it->UpdateProtector();
  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (!it->HasAccess()) {
          it->isolate()->ReportFailedAccessCheck(it->GetHolder<JSObject>());
          RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
          return Just(true);
        }
        break;

      // If there's an interceptor, try to store the property with the
      // interceptor.
      // In case of success, the attributes will have been reset to the default
      // attributes of the interceptor, rather than the incoming attributes.
      //
      // TODO(verwaest): JSProxy afterwards verify the attributes that the
      // JSProxy claims it has, and verifies that they are compatible. If not,
      // they throw. Here we should do the same.
      case LookupIterator::INTERCEPTOR:
        if (handling == DONT_FORCE_FIELD) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
        }
        break;

      case LookupIterator::ACCESSOR: {
        Handle<Object> accessors = it->GetAccessors();

        // Special handling for AccessorInfo, which behaves like a data
        // property.
        if (accessors->IsAccessorInfo() && handling == DONT_FORCE_FIELD) {
          PropertyAttributes current_attributes = it->property_attributes();
          // Ensure the context isn't changed after calling into accessors.
          AssertNoContextChange ncc(it->isolate());

          // Update the attributes before calling the setter. The setter may
          // later change the shape of the property.
          if (current_attributes != attributes) {
            it->TransitionToAccessorPair(accessors, attributes);
          }

          return JSObject::SetPropertyWithAccessor(it, value, should_throw);
        }

        it->ReconfigureDataProperty(value, attributes);
        return Just(true);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return RedefineIncompatibleProperty(it->isolate(), it->GetName(), value,
                                            should_throw);

      case LookupIterator::DATA: {
        // Regular property update if the attributes match.
        if (it->property_attributes() == attributes) {
          return SetDataProperty(it, value);
        }

        // Special case: properties of typed arrays cannot be reconfigured to
        // non-writable nor to non-enumerable.
        if (it->IsElement() && object->HasFixedTypedArrayElements()) {
          return RedefineIncompatibleProperty(it->isolate(), it->GetName(),
                                              value, should_throw);
        }

        // Reconfigure the data property if the attributes mismatch.
        it->ReconfigureDataProperty(value, attributes);

        return Just(true);
      }
    }
  }

  return AddDataProperty(it, value, attributes, should_throw,
                         StoreOrigin::kNamed);
}

MaybeHandle<Object> JSObject::SetOwnPropertyIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  DCHECK(!value->IsTheHole());
  LookupIterator it(object, name, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::SetOwnElementIgnoreAttributes(
    Handle<JSObject> object, uint32_t index, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::DefinePropertyOrElementIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithInterceptor(
    LookupIterator* it) {
  return GetPropertyAttributesWithInterceptorInternal(it, it->GetInterceptor());
}

Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::GetPropertyAttributes(it);
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (result.IsNothing()) return result;
        if (result.FromJust() != ABSENT) return result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        return JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(ABSENT);
      case LookupIterator::ACCESSOR:
        if (it->GetHolder<Object>()->IsJSModuleNamespace()) {
          return JSModuleNamespace::GetPropertyAttributes(it);
        } else {
          return Just(it->property_attributes());
        }
      case LookupIterator::DATA:
        return Just(it->property_attributes());
    }
  }
  return Just(ABSENT);
}


Handle<NormalizedMapCache> NormalizedMapCache::New(Isolate* isolate) {
  Handle<WeakFixedArray> array(
      isolate->factory()->NewWeakFixedArray(kEntries, TENURED));
  return Handle<NormalizedMapCache>::cast(array);
}


MaybeHandle<Map> NormalizedMapCache::Get(Handle<Map> fast_map,
                                         PropertyNormalizationMode mode) {
  DisallowHeapAllocation no_gc;
  MaybeObject* value = WeakFixedArray::Get(GetIndex(fast_map));
  HeapObject* heap_object;
  if (!value->GetHeapObjectIfWeak(&heap_object)) {
    return MaybeHandle<Map>();
  }

  Map* normalized_map = Map::cast(heap_object);
  if (!normalized_map->EquivalentToForNormalization(*fast_map, mode)) {
    return MaybeHandle<Map>();
  }
  return handle(normalized_map, GetIsolate());
}

void NormalizedMapCache::Set(Handle<Map> fast_map, Handle<Map> normalized_map) {
  DisallowHeapAllocation no_gc;
  DCHECK(normalized_map->is_dictionary_map());
  WeakFixedArray::Set(GetIndex(fast_map),
                      HeapObjectReference::Weak(*normalized_map));
}

void JSObject::NormalizeProperties(Handle<JSObject> object,
                                   PropertyNormalizationMode mode,
                                   int expected_additional_properties,
                                   const char* reason) {
  if (!object->HasFastProperties()) return;

  Handle<Map> map(object->map(), object->GetIsolate());
  Handle<Map> new_map = Map::Normalize(object->GetIsolate(), map, mode, reason);

  MigrateToMap(object, new_map, expected_additional_properties);
}


void JSObject::MigrateSlowToFast(Handle<JSObject> object,
                                 int unused_property_fields,
                                 const char* reason) {
  if (object->HasFastProperties()) return;
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);

  // Make sure we preserve dictionary representation if there are too many
  // descriptors.
  int number_of_elements = dictionary->NumberOfElements();
  if (number_of_elements > kMaxNumberOfDescriptors) return;

  Handle<FixedArray> iteration_order =
      NameDictionary::IterationIndices(isolate, dictionary);

  int instance_descriptor_length = iteration_order->length();
  int number_of_fields = 0;

  // Compute the length of the instance descriptor.
  ReadOnlyRoots roots(isolate);
  for (int i = 0; i < instance_descriptor_length; i++) {
    int index = Smi::ToInt(iteration_order->get(i));
    DCHECK(dictionary->IsKey(roots, dictionary->KeyAt(index)));

    PropertyKind kind = dictionary->DetailsAt(index).kind();
    if (kind == kData) {
      if (FLAG_track_constant_fields) {
        number_of_fields += 1;
      } else {
        Object* value = dictionary->ValueAt(index);
        if (!value->IsJSFunction()) {
          number_of_fields += 1;
        }
      }
    }
  }

  Handle<Map> old_map(object->map(), isolate);

  int inobject_props = old_map->GetInObjectProperties();

  // Allocate new map.
  Handle<Map> new_map = Map::CopyDropDescriptors(isolate, old_map);
  if (new_map->has_named_interceptor() || new_map->is_access_check_needed()) {
    // Force certain slow paths when API interceptors are used, or if an access
    // check is required.
    new_map->set_may_have_interesting_symbols(true);
  }
  new_map->set_is_dictionary_map(false);

  NotifyMapChange(old_map, new_map, isolate);

  if (FLAG_trace_maps) {
    LOG(isolate, MapEvent("SlowToFast", *old_map, *new_map, reason));
  }

  if (instance_descriptor_length == 0) {
    DisallowHeapAllocation no_gc;
    DCHECK_LE(unused_property_fields, inobject_props);
    // Transform the object.
    new_map->SetInObjectUnusedPropertyFields(inobject_props);
    object->synchronized_set_map(*new_map);
    object->SetProperties(ReadOnlyRoots(isolate).empty_fixed_array());
    // Check that it really works.
    DCHECK(object->HasFastProperties());
    return;
  }

  // Allocate the instance descriptor.
  Handle<DescriptorArray> descriptors = DescriptorArray::Allocate(
      isolate, instance_descriptor_length, 0, TENURED);

  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;
  if (number_of_allocated_fields < 0) {
    // There is enough inobject space for all fields (including unused).
    number_of_allocated_fields = 0;
    unused_property_fields = inobject_props - number_of_fields;
  }

  // Allocate the property array for the fields.
  Handle<PropertyArray> fields =
      factory->NewPropertyArray(number_of_allocated_fields);

  bool is_transitionable_elements_kind =
      IsTransitionableFastElementsKind(old_map->elements_kind());

  // Fill in the instance descriptor and the fields.
  int current_offset = 0;
  for (int i = 0; i < instance_descriptor_length; i++) {
    int index = Smi::ToInt(iteration_order->get(i));
    Name* k = dictionary->NameAt(index);
    // Dictionary keys are internalized upon insertion.
    // TODO(jkummerow): Turn this into a DCHECK if it's not hit in the wild.
    CHECK(k->IsUniqueName());
    Handle<Name> key(k, isolate);

    // Properly mark the {new_map} if the {key} is an "interesting symbol".
    if (key->IsInterestingSymbol()) {
      new_map->set_may_have_interesting_symbols(true);
    }

    Object* value = dictionary->ValueAt(index);

    PropertyDetails details = dictionary->DetailsAt(index);
    DCHECK_EQ(kField, details.location());
    DCHECK_EQ(PropertyConstness::kMutable, details.constness());

    Descriptor d;
    if (details.kind() == kData) {
      if (!FLAG_track_constant_fields && value->IsJSFunction()) {
        d = Descriptor::DataConstant(key, handle(value, isolate),
                                     details.attributes());
      } else {
        // Ensure that we make constant field only when elements kind is not
        // transitionable.
        PropertyConstness constness =
            FLAG_track_constant_fields && !is_transitionable_elements_kind
                ? PropertyConstness::kConst
                : PropertyConstness::kMutable;
        d = Descriptor::DataField(
            key, current_offset, details.attributes(), constness,
            // TODO(verwaest): value->OptimalRepresentation();
            Representation::Tagged(),
            MaybeObjectHandle(FieldType::Any(isolate)));
      }
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      d = Descriptor::AccessorConstant(key, handle(value, isolate),
                                       details.attributes());
    }
    details = d.GetDetails();
    if (details.location() == kField) {
      if (current_offset < inobject_props) {
        object->InObjectPropertyAtPut(current_offset, value,
                                      UPDATE_WRITE_BARRIER);
      } else {
        int offset = current_offset - inobject_props;
        fields->set(offset, value);
      }
      current_offset += details.field_width_in_words();
    }
    descriptors->Set(i, &d);
  }
  DCHECK(current_offset == number_of_fields);

  descriptors->Sort();

  Handle<LayoutDescriptor> layout_descriptor = LayoutDescriptor::New(
      isolate, new_map, descriptors, descriptors->number_of_descriptors());

  DisallowHeapAllocation no_gc;
  new_map->InitializeDescriptors(*descriptors, *layout_descriptor);
  if (number_of_allocated_fields == 0) {
    new_map->SetInObjectUnusedPropertyFields(unused_property_fields);
  } else {
    new_map->SetOutOfObjectUnusedPropertyFields(unused_property_fields);
  }

  // Transform the object.
  object->synchronized_set_map(*new_map);

  object->SetProperties(*fields);
  DCHECK(object->IsJSObject());

  // Check that it really works.
  DCHECK(object->HasFastProperties());
}

void JSObject::RequireSlowElements(NumberDictionary* dictionary) {
  if (dictionary->requires_slow_elements()) return;
  dictionary->set_requires_slow_elements();
  if (map()->is_prototype_map()) {
    // If this object is a prototype (the callee will check), invalidate any
    // prototype chains involving it.
    InvalidatePrototypeChains(map());
  }
}

Handle<NumberDictionary> JSObject::NormalizeElements(Handle<JSObject> object) {
  DCHECK(!object->HasFixedTypedArrayElements());
  Isolate* isolate = object->GetIsolate();
  bool is_sloppy_arguments = object->HasSloppyArgumentsElements();
  {
    DisallowHeapAllocation no_gc;
    FixedArrayBase* elements = object->elements();

    if (is_sloppy_arguments) {
      elements = SloppyArgumentsElements::cast(elements)->arguments();
    }

    if (elements->IsNumberDictionary()) {
      return handle(NumberDictionary::cast(elements), isolate);
    }
  }

  DCHECK(object->HasSmiOrObjectElements() || object->HasDoubleElements() ||
         object->HasFastArgumentsElements() ||
         object->HasFastStringWrapperElements());

  Handle<NumberDictionary> dictionary =
      object->GetElementsAccessor()->Normalize(object);

  // Switch to using the dictionary as the backing storage for elements.
  ElementsKind target_kind = is_sloppy_arguments
                                 ? SLOW_SLOPPY_ARGUMENTS_ELEMENTS
                                 : object->HasFastStringWrapperElements()
                                       ? SLOW_STRING_WRAPPER_ELEMENTS
                                       : DICTIONARY_ELEMENTS;
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(object, target_kind);
  // Set the new map first to satify the elements type assert in set_elements().
  JSObject::MigrateToMap(object, new_map);

  if (is_sloppy_arguments) {
    SloppyArgumentsElements::cast(object->elements())
        ->set_arguments(*dictionary);
  } else {
    object->set_elements(*dictionary);
  }

  isolate->counters()->elements_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    StdoutStream os;
    os << "Object elements have been normalized:\n";
    object->Print(os);
  }
#endif

  DCHECK(object->HasDictionaryElements() ||
         object->HasSlowArgumentsElements() ||
         object->HasSlowStringWrapperElements());
  return dictionary;
}

namespace {

Object* SetHashAndUpdateProperties(Isolate* isolate, HeapObject* properties,
                                   int hash) {
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);
  DCHECK(PropertyArray::HashField::is_valid(hash));

  Heap* heap = isolate->heap();
  ReadOnlyRoots roots(heap);
  if (properties == roots.empty_fixed_array() ||
      properties == roots.empty_property_array() ||
      properties == heap->empty_property_dictionary()) {
    return Smi::FromInt(hash);
  }

  if (properties->IsPropertyArray()) {
    PropertyArray::cast(properties)->SetHash(hash);
    DCHECK_LT(0, PropertyArray::cast(properties)->length());
    return properties;
  }

  if (properties->IsGlobalDictionary()) {
    GlobalDictionary::cast(properties)->SetHash(hash);
    return properties;
  }

  DCHECK(properties->IsNameDictionary());
  NameDictionary::cast(properties)->SetHash(hash);
  return properties;
}

int GetIdentityHashHelper(Isolate* isolate, JSReceiver* object) {
  DisallowHeapAllocation no_gc;
  Object* properties = object->raw_properties_or_hash();
  if (properties->IsSmi()) {
    return Smi::ToInt(properties);
  }

  if (properties->IsPropertyArray()) {
    return PropertyArray::cast(properties)->Hash();
  }

  if (properties->IsNameDictionary()) {
    return NameDictionary::cast(properties)->Hash();
  }

  if (properties->IsGlobalDictionary()) {
    return GlobalDictionary::cast(properties)->Hash();
  }

#ifdef DEBUG
  FixedArray* empty_fixed_array = ReadOnlyRoots(isolate).empty_fixed_array();
  FixedArray* empty_property_dictionary =
      isolate->heap()->empty_property_dictionary();
  DCHECK(properties == empty_fixed_array ||
         properties == empty_property_dictionary);
#endif

  return PropertyArray::kNoHashSentinel;
}
}  // namespace

void JSReceiver::SetIdentityHash(int hash) {
  DisallowHeapAllocation no_gc;
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);
  DCHECK(PropertyArray::HashField::is_valid(hash));

  HeapObject* existing_properties = HeapObject::cast(raw_properties_or_hash());
  Object* new_properties =
      SetHashAndUpdateProperties(GetIsolate(), existing_properties, hash);
  set_raw_properties_or_hash(new_properties);
}

void JSReceiver::SetProperties(HeapObject* properties) {
  DCHECK_IMPLIES(properties->IsPropertyArray() &&
                     PropertyArray::cast(properties)->length() == 0,
                 properties == GetReadOnlyRoots().empty_property_array());
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  int hash = GetIdentityHashHelper(isolate, this);
  Object* new_properties = properties;

  // TODO(cbruni): Make GetIdentityHashHelper return a bool so that we
  // don't have to manually compare against kNoHashSentinel.
  if (hash != PropertyArray::kNoHashSentinel) {
    new_properties = SetHashAndUpdateProperties(isolate, properties, hash);
  }

  set_raw_properties_or_hash(new_properties);
}

Object* JSReceiver::GetIdentityHash(Isolate* isolate) {
  DisallowHeapAllocation no_gc;

  int hash = GetIdentityHashHelper(isolate, this);
  if (hash == PropertyArray::kNoHashSentinel) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  return Smi::FromInt(hash);
}

// static
Smi* JSReceiver::CreateIdentityHash(Isolate* isolate, JSReceiver* key) {
  DisallowHeapAllocation no_gc;
  int hash = isolate->GenerateIdentityHash(PropertyArray::HashField::kMax);
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);

  key->SetIdentityHash(hash);
  return Smi::FromInt(hash);
}

Smi* JSReceiver::GetOrCreateIdentityHash(Isolate* isolate) {
  DisallowHeapAllocation no_gc;

  Object* hash_obj = GetIdentityHash(isolate);
  if (!hash_obj->IsUndefined(isolate)) {
    return Smi::cast(hash_obj);
  }

  return JSReceiver::CreateIdentityHash(isolate, this);
}

Maybe<bool> JSObject::DeletePropertyWithInterceptor(LookupIterator* it,
                                                    ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  Handle<InterceptorInfo> interceptor(it->GetInterceptor());
  if (interceptor->deleter()->IsUndefined(isolate)) return Nothing<bool>();

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }

  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);
  Handle<Object> result;
  if (it->IsElement()) {
    result = args.CallIndexedDeleter(interceptor, it->index());
  } else {
    result = args.CallNamedDeleter(interceptor, it->name());
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (result.is_null()) return Nothing<bool>();

  DCHECK(result->IsBoolean());
  // Rebox CustomArguments::kReturnValueOffset before returning.
  return Just(result->IsTrue(isolate));
}

void JSReceiver::DeleteNormalizedProperty(Handle<JSReceiver> object,
                                          int entry) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();

  if (object->IsJSGlobalObject()) {
    // If we have a global object, invalidate the cell and swap in a new one.
    Handle<GlobalDictionary> dictionary(
        JSGlobalObject::cast(*object)->global_dictionary(), isolate);
    DCHECK_NE(GlobalDictionary::kNotFound, entry);

    auto cell = PropertyCell::InvalidateEntry(isolate, dictionary, entry);
    cell->set_value(ReadOnlyRoots(isolate).the_hole_value());
    cell->set_property_details(
        PropertyDetails::Empty(PropertyCellType::kUninitialized));
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);
    DCHECK_NE(NameDictionary::kNotFound, entry);

    dictionary = NameDictionary::DeleteEntry(isolate, dictionary, entry);
    object->SetProperties(*dictionary);
  }
  if (object->map()->is_prototype_map()) {
    // Invalidate prototype validity cell as this may invalidate transitioning
    // store IC handlers.
    JSObject::InvalidatePrototypeChains(object->map());
  }
}


Maybe<bool> JSReceiver::DeleteProperty(LookupIterator* it,
                                       LanguageMode language_mode) {
  it->UpdateProtector();

  Isolate* isolate = it->isolate();

  if (it->state() == LookupIterator::JSPROXY) {
    return JSProxy::DeletePropertyOrElement(it->GetHolder<JSProxy>(),
                                            it->GetName(), language_mode);
  }

  if (it->GetReceiver()->IsJSProxy()) {
    if (it->state() != LookupIterator::NOT_FOUND) {
      DCHECK_EQ(LookupIterator::DATA, it->state());
      DCHECK(it->name()->IsPrivate());
      it->Delete();
    }
    return Just(true);
  }
  Handle<JSObject> receiver = Handle<JSObject>::cast(it->GetReceiver());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
        RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
        return Just(false);
      case LookupIterator::INTERCEPTOR: {
        ShouldThrow should_throw =
            is_sloppy(language_mode) ? kDontThrow : kThrowOnError;
        Maybe<bool> result =
            JSObject::DeletePropertyWithInterceptor(it, should_throw);
        // An exception was thrown in the interceptor. Propagate.
        if (isolate->has_pending_exception()) return Nothing<bool>();
        // Delete with interceptor succeeded. Return result.
        // TODO(neis): In strict mode, we should probably throw if the
        // interceptor returns false.
        if (result.IsJust()) return result;
        break;
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(true);
      case LookupIterator::DATA:
      case LookupIterator::ACCESSOR: {
        if (!it->IsConfigurable()) {
          // Fail if the property is not configurable.
          if (is_strict(language_mode)) {
            isolate->Throw(*isolate->factory()->NewTypeError(
                MessageTemplate::kStrictDeleteProperty, it->GetName(),
                receiver));
            return Nothing<bool>();
          }
          return Just(false);
        }

        it->Delete();

        return Just(true);
      }
    }
  }

  return Just(true);
}


Maybe<bool> JSReceiver::DeleteElement(Handle<JSReceiver> object, uint32_t index,
                                      LanguageMode language_mode) {
  LookupIterator it(object->GetIsolate(), object, index, object,
                    LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}


Maybe<bool> JSReceiver::DeleteProperty(Handle<JSReceiver> object,
                                       Handle<Name> name,
                                       LanguageMode language_mode) {
  LookupIterator it(object, name, object, LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}


Maybe<bool> JSReceiver::DeletePropertyOrElement(Handle<JSReceiver> object,
                                                Handle<Name> name,
                                                LanguageMode language_mode) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      object->GetIsolate(), object, name, object, LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}

// ES6 19.1.2.4
// static
Object* JSReceiver::DefineProperty(Isolate* isolate, Handle<Object> object,
                                   Handle<Object> key,
                                   Handle<Object> attributes) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperty");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name));
  }
  // 2. Let key be ToPropertyKey(P).
  // 3. ReturnIfAbrupt(key).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key, ToPropertyKey(isolate, key));
  // 4. Let desc be ToPropertyDescriptor(Attributes).
  // 5. ReturnIfAbrupt(desc).
  PropertyDescriptor desc;
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, attributes, &desc)) {
    return ReadOnlyRoots(isolate).exception();
  }
  // 6. Let success be DefinePropertyOrThrow(O,key, desc).
  Maybe<bool> success = DefineOwnProperty(
      isolate, Handle<JSReceiver>::cast(object), key, &desc, kThrowOnError);
  // 7. ReturnIfAbrupt(success).
  MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
  CHECK(success.FromJust());
  // 8. Return O.
  return *object;
}


// ES6 19.1.2.3.1
// static
MaybeHandle<Object> JSReceiver::DefineProperties(Isolate* isolate,
                                                 Handle<Object> object,
                                                 Handle<Object> properties) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperties");
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name),
                    Object);
  }
  // 2. Let props be ToObject(Properties).
  // 3. ReturnIfAbrupt(props).
  Handle<JSReceiver> props;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, props,
                             Object::ToObject(isolate, properties), Object);

  // 4. Let keys be props.[[OwnPropertyKeys]]().
  // 5. ReturnIfAbrupt(keys).
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, keys, KeyAccumulator::GetKeys(props, KeyCollectionMode::kOwnOnly,
                                             ALL_PROPERTIES),
      Object);
  // 6. Let descriptors be an empty List.
  int capacity = keys->length();
  std::vector<PropertyDescriptor> descriptors(capacity);
  size_t descriptors_index = 0;
  // 7. Repeat for each element nextKey of keys in List order,
  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> next_key(keys->get(i), isolate);
    // 7a. Let propDesc be props.[[GetOwnProperty]](nextKey).
    // 7b. ReturnIfAbrupt(propDesc).
    bool success = false;
    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate, props, next_key, &success, LookupIterator::OWN);
    DCHECK(success);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    if (maybe.IsNothing()) return MaybeHandle<Object>();
    PropertyAttributes attrs = maybe.FromJust();
    // 7c. If propDesc is not undefined and propDesc.[[Enumerable]] is true:
    if (attrs == ABSENT) continue;
    if (attrs & DONT_ENUM) continue;
    // 7c i. Let descObj be Get(props, nextKey).
    // 7c ii. ReturnIfAbrupt(descObj).
    Handle<Object> desc_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, desc_obj, Object::GetProperty(&it),
                               Object);
    // 7c iii. Let desc be ToPropertyDescriptor(descObj).
    success = PropertyDescriptor::ToPropertyDescriptor(
        isolate, desc_obj, &descriptors[descriptors_index]);
    // 7c iv. ReturnIfAbrupt(desc).
    if (!success) return MaybeHandle<Object>();
    // 7c v. Append the pair (a two element List) consisting of nextKey and
    //       desc to the end of descriptors.
    descriptors[descriptors_index].set_name(next_key);
    descriptors_index++;
  }
  // 8. For each pair from descriptors in list order,
  for (size_t i = 0; i < descriptors_index; ++i) {
    PropertyDescriptor* desc = &descriptors[i];
    // 8a. Let P be the first element of pair.
    // 8b. Let desc be the second element of pair.
    // 8c. Let status be DefinePropertyOrThrow(O, P, desc).
    Maybe<bool> status =
        DefineOwnProperty(isolate, Handle<JSReceiver>::cast(object),
                          desc->name(), desc, kThrowOnError);
    // 8d. ReturnIfAbrupt(status).
    if (status.IsNothing()) return MaybeHandle<Object>();
    CHECK(status.FromJust());
  }
  // 9. Return o.
  return object;
}

// static
Maybe<bool> JSReceiver::DefineOwnProperty(Isolate* isolate,
                                          Handle<JSReceiver> object,
                                          Handle<Object> key,
                                          PropertyDescriptor* desc,
                                          ShouldThrow should_throw) {
  if (object->IsJSArray()) {
    return JSArray::DefineOwnProperty(isolate, Handle<JSArray>::cast(object),
                                      key, desc, should_throw);
  }
  if (object->IsJSProxy()) {
    return JSProxy::DefineOwnProperty(isolate, Handle<JSProxy>::cast(object),
                                      key, desc, should_throw);
  }
  if (object->IsJSTypedArray()) {
    return JSTypedArray::DefineOwnProperty(
        isolate, Handle<JSTypedArray>::cast(object), key, desc, should_throw);
  }

  // OrdinaryDefineOwnProperty, by virtue of calling
  // DefineOwnPropertyIgnoreAttributes, can handle arguments
  // (ES#sec-arguments-exotic-objects-defineownproperty-p-desc).
  return OrdinaryDefineOwnProperty(isolate, Handle<JSObject>::cast(object), key,
                                   desc, should_throw);
}


// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(Isolate* isolate,
                                                  Handle<JSObject> object,
                                                  Handle<Object> key,
                                                  PropertyDescriptor* desc,
                                                  ShouldThrow should_throw) {
  bool success = false;
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey...
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, key, &success, LookupIterator::OWN);
  DCHECK(success);  // ...so creating a LookupIterator can't fail.

  // Deal with access checks first.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(it.GetHolder<JSObject>());
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      return Just(true);
    }
    it.Next();
  }

  return OrdinaryDefineOwnProperty(&it, desc, should_throw);
}


// ES6 9.1.6.1
// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(LookupIterator* it,
                                                  PropertyDescriptor* desc,
                                                  ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  // 1. Let current be O.[[GetOwnProperty]](P).
  // 2. ReturnIfAbrupt(current).
  PropertyDescriptor current;
  MAYBE_RETURN(GetOwnPropertyDescriptor(it, &current), Nothing<bool>());

  it->Restart();
  // Handle interceptor
  for (; it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::INTERCEPTOR) {
      if (it->HolderIsReceiverOrHiddenPrototype()) {
        Maybe<bool> result = DefinePropertyWithInterceptorInternal(
            it, it->GetInterceptor(), should_throw, *desc);
        if (result.IsNothing() || result.FromJust()) {
          return result;
        }
      }
    }
  }

  // TODO(jkummerow/verwaest): It would be nice if we didn't have to reset
  // the iterator every time. Currently, the reasons why we need it are:
  // - handle interceptors correctly
  // - handle accessors correctly (which might change the holder's map)
  it->Restart();
  // 3. Let extensible be the value of the [[Extensible]] internal slot of O.
  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  bool extensible = JSObject::IsExtensible(object);

  return ValidateAndApplyPropertyDescriptor(
      isolate, it, extensible, desc, &current, should_throw, Handle<Name>());
}


// ES6 9.1.6.2
// static
Maybe<bool> JSReceiver::IsCompatiblePropertyDescriptor(
    Isolate* isolate, bool extensible, PropertyDescriptor* desc,
    PropertyDescriptor* current, Handle<Name> property_name,
    ShouldThrow should_throw) {
  // 1. Return ValidateAndApplyPropertyDescriptor(undefined, undefined,
  //    Extensible, Desc, Current).
  return ValidateAndApplyPropertyDescriptor(
      isolate, nullptr, extensible, desc, current, should_throw, property_name);
}


// ES6 9.1.6.3
// static
Maybe<bool> JSReceiver::ValidateAndApplyPropertyDescriptor(
    Isolate* isolate, LookupIterator* it, bool extensible,
    PropertyDescriptor* desc, PropertyDescriptor* current,
    ShouldThrow should_throw, Handle<Name> property_name) {
  // We either need a LookupIterator, or a property name.
  DCHECK((it == nullptr) != property_name.is_null());
  Handle<JSObject> object;
  if (it != nullptr) object = Handle<JSObject>::cast(it->GetReceiver());
  bool desc_is_data_descriptor = PropertyDescriptor::IsDataDescriptor(desc);
  bool desc_is_accessor_descriptor =
      PropertyDescriptor::IsAccessorDescriptor(desc);
  bool desc_is_generic_descriptor =
      PropertyDescriptor::IsGenericDescriptor(desc);
  // 1. (Assert)
  // 2. If current is undefined, then
  if (current->is_empty()) {
    // 2a. If extensible is false, return false.
    if (!extensible) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kDefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 2c. If IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then:
    // (This is equivalent to !IsAccessorDescriptor(desc).)
    DCHECK((desc_is_generic_descriptor || desc_is_data_descriptor) ==
           !desc_is_accessor_descriptor);
    if (!desc_is_accessor_descriptor) {
      // 2c i. If O is not undefined, create an own data property named P of
      // object O whose [[Value]], [[Writable]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != nullptr) {
        if (!desc->has_writable()) desc->set_writable(false);
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> value(
            desc->has_value()
                ? desc->value()
                : Handle<Object>::cast(isolate->factory()->undefined_value()));
        MaybeHandle<Object> result =
            JSObject::DefineOwnPropertyIgnoreAttributes(it, value,
                                                        desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    } else {
      // 2d. Else Desc must be an accessor Property Descriptor,
      DCHECK(desc_is_accessor_descriptor);
      // 2d i. If O is not undefined, create an own accessor property named P
      // of object O whose [[Get]], [[Set]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != nullptr) {
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> getter(
            desc->has_get()
                ? desc->get()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        Handle<Object> setter(
            desc->has_set()
                ? desc->set()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        MaybeHandle<Object> result =
            JSObject::DefineAccessor(it, getter, setter, desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    }
    // 2e. Return true.
    return Just(true);
  }
  // 3. Return true, if every field in Desc is absent.
  // 4. Return true, if every field in Desc also occurs in current and the
  // value of every field in Desc is the same value as the corresponding field
  // in current when compared using the SameValue algorithm.
  if ((!desc->has_enumerable() ||
       desc->enumerable() == current->enumerable()) &&
      (!desc->has_configurable() ||
       desc->configurable() == current->configurable()) &&
      (!desc->has_value() ||
       (current->has_value() && current->value()->SameValue(*desc->value()))) &&
      (!desc->has_writable() ||
       (current->has_writable() && current->writable() == desc->writable())) &&
      (!desc->has_get() ||
       (current->has_get() && current->get()->SameValue(*desc->get()))) &&
      (!desc->has_set() ||
       (current->has_set() && current->set()->SameValue(*desc->set())))) {
    return Just(true);
  }
  // 5. If the [[Configurable]] field of current is false, then
  if (!current->configurable()) {
    // 5a. Return false, if the [[Configurable]] field of Desc is true.
    if (desc->has_configurable() && desc->configurable()) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 5b. Return false, if the [[Enumerable]] field of Desc is present and the
    // [[Enumerable]] fields of current and Desc are the Boolean negation of
    // each other.
    if (desc->has_enumerable() && desc->enumerable() != current->enumerable()) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
  }

  bool current_is_data_descriptor =
      PropertyDescriptor::IsDataDescriptor(current);
  // 6. If IsGenericDescriptor(Desc) is true, no further validation is required.
  if (desc_is_generic_descriptor) {
    // Nothing to see here.

    // 7. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) have
    // different results, then:
  } else if (current_is_data_descriptor != desc_is_data_descriptor) {
    // 7a. Return false, if the [[Configurable]] field of current is false.
    if (!current->configurable()) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 7b. If IsDataDescriptor(current) is true, then:
    if (current_is_data_descriptor) {
      // 7b i. If O is not undefined, convert the property named P of object O
      // from a data property to an accessor property. Preserve the existing
      // values of the converted property's [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property's attributes to their
      // default values.
      // --> Folded into step 10.
    } else {
      // 7c i. If O is not undefined, convert the property named P of object O
      // from an accessor property to a data property. Preserve the existing
      // values of the converted property’s [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property’s attributes to their
      // default values.
      // --> Folded into step 10.
    }

    // 8. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both
    // true, then:
  } else if (current_is_data_descriptor && desc_is_data_descriptor) {
    // 8a. If the [[Configurable]] field of current is false, then:
    if (!current->configurable()) {
      // 8a i. Return false, if the [[Writable]] field of current is false and
      // the [[Writable]] field of Desc is true.
      if (!current->writable() && desc->has_writable() && desc->writable()) {
        RETURN_FAILURE(
            isolate, should_throw,
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
      // 8a ii. If the [[Writable]] field of current is false, then:
      if (!current->writable()) {
        // 8a ii 1. Return false, if the [[Value]] field of Desc is present and
        // SameValue(Desc.[[Value]], current.[[Value]]) is false.
        if (desc->has_value() && !desc->value()->SameValue(*current->value())) {
          RETURN_FAILURE(
              isolate, should_throw,
              NewTypeError(MessageTemplate::kRedefineDisallowed,
                           it != nullptr ? it->GetName() : property_name));
        }
      }
    }
  } else {
    // 9. Else IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc)
    // are both true,
    DCHECK(PropertyDescriptor::IsAccessorDescriptor(current) &&
           desc_is_accessor_descriptor);
    // 9a. If the [[Configurable]] field of current is false, then:
    if (!current->configurable()) {
      // 9a i. Return false, if the [[Set]] field of Desc is present and
      // SameValue(Desc.[[Set]], current.[[Set]]) is false.
      if (desc->has_set() && !desc->set()->SameValue(*current->set())) {
        RETURN_FAILURE(
            isolate, should_throw,
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
      // 9a ii. Return false, if the [[Get]] field of Desc is present and
      // SameValue(Desc.[[Get]], current.[[Get]]) is false.
      if (desc->has_get() && !desc->get()->SameValue(*current->get())) {
        RETURN_FAILURE(
            isolate, should_throw,
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
    }
  }

  // 10. If O is not undefined, then:
  if (it != nullptr) {
    // 10a. For each field of Desc that is present, set the corresponding
    // attribute of the property named P of object O to the value of the field.
    PropertyAttributes attrs = NONE;

    if (desc->has_enumerable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->enumerable() ? NONE : DONT_ENUM));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->enumerable() ? NONE : DONT_ENUM));
    }
    if (desc->has_configurable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->configurable() ? NONE : DONT_DELETE));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->configurable() ? NONE : DONT_DELETE));
    }
    if (desc_is_data_descriptor ||
        (desc_is_generic_descriptor && current_is_data_descriptor)) {
      if (desc->has_writable()) {
        attrs = static_cast<PropertyAttributes>(
            attrs | (desc->writable() ? NONE : READ_ONLY));
      } else {
        attrs = static_cast<PropertyAttributes>(
            attrs | (current->writable() ? NONE : READ_ONLY));
      }
      Handle<Object> value(
          desc->has_value() ? desc->value()
                            : current->has_value()
                                  ? current->value()
                                  : Handle<Object>::cast(
                                        isolate->factory()->undefined_value()));
      return JSObject::DefineOwnPropertyIgnoreAttributes(it, value, attrs,
                                                         should_throw);
    } else {
      DCHECK(desc_is_accessor_descriptor ||
             (desc_is_generic_descriptor &&
              PropertyDescriptor::IsAccessorDescriptor(current)));
      Handle<Object> getter(
          desc->has_get()
              ? desc->get()
              : current->has_get()
                    ? current->get()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      Handle<Object> setter(
          desc->has_set()
              ? desc->set()
              : current->has_set()
                    ? current->set()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      MaybeHandle<Object> result =
          JSObject::DefineAccessor(it, getter, setter, attrs);
      if (result.is_null()) return Nothing<bool>();
    }
  }

  // 11. Return true.
  return Just(true);
}

// static
Maybe<bool> JSReceiver::CreateDataProperty(Isolate* isolate,
                                           Handle<JSReceiver> object,
                                           Handle<Name> key,
                                           Handle<Object> value,
                                           ShouldThrow should_throw) {
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, object, key,
                                                        LookupIterator::OWN);
  return CreateDataProperty(&it, value, should_throw);
}

// static
Maybe<bool> JSReceiver::CreateDataProperty(LookupIterator* it,
                                           Handle<Object> value,
                                           ShouldThrow should_throw) {
  DCHECK(!it->check_prototype_chain());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  if (receiver->IsJSObject()) {
    return JSObject::CreateDataProperty(it, value, should_throw);  // Shortcut.
  }

  PropertyDescriptor new_desc;
  new_desc.set_value(value);
  new_desc.set_writable(true);
  new_desc.set_enumerable(true);
  new_desc.set_configurable(true);

  return JSReceiver::DefineOwnProperty(isolate, receiver, it->GetName(),
                                       &new_desc, should_throw);
}

Maybe<bool> JSObject::CreateDataProperty(LookupIterator* it,
                                         Handle<Object> value,
                                         ShouldThrow should_throw) {
  DCHECK(it->GetReceiver()->IsJSObject());
  MAYBE_RETURN(JSReceiver::GetPropertyAttributes(it), Nothing<bool>());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  if (it->IsFound()) {
    Maybe<PropertyAttributes> attributes = GetPropertyAttributes(it);
    MAYBE_RETURN(attributes, Nothing<bool>());
    if ((attributes.FromJust() & DONT_DELETE) != 0) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kRedefineDisallowed, it->GetName()));
    }
  } else {
    if (!JSObject::IsExtensible(Handle<JSObject>::cast(it->GetReceiver()))) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kDefineDisallowed, it->GetName()));
    }
  }

  RETURN_ON_EXCEPTION_VALUE(it->isolate(),
                            DefineOwnPropertyIgnoreAttributes(it, value, NONE),
                            Nothing<bool>());

  return Just(true);
}

// TODO(jkummerow): Consider unification with FastAsArrayLength() in
// accessors.cc.
bool PropertyKeyToArrayLength(Handle<Object> value, uint32_t* length) {
  DCHECK(value->IsNumber() || value->IsName());
  if (value->ToArrayLength(length)) return true;
  if (value->IsString()) return String::cast(*value)->AsArrayIndex(length);
  return false;
}

bool PropertyKeyToArrayIndex(Handle<Object> index_obj, uint32_t* output) {
  return PropertyKeyToArrayLength(index_obj, output) && *output != kMaxUInt32;
}


// ES6 9.4.2.1
// static
Maybe<bool> JSArray::DefineOwnProperty(Isolate* isolate, Handle<JSArray> o,
                                       Handle<Object> name,
                                       PropertyDescriptor* desc,
                                       ShouldThrow should_throw) {
  // 1. Assert: IsPropertyKey(P) is true. ("P" is |name|.)
  // 2. If P is "length", then:
  // TODO(jkummerow): Check if we need slow string comparison.
  if (*name == ReadOnlyRoots(isolate).length_string()) {
    // 2a. Return ArraySetLength(A, Desc).
    return ArraySetLength(isolate, o, desc, should_throw);
  }
  // 3. Else if P is an array index, then:
  uint32_t index = 0;
  if (PropertyKeyToArrayIndex(name, &index)) {
    // 3a. Let oldLenDesc be OrdinaryGetOwnProperty(A, "length").
    PropertyDescriptor old_len_desc;
    Maybe<bool> success = GetOwnPropertyDescriptor(
        isolate, o, isolate->factory()->length_string(), &old_len_desc);
    // 3b. (Assert)
    DCHECK(success.FromJust());
    USE(success);
    // 3c. Let oldLen be oldLenDesc.[[Value]].
    uint32_t old_len = 0;
    CHECK(old_len_desc.value()->ToArrayLength(&old_len));
    // 3d. Let index be ToUint32(P).
    // (Already done above.)
    // 3e. (Assert)
    // 3f. If index >= oldLen and oldLenDesc.[[Writable]] is false,
    //     return false.
    if (index >= old_len && old_len_desc.has_writable() &&
        !old_len_desc.writable()) {
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kDefineDisallowed, name));
    }
    // 3g. Let succeeded be OrdinaryDefineOwnProperty(A, P, Desc).
    Maybe<bool> succeeded =
        OrdinaryDefineOwnProperty(isolate, o, name, desc, should_throw);
    // 3h. Assert: succeeded is not an abrupt completion.
    //     In our case, if should_throw == kThrowOnError, it can be!
    // 3i. If succeeded is false, return false.
    if (succeeded.IsNothing() || !succeeded.FromJust()) return succeeded;
    // 3j. If index >= oldLen, then:
    if (index >= old_len) {
      // 3j i. Set oldLenDesc.[[Value]] to index + 1.
      old_len_desc.set_value(isolate->factory()->NewNumberFromUint(index + 1));
      // 3j ii. Let succeeded be
      //        OrdinaryDefineOwnProperty(A, "length", oldLenDesc).
      succeeded = OrdinaryDefineOwnProperty(isolate, o,
                                            isolate->factory()->length_string(),
                                            &old_len_desc, should_throw);
      // 3j iii. Assert: succeeded is true.
      DCHECK(succeeded.FromJust());
      USE(succeeded);
    }
    // 3k. Return true.
    return Just(true);
  }

  // 4. Return OrdinaryDefineOwnProperty(A, P, Desc).
  return OrdinaryDefineOwnProperty(isolate, o, name, desc, should_throw);
}


// Part of ES6 9.4.2.4 ArraySetLength.
// static
bool JSArray::AnythingToArrayLength(Isolate* isolate,
                                    Handle<Object> length_object,
                                    uint32_t* output) {
  // Fast path: check numbers and strings that can be converted directly
  // and unobservably.
  if (length_object->ToArrayLength(output)) return true;
  if (length_object->IsString() &&
      Handle<String>::cast(length_object)->AsArrayIndex(output)) {
    return true;
  }
  // Slow path: follow steps in ES6 9.4.2.4 "ArraySetLength".
  // 3. Let newLen be ToUint32(Desc.[[Value]]).
  Handle<Object> uint32_v;
  if (!Object::ToUint32(isolate, length_object).ToHandle(&uint32_v)) {
    // 4. ReturnIfAbrupt(newLen).
    return false;
  }
  // 5. Let numberLen be ToNumber(Desc.[[Value]]).
  Handle<Object> number_v;
  if (!Object::ToNumber(isolate, length_object).ToHandle(&number_v)) {
    // 6. ReturnIfAbrupt(newLen).
    return false;
  }
  // 7. If newLen != numberLen, throw a RangeError exception.
  if (uint32_v->Number() != number_v->Number()) {
    Handle<Object> exception =
        isolate->factory()->NewRangeError(MessageTemplate::kInvalidArrayLength);
    isolate->Throw(*exception);
    return false;
  }
  CHECK(uint32_v->ToArrayLength(output));
  return true;
}


// ES6 9.4.2.4
// static
Maybe<bool> JSArray::ArraySetLength(Isolate* isolate, Handle<JSArray> a,
                                    PropertyDescriptor* desc,
                                    ShouldThrow should_throw) {
  // 1. If the [[Value]] field of Desc is absent, then
  if (!desc->has_value()) {
    // 1a. Return OrdinaryDefineOwnProperty(A, "length", Desc).
    return OrdinaryDefineOwnProperty(
        isolate, a, isolate->factory()->length_string(), desc, should_throw);
  }
  // 2. Let newLenDesc be a copy of Desc.
  // (Actual copying is not necessary.)
  PropertyDescriptor* new_len_desc = desc;
  // 3. - 7. Convert Desc.[[Value]] to newLen.
  uint32_t new_len = 0;
  if (!AnythingToArrayLength(isolate, desc->value(), &new_len)) {
    DCHECK(isolate->has_pending_exception());
    return Nothing<bool>();
  }
  // 8. Set newLenDesc.[[Value]] to newLen.
  // (Done below, if needed.)
  // 9. Let oldLenDesc be OrdinaryGetOwnProperty(A, "length").
  PropertyDescriptor old_len_desc;
  Maybe<bool> success = GetOwnPropertyDescriptor(
      isolate, a, isolate->factory()->length_string(), &old_len_desc);
  // 10. (Assert)
  DCHECK(success.FromJust());
  USE(success);
  // 11. Let oldLen be oldLenDesc.[[Value]].
  uint32_t old_len = 0;
  CHECK(old_len_desc.value()->ToArrayLength(&old_len));
  // 12. If newLen >= oldLen, then
  if (new_len >= old_len) {
    // 8. Set newLenDesc.[[Value]] to newLen.
    // 12a. Return OrdinaryDefineOwnProperty(A, "length", newLenDesc).
    new_len_desc->set_value(isolate->factory()->NewNumberFromUint(new_len));
    return OrdinaryDefineOwnProperty(isolate, a,
                                     isolate->factory()->length_string(),
                                     new_len_desc, should_throw);
  }
  // 13. If oldLenDesc.[[Writable]] is false, return false.
  if (!old_len_desc.writable()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kRedefineDisallowed,
                                isolate->factory()->length_string()));
  }
  // 14. If newLenDesc.[[Writable]] is absent or has the value true,
  // let newWritable be true.
  bool new_writable = false;
  if (!new_len_desc->has_writable() || new_len_desc->writable()) {
    new_writable = true;
  } else {
    // 15. Else,
    // 15a. Need to defer setting the [[Writable]] attribute to false in case
    //      any elements cannot be deleted.
    // 15b. Let newWritable be false. (It's initialized as "false" anyway.)
    // 15c. Set newLenDesc.[[Writable]] to true.
    // (Not needed.)
  }
  // Most of steps 16 through 19 is implemented by JSArray::SetLength.
  JSArray::SetLength(a, new_len);
  // Steps 19d-ii, 20.
  if (!new_writable) {
    PropertyDescriptor readonly;
    readonly.set_writable(false);
    Maybe<bool> success = OrdinaryDefineOwnProperty(
        isolate, a, isolate->factory()->length_string(), &readonly,
        should_throw);
    DCHECK(success.FromJust());
    USE(success);
  }
  uint32_t actual_new_len = 0;
  CHECK(a->length()->ToArrayLength(&actual_new_len));
  // Steps 19d-v, 21. Return false if there were non-deletable elements.
  bool result = actual_new_len == new_len;
  if (!result) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kStrictDeleteProperty,
                     isolate->factory()->NewNumberFromUint(actual_new_len - 1),
                     a));
  }
  return Just(result);
}


// ES6 9.5.6
// static
Maybe<bool> JSProxy::DefineOwnProperty(Isolate* isolate, Handle<JSProxy> proxy,
                                       Handle<Object> key,
                                       PropertyDescriptor* desc,
                                       ShouldThrow should_throw) {
  STACK_CHECK(isolate, Nothing<bool>());
  if (key->IsSymbol() && Handle<Symbol>::cast(key)->IsPrivate()) {
    DCHECK(!Handle<Symbol>::cast(key)->IsPrivateField());
    return JSProxy::SetPrivateSymbol(isolate, proxy, Handle<Symbol>::cast(key),
                                     desc, should_throw);
  }
  Handle<String> trap_name = isolate->factory()->defineProperty_string();
  // 1. Assert: IsPropertyKey(P) is true.
  DCHECK(key->IsName() || key->IsNumber());
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 6. Let trap be ? GetMethod(handler, "defineProperty").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name),
      Nothing<bool>());
  // 7. If trap is undefined, then:
  if (trap->IsUndefined(isolate)) {
    // 7a. Return target.[[DefineOwnProperty]](P, Desc).
    return JSReceiver::DefineOwnProperty(isolate, target, key, desc,
                                         should_throw);
  }
  // 8. Let descObj be FromPropertyDescriptor(Desc).
  Handle<Object> desc_obj = desc->ToObject(isolate);
  // 9. Let booleanTrapResult be
  //    ToBoolean(? Call(trap, handler, «target, P, descObj»)).
  Handle<Name> property_name =
      key->IsName()
          ? Handle<Name>::cast(key)
          : Handle<Name>::cast(isolate->factory()->NumberToString(key));
  // Do not leak private property names.
  DCHECK(!property_name->IsPrivate());
  Handle<Object> trap_result_obj;
  Handle<Object> args[] = {target, property_name, desc_obj};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result_obj,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  // 10. If booleanTrapResult is false, return false.
  if (!trap_result_obj->BooleanValue(isolate)) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, property_name));
  }
  // 11. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> target_found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, key, &target_desc);
  MAYBE_RETURN(target_found, Nothing<bool>());
  // 12. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> maybe_extensible = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(maybe_extensible, Nothing<bool>());
  bool extensible_target = maybe_extensible.FromJust();
  // 13. If Desc has a [[Configurable]] field and if Desc.[[Configurable]]
  //     is false, then:
  // 13a. Let settingConfigFalse be true.
  // 14. Else let settingConfigFalse be false.
  bool setting_config_false = desc->has_configurable() && !desc->configurable();
  // 15. If targetDesc is undefined, then
  if (!target_found.FromJust()) {
    // 15a. If extensibleTarget is false, throw a TypeError exception.
    if (!extensible_target) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyNonExtensible, property_name));
      return Nothing<bool>();
    }
    // 15b. If settingConfigFalse is true, throw a TypeError exception.
    if (setting_config_false) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyNonConfigurable, property_name));
      return Nothing<bool>();
    }
  } else {
    // 16. Else targetDesc is not undefined,
    // 16a. If IsCompatiblePropertyDescriptor(extensibleTarget, Desc,
    //      targetDesc) is false, throw a TypeError exception.
    Maybe<bool> valid =
        IsCompatiblePropertyDescriptor(isolate, extensible_target, desc,
                                       &target_desc, property_name, kDontThrow);
    MAYBE_RETURN(valid, Nothing<bool>());
    if (!valid.FromJust()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyIncompatible, property_name));
      return Nothing<bool>();
    }
    // 16b. If settingConfigFalse is true and targetDesc.[[Configurable]] is
    //      true, throw a TypeError exception.
    if (setting_config_false && target_desc.configurable()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyNonConfigurable, property_name));
      return Nothing<bool>();
    }
  }
  // 17. Return true.
  return Just(true);
}

// static
Maybe<bool> JSProxy::SetPrivateSymbol(Isolate* isolate, Handle<JSProxy> proxy,
                                      Handle<Symbol> private_name,
                                      PropertyDescriptor* desc,
                                      ShouldThrow should_throw) {
  DCHECK(!private_name->IsPrivateField());
  // Despite the generic name, this can only add private data properties.
  if (!PropertyDescriptor::IsDataDescriptor(desc) ||
      desc->ToAttributes() != DONT_ENUM) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyPrivate));
  }
  DCHECK(proxy->map()->is_dictionary_map());
  Handle<Object> value =
      desc->has_value()
          ? desc->value()
          : Handle<Object>::cast(isolate->factory()->undefined_value());

  LookupIterator it(proxy, private_name, proxy);

  if (it.IsFound()) {
    DCHECK_EQ(LookupIterator::DATA, it.state());
    DCHECK_EQ(DONT_ENUM, it.property_attributes());
    it.WriteDataValue(value, false);
    return Just(true);
  }

  Handle<NameDictionary> dict(proxy->property_dictionary(), isolate);
  PropertyDetails details(kData, DONT_ENUM, PropertyCellType::kNoCell);
  Handle<NameDictionary> result =
      NameDictionary::Add(isolate, dict, private_name, value, details);
  if (!dict.is_identical_to(result)) proxy->SetProperties(*result);
  return Just(true);
}

// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(Isolate* isolate,
                                                 Handle<JSReceiver> object,
                                                 Handle<Object> key,
                                                 PropertyDescriptor* desc) {
  bool success = false;
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey...
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, key, &success, LookupIterator::OWN);
  DCHECK(success);  // ...so creating a LookupIterator can't fail.
  return GetOwnPropertyDescriptor(&it, desc);
}

namespace {

Maybe<bool> GetPropertyDescriptorWithInterceptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  if (it->state() == LookupIterator::ACCESS_CHECK) {
    if (it->HasAccess()) {
      it->Next();
    } else if (!JSObject::AllCanRead(it) ||
               it->state() != LookupIterator::INTERCEPTOR) {
      it->Restart();
      return Just(false);
    }
  }

  if (it->state() != LookupIterator::INTERCEPTOR) return Just(false);

  Isolate* isolate = it->isolate();
  Handle<InterceptorInfo> interceptor = it->GetInterceptor();
  if (interceptor->descriptor()->IsUndefined(isolate)) return Just(false);

  Handle<Object> result;
  Handle<JSObject> holder = it->GetHolder<JSObject>();

  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }

  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, kDontThrow);
  if (it->IsElement()) {
    result = args.CallIndexedDescriptor(interceptor, it->index());
  } else {
    result = args.CallNamedDescriptor(interceptor, it->name());
  }
  if (!result.is_null()) {
    // Request successfully intercepted, try to set the property
    // descriptor.
    Utils::ApiCheck(
        PropertyDescriptor::ToPropertyDescriptor(isolate, result, desc),
        it->IsElement() ? "v8::IndexedPropertyDescriptorCallback"
                        : "v8::NamedPropertyDescriptorCallback",
        "Invalid property descriptor.");

    return Just(true);
  }

  it->Next();
  return Just(false);
}
}  // namespace

// ES6 9.1.5.1
// Returns true on success, false if the property didn't exist, nothing if
// an exception was thrown.
// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  Isolate* isolate = it->isolate();
  // "Virtual" dispatch.
  if (it->IsFound() && it->GetHolder<JSReceiver>()->IsJSProxy()) {
    return JSProxy::GetOwnPropertyDescriptor(isolate, it->GetHolder<JSProxy>(),
                                             it->GetName(), desc);
  }

  Maybe<bool> intercepted = GetPropertyDescriptorWithInterceptor(it, desc);
  MAYBE_RETURN(intercepted, Nothing<bool>());
  if (intercepted.FromJust()) {
    return Just(true);
  }

  // Request was not intercepted, continue as normal.
  // 1. (Assert)
  // 2. If O does not have an own property with key P, return undefined.
  Maybe<PropertyAttributes> maybe = JSObject::GetPropertyAttributes(it);
  MAYBE_RETURN(maybe, Nothing<bool>());
  PropertyAttributes attrs = maybe.FromJust();
  if (attrs == ABSENT) return Just(false);
  DCHECK(!isolate->has_pending_exception());

  // 3. Let D be a newly created Property Descriptor with no fields.
  DCHECK(desc->is_empty());
  // 4. Let X be O's own property whose key is P.
  // 5. If X is a data property, then
  bool is_accessor_pair = it->state() == LookupIterator::ACCESSOR &&
                          it->GetAccessors()->IsAccessorPair();
  if (!is_accessor_pair) {
    // 5a. Set D.[[Value]] to the value of X's [[Value]] attribute.
    Handle<Object> value;
    if (!Object::GetProperty(it).ToHandle(&value)) {
      DCHECK(isolate->has_pending_exception());
      return Nothing<bool>();
    }
    desc->set_value(value);
    // 5b. Set D.[[Writable]] to the value of X's [[Writable]] attribute
    desc->set_writable((attrs & READ_ONLY) == 0);
  } else {
    // 6. Else X is an accessor property, so
    Handle<AccessorPair> accessors =
        Handle<AccessorPair>::cast(it->GetAccessors());
    // 6a. Set D.[[Get]] to the value of X's [[Get]] attribute.
    desc->set_get(
        AccessorPair::GetComponent(isolate, accessors, ACCESSOR_GETTER));
    // 6b. Set D.[[Set]] to the value of X's [[Set]] attribute.
    desc->set_set(
        AccessorPair::GetComponent(isolate, accessors, ACCESSOR_SETTER));
  }

  // 7. Set D.[[Enumerable]] to the value of X's [[Enumerable]] attribute.
  desc->set_enumerable((attrs & DONT_ENUM) == 0);
  // 8. Set D.[[Configurable]] to the value of X's [[Configurable]] attribute.
  desc->set_configurable((attrs & DONT_DELETE) == 0);
  // 9. Return D.
  DCHECK(PropertyDescriptor::IsAccessorDescriptor(desc) !=
         PropertyDescriptor::IsDataDescriptor(desc));
  return Just(true);
}


// ES6 9.5.5
// static
Maybe<bool> JSProxy::GetOwnPropertyDescriptor(Isolate* isolate,
                                              Handle<JSProxy> proxy,
                                              Handle<Name> name,
                                              PropertyDescriptor* desc) {
  DCHECK(!name->IsPrivate());
  STACK_CHECK(isolate, Nothing<bool>());

  Handle<String> trap_name =
      isolate->factory()->getOwnPropertyDescriptor_string();
  // 1. (Assert)
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 6. Let trap be ? GetMethod(handler, "getOwnPropertyDescriptor").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name),
      Nothing<bool>());
  // 7. If trap is undefined, then
  if (trap->IsUndefined(isolate)) {
    // 7a. Return target.[[GetOwnProperty]](P).
    return JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, desc);
  }
  // 8. Let trapResultObj be ? Call(trap, handler, «target, P»).
  Handle<Object> trap_result_obj;
  Handle<Object> args[] = {target, name};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result_obj,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  // 9. If Type(trapResultObj) is neither Object nor Undefined, throw a
  //    TypeError exception.
  if (!trap_result_obj->IsJSReceiver() &&
      !trap_result_obj->IsUndefined(isolate)) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyGetOwnPropertyDescriptorInvalid, name));
    return Nothing<bool>();
  }
  // 10. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN(found, Nothing<bool>());
  // 11. If trapResultObj is undefined, then
  if (trap_result_obj->IsUndefined(isolate)) {
    // 11a. If targetDesc is undefined, return undefined.
    if (!found.FromJust()) return Just(false);
    // 11b. If targetDesc.[[Configurable]] is false, throw a TypeError
    //      exception.
    if (!target_desc.configurable()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyGetOwnPropertyDescriptorUndefined, name));
      return Nothing<bool>();
    }
    // 11c. Let extensibleTarget be ? IsExtensible(target).
    Maybe<bool> extensible_target = JSReceiver::IsExtensible(target);
    MAYBE_RETURN(extensible_target, Nothing<bool>());
    // 11d. (Assert)
    // 11e. If extensibleTarget is false, throw a TypeError exception.
    if (!extensible_target.FromJust()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyGetOwnPropertyDescriptorNonExtensible, name));
      return Nothing<bool>();
    }
    // 11f. Return undefined.
    return Just(false);
  }
  // 12. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> extensible_target = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(extensible_target, Nothing<bool>());
  // 13. Let resultDesc be ? ToPropertyDescriptor(trapResultObj).
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, trap_result_obj,
                                                desc)) {
    DCHECK(isolate->has_pending_exception());
    return Nothing<bool>();
  }
  // 14. Call CompletePropertyDescriptor(resultDesc).
  PropertyDescriptor::CompletePropertyDescriptor(isolate, desc);
  // 15. Let valid be IsCompatiblePropertyDescriptor (extensibleTarget,
  //     resultDesc, targetDesc).
  Maybe<bool> valid =
      IsCompatiblePropertyDescriptor(isolate, extensible_target.FromJust(),
                                     desc, &target_desc, name, kDontThrow);
  MAYBE_RETURN(valid, Nothing<bool>());
  // 16. If valid is false, throw a TypeError exception.
  if (!valid.FromJust()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyGetOwnPropertyDescriptorIncompatible, name));
    return Nothing<bool>();
  }
  // 17. If resultDesc.[[Configurable]] is false, then
  if (!desc->configurable()) {
    // 17a. If targetDesc is undefined or targetDesc.[[Configurable]] is true:
    if (target_desc.is_empty() || target_desc.configurable()) {
      // 17a i. Throw a TypeError exception.
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyGetOwnPropertyDescriptorNonConfigurable,
          name));
      return Nothing<bool>();
    }
  }
  // 18. Return resultDesc.
  return Just(true);
}


Maybe<bool> JSReceiver::SetIntegrityLevel(Handle<JSReceiver> receiver,
                                          IntegrityLevel level,
                                          ShouldThrow should_throw) {
  DCHECK(level == SEALED || level == FROZEN);

  if (receiver->IsJSObject()) {
    Handle<JSObject> object = Handle<JSObject>::cast(receiver);

    if (!object->HasSloppyArgumentsElements() &&
        !object->IsJSModuleNamespace()) {  // Fast path.
      // Prevent memory leaks by not adding unnecessary transitions.
      Maybe<bool> test = JSObject::TestIntegrityLevel(object, level);
      MAYBE_RETURN(test, Nothing<bool>());
      if (test.FromJust()) return test;

      if (level == SEALED) {
        return JSObject::PreventExtensionsWithTransition<SEALED>(object,
                                                                 should_throw);
      } else {
        return JSObject::PreventExtensionsWithTransition<FROZEN>(object,
                                                                 should_throw);
      }
    }
  }

  Isolate* isolate = receiver->GetIsolate();

  MAYBE_RETURN(JSReceiver::PreventExtensions(receiver, should_throw),
               Nothing<bool>());

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(receiver), Nothing<bool>());

  PropertyDescriptor no_conf;
  no_conf.set_configurable(false);

  PropertyDescriptor no_conf_no_write;
  no_conf_no_write.set_configurable(false);
  no_conf_no_write.set_writable(false);

  if (level == SEALED) {
    for (int i = 0; i < keys->length(); ++i) {
      Handle<Object> key(keys->get(i), isolate);
      MAYBE_RETURN(
          DefineOwnProperty(isolate, receiver, key, &no_conf, kThrowOnError),
          Nothing<bool>());
    }
    return Just(true);
  }

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      PropertyDescriptor desc =
          PropertyDescriptor::IsAccessorDescriptor(&current_desc)
              ? no_conf
              : no_conf_no_write;
      MAYBE_RETURN(
          DefineOwnProperty(isolate, receiver, key, &desc, kThrowOnError),
          Nothing<bool>());
    }
  }
  return Just(true);
}

namespace {

template <typename Dictionary>
bool TestDictionaryPropertiesIntegrityLevel(Dictionary* dict,
                                            ReadOnlyRoots roots,
                                            PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);

  uint32_t capacity = dict->Capacity();
  for (uint32_t i = 0; i < capacity; i++) {
    Object* key;
    if (!dict->ToKey(roots, i, &key)) continue;
    if (key->FilterKey(ALL_PROPERTIES)) continue;
    PropertyDetails details = dict->DetailsAt(i);
    if (details.IsConfigurable()) return false;
    if (level == FROZEN && details.kind() == kData && !details.IsReadOnly()) {
      return false;
    }
  }
  return true;
}

bool TestFastPropertiesIntegrityLevel(Map* map, PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);
  DCHECK(!map->IsCustomElementsReceiverMap());
  DCHECK(!map->is_dictionary_map());

  DescriptorArray* descriptors = map->instance_descriptors();
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  for (int i = 0; i < number_of_own_descriptors; i++) {
    if (descriptors->GetKey(i)->IsPrivate()) continue;
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.IsConfigurable()) return false;
    if (level == FROZEN && details.kind() == kData && !details.IsReadOnly()) {
      return false;
    }
  }
  return true;
}

bool TestPropertiesIntegrityLevel(JSObject* object, PropertyAttributes level) {
  DCHECK(!object->map()->IsCustomElementsReceiverMap());

  if (object->HasFastProperties()) {
    return TestFastPropertiesIntegrityLevel(object->map(), level);
  }

  return TestDictionaryPropertiesIntegrityLevel(
      object->property_dictionary(), object->GetReadOnlyRoots(), level);
}

bool TestElementsIntegrityLevel(JSObject* object, PropertyAttributes level) {
  DCHECK(!object->HasSloppyArgumentsElements());

  ElementsKind kind = object->GetElementsKind();

  if (IsDictionaryElementsKind(kind)) {
    return TestDictionaryPropertiesIntegrityLevel(
        NumberDictionary::cast(object->elements()), object->GetReadOnlyRoots(),
        level);
  }

  ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
  // Only DICTIONARY_ELEMENTS and SLOW_SLOPPY_ARGUMENTS_ELEMENTS have
  // PropertyAttributes so just test if empty
  return accessor->NumberOfElements(object) == 0;
}

bool FastTestIntegrityLevel(JSObject* object, PropertyAttributes level) {
  DCHECK(!object->map()->IsCustomElementsReceiverMap());

  return !object->map()->is_extensible() &&
         TestElementsIntegrityLevel(object, level) &&
         TestPropertiesIntegrityLevel(object, level);
}

Maybe<bool> GenericTestIntegrityLevel(Handle<JSReceiver> receiver,
                                      PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);

  Maybe<bool> extensible = JSReceiver::IsExtensible(receiver);
  MAYBE_RETURN(extensible, Nothing<bool>());
  if (extensible.FromJust()) return Just(false);

  Isolate* isolate = receiver->GetIsolate();

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(receiver), Nothing<bool>());

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      if (current_desc.configurable()) return Just(false);
      if (level == FROZEN &&
          PropertyDescriptor::IsDataDescriptor(&current_desc) &&
          current_desc.writable()) {
        return Just(false);
      }
    }
  }
  return Just(true);
}

}  // namespace

Maybe<bool> JSReceiver::TestIntegrityLevel(Handle<JSReceiver> receiver,
                                           IntegrityLevel level) {
  if (!receiver->map()->IsCustomElementsReceiverMap()) {
    return JSObject::TestIntegrityLevel(Handle<JSObject>::cast(receiver),
                                        level);
  }
  return GenericTestIntegrityLevel(receiver, level);
}

Maybe<bool> JSObject::TestIntegrityLevel(Handle<JSObject> object,
                                         IntegrityLevel level) {
  if (!object->map()->IsCustomElementsReceiverMap() &&
      !object->HasSloppyArgumentsElements()) {
    return Just(FastTestIntegrityLevel(*object, level));
  }
  return GenericTestIntegrityLevel(Handle<JSReceiver>::cast(object), level);
}

Maybe<bool> JSReceiver::PreventExtensions(Handle<JSReceiver> object,
                                          ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::PreventExtensions(Handle<JSProxy>::cast(object),
                                      should_throw);
  }
  DCHECK(object->IsJSObject());
  return JSObject::PreventExtensions(Handle<JSObject>::cast(object),
                                     should_throw);
}


Maybe<bool> JSProxy::PreventExtensions(Handle<JSProxy> proxy,
                                       ShouldThrow should_throw) {
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(isolate, Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->preventExtensions_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined(isolate)) {
    return JSReceiver::PreventExtensions(target, should_throw);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  if (!trap_result->BooleanValue(isolate)) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kProxyTrapReturnedFalsish, trap_name));
  }

  // Enforce the invariant.
  Maybe<bool> target_result = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(target_result, Nothing<bool>());
  if (target_result.FromJust()) {
    isolate->Throw(*factory->NewTypeError(
        MessageTemplate::kProxyPreventExtensionsExtensible));
    return Nothing<bool>();
  }
  return Just(true);
}


Maybe<bool> JSObject::PreventExtensions(Handle<JSObject> object,
                                        ShouldThrow should_throw) {
  Isolate* isolate = object->GetIsolate();

  if (!object->HasSloppyArgumentsElements()) {
    return PreventExtensionsWithTransition<NONE>(object, should_throw);
  }

  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (!object->map()->is_extensible()) return Just(true);

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensions(PrototypeIterator::GetCurrent<JSObject>(iter),
                             should_throw);
  }

  if (object->map()->has_named_interceptor() ||
      object->map()->has_indexed_interceptor()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kCannotPreventExt));
  }

  if (!object->HasFixedTypedArrayElements()) {
    // If there are fast elements we normalize.
    Handle<NumberDictionary> dictionary = NormalizeElements(object);
    DCHECK(object->HasDictionaryElements() ||
           object->HasSlowArgumentsElements());

    // Make sure that we never go back to fast case.
    object->RequireSlowElements(*dictionary);
  }

  // Do a map transition, other objects with this map may still
  // be extensible.
  // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
  Handle<Map> new_map =
      Map::Copy(isolate, handle(object->map(), isolate), "PreventExtensions");

  new_map->set_is_extensible(false);
  JSObject::MigrateToMap(object, new_map);
  DCHECK(!object->map()->is_extensible());

  return Just(true);
}


Maybe<bool> JSReceiver::IsExtensible(Handle<JSReceiver> object) {
  if (object->IsJSProxy()) {
    return JSProxy::IsExtensible(Handle<JSProxy>::cast(object));
  }
  return Just(JSObject::IsExtensible(Handle<JSObject>::cast(object)));
}


Maybe<bool> JSProxy::IsExtensible(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(isolate, Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->isExtensible_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined(isolate)) {
    return JSReceiver::IsExtensible(target);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());

  // Enforce the invariant.
  Maybe<bool> target_result = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(target_result, Nothing<bool>());
  if (target_result.FromJust() != trap_result->BooleanValue(isolate)) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyIsExtensibleInconsistent,
                               factory->ToBoolean(target_result.FromJust())));
    return Nothing<bool>();
  }
  return target_result;
}


bool JSObject::IsExtensible(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    return true;
  }
  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, *object);
    if (iter.IsAtEnd()) return false;
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    return iter.GetCurrent<JSObject>()->map()->is_extensible();
  }
  return object->map()->is_extensible();
}

namespace {

template <typename Dictionary>
void ApplyAttributesToDictionary(Isolate* isolate, ReadOnlyRoots roots,
                                 Handle<Dictionary> dictionary,
                                 const PropertyAttributes attributes) {
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k;
    if (!dictionary->ToKey(roots, i, &k)) continue;
    if (k->FilterKey(ALL_PROPERTIES)) continue;
    PropertyDetails details = dictionary->DetailsAt(i);
    int attrs = attributes;
    // READ_ONLY is an invalid attribute for JS setters/getters.
    if ((attributes & READ_ONLY) && details.kind() == kAccessor) {
      Object* v = dictionary->ValueAt(i);
      if (v->IsAccessorPair()) attrs &= ~READ_ONLY;
    }
    details = details.CopyAddAttributes(static_cast<PropertyAttributes>(attrs));
    dictionary->DetailsAtPut(isolate, i, details);
  }
}

}  // namespace

template <PropertyAttributes attrs>
Maybe<bool> JSObject::PreventExtensionsWithTransition(
    Handle<JSObject> object, ShouldThrow should_throw) {
  STATIC_ASSERT(attrs == NONE || attrs == SEALED || attrs == FROZEN);

  // Sealing/freezing sloppy arguments or namespace objects should be handled
  // elsewhere.
  DCHECK(!object->HasSloppyArgumentsElements());
  DCHECK_IMPLIES(object->IsJSModuleNamespace(), attrs == NONE);

  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (attrs == NONE && !object->map()->is_extensible()) return Just(true);

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensionsWithTransition<attrs>(
        PrototypeIterator::GetCurrent<JSObject>(iter), should_throw);
  }

  if (object->map()->has_named_interceptor() ||
      object->map()->has_indexed_interceptor()) {
    MessageTemplate::Template message = MessageTemplate::kNone;
    switch (attrs) {
      case NONE:
        message = MessageTemplate::kCannotPreventExt;
        break;

      case SEALED:
        message = MessageTemplate::kCannotSeal;
        break;

      case FROZEN:
        message = MessageTemplate::kCannotFreeze;
        break;
    }
    RETURN_FAILURE(isolate, should_throw, NewTypeError(message));
  }

  Handle<NumberDictionary> new_element_dictionary;
  if (!object->HasFixedTypedArrayElements() &&
      !object->HasDictionaryElements() &&
      !object->HasSlowStringWrapperElements()) {
    int length = object->IsJSArray()
                     ? Smi::ToInt(Handle<JSArray>::cast(object)->length())
                     : object->elements()->length();
    new_element_dictionary =
        length == 0 ? isolate->factory()->empty_slow_element_dictionary()
                    : object->GetElementsAccessor()->Normalize(object);
  }

  Handle<Symbol> transition_marker;
  if (attrs == NONE) {
    transition_marker = isolate->factory()->nonextensible_symbol();
  } else if (attrs == SEALED) {
    transition_marker = isolate->factory()->sealed_symbol();
  } else {
    DCHECK(attrs == FROZEN);
    transition_marker = isolate->factory()->frozen_symbol();
  }

  Handle<Map> old_map(object->map(), isolate);
  TransitionsAccessor transitions(isolate, old_map);
  Map* transition = transitions.SearchSpecial(*transition_marker);
  if (transition != nullptr) {
    Handle<Map> transition_map(transition, isolate);
    DCHECK(transition_map->has_dictionary_elements() ||
           transition_map->has_fixed_typed_array_elements() ||
           transition_map->elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS);
    DCHECK(!transition_map->is_extensible());
    JSObject::MigrateToMap(object, transition_map);
  } else if (transitions.CanHaveMoreTransitions()) {
    // Create a new descriptor array with the appropriate property attributes
    Handle<Map> new_map = Map::CopyForPreventExtensions(
        isolate, old_map, attrs, transition_marker, "CopyForPreventExtensions");
    JSObject::MigrateToMap(object, new_map);
  } else {
    DCHECK(old_map->is_dictionary_map() || !old_map->is_prototype_map());
    // Slow path: need to normalize properties for safety
    NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0,
                        "SlowPreventExtensions");

    // Create a new map, since other objects with this map may be extensible.
    // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
    Handle<Map> new_map = Map::Copy(isolate, handle(object->map(), isolate),
                                    "SlowCopyForPreventExtensions");
    new_map->set_is_extensible(false);
    if (!new_element_dictionary.is_null()) {
      ElementsKind new_kind =
          IsStringWrapperElementsKind(old_map->elements_kind())
              ? SLOW_STRING_WRAPPER_ELEMENTS
              : DICTIONARY_ELEMENTS;
      new_map->set_elements_kind(new_kind);
    }
    JSObject::MigrateToMap(object, new_map);

    if (attrs != NONE) {
      ReadOnlyRoots roots(isolate);
      if (object->IsJSGlobalObject()) {
        Handle<GlobalDictionary> dictionary(
            JSGlobalObject::cast(*object)->global_dictionary(), isolate);
        ApplyAttributesToDictionary(isolate, roots, dictionary, attrs);
      } else {
        Handle<NameDictionary> dictionary(object->property_dictionary(),
                                          isolate);
        ApplyAttributesToDictionary(isolate, roots, dictionary, attrs);
      }
    }
  }

  // Both seal and preventExtensions always go through without modifications to
  // typed array elements. Freeze works only if there are no actual elements.
  if (object->HasFixedTypedArrayElements()) {
    if (attrs == FROZEN &&
        JSArrayBufferView::cast(*object)->byte_length() > 0) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kCannotFreezeArrayBufferView));
      return Nothing<bool>();
    }
    return Just(true);
  }

  DCHECK(object->map()->has_dictionary_elements() ||
         object->map()->elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS);
  if (!new_element_dictionary.is_null()) {
    object->set_elements(*new_element_dictionary);
  }

  if (object->elements() !=
      ReadOnlyRoots(isolate).empty_slow_element_dictionary()) {
    Handle<NumberDictionary> dictionary(object->element_dictionary(), isolate);
    // Make sure we never go back to the fast case
    object->RequireSlowElements(*dictionary);
    if (attrs != NONE) {
      ApplyAttributesToDictionary(isolate, ReadOnlyRoots(isolate), dictionary,
                                  attrs);
    }
  }

  return Just(true);
}


Handle<Object> JSObject::FastPropertyAt(Handle<JSObject> object,
                                        Representation representation,
                                        FieldIndex index) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsUnboxedDoubleField(index)) {
    double value = object->RawFastDoublePropertyAt(index);
    return isolate->factory()->NewHeapNumber(value);
  }
  Handle<Object> raw_value(object->RawFastPropertyAt(index), isolate);
  return Object::WrapForRead(isolate, raw_value, representation);
}

// static
MaybeHandle<Object> JSReceiver::ToPrimitive(Handle<JSReceiver> receiver,
                                            ToPrimitiveHint hint) {
  Isolate* const isolate = receiver->GetIsolate();
  Handle<Object> exotic_to_prim;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, exotic_to_prim,
      GetMethod(receiver, isolate->factory()->to_primitive_symbol()), Object);
  if (!exotic_to_prim->IsUndefined(isolate)) {
    Handle<Object> hint_string =
        isolate->factory()->ToPrimitiveHintString(hint);
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, exotic_to_prim, receiver, 1, &hint_string),
        Object);
    if (result->IsPrimitive()) return result;
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                    Object);
  }
  return OrdinaryToPrimitive(receiver, (hint == ToPrimitiveHint::kString)
                                           ? OrdinaryToPrimitiveHint::kString
                                           : OrdinaryToPrimitiveHint::kNumber);
}


// static
MaybeHandle<Object> JSReceiver::OrdinaryToPrimitive(
    Handle<JSReceiver> receiver, OrdinaryToPrimitiveHint hint) {
  Isolate* const isolate = receiver->GetIsolate();
  Handle<String> method_names[2];
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      method_names[0] = isolate->factory()->valueOf_string();
      method_names[1] = isolate->factory()->toString_string();
      break;
    case OrdinaryToPrimitiveHint::kString:
      method_names[0] = isolate->factory()->toString_string();
      method_names[1] = isolate->factory()->valueOf_string();
      break;
  }
  for (Handle<String> name : method_names) {
    Handle<Object> method;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, method,
                               JSReceiver::GetProperty(isolate, receiver, name),
                               Object);
    if (method->IsCallable()) {
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          Execution::Call(isolate, method, receiver, 0, nullptr), Object);
      if (result->IsPrimitive()) return result;
    }
  }
  THROW_NEW_ERROR(isolate,
                  NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                  Object);
}


// TODO(cbruni/jkummerow): Consider moving this into elements.cc.
bool JSObject::HasEnumerableElements() {
  // TODO(cbruni): cleanup
  JSObject* object = this;
  switch (object->GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS: {
      int length = object->IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object)->length())
                       : object->elements()->length();
      return length > 0;
    }
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_ELEMENTS: {
      FixedArray* elements = FixedArray::cast(object->elements());
      int length = object->IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object)->length())
                       : elements->length();
      Isolate* isolate = GetIsolate();
      for (int i = 0; i < length; i++) {
        if (!elements->is_the_hole(isolate, i)) return true;
      }
      return false;
    }
    case HOLEY_DOUBLE_ELEMENTS: {
      int length = object->IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object)->length())
                       : object->elements()->length();
      // Zero-length arrays would use the empty FixedArray...
      if (length == 0) return false;
      // ...so only cast to FixedDoubleArray otherwise.
      FixedDoubleArray* elements = FixedDoubleArray::cast(object->elements());
      for (int i = 0; i < length; i++) {
        if (!elements->is_the_hole(i)) return true;
      }
      return false;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        int length = object->elements()->length();
        return length > 0;
      }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* elements = NumberDictionary::cast(object->elements());
      return elements->NumberOfEnumerableProperties() > 0;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      // We're approximating non-empty arguments objects here.
      return true;
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      if (String::cast(JSValue::cast(object)->value())->length() > 0) {
        return true;
      }
      return object->elements()->length() > 0;
    case NO_ELEMENTS:
      return false;
  }
  UNREACHABLE();
}

int Map::NumberOfEnumerableProperties() const {
  int result = 0;
  DescriptorArray* descs = instance_descriptors();
  int limit = NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    if ((descs->GetDetails(i).attributes() & ONLY_ENUMERABLE) == 0 &&
        !descs->GetKey(i)->FilterKey(ENUMERABLE_STRINGS)) {
      result++;
    }
  }
  return result;
}

int Map::NextFreePropertyIndex() const {
  int free_index = 0;
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < number_of_own_descriptors; i++) {
    PropertyDetails details = descs->GetDetails(i);
    if (details.location() == kField) {
      int candidate = details.field_index() + details.field_width_in_words();
      if (candidate > free_index) free_index = candidate;
    }
  }
  return free_index;
}

bool Map::OnlyHasSimpleProperties() const {
  // Wrapped string elements aren't explicitly stored in the elements backing
  // store, but are loaded indirectly from the underlying string.
  return !IsStringWrapperElementsKind(elements_kind()) &&
         !IsSpecialReceiverMap() && !has_hidden_prototype() &&
         !is_dictionary_map();
}

V8_WARN_UNUSED_RESULT Maybe<bool> FastGetOwnValuesOrEntries(
    Isolate* isolate, Handle<JSReceiver> receiver, bool get_entries,
    Handle<FixedArray>* result) {
  Handle<Map> map(JSReceiver::cast(*receiver)->map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> object(JSObject::cast(*receiver), isolate);

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  int number_of_own_elements =
      object->GetElementsAccessor()->GetCapacity(*object, object->elements());
  Handle<FixedArray> values_or_entries = isolate->factory()->NewFixedArray(
      number_of_own_descriptors + number_of_own_elements);
  int count = 0;

  if (object->elements() != ReadOnlyRoots(isolate).empty_fixed_array()) {
    MAYBE_RETURN(object->GetElementsAccessor()->CollectValuesOrEntries(
                     isolate, object, values_or_entries, get_entries, &count,
                     ENUMERABLE_STRINGS),
                 Nothing<bool>());
  }

  bool stable = object->map() == *map;

  for (int index = 0; index < number_of_own_descriptors; index++) {
    Handle<Name> next_key(descriptors->GetKey(index), isolate);
    if (!next_key->IsString()) continue;
    Handle<Object> prop_value;

    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      PropertyDetails details = descriptors->GetDetails(index);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetStrongValue(index), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex field_index = FieldIndex::ForDescriptor(*map, index);
          prop_value =
              JSObject::FastPropertyAt(object, representation, field_index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value,
            JSReceiver::GetProperty(isolate, object, next_key),
            Nothing<bool>());
        stable = object->map() == *map;
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(isolate, object, next_key,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }

    if (get_entries) {
      prop_value = MakeEntryPair(isolate, next_key, prop_value);
    }

    values_or_entries->set(count, *prop_value);
    count++;
  }

  DCHECK_LE(count, values_or_entries->length());
  *result = FixedArray::ShrinkOrEmpty(isolate, values_or_entries, count);
  return Just(true);
}

MaybeHandle<FixedArray> GetOwnValuesOrEntries(Isolate* isolate,
                                              Handle<JSReceiver> object,
                                              PropertyFilter filter,
                                              bool try_fast_path,
                                              bool get_entries) {
  Handle<FixedArray> values_or_entries;
  if (try_fast_path && filter == ENUMERABLE_STRINGS) {
    Maybe<bool> fast_values_or_entries = FastGetOwnValuesOrEntries(
        isolate, object, get_entries, &values_or_entries);
    if (fast_values_or_entries.IsNothing()) return MaybeHandle<FixedArray>();
    if (fast_values_or_entries.FromJust()) return values_or_entries;
  }

  PropertyFilter key_filter =
      static_cast<PropertyFilter>(filter & ~ONLY_ENUMERABLE);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly, key_filter,
                              GetKeysConversion::kConvertToString),
      MaybeHandle<FixedArray>());

  values_or_entries = isolate->factory()->NewFixedArray(keys->length());
  int length = 0;

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key = Handle<Name>::cast(handle(keys->get(i), isolate));

    if (filter & ONLY_ENUMERABLE) {
      PropertyDescriptor descriptor;
      Maybe<bool> did_get_descriptor = JSReceiver::GetOwnPropertyDescriptor(
          isolate, object, key, &descriptor);
      MAYBE_RETURN(did_get_descriptor, MaybeHandle<FixedArray>());
      if (!did_get_descriptor.FromJust() || !descriptor.enumerable()) continue;
    }

    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, JSReceiver::GetPropertyOrElement(isolate, object, key),
        MaybeHandle<FixedArray>());

    if (get_entries) {
      Handle<FixedArray> entry_storage =
          isolate->factory()->NewUninitializedFixedArray(2);
      entry_storage->set(0, *key);
      entry_storage->set(1, *value);
      value = isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                         PACKED_ELEMENTS, 2);
    }

    values_or_entries->set(length, *value);
    length++;
  }
  DCHECK_LE(length, values_or_entries->length());
  return FixedArray::ShrinkOrEmpty(isolate, values_or_entries, length);
}

MaybeHandle<FixedArray> JSReceiver::GetOwnValues(Handle<JSReceiver> object,
                                                 PropertyFilter filter,
                                                 bool try_fast_path) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter,
                               try_fast_path, false);
}

MaybeHandle<FixedArray> JSReceiver::GetOwnEntries(Handle<JSReceiver> object,
                                                  PropertyFilter filter,
                                                  bool try_fast_path) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter,
                               try_fast_path, true);
}

Handle<FixedArray> JSReceiver::GetOwnElementIndices(Isolate* isolate,
                                                    Handle<JSReceiver> receiver,
                                                    Handle<JSObject> object) {
  KeyAccumulator accumulator(isolate, KeyCollectionMode::kOwnOnly,
                             ALL_PROPERTIES);
  accumulator.CollectOwnElementIndices(receiver, object);
  Handle<FixedArray> keys =
      accumulator.GetKeys(GetKeysConversion::kKeepNumbers);
  DCHECK(keys->ContainsSortedNumbers());
  return keys;
}

bool Map::DictionaryElementsInPrototypeChainOnly(Isolate* isolate) {
  if (IsDictionaryElementsKind(elements_kind())) {
    return false;
  }

  for (PrototypeIterator iter(isolate, this); !iter.IsAtEnd(); iter.Advance()) {
    // Be conservative, don't walk into proxies.
    if (iter.GetCurrent()->IsJSProxy()) return true;
    // String wrappers have non-configurable, non-writable elements.
    if (iter.GetCurrent()->IsStringWrapper()) return true;
    JSObject* current = iter.GetCurrent<JSObject>();

    if (current->HasDictionaryElements() &&
        current->element_dictionary()->requires_slow_elements()) {
      return true;
    }

    if (current->HasSlowArgumentsElements()) {
      FixedArray* parameter_map = FixedArray::cast(current->elements());
      Object* arguments = parameter_map->get(1);
      if (NumberDictionary::cast(arguments)->requires_slow_elements()) {
        return true;
      }
    }
  }

  return false;
}


MaybeHandle<Object> JSObject::DefineAccessor(Handle<JSObject> object,
                                             Handle<Name> name,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return DefineAccessor(&it, getter, setter, attributes);
}


MaybeHandle<Object> JSObject::DefineAccessor(LookupIterator* it,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = it->isolate();

  it->UpdateProtector();

  if (it->state() == LookupIterator::ACCESS_CHECK) {
    if (!it->HasAccess()) {
      isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return isolate->factory()->undefined_value();
    }
    it->Next();
  }

  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  // Ignore accessors on typed arrays.
  if (it->IsElement() && object->HasFixedTypedArrayElements()) {
    return it->factory()->undefined_value();
  }

  DCHECK(getter->IsCallable() || getter->IsUndefined(isolate) ||
         getter->IsNull(isolate) || getter->IsFunctionTemplateInfo());
  DCHECK(setter->IsCallable() || setter->IsUndefined(isolate) ||
         setter->IsNull(isolate) || setter->IsFunctionTemplateInfo());
  it->TransitionToAccessorProperty(getter, setter, attributes);

  return isolate->factory()->undefined_value();
}

MaybeHandle<Object> JSObject::SetAccessor(Handle<JSObject> object,
                                          Handle<Name> name,
                                          Handle<AccessorInfo> info,
                                          PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);

  // Duplicate ACCESS_CHECK outside of GetPropertyAttributes for the case that
  // the FailedAccessCheckCallbackFunction doesn't throw an exception.
  //
  // TODO(verwaest): Force throw an exception if the callback doesn't, so we can
  // remove reliance on default return values.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return it.factory()->undefined_value();
    }
    it.Next();
  }

  // Ignore accessors on typed arrays.
  if (it.IsElement() && object->HasFixedTypedArrayElements()) {
    return it.factory()->undefined_value();
  }

  CHECK(GetPropertyAttributes(&it).IsJust());

  // ES5 forbids turning a property into an accessor if it's not
  // configurable. See 8.6.1 (Table 5).
  if (it.IsFound() && !it.IsConfigurable()) {
    return it.factory()->undefined_value();
  }

  it.TransitionToAccessorPair(info, attributes);

  return object;
}

Object* JSObject::SlowReverseLookup(Object* value) {
  if (HasFastProperties()) {
    int number_of_own_descriptors = map()->NumberOfOwnDescriptors();
    DescriptorArray* descs = map()->instance_descriptors();
    bool value_is_number = value->IsNumber();
    for (int i = 0; i < number_of_own_descriptors; i++) {
      PropertyDetails details = descs->GetDetails(i);
      if (details.location() == kField) {
        DCHECK_EQ(kData, details.kind());
        FieldIndex field_index = FieldIndex::ForDescriptor(map(), i);
        if (IsUnboxedDoubleField(field_index)) {
          if (value_is_number) {
            double property = RawFastDoublePropertyAt(field_index);
            if (property == value->Number()) {
              return descs->GetKey(i);
            }
          }
        } else {
          Object* property = RawFastPropertyAt(field_index);
          if (field_index.is_double()) {
            DCHECK(property->IsMutableHeapNumber());
            if (value_is_number && property->Number() == value->Number()) {
              return descs->GetKey(i);
            }
          } else if (property == value) {
            return descs->GetKey(i);
          }
        }
      } else {
        DCHECK_EQ(kDescriptor, details.location());
        if (details.kind() == kData) {
          if (descs->GetStrongValue(i) == value) {
            return descs->GetKey(i);
          }
        }
      }
    }
    return GetReadOnlyRoots().undefined_value();
  } else if (IsJSGlobalObject()) {
    return JSGlobalObject::cast(this)->global_dictionary()->SlowReverseLookup(
        value);
  } else {
    return property_dictionary()->SlowReverseLookup(value);
  }
}

Handle<Map> Map::RawCopy(Isolate* isolate, Handle<Map> map, int instance_size,
                         int inobject_properties) {
  Handle<Map> result = isolate->factory()->NewMap(
      map->instance_type(), instance_size, TERMINAL_FAST_ELEMENTS_KIND,
      inobject_properties);
  Handle<Object> prototype(map->prototype(), isolate);
  Map::SetPrototype(isolate, result, prototype);
  result->set_constructor_or_backpointer(map->GetConstructor());
  result->set_bit_field(map->bit_field());
  result->set_bit_field2(map->bit_field2());
  int new_bit_field3 = map->bit_field3();
  new_bit_field3 = OwnsDescriptorsBit::update(new_bit_field3, true);
  new_bit_field3 = NumberOfOwnDescriptorsBits::update(new_bit_field3, 0);
  new_bit_field3 = EnumLengthBits::update(new_bit_field3,
                                          kInvalidEnumCacheSentinel);
  new_bit_field3 = IsDeprecatedBit::update(new_bit_field3, false);
  if (!map->is_dictionary_map()) {
    new_bit_field3 = IsUnstableBit::update(new_bit_field3, false);
  }
  result->set_bit_field3(new_bit_field3);
  return result;
}

Handle<Map> Map::Normalize(Isolate* isolate, Handle<Map> fast_map,
                           PropertyNormalizationMode mode, const char* reason) {
  DCHECK(!fast_map->is_dictionary_map());

  Handle<Object> maybe_cache(isolate->native_context()->normalized_map_cache(),
                             isolate);
  bool use_cache =
      !fast_map->is_prototype_map() && !maybe_cache->IsUndefined(isolate);
  Handle<NormalizedMapCache> cache;
  if (use_cache) cache = Handle<NormalizedMapCache>::cast(maybe_cache);

  Handle<Map> new_map;
  if (use_cache && cache->Get(fast_map, mode).ToHandle(&new_map)) {
#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) new_map->DictionaryMapVerify(isolate);
#endif
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      // The cached map should match newly created normalized map bit-by-bit,
      // except for the code cache, which can contain some ICs which can be
      // applied to the shared map, dependent code and weak cell cache.
      Handle<Map> fresh = Map::CopyNormalized(isolate, fast_map, mode);

      if (new_map->is_prototype_map()) {
        // For prototype maps, the PrototypeInfo is not copied.
        DCHECK_EQ(0, memcmp(reinterpret_cast<void*>(fresh->address()),
                            reinterpret_cast<void*>(new_map->address()),
                            kTransitionsOrPrototypeInfoOffset));
        DCHECK_EQ(fresh->raw_transitions(),
                  MaybeObject::FromObject(Smi::kZero));
        STATIC_ASSERT(kDescriptorsOffset ==
                      kTransitionsOrPrototypeInfoOffset + kPointerSize);
        DCHECK_EQ(0, memcmp(HeapObject::RawField(*fresh, kDescriptorsOffset),
                            HeapObject::RawField(*new_map, kDescriptorsOffset),
                            kDependentCodeOffset - kDescriptorsOffset));
      } else {
        DCHECK_EQ(0, memcmp(reinterpret_cast<void*>(fresh->address()),
                            reinterpret_cast<void*>(new_map->address()),
                            Map::kDependentCodeOffset));
      }
      STATIC_ASSERT(Map::kPrototypeValidityCellOffset ==
                    Map::kDependentCodeOffset + kPointerSize);
      int offset = Map::kPrototypeValidityCellOffset + kPointerSize;
      DCHECK_EQ(0, memcmp(reinterpret_cast<void*>(fresh->address() + offset),
                          reinterpret_cast<void*>(new_map->address() + offset),
                          Map::kSize - offset));
    }
#endif
  } else {
    new_map = Map::CopyNormalized(isolate, fast_map, mode);
    if (use_cache) {
      cache->Set(fast_map, new_map);
      isolate->counters()->maps_normalized()->Increment();
    }
    if (FLAG_trace_maps) {
      LOG(isolate, MapEvent("Normalize", *fast_map, *new_map, reason));
    }
  }
  fast_map->NotifyLeafMapLayoutChange(isolate);
  return new_map;
}

Handle<Map> Map::CopyNormalized(Isolate* isolate, Handle<Map> map,
                                PropertyNormalizationMode mode) {
  int new_instance_size = map->instance_size();
  if (mode == CLEAR_INOBJECT_PROPERTIES) {
    new_instance_size -= map->GetInObjectProperties() * kPointerSize;
  }

  Handle<Map> result = RawCopy(
      isolate, map, new_instance_size,
      mode == CLEAR_INOBJECT_PROPERTIES ? 0 : map->GetInObjectProperties());
  // Clear the unused_property_fields explicitly as this field should not
  // be accessed for normalized maps.
  result->SetInObjectUnusedPropertyFields(0);
  result->set_is_dictionary_map(true);
  result->set_is_migration_target(false);
  result->set_may_have_interesting_symbols(true);
  result->set_construction_counter(kNoSlackTracking);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) result->DictionaryMapVerify(isolate);
#endif

  return result;
}

// Return an immutable prototype exotic object version of the input map.
// Never even try to cache it in the transition tree, as it is intended
// for the global object and its prototype chain, and excluding it saves
// memory on the map transition tree.

// static
Handle<Map> Map::TransitionToImmutableProto(Isolate* isolate, Handle<Map> map) {
  Handle<Map> new_map = Map::Copy(isolate, map, "ImmutablePrototype");
  new_map->set_is_immutable_proto(true);
  return new_map;
}

namespace {
void EnsureInitialMap(Isolate* isolate, Handle<Map> map) {
#ifdef DEBUG
  // Strict function maps have Function as a constructor but the
  // Function's initial map is a sloppy function map. Same holds for
  // GeneratorFunction / AsyncFunction and its initial map.
  Object* constructor = map->GetConstructor();
  DCHECK(constructor->IsJSFunction());
  DCHECK(*map == JSFunction::cast(constructor)->initial_map() ||
         *map == *isolate->strict_function_map() ||
         *map == *isolate->strict_function_with_name_map() ||
         *map == *isolate->generator_function_map() ||
         *map == *isolate->generator_function_with_name_map() ||
         *map == *isolate->generator_function_with_home_object_map() ||
         *map == *isolate->generator_function_with_name_and_home_object_map() ||
         *map == *isolate->async_function_map() ||
         *map == *isolate->async_function_with_name_map() ||
         *map == *isolate->async_function_with_home_object_map() ||
         *map == *isolate->async_function_with_name_and_home_object_map());
#endif
  // Initial maps must always own their descriptors and it's descriptor array
  // does not contain descriptors that do not belong to the map.
  DCHECK(map->owns_descriptors());
  DCHECK_EQ(map->NumberOfOwnDescriptors(),
            map->instance_descriptors()->number_of_descriptors());
}
}  // namespace

// static
Handle<Map> Map::CopyInitialMapNormalized(Isolate* isolate, Handle<Map> map,
                                          PropertyNormalizationMode mode) {
  EnsureInitialMap(isolate, map);
  return CopyNormalized(isolate, map, mode);
}

// static
Handle<Map> Map::CopyInitialMap(Isolate* isolate, Handle<Map> map,
                                int instance_size, int inobject_properties,
                                int unused_property_fields) {
  EnsureInitialMap(isolate, map);
  Handle<Map> result =
      RawCopy(isolate, map, instance_size, inobject_properties);

  // Please note instance_type and instance_size are set when allocated.
  result->SetInObjectUnusedPropertyFields(unused_property_fields);

  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors > 0) {
    // The copy will use the same descriptors array.
    result->UpdateDescriptors(map->instance_descriptors(),
                              map->GetLayoutDescriptor());
    result->SetNumberOfOwnDescriptors(number_of_own_descriptors);

    DCHECK_EQ(result->NumberOfFields(),
              result->GetInObjectProperties() - result->UnusedPropertyFields());
  }

  return result;
}

Handle<Map> Map::CopyDropDescriptors(Isolate* isolate, Handle<Map> map) {
  Handle<Map> result =
      RawCopy(isolate, map, map->instance_size(),
              map->IsJSObjectMap() ? map->GetInObjectProperties() : 0);

  // Please note instance_type and instance_size are set when allocated.
  if (map->IsJSObjectMap()) {
    result->CopyUnusedPropertyFields(*map);
  }
  map->NotifyLeafMapLayoutChange(isolate);
  return result;
}

Handle<Map> Map::ShareDescriptor(Isolate* isolate, Handle<Map> map,
                                 Handle<DescriptorArray> descriptors,
                                 Descriptor* descriptor) {
  // Sanity check. This path is only to be taken if the map owns its descriptor
  // array, implying that its NumberOfOwnDescriptors equals the number of
  // descriptors in the descriptor array.
  DCHECK_EQ(map->NumberOfOwnDescriptors(),
            map->instance_descriptors()->number_of_descriptors());

  Handle<Map> result = CopyDropDescriptors(isolate, map);
  Handle<Name> name = descriptor->GetKey();

  // Properly mark the {result} if the {name} is an "interesting symbol".
  if (name->IsInterestingSymbol()) {
    result->set_may_have_interesting_symbols(true);
  }

  // Ensure there's space for the new descriptor in the shared descriptor array.
  if (descriptors->NumberOfSlackDescriptors() == 0) {
    int old_size = descriptors->number_of_descriptors();
    if (old_size == 0) {
      descriptors = DescriptorArray::Allocate(isolate, 0, 1);
    } else {
      int slack = SlackForArraySize(old_size, kMaxNumberOfDescriptors);
      EnsureDescriptorSlack(isolate, map, slack);
      descriptors = handle(map->instance_descriptors(), isolate);
    }
  }

  Handle<LayoutDescriptor> layout_descriptor =
      FLAG_unbox_double_fields
          ? LayoutDescriptor::ShareAppend(isolate, map,
                                          descriptor->GetDetails())
          : handle(LayoutDescriptor::FastPointerLayout(), isolate);

  {
    DisallowHeapAllocation no_gc;
    descriptors->Append(descriptor);
    result->InitializeDescriptors(*descriptors, *layout_descriptor);
  }

  DCHECK(result->NumberOfOwnDescriptors() == map->NumberOfOwnDescriptors() + 1);
  ConnectTransition(isolate, map, result, name, SIMPLE_PROPERTY_TRANSITION);

  return result;
}

void Map::ConnectTransition(Isolate* isolate, Handle<Map> parent,
                            Handle<Map> child, Handle<Name> name,
                            SimpleTransitionFlag flag) {
  DCHECK_IMPLIES(name->IsInterestingSymbol(),
                 child->may_have_interesting_symbols());
  DCHECK_IMPLIES(parent->may_have_interesting_symbols(),
                 child->may_have_interesting_symbols());
  // Do not track transitions during bootstrap except for element transitions.
  if (isolate->bootstrapper()->IsActive() &&
      !name.is_identical_to(isolate->factory()->elements_transition_symbol())) {
    if (FLAG_trace_maps) {
      LOG(isolate,
          MapEvent("Transition", *parent, *child,
                   child->is_prototype_map() ? "prototype" : "", *name));
    }
    return;
  }
  if (!parent->GetBackPointer()->IsUndefined(isolate)) {
    parent->set_owns_descriptors(false);
  } else {
    // |parent| is initial map and it must keep the ownership, there must be no
    // descriptors in the descriptors array that do not belong to the map.
    DCHECK(parent->owns_descriptors());
    DCHECK_EQ(parent->NumberOfOwnDescriptors(),
              parent->instance_descriptors()->number_of_descriptors());
  }
  if (parent->is_prototype_map()) {
    DCHECK(child->is_prototype_map());
    if (FLAG_trace_maps) {
      LOG(isolate, MapEvent("Transition", *parent, *child, "prototype", *name));
    }
  } else {
    TransitionsAccessor(isolate, parent).Insert(name, child, flag);
    if (FLAG_trace_maps) {
      LOG(isolate, MapEvent("Transition", *parent, *child, "", *name));
    }
  }
}

Handle<Map> Map::CopyReplaceDescriptors(
    Isolate* isolate, Handle<Map> map, Handle<DescriptorArray> descriptors,
    Handle<LayoutDescriptor> layout_descriptor, TransitionFlag flag,
    MaybeHandle<Name> maybe_name, const char* reason,
    SimpleTransitionFlag simple_flag) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  Handle<Map> result = CopyDropDescriptors(isolate, map);

  // Properly mark the {result} if the {name} is an "interesting symbol".
  Handle<Name> name;
  if (maybe_name.ToHandle(&name) && name->IsInterestingSymbol()) {
    result->set_may_have_interesting_symbols(true);
  }

  if (!map->is_prototype_map()) {
    if (flag == INSERT_TRANSITION &&
        TransitionsAccessor(isolate, map).CanHaveMoreTransitions()) {
      result->InitializeDescriptors(*descriptors, *layout_descriptor);

      DCHECK(!maybe_name.is_null());
      ConnectTransition(isolate, map, result, name, simple_flag);
    } else {
      descriptors->GeneralizeAllFields();
      result->InitializeDescriptors(*descriptors,
                                    LayoutDescriptor::FastPointerLayout());
    }
  } else {
    result->InitializeDescriptors(*descriptors, *layout_descriptor);
  }
  if (FLAG_trace_maps &&
      // Mirror conditions above that did not call ConnectTransition().
      (map->is_prototype_map() ||
       !(flag == INSERT_TRANSITION &&
         TransitionsAccessor(isolate, map).CanHaveMoreTransitions()))) {
    LOG(isolate, MapEvent("ReplaceDescriptors", *map, *result, reason,
                          maybe_name.is_null() ? nullptr : *name));
  }
  return result;
}


// Creates transition tree starting from |split_map| and adding all descriptors
// starting from descriptor with index |split_map|.NumberOfOwnDescriptors().
// The way how it is done is tricky because of GC and special descriptors
// marking logic.
Handle<Map> Map::AddMissingTransitions(
    Isolate* isolate, Handle<Map> split_map,
    Handle<DescriptorArray> descriptors,
    Handle<LayoutDescriptor> full_layout_descriptor) {
  DCHECK(descriptors->IsSortedNoDuplicates());
  int split_nof = split_map->NumberOfOwnDescriptors();
  int nof_descriptors = descriptors->number_of_descriptors();
  DCHECK_LT(split_nof, nof_descriptors);

  // Start with creating last map which will own full descriptors array.
  // This is necessary to guarantee that GC will mark the whole descriptor
  // array if any of the allocations happening below fail.
  // Number of unused properties is temporarily incorrect and the layout
  // descriptor could unnecessarily be in slow mode but we will fix after
  // all the other intermediate maps are created.
  // Also the last map might have interesting symbols, we temporarily set
  // the flag and clear it right before the descriptors are installed. This
  // makes heap verification happy and ensures the flag ends up accurate.
  Handle<Map> last_map = CopyDropDescriptors(isolate, split_map);
  last_map->InitializeDescriptors(*descriptors, *full_layout_descriptor);
  last_map->SetInObjectUnusedPropertyFields(0);
  last_map->set_may_have_interesting_symbols(true);

  // During creation of intermediate maps we violate descriptors sharing
  // invariant since the last map is not yet connected to the transition tree
  // we create here. But it is safe because GC never trims map's descriptors
  // if there are no dead transitions from that map and this is exactly the
  // case for all the intermediate maps we create here.
  Handle<Map> map = split_map;
  for (int i = split_nof; i < nof_descriptors - 1; ++i) {
    Handle<Map> new_map = CopyDropDescriptors(isolate, map);
    InstallDescriptors(isolate, map, new_map, i, descriptors,
                       full_layout_descriptor);
    map = new_map;
  }
  map->NotifyLeafMapLayoutChange(isolate);
  last_map->set_may_have_interesting_symbols(false);
  InstallDescriptors(isolate, map, last_map, nof_descriptors - 1, descriptors,
                     full_layout_descriptor);
  return last_map;
}


// Since this method is used to rewrite an existing transition tree, it can
// always insert transitions without checking.
void Map::InstallDescriptors(Isolate* isolate, Handle<Map> parent,
                             Handle<Map> child, int new_descriptor,
                             Handle<DescriptorArray> descriptors,
                             Handle<LayoutDescriptor> full_layout_descriptor) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  child->set_instance_descriptors(*descriptors);
  child->SetNumberOfOwnDescriptors(new_descriptor + 1);
  child->CopyUnusedPropertyFields(*parent);
  PropertyDetails details = descriptors->GetDetails(new_descriptor);
  if (details.location() == kField) {
    child->AccountAddedPropertyField();
  }

  if (FLAG_unbox_double_fields) {
    Handle<LayoutDescriptor> layout_descriptor =
        LayoutDescriptor::AppendIfFastOrUseFull(isolate, parent, details,
                                                full_layout_descriptor);
    child->set_layout_descriptor(*layout_descriptor);
#ifdef VERIFY_HEAP
    // TODO(ishell): remove these checks from VERIFY_HEAP mode.
    if (FLAG_verify_heap) {
      CHECK(child->layout_descriptor()->IsConsistentWithMap(*child));
    }
#else
    SLOW_DCHECK(child->layout_descriptor()->IsConsistentWithMap(*child));
#endif
    child->set_visitor_id(Map::GetVisitorId(*child));
  }

  Handle<Name> name = handle(descriptors->GetKey(new_descriptor), isolate);
  if (parent->may_have_interesting_symbols() || name->IsInterestingSymbol()) {
    child->set_may_have_interesting_symbols(true);
  }
  ConnectTransition(isolate, parent, child, name, SIMPLE_PROPERTY_TRANSITION);
}

Handle<Map> Map::CopyAsElementsKind(Isolate* isolate, Handle<Map> map,
                                    ElementsKind kind, TransitionFlag flag) {
  // Only certain objects are allowed to have non-terminal fast transitional
  // elements kinds.
  DCHECK(map->IsJSObjectMap());
  DCHECK_IMPLIES(
      !map->CanHaveFastTransitionableElementsKind(),
      IsDictionaryElementsKind(kind) || IsTerminalElementsKind(kind));

  Map* maybe_elements_transition_map = nullptr;
  if (flag == INSERT_TRANSITION) {
    // Ensure we are requested to add elements kind transition "near the root".
    DCHECK_EQ(map->FindRootMap(isolate)->NumberOfOwnDescriptors(),
              map->NumberOfOwnDescriptors());

    maybe_elements_transition_map = map->ElementsTransitionMap();
    DCHECK(maybe_elements_transition_map == nullptr ||
           (maybe_elements_transition_map->elements_kind() ==
                DICTIONARY_ELEMENTS &&
            kind == DICTIONARY_ELEMENTS));
    DCHECK(!IsFastElementsKind(kind) ||
           IsMoreGeneralElementsKindTransition(map->elements_kind(), kind));
    DCHECK(kind != map->elements_kind());
  }

  bool insert_transition =
      flag == INSERT_TRANSITION &&
      TransitionsAccessor(isolate, map).CanHaveMoreTransitions() &&
      maybe_elements_transition_map == nullptr;

  if (insert_transition) {
    Handle<Map> new_map = CopyForTransition(isolate, map, "CopyAsElementsKind");
    new_map->set_elements_kind(kind);

    Handle<Name> name = isolate->factory()->elements_transition_symbol();
    ConnectTransition(isolate, map, new_map, name, SPECIAL_TRANSITION);
    return new_map;
  }

  // Create a new free-floating map only if we are not allowed to store it.
  Handle<Map> new_map = Copy(isolate, map, "CopyAsElementsKind");
  new_map->set_elements_kind(kind);
  return new_map;
}

Handle<Map> Map::AsLanguageMode(Isolate* isolate, Handle<Map> initial_map,
                                Handle<SharedFunctionInfo> shared_info) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  // Initial map for sloppy mode function is stored in the function
  // constructor. Initial maps for strict mode are cached as special transitions
  // using |strict_function_transition_symbol| as a key.
  if (is_sloppy(shared_info->language_mode())) return initial_map;

  Handle<Map> function_map(Map::cast(isolate->native_context()->get(
                               shared_info->function_map_index())),
                           isolate);

  STATIC_ASSERT(LanguageModeSize == 2);
  DCHECK_EQ(LanguageMode::kStrict, shared_info->language_mode());
  Handle<Symbol> transition_symbol =
      isolate->factory()->strict_function_transition_symbol();
  Map* maybe_transition = TransitionsAccessor(isolate, initial_map)
                              .SearchSpecial(*transition_symbol);
  if (maybe_transition != nullptr) {
    return handle(maybe_transition, isolate);
  }
  initial_map->NotifyLeafMapLayoutChange(isolate);

  // Create new map taking descriptors from the |function_map| and all
  // the other details from the |initial_map|.
  Handle<Map> map =
      Map::CopyInitialMap(isolate, function_map, initial_map->instance_size(),
                          initial_map->GetInObjectProperties(),
                          initial_map->UnusedPropertyFields());
  map->SetConstructor(initial_map->GetConstructor());
  map->set_prototype(initial_map->prototype());
  map->set_construction_counter(initial_map->construction_counter());

  if (TransitionsAccessor(isolate, initial_map).CanHaveMoreTransitions()) {
    Map::ConnectTransition(isolate, initial_map, map, transition_symbol,
                           SPECIAL_TRANSITION);
  }
  return map;
}

Handle<Map> Map::CopyForTransition(Isolate* isolate, Handle<Map> map,
                                   const char* reason) {
  DCHECK(!map->is_prototype_map());
  Handle<Map> new_map = CopyDropDescriptors(isolate, map);

  if (map->owns_descriptors()) {
    // In case the map owned its own descriptors, share the descriptors and
    // transfer ownership to the new map.
    // The properties did not change, so reuse descriptors.
    new_map->InitializeDescriptors(map->instance_descriptors(),
                                   map->GetLayoutDescriptor());
  } else {
    // In case the map did not own its own descriptors, a split is forced by
    // copying the map; creating a new descriptor array cell.
    Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
    int number_of_own_descriptors = map->NumberOfOwnDescriptors();
    Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
        isolate, descriptors, number_of_own_descriptors);
    Handle<LayoutDescriptor> new_layout_descriptor(map->GetLayoutDescriptor(),
                                                   isolate);
    new_map->InitializeDescriptors(*new_descriptors, *new_layout_descriptor);
  }

  if (FLAG_trace_maps) {
    LOG(isolate, MapEvent("CopyForTransition", *map, *new_map, reason));
  }
  return new_map;
}

Handle<Map> Map::Copy(Isolate* isolate, Handle<Map> map, const char* reason) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      isolate, descriptors, number_of_own_descriptors);
  Handle<LayoutDescriptor> new_layout_descriptor(map->GetLayoutDescriptor(),
                                                 isolate);
  return CopyReplaceDescriptors(
      isolate, map, new_descriptors, new_layout_descriptor, OMIT_TRANSITION,
      MaybeHandle<Name>(), reason, SPECIAL_TRANSITION);
}


Handle<Map> Map::Create(Isolate* isolate, int inobject_properties) {
  Handle<Map> copy =
      Copy(isolate, handle(isolate->object_function()->initial_map(), isolate),
           "MapCreate");

  // Check that we do not overflow the instance size when adding the extra
  // inobject properties. If the instance size overflows, we allocate as many
  // properties as we can as inobject properties.
  if (inobject_properties > JSObject::kMaxInObjectProperties) {
    inobject_properties = JSObject::kMaxInObjectProperties;
  }

  int new_instance_size =
      JSObject::kHeaderSize + kPointerSize * inobject_properties;

  // Adjust the map with the extra inobject properties.
  copy->set_instance_size(new_instance_size);
  copy->SetInObjectPropertiesStartInWords(JSObject::kHeaderSize / kPointerSize);
  DCHECK_EQ(copy->GetInObjectProperties(), inobject_properties);
  copy->SetInObjectUnusedPropertyFields(inobject_properties);
  copy->set_visitor_id(Map::GetVisitorId(*copy));
  return copy;
}

Handle<Map> Map::CopyForPreventExtensions(Isolate* isolate, Handle<Map> map,
                                          PropertyAttributes attrs_to_add,
                                          Handle<Symbol> transition_marker,
                                          const char* reason) {
  int num_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_desc = DescriptorArray::CopyUpToAddAttributes(
      isolate, handle(map->instance_descriptors(), isolate), num_descriptors,
      attrs_to_add);
  Handle<LayoutDescriptor> new_layout_descriptor(map->GetLayoutDescriptor(),
                                                 isolate);
  Handle<Map> new_map = CopyReplaceDescriptors(
      isolate, map, new_desc, new_layout_descriptor, INSERT_TRANSITION,
      transition_marker, reason, SPECIAL_TRANSITION);
  new_map->set_is_extensible(false);
  if (!IsFixedTypedArrayElementsKind(map->elements_kind())) {
    ElementsKind new_kind = IsStringWrapperElementsKind(map->elements_kind())
                                ? SLOW_STRING_WRAPPER_ELEMENTS
                                : DICTIONARY_ELEMENTS;
    new_map->set_elements_kind(new_kind);
  }
  return new_map;
}

namespace {

bool CanHoldValue(DescriptorArray* descriptors, int descriptor,
                  PropertyConstness constness, Object* value) {
  PropertyDetails details = descriptors->GetDetails(descriptor);
  if (details.location() == kField) {
    if (details.kind() == kData) {
      return IsGeneralizableTo(constness, details.constness()) &&
             value->FitsRepresentation(details.representation()) &&
             descriptors->GetFieldType(descriptor)->NowContains(value);
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      return false;
    }

  } else {
    DCHECK_EQ(kDescriptor, details.location());
    DCHECK_EQ(PropertyConstness::kConst, details.constness());
    if (details.kind() == kData) {
      DCHECK(!FLAG_track_constant_fields);
      DCHECK(descriptors->GetStrongValue(descriptor) != value ||
             value->FitsRepresentation(details.representation()));
      return descriptors->GetStrongValue(descriptor) == value;
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      return false;
    }
  }
  UNREACHABLE();
}

Handle<Map> UpdateDescriptorForValue(Isolate* isolate, Handle<Map> map,
                                     int descriptor,
                                     PropertyConstness constness,
                                     Handle<Object> value) {
  if (CanHoldValue(map->instance_descriptors(), descriptor, constness,
                   *value)) {
    return map;
  }

  PropertyAttributes attributes =
      map->instance_descriptors()->GetDetails(descriptor).attributes();
  Representation representation = value->OptimalRepresentation();
  Handle<FieldType> type = value->OptimalType(isolate, representation);

  MapUpdater mu(isolate, map);
  return mu.ReconfigureToDataField(descriptor, attributes, constness,
                                   representation, type);
}

}  // namespace

// static
Handle<Map> Map::PrepareForDataProperty(Isolate* isolate, Handle<Map> map,
                                        int descriptor,
                                        PropertyConstness constness,
                                        Handle<Object> value) {
  // Dictionaries can store any property value.
  DCHECK(!map->is_dictionary_map());
  // Update to the newest map before storing the property.
  return UpdateDescriptorForValue(isolate, Update(isolate, map), descriptor,
                                  constness, value);
}

Handle<Map> Map::TransitionToDataProperty(Isolate* isolate, Handle<Map> map,
                                          Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes,
                                          PropertyConstness constness,
                                          StoreOrigin store_origin) {
  RuntimeCallTimerScope stats_scope(
      isolate, *map,
      map->is_prototype_map()
          ? RuntimeCallCounterId::kPrototypeMap_TransitionToDataProperty
          : RuntimeCallCounterId::kMap_TransitionToDataProperty);

  DCHECK(name->IsUniqueName());
  DCHECK(!map->is_dictionary_map());

  // Migrate to the newest map before storing the property.
  map = Update(isolate, map);

  Map* maybe_transition = TransitionsAccessor(isolate, map)
                              .SearchTransition(*name, kData, attributes);
  if (maybe_transition != nullptr) {
    Handle<Map> transition(maybe_transition, isolate);
    int descriptor = transition->LastAdded();

    DCHECK_EQ(attributes, transition->instance_descriptors()
                              ->GetDetails(descriptor)
                              .attributes());

    return UpdateDescriptorForValue(isolate, transition, descriptor, constness,
                                    value);
  }

  TransitionFlag flag = INSERT_TRANSITION;
  MaybeHandle<Map> maybe_map;
  if (!map->TooManyFastProperties(store_origin)) {
    if (!FLAG_track_constant_fields && value->IsJSFunction()) {
      maybe_map =
          Map::CopyWithConstant(isolate, map, name, value, attributes, flag);
    } else {
      Representation representation = value->OptimalRepresentation();
      Handle<FieldType> type = value->OptimalType(isolate, representation);
      maybe_map = Map::CopyWithField(isolate, map, name, type, attributes,
                                     constness, representation, flag);
    }
  }

  Handle<Map> result;
  if (!maybe_map.ToHandle(&result)) {
    const char* reason = "TooManyFastProperties";
#if V8_TRACE_MAPS
    std::unique_ptr<ScopedVector<char>> buffer;
    if (FLAG_trace_maps) {
      ScopedVector<char> name_buffer(100);
      name->NameShortPrint(name_buffer);
      buffer.reset(new ScopedVector<char>(128));
      SNPrintF(*buffer, "TooManyFastProperties %s", name_buffer.start());
      reason = buffer->start();
    }
#endif
    Handle<Object> maybe_constructor(map->GetConstructor(), isolate);
    if (FLAG_feedback_normalization && map->new_target_is_base() &&
        maybe_constructor->IsJSFunction() &&
        !JSFunction::cast(*maybe_constructor)->shared()->native()) {
      Handle<JSFunction> constructor =
          Handle<JSFunction>::cast(maybe_constructor);
      DCHECK_NE(*constructor,
                constructor->context()->native_context()->object_function());
      Handle<Map> initial_map(constructor->initial_map(), isolate);
      result = Map::Normalize(isolate, initial_map, CLEAR_INOBJECT_PROPERTIES,
                              reason);
      initial_map->DeprecateTransitionTree(isolate);
      Handle<Object> prototype(result->prototype(), isolate);
      JSFunction::SetInitialMap(constructor, result, prototype);

      // Deoptimize all code that embeds the previous initial map.
      initial_map->dependent_code()->DeoptimizeDependentCodeGroup(
          isolate, DependentCode::kInitialMapChangedGroup);
      if (!result->EquivalentToForNormalization(*map,
                                                CLEAR_INOBJECT_PROPERTIES)) {
        result =
            Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES, reason);
      }
    } else {
      result = Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES, reason);
    }
  }

  return result;
}

Handle<Map> Map::ReconfigureExistingProperty(Isolate* isolate, Handle<Map> map,
                                             int descriptor, PropertyKind kind,
                                             PropertyAttributes attributes) {
  // Dictionaries have to be reconfigured in-place.
  DCHECK(!map->is_dictionary_map());

  if (!map->GetBackPointer()->IsMap()) {
    // There is no benefit from reconstructing transition tree for maps without
    // back pointers.
    return CopyGeneralizeAllFields(isolate, map, map->elements_kind(),
                                   descriptor, kind, attributes,
                                   "GenAll_AttributesMismatchProtoMap");
  }

  if (FLAG_trace_generalization) {
    map->PrintReconfiguration(isolate, stdout, descriptor, kind, attributes);
  }

  MapUpdater mu(isolate, map);
  DCHECK_EQ(kData, kind);  // Only kData case is supported so far.
  Handle<Map> new_map = mu.ReconfigureToDataField(
      descriptor, attributes, kDefaultFieldConstness, Representation::None(),
      FieldType::None(isolate));
  return new_map;
}

Handle<Map> Map::TransitionToAccessorProperty(Isolate* isolate, Handle<Map> map,
                                              Handle<Name> name, int descriptor,
                                              Handle<Object> getter,
                                              Handle<Object> setter,
                                              PropertyAttributes attributes) {
  RuntimeCallTimerScope stats_scope(
      isolate,
      map->is_prototype_map()
          ? RuntimeCallCounterId::kPrototypeMap_TransitionToAccessorProperty
          : RuntimeCallCounterId::kMap_TransitionToAccessorProperty);

  // At least one of the accessors needs to be a new value.
  DCHECK(!getter->IsNull(isolate) || !setter->IsNull(isolate));
  DCHECK(name->IsUniqueName());

  // Dictionary maps can always have additional data properties.
  if (map->is_dictionary_map()) return map;

  // Migrate to the newest map before transitioning to the new property.
  map = Update(isolate, map);

  PropertyNormalizationMode mode = map->is_prototype_map()
                                       ? KEEP_INOBJECT_PROPERTIES
                                       : CLEAR_INOBJECT_PROPERTIES;

  Map* maybe_transition = TransitionsAccessor(isolate, map)
                              .SearchTransition(*name, kAccessor, attributes);
  if (maybe_transition != nullptr) {
    Handle<Map> transition(maybe_transition, isolate);
    DescriptorArray* descriptors = transition->instance_descriptors();
    int descriptor = transition->LastAdded();
    DCHECK(descriptors->GetKey(descriptor)->Equals(*name));

    DCHECK_EQ(kAccessor, descriptors->GetDetails(descriptor).kind());
    DCHECK_EQ(attributes, descriptors->GetDetails(descriptor).attributes());

    Handle<Object> maybe_pair(descriptors->GetStrongValue(descriptor), isolate);
    if (!maybe_pair->IsAccessorPair()) {
      return Map::Normalize(isolate, map, mode,
                            "TransitionToAccessorFromNonPair");
    }

    Handle<AccessorPair> pair = Handle<AccessorPair>::cast(maybe_pair);
    if (!pair->Equals(*getter, *setter)) {
      return Map::Normalize(isolate, map, mode,
                            "TransitionToDifferentAccessor");
    }

    return transition;
  }

  Handle<AccessorPair> pair;
  DescriptorArray* old_descriptors = map->instance_descriptors();
  if (descriptor != DescriptorArray::kNotFound) {
    if (descriptor != map->LastAdded()) {
      return Map::Normalize(isolate, map, mode, "AccessorsOverwritingNonLast");
    }
    PropertyDetails old_details = old_descriptors->GetDetails(descriptor);
    if (old_details.kind() != kAccessor) {
      return Map::Normalize(isolate, map, mode,
                            "AccessorsOverwritingNonAccessors");
    }

    if (old_details.attributes() != attributes) {
      return Map::Normalize(isolate, map, mode, "AccessorsWithAttributes");
    }

    Handle<Object> maybe_pair(old_descriptors->GetStrongValue(descriptor),
                              isolate);
    if (!maybe_pair->IsAccessorPair()) {
      return Map::Normalize(isolate, map, mode, "AccessorsOverwritingNonPair");
    }

    Handle<AccessorPair> current_pair = Handle<AccessorPair>::cast(maybe_pair);
    if (current_pair->Equals(*getter, *setter)) return map;

    bool overwriting_accessor = false;
    if (!getter->IsNull(isolate) &&
        !current_pair->get(ACCESSOR_GETTER)->IsNull(isolate) &&
        current_pair->get(ACCESSOR_GETTER) != *getter) {
      overwriting_accessor = true;
    }
    if (!setter->IsNull(isolate) &&
        !current_pair->get(ACCESSOR_SETTER)->IsNull(isolate) &&
        current_pair->get(ACCESSOR_SETTER) != *setter) {
      overwriting_accessor = true;
    }
    if (overwriting_accessor) {
      return Map::Normalize(isolate, map, mode,
                            "AccessorsOverwritingAccessors");
    }

    pair = AccessorPair::Copy(isolate, Handle<AccessorPair>::cast(maybe_pair));
  } else if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors ||
             map->TooManyFastProperties(StoreOrigin::kNamed)) {
    return Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES,
                          "TooManyAccessors");
  } else {
    pair = isolate->factory()->NewAccessorPair();
  }

  pair->SetComponents(*getter, *setter);

  TransitionFlag flag = INSERT_TRANSITION;
  Descriptor d = Descriptor::AccessorConstant(name, pair, attributes);
  return Map::CopyInsertDescriptor(isolate, map, &d, flag);
}

Handle<Map> Map::CopyAddDescriptor(Isolate* isolate, Handle<Map> map,
                                   Descriptor* descriptor,
                                   TransitionFlag flag) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);

  // Share descriptors only if map owns descriptors and it not an initial map.
  if (flag == INSERT_TRANSITION && map->owns_descriptors() &&
      !map->GetBackPointer()->IsUndefined(isolate) &&
      TransitionsAccessor(isolate, map).CanHaveMoreTransitions()) {
    return ShareDescriptor(isolate, map, descriptors, descriptor);
  }

  int nof = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::CopyUpTo(isolate, descriptors, nof, 1);
  new_descriptors->Append(descriptor);

  Handle<LayoutDescriptor> new_layout_descriptor =
      FLAG_unbox_double_fields
          ? LayoutDescriptor::New(isolate, map, new_descriptors, nof + 1)
          : handle(LayoutDescriptor::FastPointerLayout(), isolate);

  return CopyReplaceDescriptors(
      isolate, map, new_descriptors, new_layout_descriptor, flag,
      descriptor->GetKey(), "CopyAddDescriptor", SIMPLE_PROPERTY_TRANSITION);
}

Handle<Map> Map::CopyInsertDescriptor(Isolate* isolate, Handle<Map> map,
                                      Descriptor* descriptor,
                                      TransitionFlag flag) {
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(), isolate);

  // We replace the key if it is already present.
  int index =
      old_descriptors->SearchWithCache(isolate, *descriptor->GetKey(), *map);
  if (index != DescriptorArray::kNotFound) {
    return CopyReplaceDescriptor(isolate, map, old_descriptors, descriptor,
                                 index, flag);
  }
  return CopyAddDescriptor(isolate, map, descriptor, flag);
}

Handle<DescriptorArray> DescriptorArray::CopyUpTo(Isolate* isolate,
                                                  Handle<DescriptorArray> desc,
                                                  int enumeration_index,
                                                  int slack) {
  return DescriptorArray::CopyUpToAddAttributes(isolate, desc,
                                                enumeration_index, NONE, slack);
}

Handle<DescriptorArray> DescriptorArray::CopyUpToAddAttributes(
    Isolate* isolate, Handle<DescriptorArray> desc, int enumeration_index,
    PropertyAttributes attributes, int slack) {
  if (enumeration_index + slack == 0) {
    return isolate->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;

  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, size, slack);

  if (attributes != NONE) {
    for (int i = 0; i < size; ++i) {
      MaybeObject* value_or_field_type = desc->GetValue(i);
      Name* key = desc->GetKey(i);
      PropertyDetails details = desc->GetDetails(i);
      // Bulk attribute changes never affect private properties.
      if (!key->IsPrivate()) {
        int mask = DONT_DELETE | DONT_ENUM;
        // READ_ONLY is an invalid attribute for JS setters/getters.
        HeapObject* heap_object;
        if (details.kind() != kAccessor ||
            !(value_or_field_type->GetHeapObjectIfStrong(&heap_object) &&
              heap_object->IsAccessorPair())) {
          mask |= READ_ONLY;
        }
        details = details.CopyAddAttributes(
            static_cast<PropertyAttributes>(attributes & mask));
      }
      descriptors->Set(i, key, value_or_field_type, details);
    }
  } else {
    for (int i = 0; i < size; ++i) {
      descriptors->CopyFrom(i, *desc);
    }
  }

  if (desc->number_of_descriptors() != enumeration_index) descriptors->Sort();

  return descriptors;
}

// Create a new descriptor array with only enumerable, configurable, writeable
// data properties, but identical field locations.
Handle<DescriptorArray> DescriptorArray::CopyForFastObjectClone(
    Isolate* isolate, Handle<DescriptorArray> src, int enumeration_index,
    int slack) {
  if (enumeration_index + slack == 0) {
    return isolate->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;
  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, size, slack);

  for (int i = 0; i < size; ++i) {
    Name* key = src->GetKey(i);
    PropertyDetails details = src->GetDetails(i);

    DCHECK(!key->IsPrivateField());
    DCHECK(details.IsEnumerable());
    DCHECK_EQ(details.kind(), kData);

    // Ensure the ObjectClone property details are NONE, and that all source
    // details did not contain DONT_ENUM.
    PropertyDetails new_details(kData, NONE, details.location(),
                                details.constness(), details.representation(),
                                details.field_index());
    // Do not propagate the field type of normal object fields from the
    // original descriptors since FieldType changes don't create new maps.
    MaybeObject* type = src->GetValue(i);
    if (details.location() == PropertyLocation::kField) {
      type = MaybeObject::FromObject(FieldType::Any());
    }
    descriptors->Set(i, key, type, new_details);
  }

  descriptors->Sort();

  return descriptors;
}

bool DescriptorArray::IsEqualUpTo(DescriptorArray* desc, int nof_descriptors) {
  for (int i = 0; i < nof_descriptors; i++) {
    if (GetKey(i) != desc->GetKey(i) || GetValue(i) != desc->GetValue(i)) {
      return false;
    }
    PropertyDetails details = GetDetails(i);
    PropertyDetails other_details = desc->GetDetails(i);
    if (details.kind() != other_details.kind() ||
        details.location() != other_details.location() ||
        !details.representation().Equals(other_details.representation())) {
      return false;
    }
  }
  return true;
}

Handle<Map> Map::CopyReplaceDescriptor(Isolate* isolate, Handle<Map> map,
                                       Handle<DescriptorArray> descriptors,
                                       Descriptor* descriptor,
                                       int insertion_index,
                                       TransitionFlag flag) {
  Handle<Name> key = descriptor->GetKey();
  DCHECK_EQ(*key, descriptors->GetKey(insertion_index));
  // This function does not support replacing property fields as
  // that would break property field counters.
  DCHECK_NE(kField, descriptor->GetDetails().location());
  DCHECK_NE(kField, descriptors->GetDetails(insertion_index).location());

  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      isolate, descriptors, map->NumberOfOwnDescriptors());

  new_descriptors->Replace(insertion_index, descriptor);
  Handle<LayoutDescriptor> new_layout_descriptor = LayoutDescriptor::New(
      isolate, map, new_descriptors, new_descriptors->number_of_descriptors());

  SimpleTransitionFlag simple_flag =
      (insertion_index == descriptors->number_of_descriptors() - 1)
          ? SIMPLE_PROPERTY_TRANSITION
          : PROPERTY_TRANSITION;
  return CopyReplaceDescriptors(isolate, map, new_descriptors,
                                new_layout_descriptor, flag, key,
                                "CopyReplaceDescriptor", simple_flag);
}

Handle<FixedArray> FixedArray::SetAndGrow(Isolate* isolate,
                                          Handle<FixedArray> array, int index,
                                          Handle<Object> value,
                                          PretenureFlag pretenure) {
  if (index < array->length()) {
    array->set(index, *value);
    return array;
  }
  int capacity = array->length();
  do {
    capacity = JSObject::NewElementsCapacity(capacity);
  } while (capacity <= index);
  Handle<FixedArray> new_array =
      isolate->factory()->NewUninitializedFixedArray(capacity, pretenure);
  array->CopyTo(0, *new_array, 0, array->length());
  new_array->FillWithHoles(array->length(), new_array->length());
  new_array->set(index, *value);
  return new_array;
}

bool FixedArray::ContainsSortedNumbers() {
  for (int i = 1; i < length(); ++i) {
    Object* a_obj = get(i - 1);
    Object* b_obj = get(i);
    if (!a_obj->IsNumber() || !b_obj->IsNumber()) return false;

    uint32_t a = NumberToUint32(a_obj);
    uint32_t b = NumberToUint32(b_obj);

    if (a > b) return false;
  }
  return true;
}

Handle<FixedArray> FixedArray::ShrinkOrEmpty(Isolate* isolate,
                                             Handle<FixedArray> array,
                                             int new_length) {
  if (new_length == 0) {
    return array->GetReadOnlyRoots().empty_fixed_array_handle();
  } else {
    array->Shrink(isolate, new_length);
    return array;
  }
}

void FixedArray::Shrink(Isolate* isolate, int new_length) {
  DCHECK(0 < new_length && new_length <= length());
  if (new_length < length()) {
    isolate->heap()->RightTrimFixedArray(this, length() - new_length);
  }
}

void FixedArray::CopyTo(int pos, FixedArray* dest, int dest_pos,
                        int len) const {
  DisallowHeapAllocation no_gc;
  // Return early if len == 0 so that we don't try to read the write barrier off
  // a canonical read-only empty fixed array.
  if (len == 0) return;
  WriteBarrierMode mode = dest->GetWriteBarrierMode(no_gc);
  for (int index = 0; index < len; index++) {
    dest->set(dest_pos+index, get(pos+index), mode);
  }
}

#ifdef DEBUG
bool FixedArray::IsEqualTo(FixedArray* other) {
  if (length() != other->length()) return false;
  for (int i = 0 ; i < length(); ++i) {
    if (get(i) != other->get(i)) return false;
  }
  return true;
}
#endif

void JSObject::PrototypeRegistryCompactionCallback(HeapObject* value,
                                                   int old_index,
                                                   int new_index) {
  DCHECK(value->IsMap() && Map::cast(value)->is_prototype_map());
  Map* map = Map::cast(value);
  DCHECK(map->prototype_info()->IsPrototypeInfo());
  PrototypeInfo* proto_info = PrototypeInfo::cast(map->prototype_info());
  DCHECK_EQ(old_index, proto_info->registry_slot());
  proto_info->set_registry_slot(new_index);
}

// static
Handle<ArrayList> ArrayList::Add(Isolate* isolate, Handle<ArrayList> array,
                                 Handle<Object> obj) {
  int length = array->Length();
  array = EnsureSpace(isolate, array, length + 1);
  // Check that GC didn't remove elements from the array.
  DCHECK_EQ(array->Length(), length);
  array->Set(length, *obj);
  array->SetLength(length + 1);
  return array;
}

// static
Handle<ArrayList> ArrayList::Add(Isolate* isolate, Handle<ArrayList> array,
                                 Handle<Object> obj1, Handle<Object> obj2) {
  int length = array->Length();
  array = EnsureSpace(isolate, array, length + 2);
  // Check that GC didn't remove elements from the array.
  DCHECK_EQ(array->Length(), length);
  array->Set(length, *obj1);
  array->Set(length + 1, *obj2);
  array->SetLength(length + 2);
  return array;
}

// static
Handle<ArrayList> ArrayList::New(Isolate* isolate, int size) {
  Handle<FixedArray> fixed_array =
      isolate->factory()->NewFixedArray(size + kFirstIndex);
  fixed_array->set_map_no_write_barrier(
      ReadOnlyRoots(isolate).array_list_map());
  Handle<ArrayList> result = Handle<ArrayList>::cast(fixed_array);
  result->SetLength(0);
  return result;
}

Handle<FixedArray> ArrayList::Elements(Isolate* isolate,
                                       Handle<ArrayList> array) {
  int length = array->Length();
  Handle<FixedArray> result = isolate->factory()->NewFixedArray(length);
  // Do not copy the first entry, i.e., the length.
  array->CopyTo(kFirstIndex, *result, 0, length);
  return result;
}

namespace {

Handle<FixedArray> EnsureSpaceInFixedArray(Isolate* isolate,
                                           Handle<FixedArray> array,
                                           int length) {
  int capacity = array->length();
  if (capacity < length) {
    int new_capacity = length;
    new_capacity = new_capacity + Max(new_capacity / 2, 2);
    int grow_by = new_capacity - capacity;
    array = isolate->factory()->CopyFixedArrayAndGrow(array, grow_by);
  }
  return array;
}

}  // namespace

// static
Handle<ArrayList> ArrayList::EnsureSpace(Isolate* isolate,
                                         Handle<ArrayList> array, int length) {
  const bool empty = (array->length() == 0);
  auto ret = EnsureSpaceInFixedArray(isolate, array, kFirstIndex + length);
  if (empty) {
    ret->set_map_no_write_barrier(array->GetReadOnlyRoots().array_list_map());

    Handle<ArrayList>::cast(ret)->SetLength(0);
  }
  return Handle<ArrayList>::cast(ret);
}

// static
Handle<WeakArrayList> WeakArrayList::AddToEnd(Isolate* isolate,
                                              Handle<WeakArrayList> array,
                                              const MaybeObjectHandle& value) {
  int length = array->length();
  array = EnsureSpace(isolate, array, length + 1);
  // Reload length; GC might have removed elements from the array.
  length = array->length();
  array->Set(length, *value);
  array->set_length(length + 1);
  return array;
}

bool WeakArrayList::IsFull() { return length() == capacity(); }

// static
Handle<WeakArrayList> WeakArrayList::EnsureSpace(Isolate* isolate,
                                                 Handle<WeakArrayList> array,
                                                 int length,
                                                 PretenureFlag pretenure) {
  int capacity = array->capacity();
  if (capacity < length) {
    int new_capacity = length;
    new_capacity = new_capacity + Max(new_capacity / 2, 2);
    int grow_by = new_capacity - capacity;
    array =
        isolate->factory()->CopyWeakArrayListAndGrow(array, grow_by, pretenure);
  }
  return array;
}

int WeakArrayList::CountLiveWeakReferences() const {
  int live_weak_references = 0;
  for (int i = 0; i < length(); i++) {
    if (Get(i)->IsWeak()) {
      ++live_weak_references;
    }
  }
  return live_weak_references;
}

bool WeakArrayList::RemoveOne(const MaybeObjectHandle& value) {
  if (length() == 0) return false;
  // Optimize for the most recently added element to be removed again.
  int last_index = length() - 1;
  for (int i = last_index; i >= 0; --i) {
    if (Get(i) == *value) {
      // Move the last element into the this slot (or no-op, if this is the
      // last slot).
      Set(i, Get(last_index));
      Set(last_index, HeapObjectReference::ClearedValue());
      set_length(last_index);
      return true;
    }
  }
  return false;
}

// static
Handle<WeakArrayList> PrototypeUsers::Add(Isolate* isolate,
                                          Handle<WeakArrayList> array,
                                          Handle<Map> value,
                                          int* assigned_index) {
  int length = array->length();
  if (length == 0) {
    // Uninitialized WeakArrayList; need to initialize empty_slot_index.
    array = WeakArrayList::EnsureSpace(isolate, array, kFirstIndex + 1);
    set_empty_slot_index(*array, kNoEmptySlotsMarker);
    array->Set(kFirstIndex, HeapObjectReference::Weak(*value));
    array->set_length(kFirstIndex + 1);
    if (assigned_index != nullptr) *assigned_index = kFirstIndex;
    return array;
  }

  // If the array has unfilled space at the end, use it.
  if (!array->IsFull()) {
    array->Set(length, HeapObjectReference::Weak(*value));
    array->set_length(length + 1);
    if (assigned_index != nullptr) *assigned_index = length;
    return array;
  }

  // If there are empty slots, use one of them.
  int empty_slot = Smi::ToInt(empty_slot_index(*array));
  if (empty_slot != kNoEmptySlotsMarker) {
    DCHECK_GE(empty_slot, kFirstIndex);
    CHECK_LT(empty_slot, array->length());
    int next_empty_slot = Smi::ToInt(array->Get(empty_slot)->cast<Smi>());

    array->Set(empty_slot, HeapObjectReference::Weak(*value));
    if (assigned_index != nullptr) *assigned_index = empty_slot;

    set_empty_slot_index(*array, next_empty_slot);
    return array;
  } else {
    DCHECK_EQ(empty_slot, kNoEmptySlotsMarker);
  }

  // Array full and no empty slots. Grow the array.
  array = WeakArrayList::EnsureSpace(isolate, array, length + 1);
  array->Set(length, HeapObjectReference::Weak(*value));
  array->set_length(length + 1);
  if (assigned_index != nullptr) *assigned_index = length;
  return array;
}

WeakArrayList* PrototypeUsers::Compact(Handle<WeakArrayList> array, Heap* heap,
                                       CompactionCallback callback,
                                       PretenureFlag pretenure) {
  if (array->length() == 0) {
    return *array;
  }
  int new_length = kFirstIndex + array->CountLiveWeakReferences();
  if (new_length == array->length()) {
    return *array;
  }

  Handle<WeakArrayList> new_array = WeakArrayList::EnsureSpace(
      heap->isolate(),
      handle(ReadOnlyRoots(heap).empty_weak_array_list(), heap->isolate()),
      new_length, pretenure);
  // Allocation might have caused GC and turned some of the elements into
  // cleared weak heap objects. Count the number of live objects again.
  int copy_to = kFirstIndex;
  for (int i = kFirstIndex; i < array->length(); i++) {
    MaybeObject* element = array->Get(i);
    HeapObject* value;
    if (element->GetHeapObjectIfWeak(&value)) {
      callback(value, i, copy_to);
      new_array->Set(copy_to++, element);
    } else {
      DCHECK(element->IsCleared() || element->IsSmi());
    }
  }
  new_array->set_length(copy_to);
  set_empty_slot_index(*new_array, kNoEmptySlotsMarker);
  return *new_array;
}

Handle<RegExpMatchInfo> RegExpMatchInfo::ReserveCaptures(
    Isolate* isolate, Handle<RegExpMatchInfo> match_info, int capture_count) {
  DCHECK_GE(match_info->length(), kLastMatchOverhead);
  const int required_length = kFirstCaptureIndex + capture_count;
  Handle<FixedArray> result =
      EnsureSpaceInFixedArray(isolate, match_info, required_length);
  return Handle<RegExpMatchInfo>::cast(result);
}

// static
Handle<FrameArray> FrameArray::AppendJSFrame(Handle<FrameArray> in,
                                             Handle<Object> receiver,
                                             Handle<JSFunction> function,
                                             Handle<AbstractCode> code,
                                             int offset, int flags) {
  const int frame_count = in->FrameCount();
  const int new_length = LengthFor(frame_count + 1);
  Handle<FrameArray> array =
      EnsureSpace(function->GetIsolate(), in, new_length);
  array->SetReceiver(frame_count, *receiver);
  array->SetFunction(frame_count, *function);
  array->SetCode(frame_count, *code);
  array->SetOffset(frame_count, Smi::FromInt(offset));
  array->SetFlags(frame_count, Smi::FromInt(flags));
  array->set(kFrameCountIndex, Smi::FromInt(frame_count + 1));
  return array;
}

// static
Handle<FrameArray> FrameArray::AppendWasmFrame(
    Handle<FrameArray> in, Handle<WasmInstanceObject> wasm_instance,
    int wasm_function_index, wasm::WasmCode* code, int offset, int flags) {
  Isolate* isolate = wasm_instance->GetIsolate();
  const int frame_count = in->FrameCount();
  const int new_length = LengthFor(frame_count + 1);
  Handle<FrameArray> array = EnsureSpace(isolate, in, new_length);
  // The {code} will be {nullptr} for interpreted wasm frames.
  Handle<Foreign> code_foreign =
      isolate->factory()->NewForeign(reinterpret_cast<Address>(code));
  array->SetWasmInstance(frame_count, *wasm_instance);
  array->SetWasmFunctionIndex(frame_count, Smi::FromInt(wasm_function_index));
  array->SetWasmCodeObject(frame_count, *code_foreign);
  array->SetOffset(frame_count, Smi::FromInt(offset));
  array->SetFlags(frame_count, Smi::FromInt(flags));
  array->set(kFrameCountIndex, Smi::FromInt(frame_count + 1));
  return array;
}

void FrameArray::ShrinkToFit(Isolate* isolate) {
  Shrink(isolate, LengthFor(FrameCount()));
}

// static
Handle<FrameArray> FrameArray::EnsureSpace(Isolate* isolate,
                                           Handle<FrameArray> array,
                                           int length) {
  return Handle<FrameArray>::cast(
      EnsureSpaceInFixedArray(isolate, array, length));
}

Handle<DescriptorArray> DescriptorArray::Allocate(Isolate* isolate,
                                                  int number_of_descriptors,
                                                  int slack,
                                                  PretenureFlag pretenure) {
  DCHECK_LE(0, number_of_descriptors);
  Factory* factory = isolate->factory();
  // Do not use DescriptorArray::cast on incomplete object.
  int size = number_of_descriptors + slack;
  if (size == 0) return factory->empty_descriptor_array();
  // Allocate the array of keys.
  Handle<WeakFixedArray> result =
      factory->NewWeakFixedArrayWithMap<DescriptorArray>(
          RootIndex::kDescriptorArrayMap, LengthFor(size), pretenure);
  result->Set(kDescriptorLengthIndex,
              MaybeObject::FromObject(Smi::FromInt(number_of_descriptors)));
  result->Set(kEnumCacheIndex, MaybeObject::FromObject(
                                   ReadOnlyRoots(isolate).empty_enum_cache()));
  return Handle<DescriptorArray>::cast(result);
}

void DescriptorArray::ClearEnumCache() {
  set(kEnumCacheIndex,
      MaybeObject::FromObject(GetReadOnlyRoots().empty_enum_cache()));
}

void DescriptorArray::Replace(int index, Descriptor* descriptor) {
  descriptor->SetSortedKeyIndex(GetSortedKeyIndex(index));
  Set(index, descriptor);
}

// static
void DescriptorArray::SetEnumCache(Handle<DescriptorArray> descriptors,
                                   Isolate* isolate, Handle<FixedArray> keys,
                                   Handle<FixedArray> indices) {
  EnumCache* enum_cache = descriptors->GetEnumCache();
  if (enum_cache == ReadOnlyRoots(isolate).empty_enum_cache()) {
    enum_cache = *isolate->factory()->NewEnumCache(keys, indices);
    descriptors->set(kEnumCacheIndex, MaybeObject::FromObject(enum_cache));
  } else {
    enum_cache->set_keys(*keys);
    enum_cache->set_indices(*indices);
  }
}

void DescriptorArray::CopyFrom(int index, DescriptorArray* src) {
  PropertyDetails details = src->GetDetails(index);
  Set(index, src->GetKey(index), src->GetValue(index), details);
}

void DescriptorArray::Sort() {
  // In-place heap sort.
  int len = number_of_descriptors();
  // Reset sorting since the descriptor array might contain invalid pointers.
  for (int i = 0; i < len; ++i) SetSortedKey(i, i);
  // Bottom-up max-heap construction.
  // Index of the last node with children
  const int max_parent_index = (len / 2) - 1;
  for (int i = max_parent_index; i >= 0; --i) {
    int parent_index = i;
    const uint32_t parent_hash = GetSortedKey(i)->Hash();
    while (parent_index <= max_parent_index) {
      int child_index = 2 * parent_index + 1;
      uint32_t child_hash = GetSortedKey(child_index)->Hash();
      if (child_index + 1 < len) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      SwapSortedKeys(parent_index, child_index);
      // Now element at child_index could be < its children.
      parent_index = child_index;  // parent_hash remains correct.
    }
  }

  // Extract elements and create sorted array.
  for (int i = len - 1; i > 0; --i) {
    // Put max element at the back of the array.
    SwapSortedKeys(0, i);
    // Shift down the new top element.
    int parent_index = 0;
    const uint32_t parent_hash = GetSortedKey(parent_index)->Hash();
    const int max_parent_index = (i / 2) - 1;
    while (parent_index <= max_parent_index) {
      int child_index = parent_index * 2 + 1;
      uint32_t child_hash = GetSortedKey(child_index)->Hash();
      if (child_index + 1 < i) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      SwapSortedKeys(parent_index, child_index);
      parent_index = child_index;
    }
  }
  DCHECK(IsSortedNoDuplicates());
}

Handle<AccessorPair> AccessorPair::Copy(Isolate* isolate,
                                        Handle<AccessorPair> pair) {
  Handle<AccessorPair> copy = isolate->factory()->NewAccessorPair();
  copy->set_getter(pair->getter());
  copy->set_setter(pair->setter());
  return copy;
}

Handle<Object> AccessorPair::GetComponent(Isolate* isolate,
                                          Handle<AccessorPair> accessor_pair,
                                          AccessorComponent component) {
  Object* accessor = accessor_pair->get(component);
  if (accessor->IsFunctionTemplateInfo()) {
    return ApiNatives::InstantiateFunction(
               handle(FunctionTemplateInfo::cast(accessor), isolate))
        .ToHandleChecked();
  }
  if (accessor->IsNull(isolate)) {
    return isolate->factory()->undefined_value();
  }
  return handle(accessor, isolate);
}

Handle<DeoptimizationData> DeoptimizationData::New(Isolate* isolate,
                                                   int deopt_entry_count,
                                                   PretenureFlag pretenure) {
  return Handle<DeoptimizationData>::cast(isolate->factory()->NewFixedArray(
      LengthFor(deopt_entry_count), pretenure));
}

Handle<DeoptimizationData> DeoptimizationData::Empty(Isolate* isolate) {
  return Handle<DeoptimizationData>::cast(
      isolate->factory()->empty_fixed_array());
}

SharedFunctionInfo* DeoptimizationData::GetInlinedFunction(int index) {
  if (index == -1) {
    return SharedFunctionInfo::cast(SharedFunctionInfo());
  } else {
    return SharedFunctionInfo::cast(LiteralArray()->get(index));
  }
}

#ifdef DEBUG
bool DescriptorArray::IsEqualTo(DescriptorArray* other) {
  if (length() != other->length()) return false;
  for (int i = 0; i < length(); ++i) {
    if (get(i) != other->get(i)) return false;
  }
  return true;
}
#endif

// static
Handle<String> String::Trim(Isolate* isolate, Handle<String> string,
                            TrimMode mode) {
  string = String::Flatten(isolate, string);
  int const length = string->length();

  // Perform left trimming if requested.
  int left = 0;
  UnicodeCache* unicode_cache = isolate->unicode_cache();
  if (mode == kTrim || mode == kTrimStart) {
    while (left < length &&
           unicode_cache->IsWhiteSpaceOrLineTerminator(string->Get(left))) {
      left++;
    }
  }

  // Perform right trimming if requested.
  int right = length;
  if (mode == kTrim || mode == kTrimEnd) {
    while (
        right > left &&
        unicode_cache->IsWhiteSpaceOrLineTerminator(string->Get(right - 1))) {
      right--;
    }
  }

  return isolate->factory()->NewSubString(string, left, right);
}

bool String::LooksValid() {
  // TODO(leszeks): Maybe remove this check entirely, Heap::Contains uses
  // basically the same logic as the way we access the heap in the first place.
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(this);
  // RO_SPACE objects should always be valid.
  if (chunk->owner()->identity() == RO_SPACE) return true;
  if (chunk->heap() == nullptr) return false;
  return chunk->heap()->Contains(this);
}

// static
MaybeHandle<String> Name::ToFunctionName(Isolate* isolate, Handle<Name> name) {
  if (name->IsString()) return Handle<String>::cast(name);
  // ES6 section 9.2.11 SetFunctionName, step 4.
  Handle<Object> description(Handle<Symbol>::cast(name)->name(), isolate);
  if (description->IsUndefined(isolate)) {
    return isolate->factory()->empty_string();
  }
  IncrementalStringBuilder builder(isolate);
  builder.AppendCharacter('[');
  builder.AppendString(Handle<String>::cast(description));
  builder.AppendCharacter(']');
  return builder.Finish();
}

// static
MaybeHandle<String> Name::ToFunctionName(Isolate* isolate, Handle<Name> name,
                                         Handle<String> prefix) {
  Handle<String> name_string;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name_string,
                             ToFunctionName(isolate, name), String);
  IncrementalStringBuilder builder(isolate);
  builder.AppendString(prefix);
  builder.AppendCharacter(' ');
  builder.AppendString(name_string);
  return builder.Finish();
}

namespace {

bool AreDigits(const uint8_t* s, int from, int to) {
  for (int i = from; i < to; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }

  return true;
}


int ParseDecimalInteger(const uint8_t* s, int from, int to) {
  DCHECK_LT(to - from, 10);  // Overflow is not possible.
  DCHECK(from < to);
  int d = s[from] - '0';

  for (int i = from + 1; i < to; i++) {
    d = 10 * d + (s[i] - '0');
  }

  return d;
}

}  // namespace

// static
Handle<Object> String::ToNumber(Isolate* isolate, Handle<String> subject) {
  // Flatten {subject} string first.
  subject = String::Flatten(isolate, subject);

  // Fast array index case.
  uint32_t index;
  if (subject->AsArrayIndex(&index)) {
    return isolate->factory()->NewNumberFromUint(index);
  }

  // Fast case: short integer or some sorts of junk values.
  if (subject->IsSeqOneByteString()) {
    int len = subject->length();
    if (len == 0) return handle(Smi::kZero, isolate);

    DisallowHeapAllocation no_gc;
    uint8_t const* data = Handle<SeqOneByteString>::cast(subject)->GetChars();
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return isolate->factory()->nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit
      // or the 'I' character ('Infinity'). All of that have codes not greater
      // than '9' except 'I' and &nbsp;.
      if (data[start_pos] != 'I' && data[start_pos] != 0xA0) {
        return isolate->factory()->nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits
      // we know it will fit into the smi-data type.
      int d = ParseDecimalInteger(data, start_pos, len);
      if (minus) {
        if (d == 0) return isolate->factory()->minus_zero_value();
        d = -d;
      } else if (!subject->HasHashCode() && len <= String::kMaxArrayIndexSize &&
                 (len == 1 || data[0] != '0')) {
        // String hash is not calculated yet but all the data are present.
        // Update the hash field to speed up sequential convertions.
        uint32_t hash = StringHasher::MakeArrayIndexHash(d, len);
#ifdef DEBUG
        subject->Hash();  // Force hash calculation.
        DCHECK_EQ(static_cast<int>(subject->hash_field()),
                  static_cast<int>(hash));
#endif
        subject->set_hash_field(hash);
      }
      return handle(Smi::FromInt(d), isolate);
    }
  }

  // Slower case.
  int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
  return isolate->factory()->NewNumber(
      StringToDouble(isolate, isolate->unicode_cache(), subject, flags));
}


String::FlatContent String::GetFlatContent() {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  int length = this->length();
  StringShape shape(this);
  String* string = this;
  int offset = 0;
  if (shape.representation_tag() == kConsStringTag) {
    ConsString* cons = ConsString::cast(string);
    if (cons->second()->length() != 0) {
      return FlatContent();
    }
    string = cons->first();
    shape = StringShape(string);
  } else if (shape.representation_tag() == kSlicedStringTag) {
    SlicedString* slice = SlicedString::cast(string);
    offset = slice->offset();
    string = slice->parent();
    shape = StringShape(string);
    DCHECK(shape.representation_tag() != kConsStringTag &&
           shape.representation_tag() != kSlicedStringTag);
  }
  if (shape.representation_tag() == kThinStringTag) {
    ThinString* thin = ThinString::cast(string);
    string = thin->actual();
    shape = StringShape(string);
    DCHECK(!shape.IsCons());
    DCHECK(!shape.IsSliced());
  }
  if (shape.encoding_tag() == kOneByteStringTag) {
    const uint8_t* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqOneByteString::cast(string)->GetChars();
    } else {
      start = ExternalOneByteString::cast(string)->GetChars();
    }
    return FlatContent(start + offset, length);
  } else {
    DCHECK_EQ(shape.encoding_tag(), kTwoByteStringTag);
    const uc16* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqTwoByteString::cast(string)->GetChars();
    } else {
      start = ExternalTwoByteString::cast(string)->GetChars();
    }
    return FlatContent(start + offset, length);
  }
}

std::unique_ptr<char[]> String::ToCString(AllowNullsFlag allow_nulls,
                                          RobustnessFlag robust_flag,
                                          int offset, int length,
                                          int* length_return) {
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return std::unique_ptr<char[]>();
  }
  // Negative length means the to the end of the string.
  if (length < 0) length = kMaxInt - offset;

  // Compute the size of the UTF-8 string. Start at the specified offset.
  StringCharacterStream stream(this, offset);
  int character_position = offset;
  int utf8_bytes = 0;
  int last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    utf8_bytes += unibrow::Utf8::Length(character, last);
    last = character;
  }

  if (length_return) {
    *length_return = utf8_bytes;
  }

  char* result = NewArray<char>(utf8_bytes + 1);

  // Convert the UTF-16 string to a UTF-8 buffer. Start at the specified offset.
  stream.Reset(this, offset);
  character_position = offset;
  int utf8_byte_position = 0;
  last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    if (allow_nulls == DISALLOW_NULLS && character == 0) {
      character = ' ';
    }
    utf8_byte_position +=
        unibrow::Utf8::Encode(result + utf8_byte_position, character, last);
    last = character;
  }
  result[utf8_byte_position] = 0;
  return std::unique_ptr<char[]>(result);
}

std::unique_ptr<char[]> String::ToCString(AllowNullsFlag allow_nulls,
                                          RobustnessFlag robust_flag,
                                          int* length_return) {
  return ToCString(allow_nulls, robust_flag, 0, -1, length_return);
}


void Relocatable::PostGarbageCollectionProcessing(Isolate* isolate) {
  Relocatable* current = isolate->relocatable_top();
  while (current != nullptr) {
    current->PostGarbageCollection();
    current = current->prev_;
  }
}


// Reserve space for statics needing saving and restoring.
int Relocatable::ArchiveSpacePerThread() {
  return sizeof(Relocatable*);  // NOLINT
}


// Archive statics that are thread-local.
char* Relocatable::ArchiveState(Isolate* isolate, char* to) {
  *reinterpret_cast<Relocatable**>(to) = isolate->relocatable_top();
  isolate->set_relocatable_top(nullptr);
  return to + ArchiveSpacePerThread();
}


// Restore statics that are thread-local.
char* Relocatable::RestoreState(Isolate* isolate, char* from) {
  isolate->set_relocatable_top(*reinterpret_cast<Relocatable**>(from));
  return from + ArchiveSpacePerThread();
}

char* Relocatable::Iterate(RootVisitor* v, char* thread_storage) {
  Relocatable* top = *reinterpret_cast<Relocatable**>(thread_storage);
  Iterate(v, top);
  return thread_storage + ArchiveSpacePerThread();
}

void Relocatable::Iterate(Isolate* isolate, RootVisitor* v) {
  Iterate(v, isolate->relocatable_top());
}

void Relocatable::Iterate(RootVisitor* v, Relocatable* top) {
  Relocatable* current = top;
  while (current != nullptr) {
    current->IterateInstance(v);
    current = current->prev_;
  }
}


FlatStringReader::FlatStringReader(Isolate* isolate, Handle<String> str)
    : Relocatable(isolate),
      str_(str.location()),
      length_(str->length()) {
  PostGarbageCollection();
}

FlatStringReader::FlatStringReader(Isolate* isolate, Vector<const char> input)
    : Relocatable(isolate),
      str_(nullptr),
      is_one_byte_(true),
      length_(input.length()),
      start_(input.start()) {}

void FlatStringReader::PostGarbageCollection() {
  if (str_ == nullptr) return;
  Handle<String> str(str_);
  DCHECK(str->IsFlat());
  DisallowHeapAllocation no_gc;
  // This does not actually prevent the vector from being relocated later.
  String::FlatContent content = str->GetFlatContent();
  DCHECK(content.IsFlat());
  is_one_byte_ = content.IsOneByte();
  if (is_one_byte_) {
    start_ = content.ToOneByteVector().start();
  } else {
    start_ = content.ToUC16Vector().start();
  }
}


void ConsStringIterator::Initialize(ConsString* cons_string, int offset) {
  DCHECK_NOT_NULL(cons_string);
  root_ = cons_string;
  consumed_ = offset;
  // Force stack blown condition to trigger restart.
  depth_ = 1;
  maximum_depth_ = kStackSize + depth_;
  DCHECK(StackBlown());
}


String* ConsStringIterator::Continue(int* offset_out) {
  DCHECK_NE(depth_, 0);
  DCHECK_EQ(0, *offset_out);
  bool blew_stack = StackBlown();
  String* string = nullptr;
  // Get the next leaf if there is one.
  if (!blew_stack) string = NextLeaf(&blew_stack);
  // Restart search from root.
  if (blew_stack) {
    DCHECK_NULL(string);
    string = Search(offset_out);
  }
  // Ensure future calls return null immediately.
  if (string == nullptr) Reset(nullptr);
  return string;
}


String* ConsStringIterator::Search(int* offset_out) {
  ConsString* cons_string = root_;
  // Reset the stack, pushing the root string.
  depth_ = 1;
  maximum_depth_ = 1;
  frames_[0] = cons_string;
  const int consumed = consumed_;
  int offset = 0;
  while (true) {
    // Loop until the string is found which contains the target offset.
    String* string = cons_string->first();
    int length = string->length();
    int32_t type;
    if (consumed < offset + length) {
      // Target offset is in the left branch.
      // Keep going if we're still in a ConString.
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushLeft(cons_string);
        continue;
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
    } else {
      // Descend right.
      // Update progress through the string.
      offset += length;
      // Keep going if we're still in a ConString.
      string = cons_string->second();
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushRight(cons_string);
        continue;
      }
      // Need this to be updated for the current string.
      length = string->length();
      // Account for the possibility of an empty right leaf.
      // This happens only if we have asked for an offset outside the string.
      if (length == 0) {
        // Reset so future operations will return null immediately.
        Reset(nullptr);
        return nullptr;
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
      // Pop stack so next iteration is in correct place.
      Pop();
    }
    DCHECK_NE(length, 0);
    // Adjust return values and exit.
    consumed_ = offset + length;
    *offset_out = consumed - offset;
    return string;
  }
  UNREACHABLE();
}


String* ConsStringIterator::NextLeaf(bool* blew_stack) {
  while (true) {
    // Tree traversal complete.
    if (depth_ == 0) {
      *blew_stack = false;
      return nullptr;
    }
    // We've lost track of higher nodes.
    if (StackBlown()) {
      *blew_stack = true;
      return nullptr;
    }
    // Go right.
    ConsString* cons_string = frames_[OffsetForDepth(depth_ - 1)];
    String* string = cons_string->second();
    int32_t type = string->map()->instance_type();
    if ((type & kStringRepresentationMask) != kConsStringTag) {
      // Pop stack so next iteration is in correct place.
      Pop();
      int length = string->length();
      // Could be a flattened ConsString.
      if (length == 0) continue;
      consumed_ += length;
      return string;
    }
    cons_string = ConsString::cast(string);
    PushRight(cons_string);
    // Need to traverse all the way left.
    while (true) {
      // Continue left.
      string = cons_string->first();
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) != kConsStringTag) {
        AdjustMaximumDepth();
        int length = string->length();
        if (length == 0) break;  // Skip empty left-hand sides of ConsStrings.
        consumed_ += length;
        return string;
      }
      cons_string = ConsString::cast(string);
      PushLeft(cons_string);
    }
  }
  UNREACHABLE();
}


uint16_t ConsString::ConsStringGet(int index) {
  DCHECK(index >= 0 && index < this->length());

  // Check for a flattened cons string
  if (second()->length() == 0) {
    String* left = first();
    return left->Get(index);
  }

  String* string = String::cast(this);

  while (true) {
    if (StringShape(string).IsCons()) {
      ConsString* cons_string = ConsString::cast(string);
      String* left = cons_string->first();
      if (left->length() > index) {
        string = left;
      } else {
        index -= left->length();
        string = cons_string->second();
      }
    } else {
      return string->Get(index);
    }
  }

  UNREACHABLE();
}

uint16_t ThinString::ThinStringGet(int index) { return actual()->Get(index); }

uint16_t SlicedString::SlicedStringGet(int index) {
  return parent()->Get(offset() + index);
}


template <typename sinkchar>
void String::WriteToFlat(String* src,
                         sinkchar* sink,
                         int f,
                         int t) {
  String* source = src;
  int from = f;
  int to = t;
  while (true) {
    DCHECK(0 <= from && from <= to && to <= source->length());
    switch (StringShape(source).full_representation_tag()) {
      case kOneByteStringTag | kExternalStringTag: {
        CopyChars(sink, ExternalOneByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kExternalStringTag: {
        const uc16* data =
            ExternalTwoByteString::cast(source)->GetChars();
        CopyChars(sink,
                  data + from,
                  to - from);
        return;
      }
      case kOneByteStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqOneByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqTwoByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kOneByteStringTag | kConsStringTag:
      case kTwoByteStringTag | kConsStringTag: {
        ConsString* cons_string = ConsString::cast(source);
        String* first = cons_string->first();
        int boundary = first->length();
        if (to - boundary >= boundary - from) {
          // Right hand side is longer.  Recurse over left.
          if (from < boundary) {
            WriteToFlat(first, sink, from, boundary);
            if (from == 0 && cons_string->second() == first) {
              CopyChars(sink + boundary, sink, boundary);
              return;
            }
            sink += boundary - from;
            from = 0;
          } else {
            from -= boundary;
          }
          to -= boundary;
          source = cons_string->second();
        } else {
          // Left hand side is longer.  Recurse over right.
          if (to > boundary) {
            String* second = cons_string->second();
            // When repeatedly appending to a string, we get a cons string that
            // is unbalanced to the left, a list, essentially.  We inline the
            // common case of sequential one-byte right child.
            if (to - boundary == 1) {
              sink[boundary - from] = static_cast<sinkchar>(second->Get(0));
            } else if (second->IsSeqOneByteString()) {
              CopyChars(sink + boundary - from,
                        SeqOneByteString::cast(second)->GetChars(),
                        to - boundary);
            } else {
              WriteToFlat(second,
                          sink + boundary - from,
                          0,
                          to - boundary);
            }
            to = boundary;
          }
          source = first;
        }
        break;
      }
      case kOneByteStringTag | kSlicedStringTag:
      case kTwoByteStringTag | kSlicedStringTag: {
        SlicedString* slice = SlicedString::cast(source);
        unsigned offset = slice->offset();
        WriteToFlat(slice->parent(), sink, from + offset, to + offset);
        return;
      }
      case kOneByteStringTag | kThinStringTag:
      case kTwoByteStringTag | kThinStringTag:
        source = ThinString::cast(source)->actual();
        break;
    }
  }
}

template <typename SourceChar>
static void CalculateLineEndsImpl(Isolate* isolate, std::vector<int>* line_ends,
                                  Vector<const SourceChar> src,
                                  bool include_ending_line) {
  const int src_len = src.length();
  UnicodeCache* cache = isolate->unicode_cache();
  for (int i = 0; i < src_len - 1; i++) {
    SourceChar current = src[i];
    SourceChar next = src[i + 1];
    if (cache->IsLineTerminatorSequence(current, next)) line_ends->push_back(i);
  }

  if (src_len > 0 && cache->IsLineTerminatorSequence(src[src_len - 1], 0)) {
    line_ends->push_back(src_len - 1);
  }
  if (include_ending_line) {
    // Include one character beyond the end of script. The rewriter uses that
    // position for the implicit return statement.
    line_ends->push_back(src_len);
  }
}

Handle<FixedArray> String::CalculateLineEnds(Isolate* isolate,
                                             Handle<String> src,
                                             bool include_ending_line) {
  src = Flatten(isolate, src);
  // Rough estimate of line count based on a roughly estimated average
  // length of (unpacked) code.
  int line_count_estimate = src->length() >> 4;
  std::vector<int> line_ends;
  line_ends.reserve(line_count_estimate);
  { DisallowHeapAllocation no_allocation;  // ensure vectors stay valid.
    // Dispatch on type of strings.
    String::FlatContent content = src->GetFlatContent();
    DCHECK(content.IsFlat());
    if (content.IsOneByte()) {
      CalculateLineEndsImpl(isolate,
                            &line_ends,
                            content.ToOneByteVector(),
                            include_ending_line);
    } else {
      CalculateLineEndsImpl(isolate,
                            &line_ends,
                            content.ToUC16Vector(),
                            include_ending_line);
    }
  }
  int line_count = static_cast<int>(line_ends.size());
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(line_count);
  for (int i = 0; i < line_count; i++) {
    array->set(i, Smi::FromInt(line_ends[i]));
  }
  return array;
}


// Compares the contents of two strings by reading and comparing
// int-sized blocks of characters.
template <typename Char>
static inline bool CompareRawStringContents(const Char* const a,
                                            const Char* const b,
                                            int length) {
  return CompareChars(a, b, length) == 0;
}


template<typename Chars1, typename Chars2>
class RawStringComparator : public AllStatic {
 public:
  static inline bool compare(const Chars1* a, const Chars2* b, int len) {
    DCHECK(sizeof(Chars1) != sizeof(Chars2));
    for (int i = 0; i < len; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }
};


template<>
class RawStringComparator<uint16_t, uint16_t> {
 public:
  static inline bool compare(const uint16_t* a, const uint16_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};


template<>
class RawStringComparator<uint8_t, uint8_t> {
 public:
  static inline bool compare(const uint8_t* a, const uint8_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};


class StringComparator {
  class State {
   public:
    State() : is_one_byte_(true), length_(0), buffer8_(nullptr) {}

    void Init(String* string) {
      ConsString* cons_string = String::VisitFlat(this, string);
      iter_.Reset(cons_string);
      if (cons_string != nullptr) {
        int offset;
        string = iter_.Next(&offset);
        String::VisitFlat(this, string, offset);
      }
    }

    inline void VisitOneByteString(const uint8_t* chars, int length) {
      is_one_byte_ = true;
      buffer8_ = chars;
      length_ = length;
    }

    inline void VisitTwoByteString(const uint16_t* chars, int length) {
      is_one_byte_ = false;
      buffer16_ = chars;
      length_ = length;
    }

    void Advance(int consumed) {
      DCHECK(consumed <= length_);
      // Still in buffer.
      if (length_ != consumed) {
        if (is_one_byte_) {
          buffer8_ += consumed;
        } else {
          buffer16_ += consumed;
        }
        length_ -= consumed;
        return;
      }
      // Advance state.
      int offset;
      String* next = iter_.Next(&offset);
      DCHECK_EQ(0, offset);
      DCHECK_NOT_NULL(next);
      String::VisitFlat(this, next);
    }

    ConsStringIterator iter_;
    bool is_one_byte_;
    int length_;
    union {
      const uint8_t* buffer8_;
      const uint16_t* buffer16_;
    };

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

 public:
  inline StringComparator() = default;

  template<typename Chars1, typename Chars2>
  static inline bool Equals(State* state_1, State* state_2, int to_check) {
    const Chars1* a = reinterpret_cast<const Chars1*>(state_1->buffer8_);
    const Chars2* b = reinterpret_cast<const Chars2*>(state_2->buffer8_);
    return RawStringComparator<Chars1, Chars2>::compare(a, b, to_check);
  }

  bool Equals(String* string_1, String* string_2) {
    int length = string_1->length();
    state_1_.Init(string_1);
    state_2_.Init(string_2);
    while (true) {
      int to_check = Min(state_1_.length_, state_2_.length_);
      DCHECK(to_check > 0 && to_check <= length);
      bool is_equal;
      if (state_1_.is_one_byte_) {
        if (state_2_.is_one_byte_) {
          is_equal = Equals<uint8_t, uint8_t>(&state_1_, &state_2_, to_check);
        } else {
          is_equal = Equals<uint8_t, uint16_t>(&state_1_, &state_2_, to_check);
        }
      } else {
        if (state_2_.is_one_byte_) {
          is_equal = Equals<uint16_t, uint8_t>(&state_1_, &state_2_, to_check);
        } else {
          is_equal = Equals<uint16_t, uint16_t>(&state_1_, &state_2_, to_check);
        }
      }
      // Looping done.
      if (!is_equal) return false;
      length -= to_check;
      // Exit condition. Strings are equal.
      if (length == 0) return true;
      state_1_.Advance(to_check);
      state_2_.Advance(to_check);
    }
  }

 private:
  State state_1_;
  State state_2_;

  DISALLOW_COPY_AND_ASSIGN(StringComparator);
};


bool String::SlowEquals(String* other) {
  DisallowHeapAllocation no_gc;
  // Fast check: negative check with lengths.
  int len = length();
  if (len != other->length()) return false;
  if (len == 0) return true;

  // Fast check: if at least one ThinString is involved, dereference it/them
  // and restart.
  if (this->IsThinString() || other->IsThinString()) {
    if (other->IsThinString()) other = ThinString::cast(other)->actual();
    if (this->IsThinString()) {
      return ThinString::cast(this)->actual()->Equals(other);
    } else {
      return this->Equals(other);
    }
  }

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (HasHashCode() && other->HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (Hash() != other->Hash()) {
        bool found_difference = false;
        for (int i = 0; i < len; i++) {
          if (Get(i) != other->Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (Hash() != other->Hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (this->Get(0) != other->Get(0)) return false;

  if (IsSeqOneByteString() && other->IsSeqOneByteString()) {
    const uint8_t* str1 = SeqOneByteString::cast(this)->GetChars();
    const uint8_t* str2 = SeqOneByteString::cast(other)->GetChars();
    return CompareRawStringContents(str1, str2, len);
  }

  StringComparator comparator;
  return comparator.Equals(this, other);
}

bool String::SlowEquals(Isolate* isolate, Handle<String> one,
                        Handle<String> two) {
  // Fast check: negative check with lengths.
  int one_length = one->length();
  if (one_length != two->length()) return false;
  if (one_length == 0) return true;

  // Fast check: if at least one ThinString is involved, dereference it/them
  // and restart.
  if (one->IsThinString() || two->IsThinString()) {
    if (one->IsThinString())
      one = handle(ThinString::cast(*one)->actual(), isolate);
    if (two->IsThinString())
      two = handle(ThinString::cast(*two)->actual(), isolate);
    return String::Equals(isolate, one, two);
  }

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (one->HasHashCode() && two->HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (one->Hash() != two->Hash()) {
        bool found_difference = false;
        for (int i = 0; i < one_length; i++) {
          if (one->Get(i) != two->Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (one->Hash() != two->Hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (one->Get(0) != two->Get(0)) return false;

  one = String::Flatten(isolate, one);
  two = String::Flatten(isolate, two);

  DisallowHeapAllocation no_gc;
  String::FlatContent flat1 = one->GetFlatContent();
  String::FlatContent flat2 = two->GetFlatContent();

  if (flat1.IsOneByte() && flat2.IsOneByte()) {
      return CompareRawStringContents(flat1.ToOneByteVector().start(),
                                      flat2.ToOneByteVector().start(),
                                      one_length);
  } else {
    for (int i = 0; i < one_length; i++) {
      if (flat1.Get(i) != flat2.Get(i)) return false;
    }
    return true;
  }
}


// static
ComparisonResult String::Compare(Isolate* isolate, Handle<String> x,
                                 Handle<String> y) {
  // A few fast case tests before we flatten.
  if (x.is_identical_to(y)) {
    return ComparisonResult::kEqual;
  } else if (y->length() == 0) {
    return x->length() == 0 ? ComparisonResult::kEqual
                            : ComparisonResult::kGreaterThan;
  } else if (x->length() == 0) {
    return ComparisonResult::kLessThan;
  }

  int const d = x->Get(0) - y->Get(0);
  if (d < 0) {
    return ComparisonResult::kLessThan;
  } else if (d > 0) {
    return ComparisonResult::kGreaterThan;
  }

  // Slow case.
  x = String::Flatten(isolate, x);
  y = String::Flatten(isolate, y);

  DisallowHeapAllocation no_gc;
  ComparisonResult result = ComparisonResult::kEqual;
  int prefix_length = x->length();
  if (y->length() < prefix_length) {
    prefix_length = y->length();
    result = ComparisonResult::kGreaterThan;
  } else if (y->length() > prefix_length) {
    result = ComparisonResult::kLessThan;
  }
  int r;
  String::FlatContent x_content = x->GetFlatContent();
  String::FlatContent y_content = y->GetFlatContent();
  if (x_content.IsOneByte()) {
    Vector<const uint8_t> x_chars = x_content.ToOneByteVector();
    if (y_content.IsOneByte()) {
      Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  } else {
    Vector<const uc16> x_chars = x_content.ToUC16Vector();
    if (y_content.IsOneByte()) {
      Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  }
  if (r < 0) {
    result = ComparisonResult::kLessThan;
  } else if (r > 0) {
    result = ComparisonResult::kGreaterThan;
  }
  return result;
}

Object* String::IndexOf(Isolate* isolate, Handle<Object> receiver,
                        Handle<Object> search, Handle<Object> position) {
  if (receiver->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "String.prototype.indexOf")));
  }
  Handle<String> receiver_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_string,
                                     Object::ToString(isolate, receiver));

  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                     Object::ToInteger(isolate, position));

  uint32_t index = receiver_string->ToValidIndex(*position);
  return Smi::FromInt(
      String::IndexOf(isolate, receiver_string, search_string, index));
}

namespace {

template <typename T>
int SearchString(Isolate* isolate, String::FlatContent receiver_content,
                 Vector<T> pat_vector, int start_index) {
  if (receiver_content.IsOneByte()) {
    return SearchString(isolate, receiver_content.ToOneByteVector(), pat_vector,
                        start_index);
  }
  return SearchString(isolate, receiver_content.ToUC16Vector(), pat_vector,
                      start_index);
}

}  // namespace

int String::IndexOf(Isolate* isolate, Handle<String> receiver,
                    Handle<String> search, int start_index) {
  DCHECK_LE(0, start_index);
  DCHECK(start_index <= receiver->length());

  uint32_t search_length = search->length();
  if (search_length == 0) return start_index;

  uint32_t receiver_length = receiver->length();
  if (start_index + search_length > receiver_length) return -1;

  receiver = String::Flatten(isolate, receiver);
  search = String::Flatten(isolate, search);

  DisallowHeapAllocation no_gc;  // ensure vectors stay valid
  // Extract flattened substrings of cons strings before getting encoding.
  String::FlatContent receiver_content = receiver->GetFlatContent();
  String::FlatContent search_content = search->GetFlatContent();

  // dispatch on type of strings
  if (search_content.IsOneByte()) {
    Vector<const uint8_t> pat_vector = search_content.ToOneByteVector();
    return SearchString<const uint8_t>(isolate, receiver_content, pat_vector,
                                       start_index);
  }
  Vector<const uc16> pat_vector = search_content.ToUC16Vector();
  return SearchString<const uc16>(isolate, receiver_content, pat_vector,
                                  start_index);
}

MaybeHandle<String> String::GetSubstitution(Isolate* isolate, Match* match,
                                            Handle<String> replacement,
                                            int start_index) {
  DCHECK_GE(start_index, 0);

  Factory* factory = isolate->factory();

  const int replacement_length = replacement->length();
  const int captures_length = match->CaptureCount();

  replacement = String::Flatten(isolate, replacement);

  Handle<String> dollar_string =
      factory->LookupSingleCharacterStringFromCode('$');
  int next_dollar_ix =
      String::IndexOf(isolate, replacement, dollar_string, start_index);
  if (next_dollar_ix < 0) {
    return replacement;
  }

  IncrementalStringBuilder builder(isolate);

  if (next_dollar_ix > 0) {
    builder.AppendString(factory->NewSubString(replacement, 0, next_dollar_ix));
  }

  while (true) {
    const int peek_ix = next_dollar_ix + 1;
    if (peek_ix >= replacement_length) {
      builder.AppendCharacter('$');
      return builder.Finish();
    }

    int continue_from_ix = -1;
    const uint16_t peek = replacement->Get(peek_ix);
    switch (peek) {
      case '$':  // $$
        builder.AppendCharacter('$');
        continue_from_ix = peek_ix + 1;
        break;
      case '&':  // $& - match
        builder.AppendString(match->GetMatch());
        continue_from_ix = peek_ix + 1;
        break;
      case '`':  // $` - prefix
        builder.AppendString(match->GetPrefix());
        continue_from_ix = peek_ix + 1;
        break;
      case '\'':  // $' - suffix
        builder.AppendString(match->GetSuffix());
        continue_from_ix = peek_ix + 1;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        // Valid indices are $1 .. $9, $01 .. $09 and $10 .. $99
        int scaled_index = (peek - '0');
        int advance = 1;

        if (peek_ix + 1 < replacement_length) {
          const uint16_t next_peek = replacement->Get(peek_ix + 1);
          if (next_peek >= '0' && next_peek <= '9') {
            const int new_scaled_index = scaled_index * 10 + (next_peek - '0');
            if (new_scaled_index < captures_length) {
              scaled_index = new_scaled_index;
              advance = 2;
            }
          }
        }

        if (scaled_index == 0 || scaled_index >= captures_length) {
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        bool capture_exists;
        Handle<String> capture;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, capture, match->GetCapture(scaled_index, &capture_exists),
            String);
        if (capture_exists) builder.AppendString(capture);
        continue_from_ix = peek_ix + advance;
        break;
      }
      case '<': {  // $<name> - named capture
        typedef String::Match::CaptureState CaptureState;

        if (!match->HasNamedCaptures()) {
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        Handle<String> bracket_string =
            factory->LookupSingleCharacterStringFromCode('>');
        const int closing_bracket_ix =
            String::IndexOf(isolate, replacement, bracket_string, peek_ix + 1);

        if (closing_bracket_ix == -1) {
          // No closing bracket was found, treat '$<' as a string literal.
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        Handle<String> capture_name =
            factory->NewSubString(replacement, peek_ix + 1, closing_bracket_ix);
        Handle<String> capture;
        CaptureState capture_state;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, capture,
            match->GetNamedCapture(capture_name, &capture_state), String);

        switch (capture_state) {
          case CaptureState::INVALID:
          case CaptureState::UNMATCHED:
            break;
          case CaptureState::MATCHED:
            builder.AppendString(capture);
            break;
        }

        continue_from_ix = closing_bracket_ix + 1;
        break;
      }
      default:
        builder.AppendCharacter('$');
        continue_from_ix = peek_ix;
        break;
    }

    // Go the the next $ in the replacement.
    // TODO(jgruber): Single-char lookups could be much more efficient.
    DCHECK_NE(continue_from_ix, -1);
    next_dollar_ix =
        String::IndexOf(isolate, replacement, dollar_string, continue_from_ix);

    // Return if there are no more $ characters in the replacement. If we
    // haven't reached the end, we need to append the suffix.
    if (next_dollar_ix < 0) {
      if (continue_from_ix < replacement_length) {
        builder.AppendString(factory->NewSubString(
            replacement, continue_from_ix, replacement_length));
      }
      return builder.Finish();
    }

    // Append substring between the previous and the next $ character.
    if (next_dollar_ix > continue_from_ix) {
      builder.AppendString(
          factory->NewSubString(replacement, continue_from_ix, next_dollar_ix));
    }
  }

  UNREACHABLE();
}

namespace {  // for String.Prototype.lastIndexOf

template <typename schar, typename pchar>
int StringMatchBackwards(Vector<const schar> subject,
                         Vector<const pchar> pattern, int idx) {
  int pattern_length = pattern.length();
  DCHECK_GE(pattern_length, 1);
  DCHECK(idx + pattern_length <= subject.length());

  if (sizeof(schar) == 1 && sizeof(pchar) > 1) {
    for (int i = 0; i < pattern_length; i++) {
      uc16 c = pattern[i];
      if (c > String::kMaxOneByteCharCode) {
        return -1;
      }
    }
  }

  pchar pattern_first_char = pattern[0];
  for (int i = idx; i >= 0; i--) {
    if (subject[i] != pattern_first_char) continue;
    int j = 1;
    while (j < pattern_length) {
      if (pattern[j] != subject[i + j]) {
        break;
      }
      j++;
    }
    if (j == pattern_length) {
      return i;
    }
  }
  return -1;
}

}  // namespace

Object* String::LastIndexOf(Isolate* isolate, Handle<Object> receiver,
                            Handle<Object> search, Handle<Object> position) {
  if (receiver->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "String.prototype.lastIndexOf")));
  }
  Handle<String> receiver_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_string,
                                     Object::ToString(isolate, receiver));

  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                     Object::ToNumber(isolate, position));

  uint32_t start_index;

  if (position->IsNaN()) {
    start_index = receiver_string->length();
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                       Object::ToInteger(isolate, position));
    start_index = receiver_string->ToValidIndex(*position);
  }

  uint32_t pattern_length = search_string->length();
  uint32_t receiver_length = receiver_string->length();

  if (start_index + pattern_length > receiver_length) {
    start_index = receiver_length - pattern_length;
  }

  if (pattern_length == 0) {
    return Smi::FromInt(start_index);
  }

  receiver_string = String::Flatten(isolate, receiver_string);
  search_string = String::Flatten(isolate, search_string);

  int last_index = -1;
  DisallowHeapAllocation no_gc;  // ensure vectors stay valid

  String::FlatContent receiver_content = receiver_string->GetFlatContent();
  String::FlatContent search_content = search_string->GetFlatContent();

  if (search_content.IsOneByte()) {
    Vector<const uint8_t> pat_vector = search_content.ToOneByteVector();
    if (receiver_content.IsOneByte()) {
      last_index = StringMatchBackwards(receiver_content.ToOneByteVector(),
                                        pat_vector, start_index);
    } else {
      last_index = StringMatchBackwards(receiver_content.ToUC16Vector(),
                                        pat_vector, start_index);
    }
  } else {
    Vector<const uc16> pat_vector = search_content.ToUC16Vector();
    if (receiver_content.IsOneByte()) {
      last_index = StringMatchBackwards(receiver_content.ToOneByteVector(),
                                        pat_vector, start_index);
    } else {
      last_index = StringMatchBackwards(receiver_content.ToUC16Vector(),
                                        pat_vector, start_index);
    }
  }
  return Smi::FromInt(last_index);
}

bool String::IsUtf8EqualTo(Vector<const char> str, bool allow_prefix_match) {
  int slen = length();
  // Can't check exact length equality, but we can check bounds.
  int str_len = str.length();
  if (!allow_prefix_match &&
      (str_len < slen ||
          str_len > slen*static_cast<int>(unibrow::Utf8::kMaxEncodedSize))) {
    return false;
  }

  int i = 0;
  unibrow::Utf8Iterator it = unibrow::Utf8Iterator(str);
  while (i < slen && !it.Done()) {
    if (Get(i++) != *it) return false;
    ++it;
  }

  return (allow_prefix_match || i == slen) && it.Done();
}

template <>
bool String::IsEqualTo(Vector<const uint8_t> str) {
  return IsOneByteEqualTo(str);
}

template <>
bool String::IsEqualTo(Vector<const uc16> str) {
  return IsTwoByteEqualTo(str);
}

bool String::IsOneByteEqualTo(Vector<const uint8_t> str) {
  int slen = length();
  if (str.length() != slen) return false;
  DisallowHeapAllocation no_gc;
  FlatContent content = GetFlatContent();
  if (content.IsOneByte()) {
    return CompareChars(content.ToOneByteVector().start(),
                        str.start(), slen) == 0;
  }
  for (int i = 0; i < slen; i++) {
    if (Get(i) != static_cast<uint16_t>(str[i])) return false;
  }
  return true;
}


bool String::IsTwoByteEqualTo(Vector<const uc16> str) {
  int slen = length();
  if (str.length() != slen) return false;
  DisallowHeapAllocation no_gc;
  FlatContent content = GetFlatContent();
  if (content.IsTwoByte()) {
    return CompareChars(content.ToUC16Vector().start(), str.start(), slen) == 0;
  }
  for (int i = 0; i < slen; i++) {
    if (Get(i) != str[i]) return false;
  }
  return true;
}

uint32_t String::ComputeAndSetHash(Isolate* isolate) {
  // Should only be called if hash code has not yet been computed.
  DCHECK(!HasHashCode());

  // Store the hash code in the object.
  uint32_t field =
      IteratingStringHasher::Hash(this, isolate->heap()->HashSeed());
  set_hash_field(field);

  // Check the hash code is there.
  DCHECK(HasHashCode());
  uint32_t result = field >> kHashShift;
  DCHECK_NE(result, 0);  // Ensure that the hash value of 0 is never computed.
  return result;
}


bool String::ComputeArrayIndex(uint32_t* index) {
  int length = this->length();
  if (length == 0 || length > kMaxArrayIndexSize) return false;
  StringCharacterStream stream(this);
  return StringToArrayIndex(&stream, index);
}


bool String::SlowAsArrayIndex(uint32_t* index) {
  if (length() <= kMaxCachedArrayIndexLength) {
    Hash();  // force computation of hash code
    uint32_t field = hash_field();
    if ((field & kIsNotArrayIndexMask) != 0) return false;
    // Isolate the array index form the full hash field.
    *index = ArrayIndexValueBits::decode(field);
    return true;
  } else {
    return ComputeArrayIndex(index);
  }
}


Handle<String> SeqString::Truncate(Handle<SeqString> string, int new_length) {
  if (new_length == 0) return string->GetReadOnlyRoots().empty_string_handle();

  int new_size, old_size;
  int old_length = string->length();
  if (old_length <= new_length) return string;

  if (string->IsSeqOneByteString()) {
    old_size = SeqOneByteString::SizeFor(old_length);
    new_size = SeqOneByteString::SizeFor(new_length);
  } else {
    DCHECK(string->IsSeqTwoByteString());
    old_size = SeqTwoByteString::SizeFor(old_length);
    new_size = SeqTwoByteString::SizeFor(new_length);
  }

  int delta = old_size - new_size;

  Address start_of_string = string->address();
  DCHECK_OBJECT_ALIGNED(start_of_string);
  DCHECK_OBJECT_ALIGNED(start_of_string + new_size);

  Heap* heap = Heap::FromWritableHeapObject(*string);
  // Sizes are pointer size aligned, so that we can use filler objects
  // that are a multiple of pointer size.
  heap->CreateFillerObjectAt(start_of_string + new_size, delta,
                             ClearRecordedSlots::kNo);
  // We are storing the new length using release store after creating a filler
  // for the left-over space to avoid races with the sweeper thread.
  string->synchronized_set_length(new_length);

  return string;
}

void SeqOneByteString::clear_padding() {
  int data_size = SeqString::kHeaderSize + length() * kOneByteSize;
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

void SeqTwoByteString::clear_padding() {
  int data_size = SeqString::kHeaderSize + length() * kUC16Size;
  memset(reinterpret_cast<void*>(address() + data_size), 0,
         SizeFor(length()) - data_size);
}

int ExternalString::ExternalPayloadSize() const {
  int length_multiplier = IsTwoByteRepresentation() ? i::kShortSize : kCharSize;
  return length() * length_multiplier;
}

uint32_t StringHasher::MakeArrayIndexHash(uint32_t value, int length) {
  // For array indexes mix the length into the hash as an array index could
  // be zero.
  DCHECK_GT(length, 0);
  DCHECK_LE(length, String::kMaxArrayIndexSize);
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));

  value <<= String::ArrayIndexValueBits::kShift;
  value |= length << String::ArrayIndexLengthBits::kShift;

  DCHECK_EQ(value & String::kIsNotArrayIndexMask, 0);
  DCHECK_EQ(length <= String::kMaxCachedArrayIndexLength,
            Name::ContainsCachedArrayIndex(value));
  return value;
}


uint32_t StringHasher::GetHashField() {
  if (length_ <= String::kMaxHashCalcLength) {
    if (is_array_index_) {
      return MakeArrayIndexHash(array_index_, length_);
    }
    return (GetHashCore(raw_running_hash_) << String::kHashShift) |
           String::kIsNotArrayIndexMask;
  } else {
    return (length_ << String::kHashShift) | String::kIsNotArrayIndexMask;
  }
}

uint32_t StringHasher::ComputeUtf8Hash(Vector<const char> chars, uint64_t seed,
                                       int* utf16_length_out) {
  int vector_length = chars.length();
  // Handle some edge cases
  if (vector_length <= 1) {
    DCHECK(vector_length == 0 ||
           static_cast<uint8_t>(chars.start()[0]) <=
               unibrow::Utf8::kMaxOneByteChar);
    *utf16_length_out = vector_length;
    return HashSequentialString(chars.start(), vector_length, seed);
  }

  // Start with a fake length which won't affect computation.
  // It will be updated later.
  StringHasher hasher(String::kMaxArrayIndexSize, seed);
  DCHECK(hasher.is_array_index_);

  unibrow::Utf8Iterator it = unibrow::Utf8Iterator(chars);
  int utf16_length = 0;
  bool is_index = true;

  while (utf16_length < String::kMaxHashCalcLength && !it.Done()) {
    utf16_length++;
    uint16_t c = *it;
    ++it;
    hasher.AddCharacter(c);
    if (is_index) is_index = hasher.UpdateIndex(c);
  }

  // Now that hashing is done, we just need to calculate utf16_length
  while (!it.Done()) {
    ++it;
    utf16_length++;
  }

  *utf16_length_out = utf16_length;
  // Must set length here so that hash computation is correct.
  hasher.length_ = utf16_length;
  return hasher.GetHashField();
}


void IteratingStringHasher::VisitConsString(ConsString* cons_string) {
  // Run small ConsStrings through ConsStringIterator.
  if (cons_string->length() < 64) {
    ConsStringIterator iter(cons_string);
    int offset;
    String* string;
    while (nullptr != (string = iter.Next(&offset))) {
      DCHECK_EQ(0, offset);
      String::VisitFlat(this, string, 0);
    }
    return;
  }
  // Slow case.
  const int max_length = String::kMaxHashCalcLength;
  int length = std::min(cons_string->length(), max_length);
  if (cons_string->HasOnlyOneByteChars()) {
    uint8_t* buffer = new uint8_t[length];
    String::WriteToFlat(cons_string, buffer, 0, length);
    AddCharacters(buffer, length);
    delete[] buffer;
  } else {
    uint16_t* buffer = new uint16_t[length];
    String::WriteToFlat(cons_string, buffer, 0, length);
    AddCharacters(buffer, length);
    delete[] buffer;
  }
}


void String::PrintOn(FILE* file) {
  int length = this->length();
  for (int i = 0; i < length; i++) {
    PrintF(file, "%c", Get(i));
  }
}


int Map::Hash() {
  // For performance reasons we only hash the 3 most variable fields of a map:
  // constructor, prototype and bit_field2. For predictability reasons we
  // use objects' offsets in respective pages for hashing instead of raw
  // addresses.

  // Shift away the tag.
  int hash = ObjectAddressForHashing(GetConstructor()) >> 2;

  // XOR-ing the prototype and constructor directly yields too many zero bits
  // when the two pointers are close (which is fairly common).
  // To avoid this we shift the prototype bits relatively to the constructor.
  hash ^= ObjectAddressForHashing(prototype()) << (32 - kPageSizeBits);

  return hash ^ (hash >> 16) ^ bit_field2();
}


namespace {

bool CheckEquivalent(const Map* first, const Map* second) {
  return first->GetConstructor() == second->GetConstructor() &&
         first->prototype() == second->prototype() &&
         first->instance_type() == second->instance_type() &&
         first->bit_field() == second->bit_field() &&
         first->is_extensible() == second->is_extensible() &&
         first->new_target_is_base() == second->new_target_is_base() &&
         first->has_hidden_prototype() == second->has_hidden_prototype();
}

}  // namespace

bool Map::EquivalentToForTransition(const Map* other) const {
  if (!CheckEquivalent(this, other)) return false;
  if (instance_type() == JS_FUNCTION_TYPE) {
    // JSFunctions require more checks to ensure that sloppy function is
    // not equivalent to strict function.
    int nof = Min(NumberOfOwnDescriptors(), other->NumberOfOwnDescriptors());
    return instance_descriptors()->IsEqualUpTo(other->instance_descriptors(),
                                               nof);
  }
  return true;
}

bool Map::EquivalentToForElementsKindTransition(const Map* other) const {
  if (!EquivalentToForTransition(other)) return false;
#ifdef DEBUG
  // Ensure that we don't try to generate elements kind transitions from maps
  // with fields that may be generalized in-place. This must already be handled
  // during addition of a new field.
  DescriptorArray* descriptors = instance_descriptors();
  int nof = NumberOfOwnDescriptors();
  for (int i = 0; i < nof; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() == kField) {
      DCHECK(!IsInplaceGeneralizableField(details.constness(),
                                          details.representation(),
                                          descriptors->GetFieldType(i)));
    }
  }
#endif
  return true;
}

bool Map::EquivalentToForNormalization(const Map* other,
                                       PropertyNormalizationMode mode) const {
  int properties =
      mode == CLEAR_INOBJECT_PROPERTIES ? 0 : other->GetInObjectProperties();
  return CheckEquivalent(this, other) && bit_field2() == other->bit_field2() &&
         GetInObjectProperties() == properties &&
         JSObject::GetEmbedderFieldCount(this) ==
             JSObject::GetEmbedderFieldCount(other);
}


void JSFunction::MarkForOptimization(ConcurrencyMode mode) {
  Isolate* isolate = GetIsolate();
  if (!isolate->concurrent_recompilation_enabled() ||
      isolate->bootstrapper()->IsActive()) {
    mode = ConcurrencyMode::kNotConcurrent;
  }

  DCHECK(!is_compiled() || IsInterpreted());
  DCHECK(shared()->IsInterpreted());
  DCHECK(!IsOptimized());
  DCHECK(!HasOptimizedCode());
  DCHECK(shared()->allows_lazy_compilation() ||
         !shared()->optimization_disabled());

  if (mode == ConcurrencyMode::kConcurrent) {
    if (IsInOptimizationQueue()) {
      if (FLAG_trace_concurrent_recompilation) {
        PrintF("  ** Not marking ");
        ShortPrint();
        PrintF(" -- already in optimization queue.\n");
      }
      return;
    }
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Marking ");
      ShortPrint();
      PrintF(" for concurrent recompilation.\n");
    }
  }

  SetOptimizationMarker(mode == ConcurrencyMode::kConcurrent
                            ? OptimizationMarker::kCompileOptimizedConcurrent
                            : OptimizationMarker::kCompileOptimized);
}

// static
void JSFunction::EnsureFeedbackVector(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  if (function->feedback_cell()->value()->IsUndefined(isolate)) {
    Handle<SharedFunctionInfo> shared(function->shared(), isolate);
    if (!shared->HasAsmWasmData()) {
      Handle<FeedbackVector> feedback_vector =
          FeedbackVector::New(isolate, shared);
      if (function->feedback_cell() == isolate->heap()->many_closures_cell()) {
        Handle<FeedbackCell> feedback_cell =
            isolate->factory()->NewOneClosureCell(feedback_vector);
        function->set_feedback_cell(*feedback_cell);
      } else {
        function->feedback_cell()->set_value(*feedback_vector);
      }
    }
  }
}

static void GetMinInobjectSlack(Map* map, void* data) {
  int slack = map->UnusedPropertyFields();
  if (*reinterpret_cast<int*>(data) > slack) {
    *reinterpret_cast<int*>(data) = slack;
  }
}

int Map::InstanceSizeFromSlack(int slack) const {
  return instance_size() - slack * kPointerSize;
}

static void ShrinkInstanceSize(Map* map, void* data) {
  int slack = *reinterpret_cast<int*>(data);
  DCHECK_GE(slack, 0);
#ifdef DEBUG
  int old_visitor_id = Map::GetVisitorId(map);
  int new_unused = map->UnusedPropertyFields() - slack;
#endif
  map->set_instance_size(map->InstanceSizeFromSlack(slack));
  map->set_construction_counter(Map::kNoSlackTracking);
  DCHECK_EQ(old_visitor_id, Map::GetVisitorId(map));
  DCHECK_EQ(new_unused, map->UnusedPropertyFields());
}

static void StopSlackTracking(Map* map, void* data) {
  map->set_construction_counter(Map::kNoSlackTracking);
}

int Map::ComputeMinObjectSlack(Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  // Has to be an initial map.
  DCHECK(GetBackPointer()->IsUndefined(isolate));

  int slack = UnusedPropertyFields();
  TransitionsAccessor transitions(isolate, this, &no_gc);
  transitions.TraverseTransitionTree(&GetMinInobjectSlack, &slack);
  return slack;
}

void Map::CompleteInobjectSlackTracking(Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  // Has to be an initial map.
  DCHECK(GetBackPointer()->IsUndefined(isolate));

  int slack = ComputeMinObjectSlack(isolate);
  TransitionsAccessor transitions(isolate, this, &no_gc);
  if (slack != 0) {
    // Resize the initial map and all maps in its transition tree.
    transitions.TraverseTransitionTree(&ShrinkInstanceSize, &slack);
  } else {
    transitions.TraverseTransitionTree(&StopSlackTracking, nullptr);
  }
}


static bool PrototypeBenefitsFromNormalization(Handle<JSObject> object) {
  DisallowHeapAllocation no_gc;
  if (!object->HasFastProperties()) return false;
  if (object->IsJSGlobalProxy()) return false;
  if (object->GetIsolate()->bootstrapper()->IsActive()) return false;
  return !object->map()->is_prototype_map() ||
         !object->map()->should_be_fast_prototype_map();
}

// static
void JSObject::MakePrototypesFast(Handle<Object> receiver,
                                  WhereToStart where_to_start,
                                  Isolate* isolate) {
  if (!receiver->IsJSReceiver()) return;
  for (PrototypeIterator iter(isolate, Handle<JSReceiver>::cast(receiver),
                              where_to_start);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (!current->IsJSObject()) return;
    Handle<JSObject> current_obj = Handle<JSObject>::cast(current);
    Map* current_map = current_obj->map();
    if (current_map->is_prototype_map()) {
      // If the map is already marked as should be fast, we're done. Its
      // prototypes will have been marked already as well.
      if (current_map->should_be_fast_prototype_map()) return;
      Handle<Map> map(current_map, isolate);
      Map::SetShouldBeFastPrototypeMap(map, true, isolate);
      JSObject::OptimizeAsPrototype(current_obj);
    }
  }
}

// static
void JSObject::OptimizeAsPrototype(Handle<JSObject> object,
                                   bool enable_setup_mode) {
  if (object->IsJSGlobalObject()) return;
  if (enable_setup_mode && PrototypeBenefitsFromNormalization(object)) {
    // First normalize to ensure all JSFunctions are DATA_CONSTANT.
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, 0,
                                  "NormalizeAsPrototype");
  }
  if (object->map()->is_prototype_map()) {
    if (object->map()->should_be_fast_prototype_map() &&
        !object->HasFastProperties()) {
      JSObject::MigrateSlowToFast(object, 0, "OptimizeAsPrototype");
    }
  } else {
    Handle<Map> new_map = Map::Copy(object->GetIsolate(),
                                    handle(object->map(), object->GetIsolate()),
                                    "CopyAsPrototype");
    JSObject::MigrateToMap(object, new_map);
    object->map()->set_is_prototype_map(true);

    // Replace the pointer to the exact constructor with the Object function
    // from the same context if undetectable from JS. This is to avoid keeping
    // memory alive unnecessarily.
    Object* maybe_constructor = object->map()->GetConstructor();
    if (maybe_constructor->IsJSFunction()) {
      JSFunction* constructor = JSFunction::cast(maybe_constructor);
      if (!constructor->shared()->IsApiFunction()) {
        Context* context = constructor->context()->native_context();
        JSFunction* object_function = context->object_function();
        object->map()->SetConstructor(object_function);
      }
    }
  }
}


// static
void JSObject::ReoptimizeIfPrototype(Handle<JSObject> object) {
  if (!object->map()->is_prototype_map()) return;
  if (!object->map()->should_be_fast_prototype_map()) return;
  OptimizeAsPrototype(object);
}


// static
void JSObject::LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  // Contract: In line with InvalidatePrototypeChains()'s requirements,
  // leaf maps don't need to register as users, only prototypes do.
  DCHECK(user->is_prototype_map());

  Handle<Map> current_user = user;
  Handle<PrototypeInfo> current_user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  for (PrototypeIterator iter(isolate, user); !iter.IsAtEnd(); iter.Advance()) {
    // Walk up the prototype chain as far as links haven't been registered yet.
    if (current_user_info->registry_slot() != PrototypeInfo::UNREGISTERED) {
      break;
    }
    Handle<Object> maybe_proto = PrototypeIterator::GetCurrent(iter);
    // Proxies on the prototype chain are not supported. They make it
    // impossible to make any assumptions about the prototype chain anyway.
    if (maybe_proto->IsJSProxy()) return;
    Handle<JSObject> proto = Handle<JSObject>::cast(maybe_proto);
    Handle<PrototypeInfo> proto_info =
        Map::GetOrCreatePrototypeInfo(proto, isolate);
    Handle<Object> maybe_registry(proto_info->prototype_users(), isolate);
    Handle<WeakArrayList> registry =
        maybe_registry->IsSmi()
            ? handle(ReadOnlyRoots(isolate->heap()).empty_weak_array_list(),
                     isolate)
            : Handle<WeakArrayList>::cast(maybe_registry);
    int slot = 0;
    Handle<WeakArrayList> new_array =
        PrototypeUsers::Add(isolate, registry, current_user, &slot);
    current_user_info->set_registry_slot(slot);
    if (!maybe_registry.is_identical_to(new_array)) {
      proto_info->set_prototype_users(*new_array);
    }
    if (FLAG_trace_prototype_users) {
      PrintF("Registering %p as a user of prototype %p (map=%p).\n",
             reinterpret_cast<void*>(*current_user),
             reinterpret_cast<void*>(*proto),
             reinterpret_cast<void*>(proto->map()));
    }

    current_user = handle(proto->map(), isolate);
    current_user_info = proto_info;
  }
}


// Can be called regardless of whether |user| was actually registered with
// |prototype|. Returns true when there was a registration.
// static
bool JSObject::UnregisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  DCHECK(user->is_prototype_map());
  // If it doesn't have a PrototypeInfo, it was never registered.
  if (!user->prototype_info()->IsPrototypeInfo()) return false;
  // If it had no prototype before, see if it had users that might expect
  // registration.
  if (!user->prototype()->IsJSObject()) {
    Object* users =
        PrototypeInfo::cast(user->prototype_info())->prototype_users();
    return users->IsWeakArrayList();
  }
  Handle<JSObject> prototype(JSObject::cast(user->prototype()), isolate);
  Handle<PrototypeInfo> user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  int slot = user_info->registry_slot();
  if (slot == PrototypeInfo::UNREGISTERED) return false;
  DCHECK(prototype->map()->is_prototype_map());
  Object* maybe_proto_info = prototype->map()->prototype_info();
  // User knows its registry slot, prototype info and user registry must exist.
  DCHECK(maybe_proto_info->IsPrototypeInfo());
  Handle<PrototypeInfo> proto_info(PrototypeInfo::cast(maybe_proto_info),
                                   isolate);
  Handle<WeakArrayList> prototype_users(
      WeakArrayList::cast(proto_info->prototype_users()), isolate);
  DCHECK_EQ(prototype_users->Get(slot), HeapObjectReference::Weak(*user));
  PrototypeUsers::MarkSlotEmpty(*prototype_users, slot);
  if (FLAG_trace_prototype_users) {
    PrintF("Unregistering %p as a user of prototype %p.\n",
           reinterpret_cast<void*>(*user), reinterpret_cast<void*>(*prototype));
  }
  return true;
}

namespace {

// This function must be kept in sync with
// AccessorAssembler::InvalidateValidityCellIfPrototype() which does pre-checks
// before jumping here.
void InvalidateOnePrototypeValidityCellInternal(Map* map) {
  DCHECK(map->is_prototype_map());
  if (FLAG_trace_prototype_users) {
    PrintF("Invalidating prototype map %p 's cell\n",
           reinterpret_cast<void*>(map));
  }
  Object* maybe_cell = map->prototype_validity_cell();
  if (maybe_cell->IsCell()) {
    // Just set the value; the cell will be replaced lazily.
    Cell* cell = Cell::cast(maybe_cell);
    cell->set_value(Smi::FromInt(Map::kPrototypeChainInvalid));
  }
}

void InvalidatePrototypeChainsInternal(Map* map) {
  InvalidateOnePrototypeValidityCellInternal(map);

  Object* maybe_proto_info = map->prototype_info();
  if (!maybe_proto_info->IsPrototypeInfo()) return;
  PrototypeInfo* proto_info = PrototypeInfo::cast(maybe_proto_info);
  WeakArrayList* prototype_users =
      WeakArrayList::cast(proto_info->prototype_users());
  // For now, only maps register themselves as users.
  for (int i = PrototypeUsers::kFirstIndex; i < prototype_users->length();
       ++i) {
    HeapObject* heap_object;
    if (prototype_users->Get(i)->GetHeapObjectIfWeak(&heap_object) &&
        heap_object->IsMap()) {
      // Walk the prototype chain (backwards, towards leaf objects) if
      // necessary.
      InvalidatePrototypeChainsInternal(Map::cast(heap_object));
    }
  }
}

}  // namespace

// static
Map* JSObject::InvalidatePrototypeChains(Map* map) {
  DisallowHeapAllocation no_gc;
  InvalidatePrototypeChainsInternal(map);
  return map;
}

// We also invalidate global objects validity cell when a new lexical
// environment variable is added. This is necessary to ensure that
// Load/StoreGlobalIC handlers that load/store from global object's prototype
// get properly invalidated.
// Note, that the normal Load/StoreICs that load/store through the global object
// in the prototype chain are not affected by appearance of a new lexical
// variable and therefore we don't propagate invalidation down.
// static
void JSObject::InvalidatePrototypeValidityCell(JSGlobalObject* global) {
  DisallowHeapAllocation no_gc;
  InvalidateOnePrototypeValidityCellInternal(global->map());
}

// static
Handle<PrototypeInfo> Map::GetOrCreatePrototypeInfo(Handle<JSObject> prototype,
                                                    Isolate* isolate) {
  Object* maybe_proto_info = prototype->map()->prototype_info();
  if (maybe_proto_info->IsPrototypeInfo()) {
    return handle(PrototypeInfo::cast(maybe_proto_info), isolate);
  }
  Handle<PrototypeInfo> proto_info = isolate->factory()->NewPrototypeInfo();
  prototype->map()->set_prototype_info(*proto_info);
  return proto_info;
}


// static
Handle<PrototypeInfo> Map::GetOrCreatePrototypeInfo(Handle<Map> prototype_map,
                                                    Isolate* isolate) {
  Object* maybe_proto_info = prototype_map->prototype_info();
  if (maybe_proto_info->IsPrototypeInfo()) {
    return handle(PrototypeInfo::cast(maybe_proto_info), isolate);
  }
  Handle<PrototypeInfo> proto_info = isolate->factory()->NewPrototypeInfo();
  prototype_map->set_prototype_info(*proto_info);
  return proto_info;
}

// static
void Map::SetShouldBeFastPrototypeMap(Handle<Map> map, bool value,
                                      Isolate* isolate) {
  if (value == false && !map->prototype_info()->IsPrototypeInfo()) {
    // "False" is the implicit default value, so there's nothing to do.
    return;
  }
  GetOrCreatePrototypeInfo(map, isolate)->set_should_be_fast_map(value);
}

// static
Handle<Object> Map::GetOrCreatePrototypeChainValidityCell(Handle<Map> map,
                                                          Isolate* isolate) {
  Handle<Object> maybe_prototype;
  if (map->IsJSGlobalObjectMap()) {
    DCHECK(map->is_prototype_map());
    // Global object is prototype of a global proxy and therefore we can
    // use its validity cell for guarding global object's prototype change.
    maybe_prototype = isolate->global_object();
  } else {
    maybe_prototype =
        handle(map->GetPrototypeChainRootMap(isolate)->prototype(), isolate);
  }
  if (!maybe_prototype->IsJSObject()) {
    return handle(Smi::FromInt(Map::kPrototypeChainValid), isolate);
  }
  Handle<JSObject> prototype = Handle<JSObject>::cast(maybe_prototype);
  // Ensure the prototype is registered with its own prototypes so its cell
  // will be invalidated when necessary.
  JSObject::LazyRegisterPrototypeUser(handle(prototype->map(), isolate),
                                      isolate);

  Object* maybe_cell = prototype->map()->prototype_validity_cell();
  // Return existing cell if it's still valid.
  if (maybe_cell->IsCell()) {
    Handle<Cell> cell(Cell::cast(maybe_cell), isolate);
    if (cell->value() == Smi::FromInt(Map::kPrototypeChainValid)) {
      return cell;
    }
  }
  // Otherwise create a new cell.
  Handle<Cell> cell = isolate->factory()->NewCell(
      handle(Smi::FromInt(Map::kPrototypeChainValid), isolate));
  prototype->map()->set_prototype_validity_cell(*cell);
  return cell;
}

// static
bool Map::IsPrototypeChainInvalidated(Map* map) {
  DCHECK(map->is_prototype_map());
  Object* maybe_cell = map->prototype_validity_cell();
  if (maybe_cell->IsCell()) {
    Cell* cell = Cell::cast(maybe_cell);
    return cell->value() != Smi::FromInt(Map::kPrototypeChainValid);
  }
  return true;
}

// static
void Map::SetPrototype(Isolate* isolate, Handle<Map> map,
                       Handle<Object> prototype,
                       bool enable_prototype_setup_mode) {
  RuntimeCallTimerScope stats_scope(isolate, *map,
                                    RuntimeCallCounterId::kMap_SetPrototype);

  bool is_hidden = false;
  if (prototype->IsJSObject()) {
    Handle<JSObject> prototype_jsobj = Handle<JSObject>::cast(prototype);
    JSObject::OptimizeAsPrototype(prototype_jsobj, enable_prototype_setup_mode);

    Object* maybe_constructor = prototype_jsobj->map()->GetConstructor();
    if (maybe_constructor->IsJSFunction()) {
      JSFunction* constructor = JSFunction::cast(maybe_constructor);
      Object* data = constructor->shared()->function_data();
      is_hidden = (data->IsFunctionTemplateInfo() &&
                   FunctionTemplateInfo::cast(data)->hidden_prototype()) ||
                  prototype->IsJSGlobalObject();
    } else if (maybe_constructor->IsFunctionTemplateInfo()) {
      is_hidden =
          FunctionTemplateInfo::cast(maybe_constructor)->hidden_prototype() ||
          prototype->IsJSGlobalObject();
    }
  }
  map->set_has_hidden_prototype(is_hidden);

  WriteBarrierMode wb_mode =
      prototype->IsNull(isolate) ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  map->set_prototype(*prototype, wb_mode);
}

Handle<Object> CacheInitialJSArrayMaps(Handle<Context> native_context,
                                       Handle<Map> initial_map) {
  // Replace all of the cached initial array maps in the native context with
  // the appropriate transitioned elements kind maps.
  Handle<Map> current_map = initial_map;
  ElementsKind kind = current_map->elements_kind();
  DCHECK_EQ(GetInitialFastElementsKind(), kind);
  native_context->set(Context::ArrayMapIndex(kind), *current_map);
  for (int i = GetSequenceIndexFromFastElementsKind(kind) + 1;
       i < kFastElementsKindCount; ++i) {
    Handle<Map> new_map;
    ElementsKind next_kind = GetFastElementsKindFromSequenceIndex(i);
    if (Map* maybe_elements_transition = current_map->ElementsTransitionMap()) {
      new_map = handle(maybe_elements_transition, native_context->GetIsolate());
    } else {
      new_map =
          Map::CopyAsElementsKind(native_context->GetIsolate(), current_map,
                                  next_kind, INSERT_TRANSITION);
    }
    DCHECK_EQ(next_kind, new_map->elements_kind());
    native_context->set(Context::ArrayMapIndex(next_kind), *new_map);
    current_map = new_map;
  }
  return initial_map;
}

namespace {

void SetInstancePrototype(Isolate* isolate, Handle<JSFunction> function,
                          Handle<JSReceiver> value) {
  // Now some logic for the maps of the objects that are created by using this
  // function as a constructor.
  if (function->has_initial_map()) {
    // If the function has allocated the initial map replace it with a
    // copy containing the new prototype.  Also complete any in-object
    // slack tracking that is in progress at this point because it is
    // still tracking the old copy.
    function->CompleteInobjectSlackTrackingIfActive();

    Handle<Map> initial_map(function->initial_map(), isolate);

    if (!isolate->bootstrapper()->IsActive() &&
        initial_map->instance_type() == JS_OBJECT_TYPE) {
      // Put the value in the initial map field until an initial map is needed.
      // At that point, a new initial map is created and the prototype is put
      // into the initial map where it belongs.
      function->set_prototype_or_initial_map(*value);
    } else {
      Handle<Map> new_map =
          Map::Copy(isolate, initial_map, "SetInstancePrototype");
      JSFunction::SetInitialMap(function, new_map, value);

      // If the function is used as the global Array function, cache the
      // updated initial maps (and transitioned versions) in the native context.
      Handle<Context> native_context(function->context()->native_context(),
                                     isolate);
      Handle<Object> array_function(
          native_context->get(Context::ARRAY_FUNCTION_INDEX), isolate);
      if (array_function->IsJSFunction() &&
          *function == JSFunction::cast(*array_function)) {
        CacheInitialJSArrayMaps(native_context, new_map);
      }
    }

    // Deoptimize all code that embeds the previous initial map.
    initial_map->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kInitialMapChangedGroup);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    function->set_prototype_or_initial_map(*value);
    if (value->IsJSObject()) {
      // Optimize as prototype to detach it from its transition tree.
      JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value));
    }
  }
}

}  // anonymous namespace

void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  DCHECK(function->IsConstructor() ||
         IsGeneratorFunction(function->shared()->kind()));
  Isolate* isolate = function->GetIsolate();
  Handle<JSReceiver> construct_prototype;

  // If the value is not a JSReceiver, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSReceiver()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Handle<Map> new_map =
        Map::Copy(isolate, handle(function->map(), isolate), "SetPrototype");

    JSObject::MigrateToMap(function, new_map);
    new_map->SetConstructor(*value);
    new_map->set_has_non_instance_prototype(true);

    FunctionKind kind = function->shared()->kind();
    Handle<Context> native_context(function->context()->native_context(),
                                   isolate);

    construct_prototype = Handle<JSReceiver>(
        IsGeneratorFunction(kind)
            ? IsAsyncFunction(kind)
                  ? native_context->initial_async_generator_prototype()
                  : native_context->initial_generator_prototype()
            : native_context->initial_object_prototype(),
        isolate);
  } else {
    construct_prototype = Handle<JSReceiver>::cast(value);
    function->map()->set_has_non_instance_prototype(false);
  }

  SetInstancePrototype(isolate, function, construct_prototype);
}

void JSFunction::SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                               Handle<Object> prototype) {
  if (map->prototype() != *prototype)
    Map::SetPrototype(function->GetIsolate(), map, prototype);
  function->set_prototype_or_initial_map(*map);
  map->SetConstructor(*function);
  if (FLAG_trace_maps) {
    LOG(function->GetIsolate(), MapEvent("InitialMap", nullptr, *map, "",
                                         function->shared()->DebugName()));
  }
}


#ifdef DEBUG
namespace {

bool CanSubclassHaveInobjectProperties(InstanceType instance_type) {
  switch (instance_type) {
    case JS_API_OBJECT_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_DATE_TYPE:
    case JS_FUNCTION_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_INTL_COLLATOR_TYPE:
    case JS_INTL_DATE_TIME_FORMAT_TYPE:
    case JS_INTL_LIST_FORMAT_TYPE:
    case JS_INTL_LOCALE_TYPE:
    case JS_INTL_NUMBER_FORMAT_TYPE:
    case JS_INTL_PLURAL_RULES_TYPE:
    case JS_INTL_RELATIVE_TIME_FORMAT_TYPE:
    case JS_INTL_SEGMENTER_TYPE:
    case JS_INTL_V8_BREAK_ITERATOR_TYPE:
#endif
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_MAP_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REGEXP_TYPE:
    case JS_SET_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_VALUE_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
    case WASM_GLOBAL_TYPE:
    case WASM_INSTANCE_TYPE:
    case WASM_MEMORY_TYPE:
    case WASM_MODULE_TYPE:
    case WASM_TABLE_TYPE:
      return true;

    case BIGINT_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case BYTECODE_ARRAY_TYPE:
    case BYTE_ARRAY_TYPE:
    case CELL_TYPE:
    case CODE_TYPE:
    case FILLER_TYPE:
    case FIXED_ARRAY_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
    case FEEDBACK_METADATA_TYPE:
    case FOREIGN_TYPE:
    case FREE_SPACE_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case HEAP_NUMBER_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_PROXY_TYPE:
    case MAP_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
    case ODDBALL_TYPE:
    case PROPERTY_CELL_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case SYMBOL_TYPE:
    case ALLOCATION_SITE_TYPE:

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:
#undef TYPED_ARRAY_CASE

#define MAKE_STRUCT_CASE(TYPE, Name, name) case TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      // We must not end up here for these instance types at all.
      UNREACHABLE();
    // Fall through.
    default:
      return false;
  }
}

}  // namespace
#endif


void JSFunction::EnsureHasInitialMap(Handle<JSFunction> function) {
  DCHECK(function->has_prototype_slot());
  DCHECK(function->IsConstructor() ||
         IsResumableFunction(function->shared()->kind()));
  if (function->has_initial_map()) return;
  Isolate* isolate = function->GetIsolate();

  // First create a new map with the size and number of in-object properties
  // suggested by the function.
  InstanceType instance_type;
  if (IsResumableFunction(function->shared()->kind())) {
    instance_type = IsAsyncGeneratorFunction(function->shared()->kind())
                        ? JS_ASYNC_GENERATOR_OBJECT_TYPE
                        : JS_GENERATOR_OBJECT_TYPE;
  } else {
    instance_type = JS_OBJECT_TYPE;
  }

  // The constructor should be compiled for the optimization hints to be
  // available.
  int expected_nof_properties = 0;
  if (function->shared()->is_compiled() ||
      Compiler::Compile(function, Compiler::CLEAR_EXCEPTION)) {
    DCHECK(function->shared()->is_compiled());
    expected_nof_properties = function->shared()->expected_nof_properties();
  }

  int instance_size;
  int inobject_properties;
  CalculateInstanceSizeHelper(instance_type, false, 0, expected_nof_properties,
                              &instance_size, &inobject_properties);

  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size,
                                               TERMINAL_FAST_ELEMENTS_KIND,
                                               inobject_properties);

  // Fetch or allocate prototype.
  Handle<Object> prototype;
  if (function->has_instance_prototype()) {
    prototype = handle(function->instance_prototype(), isolate);
  } else {
    prototype = isolate->factory()->NewFunctionPrototype(function);
  }
  DCHECK(map->has_fast_object_elements());

  // Finally link initial map and constructor function.
  DCHECK(prototype->IsJSReceiver());
  JSFunction::SetInitialMap(function, map, prototype);
  map->StartInobjectSlackTracking();
}

namespace {
bool FastInitializeDerivedMap(Isolate* isolate, Handle<JSFunction> new_target,
                              Handle<JSFunction> constructor,
                              Handle<Map> constructor_initial_map) {
  // Use the default intrinsic prototype instead.
  if (!new_target->has_prototype_slot()) return false;
  // Check that |function|'s initial map still in sync with the |constructor|,
  // otherwise we must create a new initial map for |function|.
  if (new_target->has_initial_map() &&
      new_target->initial_map()->GetConstructor() == *constructor) {
    DCHECK(new_target->instance_prototype()->IsJSReceiver());
    return true;
  }
  InstanceType instance_type = constructor_initial_map->instance_type();
  DCHECK(CanSubclassHaveInobjectProperties(instance_type));
  // Create a new map with the size and number of in-object properties
  // suggested by |function|.

  // Link initial map and constructor function if the new.target is actually a
  // subclass constructor.
  if (!IsDerivedConstructor(new_target->shared()->kind())) return false;

  int instance_size;
  int in_object_properties;
  int embedder_fields =
      JSObject::GetEmbedderFieldCount(*constructor_initial_map);
  bool success = JSFunction::CalculateInstanceSizeForDerivedClass(
      new_target, instance_type, embedder_fields, &instance_size,
      &in_object_properties);

  Handle<Map> map;
  if (success) {
    int pre_allocated = constructor_initial_map->GetInObjectProperties() -
                        constructor_initial_map->UnusedPropertyFields();
    CHECK_LE(constructor_initial_map->UsedInstanceSize(), instance_size);
    int unused_property_fields = in_object_properties - pre_allocated;
    map = Map::CopyInitialMap(isolate, constructor_initial_map, instance_size,
                              in_object_properties, unused_property_fields);
  } else {
    map = Map::CopyInitialMap(isolate, constructor_initial_map);
  }
  map->set_new_target_is_base(false);
  Handle<Object> prototype(new_target->instance_prototype(), isolate);
  JSFunction::SetInitialMap(new_target, map, prototype);
  DCHECK(new_target->instance_prototype()->IsJSReceiver());
  map->SetConstructor(*constructor);
  map->set_construction_counter(Map::kNoSlackTracking);
  map->StartInobjectSlackTracking();
  return true;
}

}  // namespace

// static
MaybeHandle<Map> JSFunction::GetDerivedMap(Isolate* isolate,
                                           Handle<JSFunction> constructor,
                                           Handle<JSReceiver> new_target) {
  EnsureHasInitialMap(constructor);

  Handle<Map> constructor_initial_map(constructor->initial_map(), isolate);
  if (*new_target == *constructor) return constructor_initial_map;

  Handle<Map> result_map;
  // Fast case, new.target is a subclass of constructor. The map is cacheable
  // (and may already have been cached). new.target.prototype is guaranteed to
  // be a JSReceiver.
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (FastInitializeDerivedMap(isolate, function, constructor,
                                 constructor_initial_map)) {
      return handle(function->initial_map(), isolate);
    }
  }

  // Slow path, new.target is either a proxy or can't cache the map.
  // new.target.prototype is not guaranteed to be a JSReceiver, and may need to
  // fall back to the intrinsicDefaultProto.
  Handle<Object> prototype;
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (function->has_prototype_slot()) {
      // Make sure the new.target.prototype is cached.
      EnsureHasInitialMap(function);
      prototype = handle(function->prototype(), isolate);
    } else {
      // No prototype property, use the intrinsict default proto further down.
      prototype = isolate->factory()->undefined_value();
    }
  } else {
    Handle<String> prototype_string = isolate->factory()->prototype_string();
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, prototype,
        JSReceiver::GetProperty(isolate, new_target, prototype_string), Map);
    // The above prototype lookup might change the constructor and its
    // prototype, hence we have to reload the initial map.
    EnsureHasInitialMap(constructor);
    constructor_initial_map = handle(constructor->initial_map(), isolate);
  }

  // If prototype is not a JSReceiver, fetch the intrinsicDefaultProto from the
  // correct realm. Rather than directly fetching the .prototype, we fetch the
  // constructor that points to the .prototype. This relies on
  // constructor.prototype being FROZEN for those constructors.
  if (!prototype->IsJSReceiver()) {
    Handle<Context> context;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, context,
                               JSReceiver::GetFunctionRealm(new_target), Map);
    DCHECK(context->IsNativeContext());
    Handle<Object> maybe_index = JSReceiver::GetDataProperty(
        constructor, isolate->factory()->native_context_index_symbol());
    int index = maybe_index->IsSmi() ? Smi::ToInt(*maybe_index)
                                     : Context::OBJECT_FUNCTION_INDEX;
    Handle<JSFunction> realm_constructor(JSFunction::cast(context->get(index)),
                                         isolate);
    prototype = handle(realm_constructor->prototype(), isolate);
  }

  Handle<Map> map = Map::CopyInitialMap(isolate, constructor_initial_map);
  map->set_new_target_is_base(false);
  CHECK(prototype->IsJSReceiver());
  if (map->prototype() != *prototype)
    Map::SetPrototype(isolate, map, prototype);
  map->SetConstructor(*constructor);
  return map;
}

int JSFunction::ComputeInstanceSizeWithMinSlack(Isolate* isolate) {
  CHECK(has_initial_map());
  if (initial_map()->IsInobjectSlackTrackingInProgress()) {
    int slack = initial_map()->ComputeMinObjectSlack(isolate);
    return initial_map()->InstanceSizeFromSlack(slack);
  }
  return initial_map()->instance_size();
}

void JSFunction::PrintName(FILE* out) {
  std::unique_ptr<char[]> name = shared()->DebugName()->ToCString();
  PrintF(out, "%s", name.get());
}


Handle<String> JSFunction::GetName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name =
      JSReceiver::GetDataProperty(function, isolate->factory()->name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return handle(function->shared()->DebugName(), isolate);
}


Handle<String> JSFunction::GetDebugName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name = JSReceiver::GetDataProperty(
      function, isolate->factory()->display_name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return JSFunction::GetName(function);
}

bool JSFunction::SetName(Handle<JSFunction> function, Handle<Name> name,
                         Handle<String> prefix) {
  Isolate* isolate = function->GetIsolate();
  Handle<String> function_name;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name,
                                   Name::ToFunctionName(isolate, name), false);
  if (prefix->length() > 0) {
    IncrementalStringBuilder builder(isolate);
    builder.AppendString(prefix);
    builder.AppendCharacter(' ');
    builder.AppendString(function_name);
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name, builder.Finish(),
                                     false);
  }
  RETURN_ON_EXCEPTION_VALUE(
      isolate,
      JSObject::DefinePropertyOrElementIgnoreAttributes(
          function, isolate->factory()->name_string(), function_name,
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY)),
      false);
  return true;
}

namespace {

Handle<String> NativeCodeFunctionSourceString(
    Handle<SharedFunctionInfo> shared_info) {
  Isolate* const isolate = shared_info->GetIsolate();
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("function ");
  builder.AppendString(handle(shared_info->Name(), isolate));
  builder.AppendCString("() { [native code] }");
  return builder.Finish().ToHandleChecked();
}

}  // namespace


// static
Handle<String> JSBoundFunction::ToString(Handle<JSBoundFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  return isolate->factory()->function_native_code_string();
}


// static
Handle<String> JSFunction::ToString(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);

  // Check if {function} should hide its source code.
  if (!shared_info->IsUserJavaScript()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  // Check if we should print {function} as a class.
  Handle<Object> maybe_class_positions = JSReceiver::GetDataProperty(
      function, isolate->factory()->class_positions_symbol());
  if (maybe_class_positions->IsTuple2()) {
    Tuple2* class_positions = Tuple2::cast(*maybe_class_positions);
    int start_position = Smi::ToInt(class_positions->value1());
    int end_position = Smi::ToInt(class_positions->value2());
    Handle<String> script_source(
        String::cast(Script::cast(shared_info->script())->source()), isolate);
    return isolate->factory()->NewSubString(script_source, start_position,
                                            end_position);
  }

  // Check if we have source code for the {function}.
  if (!shared_info->HasSourceCode()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  if (shared_info->function_token_position() == kNoSourcePosition) {
    // If the function token position isn't valid, return [native code] to
    // ensure calling eval on the returned source code throws rather than
    // giving inconsistent call behaviour.
    isolate->CountUsage(
        v8::Isolate::UseCounterFeature::kFunctionTokenOffsetTooLongForToString);
    return NativeCodeFunctionSourceString(shared_info);
  }
  return Handle<String>::cast(
      SharedFunctionInfo::GetSourceCodeHarmony(shared_info));
}

void Oddball::Initialize(Isolate* isolate, Handle<Oddball> oddball,
                         const char* to_string, Handle<Object> to_number,
                         const char* type_of, byte kind) {
  Handle<String> internalized_to_string =
      isolate->factory()->InternalizeUtf8String(to_string);
  Handle<String> internalized_type_of =
      isolate->factory()->InternalizeUtf8String(type_of);
  if (to_number->IsHeapNumber()) {
    oddball->set_to_number_raw_as_bits(
        Handle<HeapNumber>::cast(to_number)->value_as_bits());
  } else {
    oddball->set_to_number_raw(to_number->Number());
  }
  oddball->set_to_number(*to_number);
  oddball->set_to_string(*internalized_to_string);
  oddball->set_type_of(*internalized_type_of);
  oddball->set_kind(kind);
}

int Script::GetEvalPosition() {
  DisallowHeapAllocation no_gc;
  DCHECK(compilation_type() == Script::COMPILATION_TYPE_EVAL);
  int position = eval_from_position();
  if (position < 0) {
    // Due to laziness, the position may not have been translated from code
    // offset yet, which would be encoded as negative integer. In that case,
    // translate and set the position.
    if (!has_eval_from_shared()) {
      position = 0;
    } else {
      SharedFunctionInfo* shared = eval_from_shared();
      position = shared->abstract_code()->SourcePosition(-position);
    }
    DCHECK_GE(position, 0);
    set_eval_from_position(position);
  }
  return position;
}

void Script::InitLineEnds(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  if (!script->line_ends()->IsUndefined(isolate)) return;
  DCHECK(script->type() != Script::TYPE_WASM ||
         script->source_mapping_url()->IsString());

  Object* src_obj = script->source();
  if (!src_obj->IsString()) {
    DCHECK(src_obj->IsUndefined(isolate));
    script->set_line_ends(ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    DCHECK(src_obj->IsString());
    Handle<String> src(String::cast(src_obj), isolate);
    Handle<FixedArray> array = String::CalculateLineEnds(isolate, src, true);
    script->set_line_ends(*array);
  }

  DCHECK(script->line_ends()->IsFixedArray());
}

bool Script::GetPositionInfo(Handle<Script> script, int position,
                             PositionInfo* info, OffsetFlag offset_flag) {
  // For wasm, we do not create an artificial line_ends array, but do the
  // translation directly.
  if (script->type() != Script::TYPE_WASM) InitLineEnds(script);
  return script->GetPositionInfo(position, info, offset_flag);
}

bool Script::IsUserJavaScript() { return type() == Script::TYPE_NORMAL; }

bool Script::ContainsAsmModule() {
  DisallowHeapAllocation no_gc;
  SharedFunctionInfo::ScriptIterator iter(this->GetIsolate(), this);
  while (SharedFunctionInfo* info = iter.Next()) {
    if (info->HasAsmWasmData()) return true;
  }
  return false;
}

namespace {
bool GetPositionInfoSlow(const Script* script, int position,
                         Script::PositionInfo* info) {
  if (!script->source()->IsString()) return false;
  if (position < 0) position = 0;

  String* source_string = String::cast(script->source());
  int line = 0;
  int line_start = 0;
  int len = source_string->length();
  for (int pos = 0; pos <= len; ++pos) {
    if (pos == len || source_string->Get(pos) == '\n') {
      if (position <= pos) {
        info->line = line;
        info->column = position - line_start;
        info->line_start = line_start;
        info->line_end = pos;
        return true;
      }
      line++;
      line_start = pos + 1;
    }
  }
  return false;
}
}  // namespace

#define SMI_VALUE(x) (Smi::ToInt(x))
bool Script::GetPositionInfo(int position, PositionInfo* info,
                             OffsetFlag offset_flag) const {
  DisallowHeapAllocation no_allocation;

  // For wasm, we do not rely on the line_ends array, but do the translation
  // directly.
  if (type() == Script::TYPE_WASM) {
    DCHECK_LE(0, position);
    return WasmModuleObject::cast(wasm_module_object())
        ->GetPositionInfo(static_cast<uint32_t>(position), info);
  }

  if (line_ends()->IsUndefined()) {
    // Slow mode: we do not have line_ends. We have to iterate through source.
    if (!GetPositionInfoSlow(this, position, info)) return false;
  } else {
    DCHECK(line_ends()->IsFixedArray());
    FixedArray* ends = FixedArray::cast(line_ends());

    const int ends_len = ends->length();
    if (ends_len == 0) return false;

    // Return early on invalid positions. Negative positions behave as if 0 was
    // passed, and positions beyond the end of the script return as failure.
    if (position < 0) {
      position = 0;
    } else if (position > SMI_VALUE(ends->get(ends_len - 1))) {
      return false;
    }

    // Determine line number by doing a binary search on the line ends array.
    if (SMI_VALUE(ends->get(0)) >= position) {
      info->line = 0;
      info->line_start = 0;
      info->column = position;
    } else {
      int left = 0;
      int right = ends_len - 1;

      while (right > 0) {
        DCHECK_LE(left, right);
        const int mid = (left + right) / 2;
        if (position > SMI_VALUE(ends->get(mid))) {
          left = mid + 1;
        } else if (position <= SMI_VALUE(ends->get(mid - 1))) {
          right = mid - 1;
        } else {
          info->line = mid;
          break;
        }
      }
      DCHECK(SMI_VALUE(ends->get(info->line)) >= position &&
             SMI_VALUE(ends->get(info->line - 1)) < position);
      info->line_start = SMI_VALUE(ends->get(info->line - 1)) + 1;
      info->column = position - info->line_start;
    }

    // Line end is position of the linebreak character.
    info->line_end = SMI_VALUE(ends->get(info->line));
    if (info->line_end > 0) {
      DCHECK(source()->IsString());
      String* src = String::cast(source());
      if (src->length() >= info->line_end &&
          src->Get(info->line_end - 1) == '\r') {
        info->line_end--;
      }
    }
  }

  // Add offsets if requested.
  if (offset_flag == WITH_OFFSET) {
    if (info->line == 0) {
      info->column += column_offset();
    }
    info->line += line_offset();
  }

  return true;
}
#undef SMI_VALUE

int Script::GetColumnNumber(Handle<Script> script, int code_pos) {
  PositionInfo info;
  GetPositionInfo(script, code_pos, &info, WITH_OFFSET);
  return info.column;
}

int Script::GetColumnNumber(int code_pos) const {
  PositionInfo info;
  GetPositionInfo(code_pos, &info, WITH_OFFSET);
  return info.column;
}

int Script::GetLineNumber(Handle<Script> script, int code_pos) {
  PositionInfo info;
  GetPositionInfo(script, code_pos, &info, WITH_OFFSET);
  return info.line;
}

int Script::GetLineNumber(int code_pos) const {
  PositionInfo info;
  GetPositionInfo(code_pos, &info, WITH_OFFSET);
  return info.line;
}

Object* Script::GetNameOrSourceURL() {
  // Keep in sync with ScriptNameOrSourceURL in messages.js.
  if (!source_url()->IsUndefined()) return source_url();
  return name();
}

MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    Isolate* isolate, const FunctionLiteral* fun) {
  CHECK_NE(fun->function_literal_id(), FunctionLiteral::kIdTypeInvalid);
  // If this check fails, the problem is most probably the function id
  // renumbering done by AstFunctionLiteralIdReindexer; in particular, that
  // AstTraversalVisitor doesn't recurse properly in the construct which
  // triggers the mismatch.
  CHECK_LT(fun->function_literal_id(), shared_function_infos()->length());
  MaybeObject* shared =
      shared_function_infos()->Get(fun->function_literal_id());
  HeapObject* heap_object;
  if (!shared->GetHeapObject(&heap_object) ||
      heap_object->IsUndefined(isolate)) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  return handle(SharedFunctionInfo::cast(heap_object), isolate);
}

Script::Iterator::Iterator(Isolate* isolate)
    : iterator_(isolate->heap()->script_list()) {}

Script* Script::Iterator::Next() {
  Object* o = iterator_.Next();
  if (o != nullptr) {
    return Script::cast(o);
  }
  return nullptr;
}

Code* SharedFunctionInfo::GetCode() const {
  // ======
  // NOTE: This chain of checks MUST be kept in sync with the equivalent CSA
  // GetSharedFunctionInfoCode method in code-stub-assembler.cc.
  // ======

  Isolate* isolate = GetIsolate();
  Object* data = function_data();
  if (data->IsSmi()) {
    // Holding a Smi means we are a builtin.
    DCHECK(HasBuiltinId());
    return isolate->builtins()->builtin(builtin_id());
  } else if (data->IsBytecodeArray()) {
    // Having a bytecode array means we are a compiled, interpreted function.
    DCHECK(HasBytecodeArray());
    return isolate->builtins()->builtin(Builtins::kInterpreterEntryTrampoline);
  } else if (data->IsFixedArray()) {
    // Having a fixed array means we are an asm.js/wasm function.
    DCHECK(HasAsmWasmData());
    return isolate->builtins()->builtin(Builtins::kInstantiateAsmJs);
  } else if (data->IsUncompiledData()) {
    // Having uncompiled data (with or without scope) means we need to compile.
    DCHECK(HasUncompiledData());
    return isolate->builtins()->builtin(Builtins::kCompileLazy);
  } else if (data->IsFunctionTemplateInfo()) {
    // Having a function template info means we are an API function.
    DCHECK(IsApiFunction());
    return isolate->builtins()->builtin(Builtins::kHandleApiCall);
  } else if (data->IsWasmExportedFunctionData()) {
    // Having a WasmExportedFunctionData means the code is in there.
    DCHECK(HasWasmExportedFunctionData());
    return wasm_exported_function_data()->wrapper_code();
  } else if (data->IsInterpreterData()) {
    Code* code = InterpreterTrampoline();
    DCHECK(code->IsCode());
    DCHECK(code->is_interpreter_trampoline_builtin());
    return code;
  }
  UNREACHABLE();
}

WasmExportedFunctionData* SharedFunctionInfo::wasm_exported_function_data()
    const {
  DCHECK(HasWasmExportedFunctionData());
  return WasmExportedFunctionData::cast(function_data());
}

SharedFunctionInfo::ScriptIterator::ScriptIterator(Isolate* isolate,
                                                   Script* script)
    : ScriptIterator(isolate,
                     handle(script->shared_function_infos(), isolate)) {}

SharedFunctionInfo::ScriptIterator::ScriptIterator(
    Isolate* isolate, Handle<WeakFixedArray> shared_function_infos)
    : isolate_(isolate),
      shared_function_infos_(shared_function_infos),
      index_(0) {}

SharedFunctionInfo* SharedFunctionInfo::ScriptIterator::Next() {
  while (index_ < shared_function_infos_->length()) {
    MaybeObject* raw = shared_function_infos_->Get(index_++);
    HeapObject* heap_object;
    if (!raw->GetHeapObject(&heap_object) ||
        heap_object->IsUndefined(isolate_)) {
      continue;
    }
    return SharedFunctionInfo::cast(heap_object);
  }
  return nullptr;
}

void SharedFunctionInfo::ScriptIterator::Reset(Script* script) {
  shared_function_infos_ = handle(script->shared_function_infos(), isolate_);
  index_ = 0;
}

SharedFunctionInfo::GlobalIterator::GlobalIterator(Isolate* isolate)
    : script_iterator_(isolate),
      noscript_sfi_iterator_(isolate->heap()->noscript_shared_function_infos()),
      sfi_iterator_(isolate, script_iterator_.Next()) {}

SharedFunctionInfo* SharedFunctionInfo::GlobalIterator::Next() {
  HeapObject* next = noscript_sfi_iterator_.Next();
  if (next != nullptr) return SharedFunctionInfo::cast(next);
  for (;;) {
    next = sfi_iterator_.Next();
    if (next != nullptr) return SharedFunctionInfo::cast(next);
    Script* next_script = script_iterator_.Next();
    if (next_script == nullptr) return nullptr;
    sfi_iterator_.Reset(next_script);
  }
}

void SharedFunctionInfo::SetScript(Handle<SharedFunctionInfo> shared,
                                   Handle<Object> script_object,
                                   int function_literal_id,
                                   bool reset_preparsed_scope_data) {
  if (shared->script() == *script_object) return;
  Isolate* isolate = shared->GetIsolate();

  if (reset_preparsed_scope_data &&
      shared->HasUncompiledDataWithPreParsedScope()) {
    shared->ClearPreParsedScopeData();
  }

  // Add shared function info to new script's list. If a collection occurs,
  // the shared function info may be temporarily in two lists.
  // This is okay because the gc-time processing of these lists can tolerate
  // duplicates.
  if (script_object->IsScript()) {
    DCHECK(!shared->script()->IsScript());
    Handle<Script> script = Handle<Script>::cast(script_object);
    Handle<WeakFixedArray> list =
        handle(script->shared_function_infos(), isolate);
#ifdef DEBUG
    DCHECK_LT(function_literal_id, list->length());
    MaybeObject* maybe_object = list->Get(function_literal_id);
    HeapObject* heap_object;
    if (maybe_object->GetHeapObjectIfWeak(&heap_object)) {
      DCHECK_EQ(heap_object, *shared);
    }
#endif
    list->Set(function_literal_id, HeapObjectReference::Weak(*shared));

    // Remove shared function info from root array.
    WeakArrayList* noscript_list =
        isolate->heap()->noscript_shared_function_infos();
    CHECK(noscript_list->RemoveOne(MaybeObjectHandle::Weak(shared)));
  } else {
    DCHECK(shared->script()->IsScript());
    Handle<WeakArrayList> list =
        isolate->factory()->noscript_shared_function_infos();

#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      WeakArrayList::Iterator iterator(*list);
      HeapObject* next;
      while ((next = iterator.Next()) != nullptr) {
        DCHECK_NE(next, *shared);
      }
    }
#endif  // DEBUG

    list =
        WeakArrayList::AddToEnd(isolate, list, MaybeObjectHandle::Weak(shared));

    isolate->heap()->SetRootNoScriptSharedFunctionInfos(*list);

    // Remove shared function info from old script's list.
    Script* old_script = Script::cast(shared->script());

    // Due to liveedit, it might happen that the old_script doesn't know
    // about the SharedFunctionInfo, so we have to guard against that.
    Handle<WeakFixedArray> infos(old_script->shared_function_infos(), isolate);
    if (function_literal_id < infos->length()) {
      MaybeObject* raw =
          old_script->shared_function_infos()->Get(function_literal_id);
      HeapObject* heap_object;
      if (raw->GetHeapObjectIfWeak(&heap_object) && heap_object == *shared) {
        old_script->shared_function_infos()->Set(
            function_literal_id, HeapObjectReference::Strong(
                                     ReadOnlyRoots(isolate).undefined_value()));
      }
    }
  }

  // Finally set new script.
  shared->set_script(*script_object);
}

bool SharedFunctionInfo::HasBreakInfo() const {
  if (!HasDebugInfo()) return false;
  DebugInfo* info = DebugInfo::cast(GetDebugInfo());
  bool has_break_info = info->HasBreakInfo();
  return has_break_info;
}

bool SharedFunctionInfo::BreakAtEntry() const {
  if (!HasDebugInfo()) return false;
  DebugInfo* info = DebugInfo::cast(GetDebugInfo());
  bool break_at_entry = info->BreakAtEntry();
  return break_at_entry;
}

bool SharedFunctionInfo::HasCoverageInfo() const {
  if (!HasDebugInfo()) return false;
  DebugInfo* info = DebugInfo::cast(GetDebugInfo());
  bool has_coverage_info = info->HasCoverageInfo();
  return has_coverage_info;
}

CoverageInfo* SharedFunctionInfo::GetCoverageInfo() const {
  DCHECK(HasCoverageInfo());
  return CoverageInfo::cast(GetDebugInfo()->coverage_info());
}

String* SharedFunctionInfo::DebugName() {
  DisallowHeapAllocation no_gc;
  String* function_name = Name();
  if (function_name->length() > 0) return function_name;
  return inferred_name();
}

bool SharedFunctionInfo::PassesFilter(const char* raw_filter) {
  Vector<const char> filter = CStrVector(raw_filter);
  std::unique_ptr<char[]> cstrname(DebugName()->ToCString());
  return v8::internal::PassesFilter(CStrVector(cstrname.get()), filter);
}

bool SharedFunctionInfo::HasSourceCode() const {
  Isolate* isolate = GetIsolate();
  return !script()->IsUndefined(isolate) &&
         !reinterpret_cast<Script*>(script())->source()->IsUndefined(isolate);
}

// static
Handle<Object> SharedFunctionInfo::GetSourceCode(
    Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  if (!shared->HasSourceCode()) return isolate->factory()->undefined_value();
  Handle<String> source(String::cast(Script::cast(shared->script())->source()),
                        isolate);
  return isolate->factory()->NewSubString(source, shared->StartPosition(),
                                          shared->EndPosition());
}

// static
Handle<Object> SharedFunctionInfo::GetSourceCodeHarmony(
    Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  if (!shared->HasSourceCode()) return isolate->factory()->undefined_value();
  Handle<String> script_source(
      String::cast(Script::cast(shared->script())->source()), isolate);
  int start_pos = shared->function_token_position();
  DCHECK_NE(start_pos, kNoSourcePosition);
  Handle<String> source = isolate->factory()->NewSubString(
      script_source, start_pos, shared->EndPosition());
  if (!shared->is_wrapped()) return source;

  DCHECK(!shared->name_should_print_as_anonymous());
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("function ");
  builder.AppendString(Handle<String>(shared->Name(), isolate));
  builder.AppendCString("(");
  Handle<FixedArray> args(Script::cast(shared->script())->wrapped_arguments(),
                          isolate);
  int argc = args->length();
  for (int i = 0; i < argc; i++) {
    if (i > 0) builder.AppendCString(", ");
    builder.AppendString(Handle<String>(String::cast(args->get(i)), isolate));
  }
  builder.AppendCString(") {\n");
  builder.AppendString(source);
  builder.AppendCString("\n}");
  return builder.Finish().ToHandleChecked();
}

bool SharedFunctionInfo::IsInlineable() {
  // Check that the function has a script associated with it.
  if (!script()->IsScript()) return false;
  if (GetIsolate()->is_precise_binary_code_coverage() &&
      !has_reported_binary_coverage()) {
    // We may miss invocations if this function is inlined.
    return false;
  }
  return !optimization_disabled();
}

int SharedFunctionInfo::SourceSize() { return EndPosition() - StartPosition(); }

int SharedFunctionInfo::FindIndexInScript(Isolate* isolate) const {
  DisallowHeapAllocation no_gc;

  Object* script_obj = script();
  if (!script_obj->IsScript()) return FunctionLiteral::kIdTypeInvalid;

  WeakFixedArray* shared_info_list =
      Script::cast(script_obj)->shared_function_infos();
  SharedFunctionInfo::ScriptIterator iterator(
      isolate, Handle<WeakFixedArray>(&shared_info_list));

  for (SharedFunctionInfo* shared = iterator.Next(); shared != nullptr;
       shared = iterator.Next()) {
    if (shared == this) {
      return iterator.CurrentIndex();
    }
  }

  return FunctionLiteral::kIdTypeInvalid;
}

void JSFunction::CalculateInstanceSizeHelper(InstanceType instance_type,
                                             bool has_prototype_slot,
                                             int requested_embedder_fields,
                                             int requested_in_object_properties,
                                             int* instance_size,
                                             int* in_object_properties) {
  DCHECK_LE(static_cast<unsigned>(requested_embedder_fields),
            JSObject::kMaxEmbedderFields);
  int header_size = JSObject::GetHeaderSize(instance_type, has_prototype_slot);
  int max_nof_fields =
      (JSObject::kMaxInstanceSize - header_size) >> kPointerSizeLog2;
  CHECK_LE(max_nof_fields, JSObject::kMaxInObjectProperties);
  CHECK_LE(static_cast<unsigned>(requested_embedder_fields),
           static_cast<unsigned>(max_nof_fields));
  *in_object_properties = Min(requested_in_object_properties,
                              max_nof_fields - requested_embedder_fields);
  *instance_size =
      header_size +
      ((requested_embedder_fields + *in_object_properties) << kPointerSizeLog2);
  CHECK_EQ(*in_object_properties,
           ((*instance_size - header_size) >> kPointerSizeLog2) -
               requested_embedder_fields);
  CHECK_LE(static_cast<unsigned>(*instance_size),
           static_cast<unsigned>(JSObject::kMaxInstanceSize));
}

// static
bool JSFunction::CalculateInstanceSizeForDerivedClass(
    Handle<JSFunction> function, InstanceType instance_type,
    int requested_embedder_fields, int* instance_size,
    int* in_object_properties) {
  Isolate* isolate = function->GetIsolate();
  int expected_nof_properties = 0;
  for (PrototypeIterator iter(isolate, function, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    if (!current->IsJSFunction()) break;
    Handle<JSFunction> func(Handle<JSFunction>::cast(current));
    // The super constructor should be compiled for the number of expected
    // properties to be available.
    Handle<SharedFunctionInfo> shared(func->shared(), isolate);
    if (shared->is_compiled() ||
        Compiler::Compile(func, Compiler::CLEAR_EXCEPTION)) {
      DCHECK(shared->is_compiled());
      int count = shared->expected_nof_properties();
      // Check that the estimate is sane.
      if (expected_nof_properties <= JSObject::kMaxInObjectProperties - count) {
        expected_nof_properties += count;
      } else {
        expected_nof_properties = JSObject::kMaxInObjectProperties;
      }
    } else if (!shared->is_compiled()) {
      // In case there was a compilation error for the constructor we will
      // throw an error during instantiation. Hence we directly return 0;
      return false;
    }
    if (!IsDerivedConstructor(shared->kind())) break;
  }
  CalculateInstanceSizeHelper(instance_type, true, requested_embedder_fields,
                              expected_nof_properties, instance_size,
                              in_object_properties);
  return true;
}


// Output the source code without any allocation in the heap.
std::ostream& operator<<(std::ostream& os, const SourceCodeOf& v) {
  const SharedFunctionInfo* s = v.value;
  // For some native functions there is no source.
  if (!s->HasSourceCode()) return os << "<No Source>";

  // Get the source for the script which this function came from.
  // Don't use String::cast because we don't want more assertion errors while
  // we are already creating a stack dump.
  String* script_source =
      reinterpret_cast<String*>(Script::cast(s->script())->source());

  if (!script_source->LooksValid()) return os << "<Invalid Source>";

  if (!s->is_toplevel()) {
    os << "function ";
    String* name = s->Name();
    if (name->length() > 0) {
      name->PrintUC16(os);
    }
  }

  int len = s->EndPosition() - s->StartPosition();
  if (len <= v.max_length || v.max_length < 0) {
    script_source->PrintUC16(os, s->StartPosition(), s->EndPosition());
    return os;
  } else {
    script_source->PrintUC16(os, s->StartPosition(),
                             s->StartPosition() + v.max_length);
    return os << "...\n";
  }
}


void SharedFunctionInfo::DisableOptimization(BailoutReason reason) {
  DCHECK_NE(reason, BailoutReason::kNoReason);

  set_flags(DisabledOptimizationReasonBits::update(flags(), reason));
  // Code should be the lazy compilation stub or else interpreted.
  DCHECK(abstract_code()->kind() == AbstractCode::INTERPRETED_FUNCTION ||
         abstract_code()->kind() == AbstractCode::BUILTIN);
  PROFILE(GetIsolate(), CodeDisableOptEvent(abstract_code(), this));
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for ");
    ShortPrint();
    PrintF(", reason: %s]\n", GetBailoutReason(reason));
  }
}

void SharedFunctionInfo::InitFromFunctionLiteral(
    Handle<SharedFunctionInfo> shared_info, FunctionLiteral* lit,
    bool is_toplevel) {
  Isolate* isolate = shared_info->GetIsolate();
  bool needs_position_info = true;

  // When adding fields here, make sure DeclarationScope::AnalyzePartially is
  // updated accordingly.
  shared_info->set_internal_formal_parameter_count(lit->parameter_count());
  shared_info->SetFunctionTokenPosition(lit->function_token_position(),
                                        lit->start_position());
  if (shared_info->scope_info()->HasPositionInfo()) {
    shared_info->scope_info()->SetPositionInfo(lit->start_position(),
                                               lit->end_position());
    needs_position_info = false;
  }
  shared_info->set_is_declaration(lit->is_declaration());
  shared_info->set_is_named_expression(lit->is_named_expression());
  shared_info->set_is_anonymous_expression(lit->is_anonymous_expression());
  shared_info->set_allows_lazy_compilation(lit->AllowsLazyCompilation());
  shared_info->set_language_mode(lit->language_mode());
  shared_info->set_is_wrapped(lit->is_wrapped());
  //  shared_info->set_kind(lit->kind());
  // FunctionKind must have already been set.
  DCHECK(lit->kind() == shared_info->kind());
  shared_info->set_needs_home_object(lit->scope()->NeedsHomeObject());
  DCHECK_IMPLIES(lit->requires_instance_fields_initializer(),
                 IsClassConstructor(lit->kind()));
  shared_info->set_requires_instance_fields_initializer(
      lit->requires_instance_fields_initializer());

  shared_info->set_is_toplevel(is_toplevel);
  DCHECK(shared_info->outer_scope_info()->IsTheHole());
  if (!is_toplevel) {
    Scope* outer_scope = lit->scope()->GetOuterScopeWithContext();
    if (outer_scope) {
      shared_info->set_outer_scope_info(*outer_scope->scope_info());
    }
  }

  // For lazy parsed functions, the following flags will be inaccurate since we
  // don't have the information yet. They're set later in
  // SetSharedFunctionFlagsFromLiteral (compiler.cc), when the function is
  // really parsed and compiled.
  if (lit->body() != nullptr) {
    shared_info->set_length(lit->function_length());
    shared_info->set_has_duplicate_parameters(lit->has_duplicate_parameters());
    shared_info->SetExpectedNofPropertiesFromEstimate(lit);
    DCHECK_NULL(lit->produced_preparsed_scope_data());
    if (lit->ShouldEagerCompile()) {
      // If we're about to eager compile, we'll have the function literal
      // available, so there's no need to wastefully allocate an uncompiled
      // data.
      // TODO(leszeks): This should be explicitly passed as a parameter, rather
      // than relying on a property of the literal.
      needs_position_info = false;
    }
  } else {
    // Set an invalid length for lazy functions. This way we can set the correct
    // value after compiling, but avoid overwriting values set manually by the
    // bootstrapper.
    shared_info->set_length(SharedFunctionInfo::kInvalidLength);
    if (FLAG_preparser_scope_analysis) {
      ProducedPreParsedScopeData* scope_data =
          lit->produced_preparsed_scope_data();
      if (scope_data != nullptr) {
        Handle<PreParsedScopeData> pre_parsed_scope_data;
        if (scope_data->Serialize(shared_info->GetIsolate())
                .ToHandle(&pre_parsed_scope_data)) {
          Handle<UncompiledData> data =
              isolate->factory()->NewUncompiledDataWithPreParsedScope(
                  lit->inferred_name(), lit->start_position(),
                  lit->end_position(), lit->function_literal_id(),
                  pre_parsed_scope_data);
          shared_info->set_uncompiled_data(*data);
          needs_position_info = false;
        }
      }
    }
  }
  if (needs_position_info) {
    Handle<UncompiledData> data =
        isolate->factory()->NewUncompiledDataWithoutPreParsedScope(
            lit->inferred_name(), lit->start_position(), lit->end_position(),
            lit->function_literal_id());
    shared_info->set_uncompiled_data(*data);
  }
}

void SharedFunctionInfo::SetExpectedNofPropertiesFromEstimate(
    FunctionLiteral* literal) {
  int estimate = literal->expected_property_count();

  // If no properties are added in the constructor, they are more likely
  // to be added later.
  if (estimate == 0) estimate = 2;

  // Inobject slack tracking will reclaim redundant inobject space later,
  // so we can afford to adjust the estimate generously.
  estimate += 8;

  // Limit actual estimate to fit in a 8 bit field, we will never allocate
  // more than this in any case.
  STATIC_ASSERT(JSObject::kMaxInObjectProperties <= kMaxUInt8);
  estimate = std::min(estimate, kMaxUInt8);

  set_expected_nof_properties(estimate);
}

void SharedFunctionInfo::SetFunctionTokenPosition(int function_token_position,
                                                  int start_position) {
  int offset;
  if (function_token_position == kNoSourcePosition) {
    offset = 0;
  } else {
    offset = start_position - function_token_position;
  }

  if (offset > kMaximumFunctionTokenOffset) {
    offset = kFunctionTokenOutOfRange;
  }
  set_raw_function_token_offset(offset);
}

int SharedFunctionInfo::StartPosition() const {
  Object* maybe_scope_info = name_or_scope_info();
  if (maybe_scope_info->IsScopeInfo()) {
    ScopeInfo* info = ScopeInfo::cast(maybe_scope_info);
    if (info->HasPositionInfo()) {
      return info->StartPosition();
    }
  } else if (HasUncompiledData()) {
    // Works with or without scope.
    return uncompiled_data()->start_position();
  } else if (IsApiFunction() || HasBuiltinId()) {
    DCHECK_IMPLIES(HasBuiltinId(), builtin_id() != Builtins::kCompileLazy);
    return 0;
  }
  return kNoSourcePosition;
}

int SharedFunctionInfo::EndPosition() const {
  Object* maybe_scope_info = name_or_scope_info();
  if (maybe_scope_info->IsScopeInfo()) {
    ScopeInfo* info = ScopeInfo::cast(maybe_scope_info);
    if (info->HasPositionInfo()) {
      return info->EndPosition();
    }
  } else if (HasUncompiledData()) {
    // Works with or without scope.
    return uncompiled_data()->end_position();
  } else if (IsApiFunction() || HasBuiltinId()) {
    DCHECK_IMPLIES(HasBuiltinId(), builtin_id() != Builtins::kCompileLazy);
    return 0;
  }
  return kNoSourcePosition;
}

int SharedFunctionInfo::FunctionLiteralId(Isolate* isolate) const {
  // Fast path for the common case when the SFI is uncompiled and so the
  // function literal id is already in the uncompiled data.
  if (HasUncompiledData()) {
    int id = uncompiled_data()->function_literal_id();
    // Make sure the id is what we should have found with the slow path.
    DCHECK_EQ(id, FindIndexInScript(isolate));
    return id;
  }

  // Otherwise, search for the function in the SFI's script's function list,
  // and return its index in that list.e
  return FindIndexInScript(isolate);
}

void SharedFunctionInfo::SetPosition(int start_position, int end_position) {
  Object* maybe_scope_info = name_or_scope_info();
  if (maybe_scope_info->IsScopeInfo()) {
    ScopeInfo* info = ScopeInfo::cast(maybe_scope_info);
    if (info->HasPositionInfo()) {
      info->SetPositionInfo(start_position, end_position);
    }
  } else if (HasUncompiledData()) {
    if (HasUncompiledDataWithPreParsedScope()) {
      // Clear out preparsed scope data, since the position setter invalidates
      // any scope data.
      ClearPreParsedScopeData();
    }
    uncompiled_data()->set_start_position(start_position);
    uncompiled_data()->set_end_position(end_position);
  } else {
    UNREACHABLE();
  }
}

void Map::StartInobjectSlackTracking() {
  DCHECK(!IsInobjectSlackTrackingInProgress());
  if (UnusedPropertyFields() == 0) return;
  set_construction_counter(Map::kSlackTrackingCounterStart);
}

void ObjectVisitor::VisitCodeTarget(Code* host, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
  Object* old_pointer = Code::GetCodeFromTargetAddress(rinfo->target_address());
  Object* new_pointer = old_pointer;
  VisitPointer(host, &new_pointer);
  DCHECK_EQ(old_pointer, new_pointer);
}

void ObjectVisitor::VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  Object* old_pointer = rinfo->target_object();
  Object* new_pointer = old_pointer;
  VisitPointer(host, &new_pointer);
  DCHECK_EQ(old_pointer, new_pointer);
}

void ObjectVisitor::VisitRelocInfo(RelocIterator* it) {
  for (; !it->done(); it->next()) {
    it->rinfo()->Visit(this);
  }
}

void Code::InvalidateEmbeddedObjects(Heap* heap) {
  HeapObject* undefined = ReadOnlyRoots(heap).undefined_value();
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      it.rinfo()->set_target_object(heap, undefined, SKIP_WRITE_BARRIER);
    }
  }
}


void Code::Relocate(intptr_t delta) {
  for (RelocIterator it(this, RelocInfo::kApplyMask); !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  Assembler::FlushICache(raw_instruction_start(), raw_instruction_size());
}

void Code::FlushICache() const {
  Assembler::FlushICache(raw_instruction_start(), raw_instruction_size());
}

void Code::CopyFromNoFlush(Heap* heap, const CodeDesc& desc) {
  // Copy code.
  CopyBytes(reinterpret_cast<byte*>(raw_instruction_start()), desc.buffer,
            static_cast<size_t>(desc.instr_size));

  // Copy unwinding info, if any.
  if (desc.unwinding_info) {
    DCHECK_GT(desc.unwinding_info_size, 0);
    set_unwinding_info_size(desc.unwinding_info_size);
    CopyBytes(reinterpret_cast<byte*>(unwinding_info_start()),
              desc.unwinding_info,
              static_cast<size_t>(desc.unwinding_info_size));
  }

  // Copy reloc info.
  CopyBytes(relocation_start(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));

  // Unbox handles and relocate.
  Assembler* origin = desc.origin;
  AllowDeferredHandleDereference embedding_raw_address;
  const int mode_mask = RelocInfo::PostCodegenRelocationMask();
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      Handle<HeapObject> p = it.rinfo()->target_object_handle(origin);
      it.rinfo()->set_target_object(heap, *p, UPDATE_WRITE_BARRIER,
                                    SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTargetMode(mode)) {
      // Rewrite code handles to direct pointers to the first instruction in the
      // code object.
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(code->raw_instruction_start(),
                                     UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(p, UPDATE_WRITE_BARRIER,
                                           SKIP_ICACHE_FLUSH);
    } else {
      intptr_t delta =
          raw_instruction_start() - reinterpret_cast<Address>(desc.buffer);
      it.rinfo()->apply(delta);
    }
  }
}


SafepointEntry Code::GetSafepointEntry(Address pc) {
  SafepointTable table(this);
  return table.FindEntry(pc);
}

int Code::OffHeapInstructionSize() const {
  DCHECK(is_off_heap_trampoline());
  if (Isolate::CurrentEmbeddedBlob() == nullptr) return raw_instruction_size();
  EmbeddedData d = EmbeddedData::FromBlob();
  return d.InstructionSizeOfBuiltin(builtin_index());
}

Address Code::OffHeapInstructionStart() const {
  DCHECK(is_off_heap_trampoline());
  if (Isolate::CurrentEmbeddedBlob() == nullptr) return raw_instruction_start();
  EmbeddedData d = EmbeddedData::FromBlob();
  return d.InstructionStartOfBuiltin(builtin_index());
}

Address Code::OffHeapInstructionEnd() const {
  DCHECK(is_off_heap_trampoline());
  if (Isolate::CurrentEmbeddedBlob() == nullptr) return raw_instruction_end();
  EmbeddedData d = EmbeddedData::FromBlob();
  return d.InstructionStartOfBuiltin(builtin_index()) +
         d.InstructionSizeOfBuiltin(builtin_index());
}

namespace {
template <typename Code>
void SetStackFrameCacheCommon(Isolate* isolate, Handle<Code> code,
                              Handle<SimpleNumberDictionary> cache) {
  Handle<Object> maybe_table(code->source_position_table(), isolate);
  if (maybe_table->IsSourcePositionTableWithFrameCache()) {
    Handle<SourcePositionTableWithFrameCache>::cast(maybe_table)
        ->set_stack_frame_cache(*cache);
    return;
  }
  DCHECK(maybe_table->IsByteArray());
  Handle<ByteArray> table(Handle<ByteArray>::cast(maybe_table));
  Handle<SourcePositionTableWithFrameCache> table_with_cache =
      isolate->factory()->NewSourcePositionTableWithFrameCache(table, cache);
  code->set_source_position_table(*table_with_cache);
}
}  // namespace

// static
void AbstractCode::SetStackFrameCache(Handle<AbstractCode> abstract_code,
                                      Handle<SimpleNumberDictionary> cache) {
  if (abstract_code->IsCode()) {
    SetStackFrameCacheCommon(
        abstract_code->GetIsolate(),
        handle(abstract_code->GetCode(), abstract_code->GetIsolate()), cache);
  } else {
    SetStackFrameCacheCommon(
        abstract_code->GetIsolate(),
        handle(abstract_code->GetBytecodeArray(), abstract_code->GetIsolate()),
        cache);
  }
}

namespace {
template <typename Code>
void DropStackFrameCacheCommon(Code* code) {
  i::Object* maybe_table = code->source_position_table();
  if (maybe_table->IsByteArray()) return;
  DCHECK(maybe_table->IsSourcePositionTableWithFrameCache());
  code->set_source_position_table(
      i::SourcePositionTableWithFrameCache::cast(maybe_table)
          ->source_position_table());
}
}  // namespace

void AbstractCode::DropStackFrameCache() {
  if (IsCode()) {
    DropStackFrameCacheCommon(GetCode());
  } else {
    DropStackFrameCacheCommon(GetBytecodeArray());
  }
}

int AbstractCode::SourcePosition(int offset) {
  int position = 0;
  // Subtract one because the current PC is one instruction after the call site.
  if (IsCode()) offset--;
  for (SourcePositionTableIterator iterator(source_position_table());
       !iterator.done() && iterator.code_offset() <= offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

int AbstractCode::SourceStatementPosition(int offset) {
  // First find the closest position.
  int position = SourcePosition(offset);
  // Now find the closest statement position before the position.
  int statement_position = 0;
  for (SourcePositionTableIterator it(source_position_table()); !it.done();
       it.Advance()) {
    if (it.is_statement()) {
      int p = it.source_position().ScriptOffset();
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
  }
  return statement_position;
}

void JSFunction::ClearTypeFeedbackInfo() {
  if (feedback_cell()->value()->IsFeedbackVector()) {
    FeedbackVector* vector = feedback_vector();
    Isolate* isolate = GetIsolate();
    if (vector->ClearSlots(isolate)) {
      IC::OnFeedbackChanged(isolate, vector, FeedbackSlot::Invalid(), this,
                            "ClearTypeFeedbackInfo");
    }
  }
}

void Code::PrintDeoptLocation(FILE* out, const char* str, Address pc) {
  Deoptimizer::DeoptInfo info = Deoptimizer::GetDeoptInfo(this, pc);
  class SourcePosition pos = info.position;
  if (info.deopt_reason != DeoptimizeReason::kUnknown || pos.IsKnown()) {
    PrintF(out, "%s", str);
    OFStream outstr(out);
    pos.Print(outstr, this);
    PrintF(out, ", %s\n", DeoptimizeReasonToString(info.deopt_reason));
  }
}


bool Code::CanDeoptAt(Address pc) {
  DeoptimizationData* deopt_data =
      DeoptimizationData::cast(deoptimization_data());
  Address code_start_address = InstructionStart();
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    Address address = code_start_address + deopt_data->Pc(i)->value();
    if (address == pc && deopt_data->BytecodeOffset(i) != BailoutId::None()) {
      return true;
    }
  }
  return false;
}


// Identify kind of code.
const char* Code::Kind2String(Kind kind) {
  switch (kind) {
#define CASE(name) case name: return #name;
    CODE_KIND_LIST(CASE)
#undef CASE
    case NUMBER_OF_KINDS: break;
  }
  UNREACHABLE();
}

// Identify kind of code.
const char* AbstractCode::Kind2String(Kind kind) {
  if (kind < AbstractCode::INTERPRETED_FUNCTION)
    return Code::Kind2String(static_cast<Code::Kind>(kind));
  if (kind == AbstractCode::INTERPRETED_FUNCTION) return "INTERPRETED_FUNCTION";
  UNREACHABLE();
}

bool Code::IsIsolateIndependent(Isolate* isolate) {
  constexpr int all_real_modes_mask =
      (1 << (RelocInfo::LAST_REAL_RELOC_MODE + 1)) - 1;
  constexpr int mode_mask = all_real_modes_mask &
                            ~RelocInfo::ModeMask(RelocInfo::COMMENT) &
                            ~RelocInfo::ModeMask(RelocInfo::CONST_POOL) &
                            ~RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) &
                            ~RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  STATIC_ASSERT(RelocInfo::LAST_REAL_RELOC_MODE == RelocInfo::VENEER_POOL);
  STATIC_ASSERT(RelocInfo::ModeMask(RelocInfo::COMMENT) ==
                (1 << RelocInfo::COMMENT));
  STATIC_ASSERT(mode_mask ==
                (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
                 RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET) |
                 RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                 RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                 RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                 RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
                 RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL) |
                 RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
                 RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
                 RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL)));

  bool is_process_independent = true;
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64) || \
    defined(V8_TARGET_ARCH_ARM)
    // On X64, ARM, ARM64 we emit relative builtin-to-builtin jumps for isolate
    // independent builtins in the snapshot. They are later rewritten as
    // pc-relative jumps to the off-heap instruction stream and are thus
    // process-independent.
    // See also: FinalizeEmbeddedCodeTargets.
    if (RelocInfo::IsCodeTargetMode(it.rinfo()->rmode())) {
      Address target_address = it.rinfo()->target_address();
      if (InstructionStream::PcIsOffHeap(isolate, target_address)) continue;

      Code* target = Code::GetCodeFromTargetAddress(target_address);
      CHECK(target->IsCode());
      if (Builtins::IsIsolateIndependentBuiltin(target)) continue;
    }
#endif
    is_process_independent = false;
  }

  return is_process_independent;
}

bool Code::Inlines(SharedFunctionInfo* sfi) {
  // We can only check for inlining for optimized code.
  DCHECK(is_optimized_code());
  DisallowHeapAllocation no_gc;
  DeoptimizationData* const data =
      DeoptimizationData::cast(deoptimization_data());
  if (data->length() == 0) return false;
  if (data->SharedFunctionInfo() == sfi) return true;
  FixedArray* const literals = data->LiteralArray();
  int const inlined_count = data->InlinedFunctionCount()->value();
  for (int i = 0; i < inlined_count; ++i) {
    if (SharedFunctionInfo::cast(literals->get(i)) == sfi) return true;
  }
  return false;
}

Code::OptimizedCodeIterator::OptimizedCodeIterator(Isolate* isolate) {
  isolate_ = isolate;
  Object* list = isolate->heap()->native_contexts_list();
  next_context_ = list->IsUndefined(isolate_) ? nullptr : Context::cast(list);
  current_code_ = nullptr;
}

Code* Code::OptimizedCodeIterator::Next() {
  do {
    Object* next;
    if (current_code_ != nullptr) {
      // Get next code in the linked list.
      next = Code::cast(current_code_)->next_code_link();
    } else if (next_context_ != nullptr) {
      // Linked list of code exhausted. Get list of next context.
      next = next_context_->OptimizedCodeListHead();
      Object* next_context = next_context_->next_context_link();
      next_context_ = next_context->IsUndefined(isolate_)
                          ? nullptr
                          : Context::cast(next_context);
    } else {
      // Exhausted contexts.
      return nullptr;
    }
    current_code_ = next->IsUndefined(isolate_) ? nullptr : Code::cast(next);
  } while (current_code_ == nullptr);
  Code* code = Code::cast(current_code_);
  DCHECK_EQ(Code::OPTIMIZED_FUNCTION, code->kind());
  return code;
}

#ifdef ENABLE_DISASSEMBLER

namespace {
void print_pc(std::ostream& os, int pc) {
  if (pc == -1) {
    os << "NA";
  } else {
    os << std::hex << pc << std::dec;
  }
}
}  // anonymous namespace

void DeoptimizationData::DeoptimizationDataPrint(std::ostream& os) {  // NOLINT
  if (length() == 0) {
    os << "Deoptimization Input Data invalidated by lazy deoptimization\n";
    return;
  }

  disasm::NameConverter converter;
  int const inlined_function_count = InlinedFunctionCount()->value();
  os << "Inlined functions (count = " << inlined_function_count << ")\n";
  for (int id = 0; id < inlined_function_count; ++id) {
    Object* info = LiteralArray()->get(id);
    os << " " << Brief(SharedFunctionInfo::cast(info)) << "\n";
  }
  os << "\n";
  int deopt_count = DeoptCount();
  os << "Deoptimization Input Data (deopt points = " << deopt_count << ")\n";
  if (0 != deopt_count) {
    os << " index  bytecode-offset    pc";
    if (FLAG_print_code_verbose) os << "  commands";
    os << "\n";
  }
  for (int i = 0; i < deopt_count; i++) {
    os << std::setw(6) << i << "  " << std::setw(15)
       << BytecodeOffset(i).ToInt() << "  " << std::setw(4);
    print_pc(os, Pc(i)->value());
    os << std::setw(2);

    if (!FLAG_print_code_verbose) {
      os << "\n";
      continue;
    }

    // Print details of the frame translation.
    int translation_index = TranslationIndex(i)->value();
    TranslationIterator iterator(TranslationByteArray(), translation_index);
    Translation::Opcode opcode =
        static_cast<Translation::Opcode>(iterator.Next());
    DCHECK(Translation::BEGIN == opcode);
    int frame_count = iterator.Next();
    int jsframe_count = iterator.Next();
    int update_feedback_count = iterator.Next();
    os << "  " << Translation::StringFor(opcode)
       << " {frame count=" << frame_count
       << ", js frame count=" << jsframe_count
       << ", update_feedback_count=" << update_feedback_count << "}\n";

    while (iterator.HasNext() &&
           Translation::BEGIN !=
           (opcode = static_cast<Translation::Opcode>(iterator.Next()))) {
      os << std::setw(31) << "    " << Translation::StringFor(opcode) << " ";

      switch (opcode) {
        case Translation::BEGIN:
          UNREACHABLE();
          break;

        case Translation::INTERPRETED_FRAME: {
          int bytecode_offset = iterator.Next();
          int shared_info_id = iterator.Next();
          unsigned height = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          os << "{bytecode_offset=" << bytecode_offset << ", function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::CONSTRUCT_STUB_FRAME: {
          int bailout_id = iterator.Next();
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          unsigned height = iterator.Next();
          os << "{bailout_id=" << bailout_id << ", function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::BUILTIN_CONTINUATION_FRAME:
        case Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME:
        case Translation::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME: {
          int bailout_id = iterator.Next();
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          unsigned height = iterator.Next();
          os << "{bailout_id=" << bailout_id << ", function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::ARGUMENTS_ADAPTOR_FRAME: {
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          unsigned height = iterator.Next();
          os << "{function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code) << "}";
          break;
        }

        case Translation::INT32_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code)
             << " (int32)}";
          break;
        }

        case Translation::INT64_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code)
             << " (int64)}";
          break;
        }

        case Translation::UINT32_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code)
             << " (uint32)}";
          break;
        }

        case Translation::BOOL_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code)
             << " (bool)}";
          break;
        }

        case Translation::FLOAT_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input="
             << RegisterConfiguration::Default()->GetFloatRegisterName(reg_code)
             << "}";
          break;
        }

        case Translation::DOUBLE_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input="
             << RegisterConfiguration::Default()->GetDoubleRegisterName(
                    reg_code)
             << "}";
          break;
        }

        case Translation::STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::INT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << " (int32)}";
          break;
        }

        case Translation::INT64_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << " (int64)}";
          break;
        }

        case Translation::UINT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << " (uint32)}";
          break;
        }

        case Translation::BOOL_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << " (bool)}";
          break;
        }

        case Translation::FLOAT_STACK_SLOT:
        case Translation::DOUBLE_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::LITERAL: {
          int literal_index = iterator.Next();
          Object* literal_value = LiteralArray()->get(literal_index);
          os << "{literal_id=" << literal_index << " (" << Brief(literal_value)
             << ")}";
          break;
        }

        case Translation::DUPLICATED_OBJECT: {
          int object_index = iterator.Next();
          os << "{object_index=" << object_index << "}";
          break;
        }

        case Translation::ARGUMENTS_ELEMENTS:
        case Translation::ARGUMENTS_LENGTH: {
          CreateArgumentsType arguments_type =
              static_cast<CreateArgumentsType>(iterator.Next());
          os << "{arguments_type=" << arguments_type << "}";
          break;
        }

        case Translation::CAPTURED_OBJECT: {
          int args_length = iterator.Next();
          os << "{length=" << args_length << "}";
          break;
        }

        case Translation::UPDATE_FEEDBACK: {
          int literal_index = iterator.Next();
          FeedbackSlot slot(iterator.Next());
          os << "{feedback={vector_index=" << literal_index << ", slot=" << slot
             << "}}";
          break;
        }
      }
      os << "\n";
    }
  }
}

const char* Code::GetName(Isolate* isolate) const {
  if (is_stub()) {
    return CodeStub::MajorName(CodeStub::GetMajorKey(this));
  } else if (kind() == BYTECODE_HANDLER) {
    return isolate->interpreter()->LookupNameOfBytecodeHandler(this);
  } else {
    // There are some handlers and ICs that we can also find names for with
    // Builtins::Lookup.
    return isolate->builtins()->Lookup(raw_instruction_start());
  }
}

void Code::PrintBuiltinCode(Isolate* isolate, const char* name) {
  DCHECK(FLAG_print_builtin_code);
  if (name == nullptr) {
    name = GetName(isolate);
  }
  if (name != nullptr &&
      PassesFilter(CStrVector(name),
                   CStrVector(FLAG_print_builtin_code_filter))) {
    CodeTracer::Scope trace_scope(isolate->GetCodeTracer());
    OFStream os(trace_scope.file());
    Disassemble(name, os);
    os << "\n";
  }
}

namespace {

inline void DisassembleCodeRange(Isolate* isolate, std::ostream& os, Code* code,
                                 Address begin, size_t size,
                                 Address current_pc) {
  Address end = begin + size;
  // TODO(mstarzinger): Refactor CodeReference to avoid the
  // unhandlified->handlified transition.
  AllowHandleAllocation allow_handles;
  DisallowHeapAllocation no_gc;
  HandleScope handle_scope(isolate);
  Disassembler::Decode(isolate, &os, reinterpret_cast<byte*>(begin),
                       reinterpret_cast<byte*>(end),
                       CodeReference(handle(code, isolate)), current_pc);
}

}  // namespace

void Code::Disassemble(const char* name, std::ostream& os, Address current_pc) {
  Isolate* isolate = GetIsolate();
  os << "kind = " << Kind2String(kind()) << "\n";
  if (is_stub()) {
    const char* n = CodeStub::MajorName(CodeStub::GetMajorKey(this));
    os << "major_key = " << (n == nullptr ? "null" : n) << "\n";
    os << "minor_key = " << CodeStub::MinorKeyFromKey(this->stub_key()) << "\n";
  }
  if (name == nullptr) {
    name = GetName(isolate);
  }
  if ((name != nullptr) && (name[0] != '\0')) {
    os << "name = " << name << "\n";
  }
  if (kind() == OPTIMIZED_FUNCTION) {
    os << "stack_slots = " << stack_slots() << "\n";
  }
  os << "compiler = " << (is_turbofanned() ? "turbofan" : "unknown") << "\n";
  os << "address = " << static_cast<const void*>(this) << "\n\n";

  if (is_off_heap_trampoline()) {
    int trampoline_size = raw_instruction_size();
    os << "Trampoline (size = " << trampoline_size << ")\n";
    DisassembleCodeRange(isolate, os, this, raw_instruction_start(),
                         trampoline_size, current_pc);
    os << "\n";
  }

  {
    int size = InstructionSize();
    int safepoint_offset =
        has_safepoint_info() ? safepoint_table_offset() : size;
    int constant_pool_offset = this->constant_pool_offset();
    int handler_offset = handler_table_offset() ? handler_table_offset() : size;

    // Stop before reaching any embedded tables
    int code_size =
        Min(handler_offset, Min(safepoint_offset, constant_pool_offset));
    os << "Instructions (size = " << code_size << ")\n";
    DisassembleCodeRange(isolate, os, this, InstructionStart(), code_size,
                         current_pc);

    if (constant_pool_offset < size) {
      int constant_pool_size = safepoint_offset - constant_pool_offset;
      DCHECK_EQ(constant_pool_size & kPointerAlignmentMask, 0);
      os << "\nConstant Pool (size = " << constant_pool_size << ")\n";
      Vector<char> buf = Vector<char>::New(50);
      intptr_t* ptr = reinterpret_cast<intptr_t*>(InstructionStart() +
                                                  constant_pool_offset);
      for (int i = 0; i < constant_pool_size; i += kPointerSize, ptr++) {
        SNPrintF(buf, "%4d %08" V8PRIxPTR, i, *ptr);
        os << static_cast<const void*>(ptr) << "  " << buf.start() << "\n";
      }
    }
  }
  os << "\n";

  SourcePositionTableIterator it(SourcePositionTable());
  if (!it.done()) {
    os << "Source positions:\n pc offset  position\n";
    for (; !it.done(); it.Advance()) {
      os << std::setw(10) << std::hex << it.code_offset() << std::dec
         << std::setw(10) << it.source_position().ScriptOffset()
         << (it.is_statement() ? "  statement" : "") << "\n";
    }
    os << "\n";
  }

  if (kind() == OPTIMIZED_FUNCTION) {
    DeoptimizationData* data =
        DeoptimizationData::cast(this->deoptimization_data());
    data->DeoptimizationDataPrint(os);
  }
  os << "\n";

  if (has_safepoint_info()) {
    SafepointTable table(this);
    os << "Safepoints (size = " << table.size() << ")\n";
    for (unsigned i = 0; i < table.length(); i++) {
      unsigned pc_offset = table.GetPcOffset(i);
      os << reinterpret_cast<const void*>(InstructionStart() + pc_offset)
         << "  ";
      os << std::setw(6) << std::hex << pc_offset << "  " << std::setw(4);
      int trampoline_pc = table.GetTrampolinePcOffset(i);
      print_pc(os, trampoline_pc);
      os << std::dec << "  ";
      table.PrintEntry(i, os);
      os << " (sp -> fp)  ";
      SafepointEntry entry = table.GetEntry(i);
      if (entry.deoptimization_index() != Safepoint::kNoDeoptimizationIndex) {
        os << std::setw(6) << entry.deoptimization_index();
      } else {
        os << "<none>";
      }
      if (entry.argument_count() > 0) {
        os << " argc: " << entry.argument_count();
      }
      os << "\n";
    }
    os << "\n";
  }

  if (handler_table_offset() > 0) {
    HandlerTable table(this);
    os << "Handler Table (size = " << table.NumberOfReturnEntries() << ")\n";
    if (kind() == OPTIMIZED_FUNCTION) {
      table.HandlerTableReturnPrint(os);
    }
    os << "\n";
  }

  os << "RelocInfo (size = " << relocation_size() << ")\n";
  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Print(isolate, os);
  }
  os << "\n";

  if (has_unwinding_info()) {
    os << "UnwindingInfo (size = " << unwinding_info_size() << ")\n";
    EhFrameDisassembler eh_frame_disassembler(
        reinterpret_cast<byte*>(unwinding_info_start()),
        reinterpret_cast<byte*>(unwinding_info_end()));
    eh_frame_disassembler.DisassembleToStream(os);
    os << "\n";
  }
}
#endif  // ENABLE_DISASSEMBLER

void BytecodeArray::Disassemble(std::ostream& os) {
  DisallowHeapAllocation no_gc;

  os << "Parameter count " << parameter_count() << "\n";
  os << "Frame size " << frame_size() << "\n";

  Address base_address = GetFirstBytecodeAddress();
  SourcePositionTableIterator source_positions(SourcePositionTable());

  // Storage for backing the handle passed to the iterator. This handle won't be
  // updated by the gc, but that's ok because we've disallowed GCs anyway.
  BytecodeArray* handle_storage = this;
  Handle<BytecodeArray> handle(&handle_storage);
  interpreter::BytecodeArrayIterator iterator(handle);
  while (!iterator.done()) {
    if (!source_positions.done() &&
        iterator.current_offset() == source_positions.code_offset()) {
      os << std::setw(5) << source_positions.source_position().ScriptOffset();
      os << (source_positions.is_statement() ? " S> " : " E> ");
      source_positions.Advance();
    } else {
      os << "         ";
    }
    Address current_address = base_address + iterator.current_offset();
    os << reinterpret_cast<const void*>(current_address) << " @ "
       << std::setw(4) << iterator.current_offset() << " : ";
    interpreter::BytecodeDecoder::Decode(
        os, reinterpret_cast<byte*>(current_address), parameter_count());
    if (interpreter::Bytecodes::IsJump(iterator.current_bytecode())) {
      Address jump_target = base_address + iterator.GetJumpTargetOffset();
      os << " (" << reinterpret_cast<void*>(jump_target) << " @ "
         << iterator.GetJumpTargetOffset() << ")";
    }
    if (interpreter::Bytecodes::IsSwitch(iterator.current_bytecode())) {
      os << " {";
      bool first_entry = true;
      for (const auto& entry : iterator.GetJumpTableTargetOffsets()) {
        if (first_entry) {
          first_entry = false;
        } else {
          os << ",";
        }
        os << " " << entry.case_value << ": @" << entry.target_offset;
      }
      os << " }";
    }
    os << std::endl;
    iterator.Advance();
  }

  os << "Constant pool (size = " << constant_pool()->length() << ")\n";
#ifdef OBJECT_PRINT
  if (constant_pool()->length() > 0) {
    constant_pool()->Print();
  }
#endif

  os << "Handler Table (size = " << handler_table()->length() << ")\n";
#ifdef ENABLE_DISASSEMBLER
  if (handler_table()->length() > 0) {
    HandlerTable table(this);
    table.HandlerTableRangePrint(os);
  }
#endif
}

void BytecodeArray::CopyBytecodesTo(BytecodeArray* to) {
  BytecodeArray* from = this;
  DCHECK_EQ(from->length(), to->length());
  CopyBytes(reinterpret_cast<byte*>(to->GetFirstBytecodeAddress()),
            reinterpret_cast<byte*>(from->GetFirstBytecodeAddress()),
            from->length());
}

void BytecodeArray::MakeOlder() {
  // BytecodeArray is aged in concurrent marker.
  // The word must be completely within the byte code array.
  Address age_addr = address() + kBytecodeAgeOffset;
  DCHECK_LE((age_addr & ~kPointerAlignmentMask) + kPointerSize,
            address() + Size());
  Age age = bytecode_age();
  if (age < kLastBytecodeAge) {
    base::AsAtomic8::Release_CompareAndSwap(reinterpret_cast<byte*>(age_addr),
                                            age, age + 1);
  }

  DCHECK_GE(bytecode_age(), kFirstBytecodeAge);
  DCHECK_LE(bytecode_age(), kLastBytecodeAge);
}

bool BytecodeArray::IsOld() const {
  return bytecode_age() >= kIsOldBytecodeAge;
}

// static
void JSArray::Initialize(Handle<JSArray> array, int capacity, int length) {
  DCHECK_GE(capacity, 0);
  array->GetIsolate()->factory()->NewJSArrayStorage(
      array, length, capacity, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
}

void JSArray::SetLength(Handle<JSArray> array, uint32_t new_length) {
  // We should never end in here with a pixel or external array.
  DCHECK(array->AllowsSetLength());
  if (array->SetLengthWouldNormalize(new_length)) {
    JSObject::NormalizeElements(array);
  }
  array->GetElementsAccessor()->SetLength(array, new_length);
}

DependentCode* DependentCode::GetDependentCode(Handle<HeapObject> object) {
  if (object->IsMap()) {
    return Handle<Map>::cast(object)->dependent_code();
  } else if (object->IsPropertyCell()) {
    return Handle<PropertyCell>::cast(object)->dependent_code();
  } else if (object->IsAllocationSite()) {
    return Handle<AllocationSite>::cast(object)->dependent_code();
  }
  UNREACHABLE();
}

void DependentCode::SetDependentCode(Handle<HeapObject> object,
                                     Handle<DependentCode> dep) {
  if (object->IsMap()) {
    Handle<Map>::cast(object)->set_dependent_code(*dep);
  } else if (object->IsPropertyCell()) {
    Handle<PropertyCell>::cast(object)->set_dependent_code(*dep);
  } else if (object->IsAllocationSite()) {
    Handle<AllocationSite>::cast(object)->set_dependent_code(*dep);
  } else {
    UNREACHABLE();
  }
}

void DependentCode::InstallDependency(Isolate* isolate,
                                      const MaybeObjectHandle& code,
                                      Handle<HeapObject> object,
                                      DependencyGroup group) {
  Handle<DependentCode> old_deps(DependentCode::GetDependentCode(object),
                                 isolate);
  Handle<DependentCode> new_deps =
      InsertWeakCode(isolate, old_deps, group, code);
  // Update the list head if necessary.
  if (!new_deps.is_identical_to(old_deps))
    DependentCode::SetDependentCode(object, new_deps);
}

Handle<DependentCode> DependentCode::InsertWeakCode(
    Isolate* isolate, Handle<DependentCode> entries, DependencyGroup group,
    const MaybeObjectHandle& code) {
  if (entries->length() == 0 || entries->group() > group) {
    // There is no such group.
    return DependentCode::New(isolate, group, code, entries);
  }
  if (entries->group() < group) {
    // The group comes later in the list.
    Handle<DependentCode> old_next(entries->next_link(), isolate);
    Handle<DependentCode> new_next =
        InsertWeakCode(isolate, old_next, group, code);
    if (!old_next.is_identical_to(new_next)) {
      entries->set_next_link(*new_next);
    }
    return entries;
  }
  DCHECK_EQ(group, entries->group());
  int count = entries->count();
  // Check for existing entry to avoid duplicates.
  for (int i = 0; i < count; i++) {
    if (entries->object_at(i) == *code) return entries;
  }
  if (entries->length() < kCodesStartIndex + count + 1) {
    entries = EnsureSpace(isolate, entries);
    // Count could have changed, reload it.
    count = entries->count();
  }
  entries->set_object_at(count, *code);
  entries->set_count(count + 1);
  return entries;
}

Handle<DependentCode> DependentCode::New(Isolate* isolate,
                                         DependencyGroup group,
                                         const MaybeObjectHandle& object,
                                         Handle<DependentCode> next) {
  Handle<DependentCode> result = Handle<DependentCode>::cast(
      isolate->factory()->NewWeakFixedArray(kCodesStartIndex + 1, TENURED));
  result->set_next_link(*next);
  result->set_flags(GroupField::encode(group) | CountField::encode(1));
  result->set_object_at(0, *object);
  return result;
}

Handle<DependentCode> DependentCode::EnsureSpace(
    Isolate* isolate, Handle<DependentCode> entries) {
  if (entries->Compact()) return entries;
  int capacity = kCodesStartIndex + DependentCode::Grow(entries->count());
  int grow_by = capacity - entries->length();
  return Handle<DependentCode>::cast(
      isolate->factory()->CopyWeakFixedArrayAndGrow(entries, grow_by, TENURED));
}


bool DependentCode::Compact() {
  int old_count = count();
  int new_count = 0;
  for (int i = 0; i < old_count; i++) {
    MaybeObject* obj = object_at(i);
    if (!obj->IsCleared()) {
      if (i != new_count) {
        copy(i, new_count);
      }
      new_count++;
    }
  }
  set_count(new_count);
  for (int i = new_count; i < old_count; i++) {
    clear_at(i);
  }
  return new_count < old_count;
}

bool DependentCode::MarkCodeForDeoptimization(
    Isolate* isolate,
    DependentCode::DependencyGroup group) {
  if (this->length() == 0 || this->group() > group) {
    // There is no such group.
    return false;
  }
  if (this->group() < group) {
    // The group comes later in the list.
    return next_link()->MarkCodeForDeoptimization(isolate, group);
  }
  DCHECK_EQ(group, this->group());
  DisallowHeapAllocation no_allocation_scope;
  // Mark all the code that needs to be deoptimized.
  bool marked = false;
  int count = this->count();
  for (int i = 0; i < count; i++) {
    MaybeObject* obj = object_at(i);
    if (obj->IsCleared()) continue;
    Code* code = Code::cast(obj->GetHeapObjectAssumeWeak());
    if (!code->marked_for_deoptimization()) {
      code->SetMarkedForDeoptimization(DependencyGroupName(group));
      marked = true;
    }
  }
  for (int i = 0; i < count; i++) {
    clear_at(i);
  }
  set_count(0);
  return marked;
}


void DependentCode::DeoptimizeDependentCodeGroup(
    Isolate* isolate,
    DependentCode::DependencyGroup group) {
  DisallowHeapAllocation no_allocation_scope;
  bool marked = MarkCodeForDeoptimization(isolate, group);
  if (marked) {
    DCHECK(AllowCodeDependencyChange::IsAllowed());
    Deoptimizer::DeoptimizeMarkedCode(isolate);
  }
}

void Code::SetMarkedForDeoptimization(const char* reason) {
  set_marked_for_deoptimization(true);
  if (FLAG_trace_deopt &&
      (deoptimization_data() != GetReadOnlyRoots().empty_fixed_array())) {
    DeoptimizationData* deopt_data =
        DeoptimizationData::cast(deoptimization_data());
    CodeTracer::Scope scope(GetHeap()->isolate()->GetCodeTracer());
    PrintF(scope.file(),
           "[marking dependent code " V8PRIxPTR_FMT
           " (opt #%d) for deoptimization, reason: %s]\n",
           reinterpret_cast<intptr_t>(this),
           deopt_data->OptimizationId()->value(), reason);
  }
}


const char* DependentCode::DependencyGroupName(DependencyGroup group) {
  switch (group) {
    case kTransitionGroup:
      return "transition";
    case kPrototypeCheckGroup:
      return "prototype-check";
    case kPropertyCellChangedGroup:
      return "property-cell-changed";
    case kFieldOwnerGroup:
      return "field-owner";
    case kInitialMapChangedGroup:
      return "initial-map-changed";
    case kAllocationSiteTenuringChangedGroup:
      return "allocation-site-tenuring-changed";
    case kAllocationSiteTransitionChangedGroup:
      return "allocation-site-transition-changed";
  }
  UNREACHABLE();
}

Handle<Map> Map::TransitionToPrototype(Isolate* isolate, Handle<Map> map,
                                       Handle<Object> prototype) {
  Handle<Map> new_map =
      TransitionsAccessor(isolate, map).GetPrototypeTransition(prototype);
  if (new_map.is_null()) {
    new_map = Copy(isolate, map, "TransitionToPrototype");
    TransitionsAccessor(isolate, map)
        .PutPrototypeTransition(prototype, new_map);
    Map::SetPrototype(isolate, new_map, prototype);
  }
  return new_map;
}


Maybe<bool> JSReceiver::SetPrototype(Handle<JSReceiver> object,
                                     Handle<Object> value, bool from_javascript,
                                     ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::SetPrototype(Handle<JSProxy>::cast(object), value,
                                 from_javascript, should_throw);
  }
  return JSObject::SetPrototype(Handle<JSObject>::cast(object), value,
                                from_javascript, should_throw);
}


// ES6: 9.5.2 [[SetPrototypeOf]] (V)
// static
Maybe<bool> JSProxy::SetPrototype(Handle<JSProxy> proxy, Handle<Object> value,
                                  bool from_javascript,
                                  ShouldThrow should_throw) {
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(isolate, Nothing<bool>());
  Handle<Name> trap_name = isolate->factory()->setPrototypeOf_string();
  // 1. Assert: Either Type(V) is Object or Type(V) is Null.
  DCHECK(value->IsJSReceiver() || value->IsNull(isolate));
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot.
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 6. Let trap be ? GetMethod(handler, "getPrototypeOf").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name),
      Nothing<bool>());
  // 7. If trap is undefined, then return target.[[SetPrototypeOf]]().
  if (trap->IsUndefined(isolate)) {
    return JSReceiver::SetPrototype(target, value, from_javascript,
                                    should_throw);
  }
  // 8. Let booleanTrapResult be ToBoolean(? Call(trap, handler, «target, V»)).
  Handle<Object> argv[] = {target, value};
  Handle<Object> trap_result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(argv), argv),
      Nothing<bool>());
  bool bool_trap_result = trap_result->BooleanValue(isolate);
  // 9. If booleanTrapResult is false, return false.
  if (!bool_trap_result) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kProxyTrapReturnedFalsish, trap_name));
  }
  // 10. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> is_extensible = JSReceiver::IsExtensible(target);
  if (is_extensible.IsNothing()) return Nothing<bool>();
  // 11. If extensibleTarget is true, return true.
  if (is_extensible.FromJust()) {
    if (bool_trap_result) return Just(true);
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kProxyTrapReturnedFalsish, trap_name));
  }
  // 12. Let targetProto be ? target.[[GetPrototypeOf]]().
  Handle<Object> target_proto;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, target_proto,
                                   JSReceiver::GetPrototype(isolate, target),
                                   Nothing<bool>());
  // 13. If SameValue(V, targetProto) is false, throw a TypeError exception.
  if (bool_trap_result && !value->SameValue(*target_proto)) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxySetPrototypeOfNonExtensible));
    return Nothing<bool>();
  }
  // 14. Return true.
  return Just(true);
}


Maybe<bool> JSObject::SetPrototype(Handle<JSObject> object,
                                   Handle<Object> value, bool from_javascript,
                                   ShouldThrow should_throw) {
  Isolate* isolate = object->GetIsolate();

#ifdef DEBUG
  int size = object->Size();
#endif

  if (from_javascript) {
    if (object->IsAccessCheckNeeded() &&
        !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kNoAccess));
    }
  } else {
    DCHECK(!object->IsAccessCheckNeeded());
  }

  // Silently ignore the change if value is not a JSObject or null.
  // SpiderMonkey behaves this way.
  if (!value->IsJSReceiver() && !value->IsNull(isolate)) return Just(true);

  bool all_extensible = object->map()->is_extensible();
  Handle<JSObject> real_receiver = object;
  if (from_javascript) {
    // Find the first object in the chain whose prototype object is not
    // hidden.
    PrototypeIterator iter(isolate, real_receiver, kStartAtPrototype,
                           PrototypeIterator::END_AT_NON_HIDDEN);
    while (!iter.IsAtEnd()) {
      // Casting to JSObject is fine because hidden prototypes are never
      // JSProxies.
      real_receiver = PrototypeIterator::GetCurrent<JSObject>(iter);
      iter.Advance();
      all_extensible = all_extensible && real_receiver->map()->is_extensible();
    }
  }
  Handle<Map> map(real_receiver->map(), isolate);

  // Nothing to do if prototype is already set.
  if (map->prototype() == *value) return Just(true);

  bool immutable_proto = map->is_immutable_proto();
  if (immutable_proto) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kImmutablePrototypeSet, object));
  }

  // From 8.6.2 Object Internal Methods
  // ...
  // In addition, if [[Extensible]] is false the value of the [[Class]] and
  // [[Prototype]] internal properties of the object may not be modified.
  // ...
  // Implementation specific extensions that modify [[Class]], [[Prototype]]
  // or [[Extensible]] must not violate the invariants defined in the preceding
  // paragraph.
  if (!all_extensible) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNonExtensibleProto, object));
  }

  // Before we can set the prototype we need to be sure prototype cycles are
  // prevented.  It is sufficient to validate that the receiver is not in the
  // new prototype chain.
  if (value->IsJSReceiver()) {
    for (PrototypeIterator iter(isolate, JSReceiver::cast(*value),
                                kStartAtReceiver);
         !iter.IsAtEnd(); iter.Advance()) {
      if (iter.GetCurrent<JSReceiver>() == *object) {
        // Cycle detected.
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kCyclicProto));
      }
    }
  }

  // Set the new prototype of the object.

  isolate->UpdateNoElementsProtectorOnSetPrototype(real_receiver);

  Handle<Map> new_map = Map::TransitionToPrototype(isolate, map, value);
  DCHECK(new_map->prototype() == *value);
  JSObject::MigrateToMap(real_receiver, new_map);

  DCHECK(size == object->Size());
  return Just(true);
}

// static
void JSObject::SetImmutableProto(Handle<JSObject> object) {
  DCHECK(!object->IsAccessCheckNeeded());  // Never called from JS
  Handle<Map> map(object->map(), object->GetIsolate());

  // Nothing to do if prototype is already set.
  if (map->is_immutable_proto()) return;

  Handle<Map> new_map =
      Map::TransitionToImmutableProto(object->GetIsolate(), map);
  object->synchronized_set_map(*new_map);
}

void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Arguments* args,
                                        uint32_t first_arg,
                                        uint32_t arg_count,
                                        EnsureElementsMode mode) {
  // Elements in |Arguments| are ordered backwards (because they're on the
  // stack), but the method that's called here iterates over them in forward
  // direction.
  return EnsureCanContainElements(
      object, args->arguments() - first_arg - (arg_count - 1), arg_count, mode);
}


ElementsAccessor* JSObject::GetElementsAccessor() {
  return ElementsAccessor::ForKind(GetElementsKind());
}

void JSObject::ValidateElements(JSObject* object) {
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    object->GetElementsAccessor()->Validate(object);
  }
#endif
}


static bool ShouldConvertToSlowElements(JSObject* object, uint32_t capacity,
                                        uint32_t index,
                                        uint32_t* new_capacity) {
  STATIC_ASSERT(JSObject::kMaxUncheckedOldFastElementsLength <=
                JSObject::kMaxUncheckedFastElementsLength);
  if (index < capacity) {
    *new_capacity = capacity;
    return false;
  }
  if (index - capacity >= JSObject::kMaxGap) return true;
  *new_capacity = JSObject::NewElementsCapacity(index + 1);
  DCHECK_LT(index, *new_capacity);
  if (*new_capacity <= JSObject::kMaxUncheckedOldFastElementsLength ||
      (*new_capacity <= JSObject::kMaxUncheckedFastElementsLength &&
       Heap::InNewSpace(object))) {
    return false;
  }
  // If the fast-case backing storage takes up much more memory than a
  // dictionary backing storage would, the object should have slow elements.
  int used_elements = object->GetFastElementsUsage();
  uint32_t size_threshold = NumberDictionary::kPreferFastElementsSizeFactor *
                            NumberDictionary::ComputeCapacity(used_elements) *
                            NumberDictionary::kEntrySize;
  return size_threshold <= *new_capacity;
}


bool JSObject::WouldConvertToSlowElements(uint32_t index) {
  if (!HasFastElements()) return false;
  uint32_t capacity = static_cast<uint32_t>(elements()->length());
  uint32_t new_capacity;
  return ShouldConvertToSlowElements(this, capacity, index, &new_capacity);
}


static ElementsKind BestFittingFastElementsKind(JSObject* object) {
  if (!object->map()->CanHaveFastTransitionableElementsKind()) {
    return HOLEY_ELEMENTS;
  }
  if (object->HasSloppyArgumentsElements()) {
    return FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  }
  if (object->HasStringWrapperElements()) {
    return FAST_STRING_WRAPPER_ELEMENTS;
  }
  DCHECK(object->HasDictionaryElements());
  NumberDictionary* dictionary = object->element_dictionary();
  ElementsKind kind = HOLEY_SMI_ELEMENTS;
  for (int i = 0; i < dictionary->Capacity(); i++) {
    Object* key = dictionary->KeyAt(i);
    if (key->IsNumber()) {
      Object* value = dictionary->ValueAt(i);
      if (!value->IsNumber()) return HOLEY_ELEMENTS;
      if (!value->IsSmi()) {
        if (!FLAG_unbox_double_arrays) return HOLEY_ELEMENTS;
        kind = HOLEY_DOUBLE_ELEMENTS;
      }
    }
  }
  return kind;
}

static bool ShouldConvertToFastElements(JSObject* object,
                                        NumberDictionary* dictionary,
                                        uint32_t index,
                                        uint32_t* new_capacity) {
  // If properties with non-standard attributes or accessors were added, we
  // cannot go back to fast elements.
  if (dictionary->requires_slow_elements()) return false;

  // Adding a property with this index will require slow elements.
  if (index >= static_cast<uint32_t>(Smi::kMaxValue)) return false;

  if (object->IsJSArray()) {
    Object* length = JSArray::cast(object)->length();
    if (!length->IsSmi()) return false;
    *new_capacity = static_cast<uint32_t>(Smi::ToInt(length));
  } else if (object->IsJSSloppyArgumentsObject()) {
    return false;
  } else {
    *new_capacity = dictionary->max_number_key() + 1;
  }
  *new_capacity = Max(index + 1, *new_capacity);

  uint32_t dictionary_size = static_cast<uint32_t>(dictionary->Capacity()) *
                             NumberDictionary::kEntrySize;

  // Turn fast if the dictionary only saves 50% space.
  return 2 * dictionary_size >= *new_capacity;
}

// static
void JSObject::AddDataElement(Handle<JSObject> object, uint32_t index,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
  DCHECK(object->map()->is_extensible());

  Isolate* isolate = object->GetIsolate();

  uint32_t old_length = 0;
  uint32_t new_capacity = 0;

  if (object->IsJSArray()) {
    CHECK(JSArray::cast(*object)->length()->ToArrayLength(&old_length));
  }

  ElementsKind kind = object->GetElementsKind();
  FixedArrayBase* elements = object->elements();
  ElementsKind dictionary_kind = DICTIONARY_ELEMENTS;
  if (IsSloppyArgumentsElementsKind(kind)) {
    elements = SloppyArgumentsElements::cast(elements)->arguments();
    dictionary_kind = SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
  } else if (IsStringWrapperElementsKind(kind)) {
    dictionary_kind = SLOW_STRING_WRAPPER_ELEMENTS;
  }

  if (attributes != NONE) {
    kind = dictionary_kind;
  } else if (elements->IsNumberDictionary()) {
    kind = ShouldConvertToFastElements(
               *object, NumberDictionary::cast(elements), index, &new_capacity)
               ? BestFittingFastElementsKind(*object)
               : dictionary_kind;
  } else if (ShouldConvertToSlowElements(
                 *object, static_cast<uint32_t>(elements->length()), index,
                 &new_capacity)) {
    kind = dictionary_kind;
  }

  ElementsKind to = value->OptimalElementsKind();
  if (IsHoleyElementsKind(kind) || !object->IsJSArray() || index > old_length) {
    to = GetHoleyElementsKind(to);
    kind = GetHoleyElementsKind(kind);
  }
  to = GetMoreGeneralElementsKind(kind, to);
  ElementsAccessor* accessor = ElementsAccessor::ForKind(to);
  accessor->Add(object, index, value, attributes, new_capacity);

  if (object->IsJSArray() && index >= old_length) {
    Handle<Object> new_length =
        isolate->factory()->NewNumberFromUint(index + 1);
    JSArray::cast(*object)->set_length(*new_length);
  }
}


bool JSArray::SetLengthWouldNormalize(uint32_t new_length) {
  if (!HasFastElements()) return false;
  uint32_t capacity = static_cast<uint32_t>(elements()->length());
  uint32_t new_capacity;
  return JSArray::SetLengthWouldNormalize(GetHeap(), new_length) &&
         ShouldConvertToSlowElements(this, capacity, new_length - 1,
                                     &new_capacity);
}


const double AllocationSite::kPretenureRatio = 0.85;


void AllocationSite::ResetPretenureDecision() {
  set_pretenure_decision(kUndecided);
  set_memento_found_count(0);
  set_memento_create_count(0);
}

PretenureFlag AllocationSite::GetPretenureMode() const {
  PretenureDecision mode = pretenure_decision();
  // Zombie objects "decide" to be untenured.
  return mode == kTenure ? TENURED : NOT_TENURED;
}

bool AllocationSite::IsNested() {
  DCHECK(FLAG_trace_track_allocation_sites);
  Object* current = boilerplate()->GetHeap()->allocation_sites_list();
  while (current->IsAllocationSite()) {
    AllocationSite* current_site = AllocationSite::cast(current);
    if (current_site->nested_site() == this) {
      return true;
    }
    current = current_site->weak_next();
  }
  return false;
}

template <AllocationSiteUpdateMode update_or_check>
bool AllocationSite::DigestTransitionFeedback(Handle<AllocationSite> site,
                                              ElementsKind to_kind) {
  Isolate* isolate = site->GetIsolate();
  bool result = false;

  if (site->PointsToLiteral() && site->boilerplate()->IsJSArray()) {
    Handle<JSArray> boilerplate(JSArray::cast(site->boilerplate()), isolate);
    ElementsKind kind = boilerplate->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      // If the array is huge, it's not likely to be defined in a local
      // function, so we shouldn't make new instances of it very often.
      uint32_t length = 0;
      CHECK(boilerplate->length()->ToArrayLength(&length));
      if (length <= kMaximumArrayBytesToPretransition) {
        if (update_or_check == AllocationSiteUpdateMode::kCheckOnly) {
          return true;
        }
        if (FLAG_trace_track_allocation_sites) {
          bool is_nested = site->IsNested();
          PrintF("AllocationSite: JSArray %p boilerplate %supdated %s->%s\n",
                 reinterpret_cast<void*>(*site), is_nested ? "(nested)" : " ",
                 ElementsKindToString(kind), ElementsKindToString(to_kind));
        }
        JSObject::TransitionElementsKind(boilerplate, to_kind);
        site->dependent_code()->DeoptimizeDependentCodeGroup(
            isolate, DependentCode::kAllocationSiteTransitionChangedGroup);
        result = true;
      }
    }
  } else {
    // The AllocationSite is for a constructed Array.
    ElementsKind kind = site->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      if (update_or_check == AllocationSiteUpdateMode::kCheckOnly) return true;
      if (FLAG_trace_track_allocation_sites) {
        PrintF("AllocationSite: JSArray %p site updated %s->%s\n",
               reinterpret_cast<void*>(*site),
               ElementsKindToString(kind),
               ElementsKindToString(to_kind));
      }
      site->SetElementsKind(to_kind);
      site->dependent_code()->DeoptimizeDependentCodeGroup(
          isolate, DependentCode::kAllocationSiteTransitionChangedGroup);
      result = true;
    }
  }
  return result;
}

bool AllocationSite::ShouldTrack(ElementsKind from, ElementsKind to) {
  return IsSmiElementsKind(from) &&
         IsMoreGeneralElementsKindTransition(from, to);
}

const char* AllocationSite::PretenureDecisionName(PretenureDecision decision) {
  switch (decision) {
    case kUndecided: return "undecided";
    case kDontTenure: return "don't tenure";
    case kMaybeTenure: return "maybe tenure";
    case kTenure: return "tenure";
    case kZombie: return "zombie";
    default: UNREACHABLE();
  }
  return nullptr;
}

template <AllocationSiteUpdateMode update_or_check>
bool JSObject::UpdateAllocationSite(Handle<JSObject> object,
                                    ElementsKind to_kind) {
  if (!object->IsJSArray()) return false;

  if (!Heap::InNewSpace(*object)) return false;

  Handle<AllocationSite> site;
  {
    DisallowHeapAllocation no_allocation;

    Heap* heap = object->GetHeap();
    AllocationMemento* memento =
        heap->FindAllocationMemento<Heap::kForRuntime>(object->map(), *object);
    if (memento == nullptr) return false;

    // Walk through to the Allocation Site
    site = handle(memento->GetAllocationSite(), heap->isolate());
  }
  return AllocationSite::DigestTransitionFeedback<update_or_check>(site,
                                                                   to_kind);
}

template bool
JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kCheckOnly>(
    Handle<JSObject> object, ElementsKind to_kind);

template bool JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kUpdate>(
    Handle<JSObject> object, ElementsKind to_kind);

void JSObject::TransitionElementsKind(Handle<JSObject> object,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = object->GetElementsKind();

  if (IsHoleyElementsKind(from_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
  }

  if (from_kind == to_kind) return;

  // This method should never be called for any other case.
  DCHECK(IsFastElementsKind(from_kind));
  DCHECK(IsFastElementsKind(to_kind));
  DCHECK_NE(TERMINAL_FAST_ELEMENTS_KIND, from_kind);

  UpdateAllocationSite(object, to_kind);
  if (object->elements() == object->GetReadOnlyRoots().empty_fixed_array() ||
      IsDoubleElementsKind(from_kind) == IsDoubleElementsKind(to_kind)) {
    // No change is needed to the elements() buffer, the transition
    // only requires a map change.
    Handle<Map> new_map = GetElementsTransitionMap(object, to_kind);
    MigrateToMap(object, new_map);
    if (FLAG_trace_elements_transitions) {
      Handle<FixedArrayBase> elms(object->elements(), object->GetIsolate());
      PrintElementsTransition(stdout, object, from_kind, elms, to_kind, elms);
    }
  } else {
    DCHECK((IsSmiElementsKind(from_kind) && IsDoubleElementsKind(to_kind)) ||
           (IsDoubleElementsKind(from_kind) && IsObjectElementsKind(to_kind)));
    uint32_t c = static_cast<uint32_t>(object->elements()->length());
    ElementsAccessor::ForKind(to_kind)->GrowCapacityAndConvert(object, c);
  }
}


bool JSArray::HasReadOnlyLength(Handle<JSArray> array) {
  Map* map = array->map();
  // Fast path: "length" is the first fast property of arrays. Since it's not
  // configurable, it's guaranteed to be the first in the descriptor array.
  if (!map->is_dictionary_map()) {
    DCHECK(map->instance_descriptors()->GetKey(0) ==
           array->GetReadOnlyRoots().length_string());
    return map->instance_descriptors()->GetDetails(0).IsReadOnly();
  }

  Isolate* isolate = array->GetIsolate();
  LookupIterator it(array, isolate->factory()->length_string(), array,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_EQ(LookupIterator::ACCESSOR, it.state());
  return it.IsReadOnly();
}


bool JSArray::WouldChangeReadOnlyLength(Handle<JSArray> array,
                                        uint32_t index) {
  uint32_t length = 0;
  CHECK(array->length()->ToArrayLength(&length));
  if (length <= index) return HasReadOnlyLength(array);
  return false;
}

template <typename BackingStore>
static int HoleyElementsUsage(JSObject* object, BackingStore* store) {
  Isolate* isolate = object->GetIsolate();
  int limit = object->IsJSArray() ? Smi::ToInt(JSArray::cast(object)->length())
                                  : store->length();
  int used = 0;
  for (int i = 0; i < limit; ++i) {
    if (!store->is_the_hole(isolate, i)) ++used;
  }
  return used;
}

int JSObject::GetFastElementsUsage() {
  FixedArrayBase* store = elements();
  switch (GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS:
    case PACKED_ELEMENTS:
      return IsJSArray() ? Smi::ToInt(JSArray::cast(this)->length())
                         : store->length();
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      store = SloppyArgumentsElements::cast(store)->arguments();
      V8_FALLTHROUGH;
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
      return HoleyElementsUsage(this, FixedArray::cast(store));
    case HOLEY_DOUBLE_ELEMENTS:
      if (elements()->length() == 0) return 0;
      return HoleyElementsUsage(this, FixedDoubleArray::cast(store));

    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NO_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    UNREACHABLE();
  }
  return 0;
}


// Certain compilers request function template instantiation when they
// see the definition of the other template functions in the
// class. This requires us to have the template functions put
// together, so even though this function belongs in objects-debug.cc,
// we keep it here instead to satisfy certain compilers.
#ifdef OBJECT_PRINT
template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::Print(std::ostream& os) {
  DisallowHeapAllocation no_gc;
  ReadOnlyRoots roots = this->GetReadOnlyRoots();
  Derived* dictionary = Derived::cast(this);
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = dictionary->KeyAt(i);
    if (!dictionary->ToKey(roots, i, &k)) continue;
    os << "\n   ";
    if (k->IsString()) {
      String::cast(k)->StringPrint(os);
    } else {
      os << Brief(k);
    }
    os << ": " << Brief(dictionary->ValueAt(i)) << " ";
    dictionary->DetailsAt(i).PrintAsSlowTo(os);
  }
}
template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::Print() {
  StdoutStream os;
  Print(os);
  os << std::endl;
}
#endif


MaybeHandle<Object> JSObject::GetPropertyWithInterceptor(LookupIterator* it,
                                                         bool* done) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return GetPropertyWithInterceptorInternal(it, it->GetInterceptor(), done);
}

Maybe<bool> JSObject::HasRealNamedProperty(Handle<JSObject> object,
                                           Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      object->GetIsolate(), object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}


Maybe<bool> JSObject::HasRealElementProperty(Handle<JSObject> object,
                                             uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}


Maybe<bool> JSObject::HasRealNamedCallbackProperty(Handle<JSObject> object,
                                                   Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      object->GetIsolate(), object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe_result = GetPropertyAttributes(&it);
  return maybe_result.IsJust() ? Just(it.state() == LookupIterator::ACCESSOR)
                               : Nothing<bool>();
}

int FixedArrayBase::GetMaxLengthForNewSpaceAllocation(ElementsKind kind) {
  return ((kMaxRegularHeapObjectSize - FixedArrayBase::kHeaderSize) >>
          ElementsKindToShiftSize(kind));
}

bool FixedArrayBase::IsCowArray() const {
  return map() == GetReadOnlyRoots().fixed_cow_array_map();
}

bool JSObject::IsApiWrapper() {
  // These object types can carry information relevant for embedders. The
  // *_API_* types are generated through templates which can have embedder
  // fields. The other types have their embedder fields added at compile time.
  auto instance_type = map()->instance_type();
  return instance_type == JS_API_OBJECT_TYPE ||
         instance_type == JS_ARRAY_BUFFER_TYPE ||
         instance_type == JS_DATA_VIEW_TYPE ||
         instance_type == JS_SPECIAL_API_OBJECT_TYPE ||
         instance_type == JS_TYPED_ARRAY_TYPE;
}

bool JSObject::IsDroppableApiWrapper() {
  auto instance_type = map()->instance_type();
  return instance_type == JS_API_OBJECT_TYPE ||
         instance_type == JS_SPECIAL_API_OBJECT_TYPE;
}

const char* Symbol::PrivateSymbolToName() const {
  ReadOnlyRoots roots = GetReadOnlyRoots();
#define SYMBOL_CHECK_AND_PRINT(_, name) \
  if (this == roots.name()) return #name;
  PRIVATE_SYMBOL_LIST_GENERATOR(SYMBOL_CHECK_AND_PRINT, /* not used */)
#undef SYMBOL_CHECK_AND_PRINT
  return "UNKNOWN";
}


void Symbol::SymbolShortPrint(std::ostream& os) {
  os << "<Symbol:";
  if (!name()->IsUndefined()) {
    os << " ";
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(name())->StringShortPrint(&accumulator, false);
    os << accumulator.ToCString().get();
  } else {
    os << " (" << PrivateSymbolToName() << ")";
  }
  os << ">";
}


// StringSharedKeys are used as keys in the eval cache.
class StringSharedKey : public HashTableKey {
 public:
  // This tuple unambiguously identifies calls to eval() or
  // CreateDynamicFunction() (such as through the Function() constructor).
  // * source is the string passed into eval(). For dynamic functions, this is
  //   the effective source for the function, some of which is implicitly
  //   generated.
  // * shared is the shared function info for the function containing the call
  //   to eval(). for dynamic functions, shared is the native context closure.
  // * When positive, position is the position in the source where eval is
  //   called. When negative, position is the negation of the position in the
  //   dynamic function's effective source where the ')' ends the parameters.
  StringSharedKey(Handle<String> source, Handle<SharedFunctionInfo> shared,
                  LanguageMode language_mode, int position)
      : HashTableKey(CompilationCacheShape::StringSharedHash(
            *source, *shared, language_mode, position)),
        source_(source),
        shared_(shared),
        language_mode_(language_mode),
        position_(position) {}

  bool IsMatch(Object* other) override {
    DisallowHeapAllocation no_allocation;
    if (!other->IsFixedArray()) {
      DCHECK(other->IsNumber());
      uint32_t other_hash = static_cast<uint32_t>(other->Number());
      return Hash() == other_hash;
    }
    FixedArray* other_array = FixedArray::cast(other);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(other_array->get(0));
    if (shared != *shared_) return false;
    int language_unchecked = Smi::ToInt(other_array->get(2));
    DCHECK(is_valid_language_mode(language_unchecked));
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    if (language_mode != language_mode_) return false;
    int position = Smi::ToInt(other_array->get(3));
    if (position != position_) return false;
    String* source = String::cast(other_array->get(1));
    return source->Equals(*source_);
  }

  Handle<Object> AsHandle(Isolate* isolate) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(4);
    array->set(0, *shared_);
    array->set(1, *source_);
    array->set(2, Smi::FromEnum(language_mode_));
    array->set(3, Smi::FromInt(position_));
    array->set_map(ReadOnlyRoots(isolate).fixed_cow_array_map());
    return array;
  }

 private:
  Handle<String> source_;
  Handle<SharedFunctionInfo> shared_;
  LanguageMode language_mode_;
  int position_;
};

v8::Promise::PromiseState JSPromise::status() const {
  int value = flags() & kStatusMask;
  DCHECK(value == 0 || value == 1 || value == 2);
  return static_cast<v8::Promise::PromiseState>(value);
}

void JSPromise::set_status(Promise::PromiseState status) {
  int value = flags() & ~kStatusMask;
  set_flags(value | status);
}

// static
const char* JSPromise::Status(v8::Promise::PromiseState status) {
  switch (status) {
    case v8::Promise::kFulfilled:
      return "resolved";
    case v8::Promise::kPending:
      return "pending";
    case v8::Promise::kRejected:
      return "rejected";
  }
  UNREACHABLE();
}

int JSPromise::async_task_id() const {
  return AsyncTaskIdField::decode(flags());
}

void JSPromise::set_async_task_id(int id) {
  set_flags(AsyncTaskIdField::update(flags(), id));
}

// static
Handle<Object> JSPromise::Fulfill(Handle<JSPromise> promise,
                                  Handle<Object> value) {
  Isolate* const isolate = promise->GetIsolate();

  // 1. Assert: The value of promise.[[PromiseState]] is "pending".
  DCHECK_EQ(Promise::kPending, promise->status());

  // 2. Let reactions be promise.[[PromiseFulfillReactions]].
  Handle<Object> reactions(promise->reactions(), isolate);

  // 3. Set promise.[[PromiseResult]] to value.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  promise->set_reactions_or_result(*value);

  // 6. Set promise.[[PromiseState]] to "fulfilled".
  promise->set_status(Promise::kFulfilled);

  // 7. Return TriggerPromiseReactions(reactions, value).
  return TriggerPromiseReactions(isolate, reactions, value,
                                 PromiseReaction::kFulfill);
}

// static
Handle<Object> JSPromise::Reject(Handle<JSPromise> promise,
                                 Handle<Object> reason, bool debug_event) {
  Isolate* const isolate = promise->GetIsolate();

  if (debug_event) isolate->debug()->OnPromiseReject(promise, reason);
  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());

  // 1. Assert: The value of promise.[[PromiseState]] is "pending".
  DCHECK_EQ(Promise::kPending, promise->status());

  // 2. Let reactions be promise.[[PromiseRejectReactions]].
  Handle<Object> reactions(promise->reactions(), isolate);

  // 3. Set promise.[[PromiseResult]] to reason.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  promise->set_reactions_or_result(*reason);

  // 6. Set promise.[[PromiseState]] to "rejected".
  promise->set_status(Promise::kRejected);

  // 7. If promise.[[PromiseIsHandled]] is false, perform
  //    HostPromiseRejectionTracker(promise, "reject").
  if (!promise->has_handler()) {
    isolate->ReportPromiseReject(promise, reason, kPromiseRejectWithNoHandler);
  }

  // 8. Return TriggerPromiseReactions(reactions, reason).
  return TriggerPromiseReactions(isolate, reactions, reason,
                                 PromiseReaction::kReject);
}

// static
MaybeHandle<Object> JSPromise::Resolve(Handle<JSPromise> promise,
                                       Handle<Object> resolution) {
  Isolate* const isolate = promise->GetIsolate();

  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());

  // 6. If SameValue(resolution, promise) is true, then
  if (promise.is_identical_to(resolution)) {
    // a. Let selfResolutionError be a newly created TypeError object.
    Handle<Object> self_resolution_error = isolate->factory()->NewTypeError(
        MessageTemplate::kPromiseCyclic, resolution);
    // b. Return RejectPromise(promise, selfResolutionError).
    return Reject(promise, self_resolution_error);
  }

  // 7. If Type(resolution) is not Object, then
  if (!resolution->IsJSReceiver()) {
    // a. Return FulfillPromise(promise, resolution).
    return Fulfill(promise, resolution);
  }

  // 8. Let then be Get(resolution, "then").
  MaybeHandle<Object> then;
  if (isolate->IsPromiseThenLookupChainIntact(
          Handle<JSReceiver>::cast(resolution))) {
    // We can skip the "then" lookup on {resolution} if its [[Prototype]]
    // is the (initial) Promise.prototype and the Promise#then protector
    // is intact, as that guards the lookup path for the "then" property
    // on JSPromise instances which have the (initial) %PromisePrototype%.
    then = isolate->promise_then();
  } else {
    then =
        JSReceiver::GetProperty(isolate, Handle<JSReceiver>::cast(resolution),
                                isolate->factory()->then_string());
  }

  // 9. If then is an abrupt completion, then
  Handle<Object> then_action;
  if (!then.ToHandle(&then_action)) {
    // a. Return RejectPromise(promise, then.[[Value]]).
    Handle<Object> reason(isolate->pending_exception(), isolate);
    isolate->clear_pending_exception();
    return Reject(promise, reason, false);
  }

  // 10. Let thenAction be then.[[Value]].
  // 11. If IsCallable(thenAction) is false, then
  if (!then_action->IsCallable()) {
    // a. Return FulfillPromise(promise, resolution).
    return Fulfill(promise, resolution);
  }

  // 12. Perform EnqueueJob("PromiseJobs", PromiseResolveThenableJob,
  //                        «promise, resolution, thenAction»).
  Handle<PromiseResolveThenableJobTask> task =
      isolate->factory()->NewPromiseResolveThenableJobTask(
          promise, Handle<JSReceiver>::cast(then_action),
          Handle<JSReceiver>::cast(resolution), isolate->native_context());
  if (isolate->debug()->is_active() && resolution->IsJSPromise()) {
    // Mark the dependency of the new {promise} on the {resolution}.
    Object::SetProperty(isolate, resolution,
                        isolate->factory()->promise_handled_by_symbol(),
                        promise, LanguageMode::kStrict)
        .Check();
  }
  isolate->EnqueueMicrotask(task);

  // 13. Return undefined.
  return isolate->factory()->undefined_value();
}

// static
Handle<Object> JSPromise::TriggerPromiseReactions(Isolate* isolate,
                                                  Handle<Object> reactions,
                                                  Handle<Object> argument,
                                                  PromiseReaction::Type type) {
  DCHECK(reactions->IsSmi() || reactions->IsPromiseReaction());

  // We need to reverse the {reactions} here, since we record them
  // on the JSPromise in the reverse order.
  {
    DisallowHeapAllocation no_gc;
    Object* current = *reactions;
    Object* reversed = Smi::kZero;
    while (!current->IsSmi()) {
      Object* next = PromiseReaction::cast(current)->next();
      PromiseReaction::cast(current)->set_next(reversed);
      reversed = current;
      current = next;
    }
    reactions = handle(reversed, isolate);
  }

  // Morph the {reactions} into PromiseReactionJobTasks
  // and push them onto the microtask queue.
  while (!reactions->IsSmi()) {
    Handle<HeapObject> task = Handle<HeapObject>::cast(reactions);
    Handle<PromiseReaction> reaction = Handle<PromiseReaction>::cast(task);
    reactions = handle(reaction->next(), isolate);

    STATIC_ASSERT(PromiseReaction::kSize == PromiseReactionJobTask::kSize);
    if (type == PromiseReaction::kFulfill) {
      task->synchronized_set_map(
          ReadOnlyRoots(isolate).promise_fulfill_reaction_job_task_map());
      Handle<PromiseFulfillReactionJobTask>::cast(task)->set_argument(
          *argument);
      Handle<PromiseFulfillReactionJobTask>::cast(task)->set_context(
          *isolate->native_context());
      STATIC_ASSERT(PromiseReaction::kFulfillHandlerOffset ==
                    PromiseFulfillReactionJobTask::kHandlerOffset);
      STATIC_ASSERT(PromiseReaction::kPromiseOrCapabilityOffset ==
                    PromiseFulfillReactionJobTask::kPromiseOrCapabilityOffset);
    } else {
      DisallowHeapAllocation no_gc;
      HeapObject* handler = reaction->reject_handler();
      task->synchronized_set_map(
          ReadOnlyRoots(isolate).promise_reject_reaction_job_task_map());
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_argument(*argument);
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_context(
          *isolate->native_context());
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_handler(handler);
      STATIC_ASSERT(PromiseReaction::kPromiseOrCapabilityOffset ==
                    PromiseRejectReactionJobTask::kPromiseOrCapabilityOffset);
    }

    isolate->EnqueueMicrotask(Handle<PromiseReactionJobTask>::cast(task));
  }

  return isolate->factory()->undefined_value();
}

namespace {

JSRegExp::Flags RegExpFlagsFromString(Handle<String> flags, bool* success) {
  JSRegExp::Flags value = JSRegExp::kNone;
  int length = flags->length();
  // A longer flags string cannot be valid.
  if (length > JSRegExp::FlagCount()) return JSRegExp::Flags(0);
  for (int i = 0; i < length; i++) {
    JSRegExp::Flag flag = JSRegExp::kNone;
    switch (flags->Get(i)) {
      case 'g':
        flag = JSRegExp::kGlobal;
        break;
      case 'i':
        flag = JSRegExp::kIgnoreCase;
        break;
      case 'm':
        flag = JSRegExp::kMultiline;
        break;
      case 's':
        flag = JSRegExp::kDotAll;
        break;
      case 'u':
        flag = JSRegExp::kUnicode;
        break;
      case 'y':
        flag = JSRegExp::kSticky;
        break;
      default:
        return JSRegExp::Flags(0);
    }
    // Duplicate flag.
    if (value & flag) return JSRegExp::Flags(0);
    value |= flag;
  }
  *success = true;
  return value;
}

}  // namespace


// static
MaybeHandle<JSRegExp> JSRegExp::New(Isolate* isolate, Handle<String> pattern,
                                    Flags flags) {
  Handle<JSFunction> constructor = isolate->regexp_function();
  Handle<JSRegExp> regexp =
      Handle<JSRegExp>::cast(isolate->factory()->NewJSObject(constructor));

  return JSRegExp::Initialize(regexp, pattern, flags);
}


// static
Handle<JSRegExp> JSRegExp::Copy(Handle<JSRegExp> regexp) {
  Isolate* const isolate = regexp->GetIsolate();
  return Handle<JSRegExp>::cast(isolate->factory()->CopyJSObject(regexp));
}


template <typename Char>
inline int CountRequiredEscapes(Handle<String> source) {
  DisallowHeapAllocation no_gc;
  int escapes = 0;
  Vector<const Char> src = source->GetCharVector<Char>();
  for (int i = 0; i < src.length(); i++) {
    if (src[i] == '\\') {
      // Escape. Skip next character;
      i++;
    } else if (src[i] == '/') {
      // Not escaped forward-slash needs escape.
      escapes++;
    }
  }
  return escapes;
}


template <typename Char, typename StringType>
inline Handle<StringType> WriteEscapedRegExpSource(Handle<String> source,
                                                   Handle<StringType> result) {
  DisallowHeapAllocation no_gc;
  Vector<const Char> src = source->GetCharVector<Char>();
  Vector<Char> dst(result->GetChars(), result->length());
  int s = 0;
  int d = 0;
  while (s < src.length()) {
    if (src[s] == '\\') {
      // Escape. Copy this and next character.
      dst[d++] = src[s++];
      if (s == src.length()) break;
    } else if (src[s] == '/') {
      // Not escaped forward-slash needs escape.
      dst[d++] = '\\';
    }
    dst[d++] = src[s++];
  }
  DCHECK_EQ(result->length(), d);
  return result;
}


MaybeHandle<String> EscapeRegExpSource(Isolate* isolate,
                                       Handle<String> source) {
  DCHECK(source->IsFlat());
  if (source->length() == 0) return isolate->factory()->query_colon_string();
  bool one_byte = source->IsOneByteRepresentationUnderneath();
  int escapes = one_byte ? CountRequiredEscapes<uint8_t>(source)
                         : CountRequiredEscapes<uc16>(source);
  if (escapes == 0) return source;
  int length = source->length() + escapes;
  if (one_byte) {
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               isolate->factory()->NewRawOneByteString(length),
                               String);
    return WriteEscapedRegExpSource<uint8_t>(source, result);
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               isolate->factory()->NewRawTwoByteString(length),
                               String);
    return WriteEscapedRegExpSource<uc16>(source, result);
  }
}


// static
MaybeHandle<JSRegExp> JSRegExp::Initialize(Handle<JSRegExp> regexp,
                                           Handle<String> source,
                                           Handle<String> flags_string) {
  Isolate* isolate = regexp->GetIsolate();
  bool success = false;
  Flags flags = RegExpFlagsFromString(flags_string, &success);
  if (!success) {
    THROW_NEW_ERROR(
        isolate,
        NewSyntaxError(MessageTemplate::kInvalidRegExpFlags, flags_string),
        JSRegExp);
  }
  return Initialize(regexp, source, flags);
}


// static
MaybeHandle<JSRegExp> JSRegExp::Initialize(Handle<JSRegExp> regexp,
                                           Handle<String> source, Flags flags) {
  Isolate* isolate = regexp->GetIsolate();
  Factory* factory = isolate->factory();
  // If source is the empty string we set it to "(?:)" instead as
  // suggested by ECMA-262, 5th, section 15.10.4.1.
  if (source->length() == 0) source = factory->query_colon_string();

  source = String::Flatten(isolate, source);

  Handle<String> escaped_source;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, escaped_source,
                             EscapeRegExpSource(isolate, source), JSRegExp);

  RETURN_ON_EXCEPTION(
      isolate, RegExpImpl::Compile(isolate, regexp, source, flags), JSRegExp);

  regexp->set_source(*escaped_source);
  regexp->set_flags(Smi::FromInt(flags));

  Map* map = regexp->map();
  Object* constructor = map->GetConstructor();
  if (constructor->IsJSFunction() &&
      JSFunction::cast(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex, Smi::kZero,
                                  SKIP_WRITE_BARRIER);
  } else {
    // Map has changed, so use generic, but slower, method.
    RETURN_ON_EXCEPTION(
        isolate,
        JSReceiver::SetProperty(isolate, regexp, factory->lastIndex_string(),
                                Handle<Smi>(Smi::kZero, isolate),
                                LanguageMode::kStrict),
        JSRegExp);
  }

  return regexp;
}


// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(Handle<String> string, JSRegExp::Flags flags)
      : HashTableKey(
            CompilationCacheShape::RegExpHash(*string, Smi::FromInt(flags))),
        string_(string),
        flags_(Smi::FromInt(flags)) {}

  // Rather than storing the key in the hash table, a pointer to the
  // stored value is stored where the key should be.  IsMatch then
  // compares the search key to the found object, rather than comparing
  // a key to a key.
  bool IsMatch(Object* obj) override {
    FixedArray* val = FixedArray::cast(obj);
    return string_->Equals(String::cast(val->get(JSRegExp::kSourceIndex)))
        && (flags_ == val->get(JSRegExp::kFlagsIndex));
  }

  Handle<String> string_;
  Smi* flags_;
};

Handle<String> OneByteStringKey::AsHandle(Isolate* isolate) {
  return isolate->factory()->NewOneByteInternalizedString(string_, HashField());
}

Handle<String> TwoByteStringKey::AsHandle(Isolate* isolate) {
  return isolate->factory()->NewTwoByteInternalizedString(string_, HashField());
}

Handle<String> SeqOneByteSubStringKey::AsHandle(Isolate* isolate) {
  return isolate->factory()->NewOneByteInternalizedSubString(
      string_, from_, length_, HashField());
}


bool SeqOneByteSubStringKey::IsMatch(Object* string) {
  Vector<const uint8_t> chars(string_->GetChars() + from_, length_);
  return String::cast(string)->IsOneByteEqualTo(chars);
}


// InternalizedStringKey carries a string/internalized-string object as key.
class InternalizedStringKey : public StringTableKey {
 public:
  explicit InternalizedStringKey(Handle<String> string)
      : StringTableKey(0), string_(string) {
    DCHECK(!string->IsInternalizedString());
    DCHECK(string->IsFlat());
    // Make sure hash_field is computed.
    string->Hash();
    set_hash_field(string->hash_field());
  }

  bool IsMatch(Object* string) override {
    return string_->SlowEquals(String::cast(string));
  }

  Handle<String> AsHandle(Isolate* isolate) override {
    // Internalize the string if possible.
    MaybeHandle<Map> maybe_map =
        isolate->factory()->InternalizedStringMapForString(string_);
    Handle<Map> map;
    if (maybe_map.ToHandle(&map)) {
      string_->set_map_no_write_barrier(*map);
      DCHECK(string_->IsInternalizedString());
      return string_;
    }
    if (FLAG_thin_strings) {
      // External strings get special treatment, to avoid copying their
      // contents.
      if (string_->IsExternalOneByteString()) {
        return isolate->factory()
            ->InternalizeExternalString<ExternalOneByteString>(string_);
      } else if (string_->IsExternalTwoByteString()) {
        return isolate->factory()
            ->InternalizeExternalString<ExternalTwoByteString>(string_);
      }
    }
    // Otherwise allocate a new internalized string.
    return isolate->factory()->NewInternalizedStringImpl(
        string_, string_->length(), string_->hash_field());
  }

 private:
  Handle<String> string_;
};

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::IteratePrefix(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointers(this, 0, kElementsStartOffset, v);
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::IterateElements(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointers(this, kElementsStartOffset,
                                      kHeaderSize + length() * kPointerSize, v);
}

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::New(
    Isolate* isolate, int at_least_space_for, PretenureFlag pretenure,
    MinimumCapacity capacity_option) {
  DCHECK_LE(0, at_least_space_for);
  DCHECK_IMPLIES(capacity_option == USE_CUSTOM_MINIMUM_CAPACITY,
                 base::bits::IsPowerOfTwo(at_least_space_for));

  int capacity = (capacity_option == USE_CUSTOM_MINIMUM_CAPACITY)
                     ? at_least_space_for
                     : ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    isolate->heap()->FatalProcessOutOfMemory("invalid table size");
  }
  return NewInternal(isolate, capacity, pretenure);
}

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::NewInternal(
    Isolate* isolate, int capacity, PretenureFlag pretenure) {
  Factory* factory = isolate->factory();
  int length = EntryToIndex(capacity);
  RootIndex map_root_index = Shape::GetMapRootIndex();
  Handle<FixedArray> array =
      factory->NewFixedArrayWithMap(map_root_index, length, pretenure);
  Handle<Derived> table = Handle<Derived>::cast(array);

  table->SetNumberOfElements(0);
  table->SetNumberOfDeletedElements(0);
  table->SetCapacity(capacity);
  return table;
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Rehash(Isolate* isolate, Derived* new_table) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = new_table->GetWriteBarrierMode(no_gc);

  DCHECK_LT(NumberOfElements(), new_table->Capacity());

  // Copy prefix to new array.
  for (int i = kPrefixStartIndex; i < kElementsStartIndex; i++) {
    new_table->set(i, get(i), mode);
  }

  // Rehash the elements.
  int capacity = this->Capacity();
  ReadOnlyRoots roots(isolate);
  for (int i = 0; i < capacity; i++) {
    uint32_t from_index = EntryToIndex(i);
    Object* k = this->get(from_index);
    if (!Shape::IsLive(roots, k)) continue;
    uint32_t hash = Shape::HashForObject(isolate, k);
    uint32_t insertion_index =
        EntryToIndex(new_table->FindInsertionEntry(hash));
    for (int j = 0; j < Shape::kEntrySize; j++) {
      new_table->set(insertion_index + j, get(from_index + j), mode);
    }
  }
  new_table->SetNumberOfElements(NumberOfElements());
  new_table->SetNumberOfDeletedElements(0);
}

template <typename Derived, typename Shape>
uint32_t HashTable<Derived, Shape>::EntryForProbe(Isolate* isolate, Object* k,
                                                  int probe,
                                                  uint32_t expected) {
  uint32_t hash = Shape::HashForObject(isolate, k);
  uint32_t capacity = this->Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  for (int i = 1; i < probe; i++) {
    if (entry == expected) return expected;
    entry = NextProbe(entry, i, capacity);
  }
  return entry;
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Swap(uint32_t entry1, uint32_t entry2,
                                     WriteBarrierMode mode) {
  int index1 = EntryToIndex(entry1);
  int index2 = EntryToIndex(entry2);
  Object* temp[Shape::kEntrySize];
  for (int j = 0; j < Shape::kEntrySize; j++) {
    temp[j] = get(index1 + j);
  }
  for (int j = 0; j < Shape::kEntrySize; j++) {
    set(index1 + j, get(index2 + j), mode);
  }
  for (int j = 0; j < Shape::kEntrySize; j++) {
    set(index2 + j, temp[j], mode);
  }
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Rehash(Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = GetWriteBarrierMode(no_gc);
  ReadOnlyRoots roots(isolate);
  uint32_t capacity = Capacity();
  bool done = false;
  for (int probe = 1; !done; probe++) {
    // All elements at entries given by one of the first _probe_ probes
    // are placed correctly. Other elements might need to be moved.
    done = true;
    for (uint32_t current = 0; current < capacity; current++) {
      Object* current_key = KeyAt(current);
      if (!Shape::IsLive(roots, current_key)) continue;
      uint32_t target = EntryForProbe(isolate, current_key, probe, current);
      if (current == target) continue;
      Object* target_key = KeyAt(target);
      if (!Shape::IsLive(roots, target_key) ||
          EntryForProbe(isolate, target_key, probe, target) != target) {
        // Put the current element into the correct position.
        Swap(current, target, mode);
        // The other element will be processed on the next iteration.
        current--;
      } else {
        // The place for the current element is occupied. Leave the element
        // for the next probe.
        done = false;
      }
    }
  }
  // Wipe deleted entries.
  Object* the_hole = roots.the_hole_value();
  Object* undefined = roots.undefined_value();
  for (uint32_t current = 0; current < capacity; current++) {
    if (KeyAt(current) == the_hole) {
      set(EntryToIndex(current) + kEntryKeyIndex, undefined);
    }
  }
  SetNumberOfDeletedElements(0);
}

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::EnsureCapacity(
    Isolate* isolate, Handle<Derived> table, int n, PretenureFlag pretenure) {
  if (table->HasSufficientCapacityToAdd(n)) return table;

  int capacity = table->Capacity();
  int new_nof = table->NumberOfElements() + n;

  const int kMinCapacityForPretenure = 256;
  bool should_pretenure =
      pretenure == TENURED ||
      ((capacity > kMinCapacityForPretenure) && !Heap::InNewSpace(*table));
  Handle<Derived> new_table = HashTable::New(
      isolate, new_nof, should_pretenure ? TENURED : NOT_TENURED);

  table->Rehash(isolate, *new_table);
  return new_table;
}

template bool
HashTable<NameDictionary, NameDictionaryShape>::HasSufficientCapacityToAdd(int);

template <typename Derived, typename Shape>
bool HashTable<Derived, Shape>::HasSufficientCapacityToAdd(
    int number_of_additional_elements) {
  int capacity = Capacity();
  int nof = NumberOfElements() + number_of_additional_elements;
  int nod = NumberOfDeletedElements();
  // Return true if:
  //   50% is still free after adding number_of_additional_elements elements and
  //   at most 50% of the free elements are deleted elements.
  if ((nof < capacity) && ((nod <= (capacity - nof) >> 1))) {
    int needed_free = nof >> 1;
    if (nof + needed_free <= capacity) return true;
  }
  return false;
}

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::Shrink(Isolate* isolate,
                                                  Handle<Derived> table,
                                                  int additionalCapacity) {
  int capacity = table->Capacity();
  int nof = table->NumberOfElements();

  // Shrink to fit the number of elements if only a quarter of the
  // capacity is filled with elements.
  if (nof > (capacity >> 2)) return table;
  // Allocate a new dictionary with room for at least the current number of
  // elements + {additionalCapacity}. The allocation method will make sure that
  // there is extra room in the dictionary for additions. Don't go lower than
  // room for {kMinShrinkCapacity} elements.
  int at_least_room_for = nof + additionalCapacity;
  int new_capacity = ComputeCapacity(at_least_room_for);
  if (new_capacity < Derived::kMinShrinkCapacity) return table;
  if (new_capacity == capacity) return table;

  const int kMinCapacityForPretenure = 256;
  bool pretenure = (at_least_room_for > kMinCapacityForPretenure) &&
                   !Heap::InNewSpace(*table);
  Handle<Derived> new_table =
      HashTable::New(isolate, new_capacity, pretenure ? TENURED : NOT_TENURED,
                     USE_CUSTOM_MINIMUM_CAPACITY);

  table->Rehash(isolate, *new_table);
  return new_table;
}

template <typename Derived, typename Shape>
uint32_t HashTable<Derived, Shape>::FindInsertionEntry(uint32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  ReadOnlyRoots roots = GetReadOnlyRoots();
  while (true) {
    if (!Shape::IsLive(roots, KeyAt(entry))) break;
    entry = NextProbe(entry, count++, capacity);
  }
  return entry;
}

void JSGlobalObject::InvalidatePropertyCell(Handle<JSGlobalObject> global,
                                            Handle<Name> name) {
  // Regardless of whether the property is there or not invalidate
  // Load/StoreGlobalICs that load/store through global object's prototype.
  JSObject::InvalidatePrototypeValidityCell(*global);

  DCHECK(!global->HasFastProperties());
  auto dictionary = handle(global->global_dictionary(), global->GetIsolate());
  int entry = dictionary->FindEntry(global->GetIsolate(), name);
  if (entry == GlobalDictionary::kNotFound) return;
  PropertyCell::InvalidateEntry(global->GetIsolate(), dictionary, entry);
}

Handle<PropertyCell> JSGlobalObject::EnsureEmptyPropertyCell(
    Handle<JSGlobalObject> global, Handle<Name> name,
    PropertyCellType cell_type, int* entry_out) {
  Isolate* isolate = global->GetIsolate();
  DCHECK(!global->HasFastProperties());
  Handle<GlobalDictionary> dictionary(global->global_dictionary(), isolate);
  int entry = dictionary->FindEntry(isolate, name);
  Handle<PropertyCell> cell;
  if (entry != GlobalDictionary::kNotFound) {
    if (entry_out) *entry_out = entry;
    cell = handle(dictionary->CellAt(entry), isolate);
    PropertyCellType original_cell_type = cell->property_details().cell_type();
    DCHECK(original_cell_type == PropertyCellType::kInvalidated ||
           original_cell_type == PropertyCellType::kUninitialized);
    DCHECK(cell->value()->IsTheHole(isolate));
    if (original_cell_type == PropertyCellType::kInvalidated) {
      cell = PropertyCell::InvalidateEntry(isolate, dictionary, entry);
    }
    PropertyDetails details(kData, NONE, cell_type);
    cell->set_property_details(details);
    return cell;
  }
  cell = isolate->factory()->NewPropertyCell(name);
  PropertyDetails details(kData, NONE, cell_type);
  dictionary = GlobalDictionary::Add(isolate, dictionary, name, cell, details,
                                     entry_out);
  // {*entry_out} is initialized inside GlobalDictionary::Add().
  global->SetProperties(*dictionary);
  return cell;
}


// This class is used for looking up two character strings in the string table.
// If we don't have a hit we don't want to waste much time so we unroll the
// string hash calculation loop here for speed.  Doesn't work if the two
// characters form a decimal integer, since such strings have a different hash
// algorithm.
class TwoCharHashTableKey : public StringTableKey {
 public:
  TwoCharHashTableKey(uint16_t c1, uint16_t c2, uint64_t seed)
      : StringTableKey(ComputeHashField(c1, c2, seed)), c1_(c1), c2_(c2) {}

  bool IsMatch(Object* o) override {
    String* other = String::cast(o);
    if (other->length() != 2) return false;
    if (other->Get(0) != c1_) return false;
    return other->Get(1) == c2_;
  }

  Handle<String> AsHandle(Isolate* isolate) override {
    // The TwoCharHashTableKey is only used for looking in the string
    // table, not for adding to it.
    UNREACHABLE();
  }

 private:
  uint32_t ComputeHashField(uint16_t c1, uint16_t c2, uint64_t seed) {
    // Char 1.
    uint32_t hash = static_cast<uint32_t>(seed);
    hash += c1;
    hash += hash << 10;
    hash ^= hash >> 6;
    // Char 2.
    hash += c2;
    hash += hash << 10;
    hash ^= hash >> 6;
    // GetHash.
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    if ((hash & String::kHashBitMask) == 0) hash = StringHasher::kZeroHash;
    hash = (hash << String::kHashShift) | String::kIsNotArrayIndexMask;
#ifdef DEBUG
    // If this assert fails then we failed to reproduce the two-character
    // version of the string hashing algorithm above.  One reason could be
    // that we were passed two digits as characters, since the hash
    // algorithm is different in that case.
    uint16_t chars[2] = {c1, c2};
    uint32_t check_hash = StringHasher::HashSequentialString(chars, 2, seed);
    DCHECK_EQ(hash, check_hash);
#endif
    return hash;
  }

  uint16_t c1_;
  uint16_t c2_;
};

MaybeHandle<String> StringTable::LookupTwoCharsStringIfExists(
    Isolate* isolate,
    uint16_t c1,
    uint16_t c2) {
  TwoCharHashTableKey key(c1, c2, isolate->heap()->HashSeed());
  Handle<StringTable> string_table = isolate->factory()->string_table();
  int entry = string_table->FindEntry(isolate, &key);
  if (entry == kNotFound) return MaybeHandle<String>();

  Handle<String> result(String::cast(string_table->KeyAt(entry)), isolate);
  DCHECK(StringShape(*result).IsInternalized());
  DCHECK_EQ(result->Hash(), key.Hash());
  return result;
}

void StringTable::EnsureCapacityForDeserialization(Isolate* isolate,
                                                   int expected) {
  Handle<StringTable> table = isolate->factory()->string_table();
  // We need a key instance for the virtual hash function.
  table = StringTable::EnsureCapacity(isolate, table, expected);
  isolate->heap()->SetRootStringTable(*table);
}

namespace {

template <class StringClass>
void MigrateExternalStringResource(Isolate* isolate, String* from, String* to) {
  StringClass* cast_from = StringClass::cast(from);
  StringClass* cast_to = StringClass::cast(to);
  const typename StringClass::Resource* to_resource = cast_to->resource();
  if (to_resource == nullptr) {
    // |to| is a just-created internalized copy of |from|. Migrate the resource.
    cast_to->SetResource(isolate, cast_from->resource());
    // Zap |from|'s resource pointer to reflect the fact that |from| has
    // relinquished ownership of its resource.
    isolate->heap()->UpdateExternalString(
        from, ExternalString::cast(from)->ExternalPayloadSize(), 0);
    cast_from->SetResource(isolate, nullptr);
  } else if (to_resource != cast_from->resource()) {
    // |to| already existed and has its own resource. Finalize |from|.
    isolate->heap()->FinalizeExternalString(from);
  }
}

void MakeStringThin(String* string, String* internalized, Isolate* isolate) {
  DCHECK_NE(string, internalized);
  DCHECK(internalized->IsInternalizedString());

  if (string->IsExternalString()) {
    if (internalized->IsExternalOneByteString()) {
      MigrateExternalStringResource<ExternalOneByteString>(isolate, string,
                                                           internalized);
    } else if (internalized->IsExternalTwoByteString()) {
      MigrateExternalStringResource<ExternalTwoByteString>(isolate, string,
                                                           internalized);
    } else {
      // If the external string is duped into an existing non-external
      // internalized string, free its resource (it's about to be rewritten
      // into a ThinString below).
      isolate->heap()->FinalizeExternalString(string);
    }
  }

  DisallowHeapAllocation no_gc;
  int old_size = string->Size();
  isolate->heap()->NotifyObjectLayoutChange(string, old_size, no_gc);
  bool one_byte = internalized->IsOneByteRepresentation();
  Handle<Map> map = one_byte ? isolate->factory()->thin_one_byte_string_map()
                             : isolate->factory()->thin_string_map();
  DCHECK_GE(old_size, ThinString::kSize);
  string->synchronized_set_map(*map);
  ThinString* thin = ThinString::cast(string);
  thin->set_actual(internalized);
  Address thin_end = thin->address() + ThinString::kSize;
  int size_delta = old_size - ThinString::kSize;
  if (size_delta != 0) {
    Heap* heap = isolate->heap();
    heap->CreateFillerObjectAt(thin_end, size_delta, ClearRecordedSlots::kNo);
  }
}

}  // namespace

// static
Handle<String> StringTable::LookupString(Isolate* isolate,
                                         Handle<String> string) {
  string = String::Flatten(isolate, string);
  if (string->IsInternalizedString()) return string;

  InternalizedStringKey key(string);
  Handle<String> result = LookupKey(isolate, &key);

  if (FLAG_thin_strings) {
    if (!string->IsInternalizedString()) {
      MakeStringThin(*string, *result, isolate);
    }
  } else {  // !FLAG_thin_strings
    if (string->IsConsString()) {
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(isolate, *result);
      cons->set_second(isolate, ReadOnlyRoots(isolate).empty_string());
    } else if (string->IsSlicedString()) {
      STATIC_ASSERT(ConsString::kSize == SlicedString::kSize);
      DisallowHeapAllocation no_gc;
      bool one_byte = result->IsOneByteRepresentation();
      Handle<Map> map = one_byte
                            ? isolate->factory()->cons_one_byte_string_map()
                            : isolate->factory()->cons_string_map();
      string->set_map(*map);
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(isolate, *result);
      cons->set_second(isolate, ReadOnlyRoots(isolate).empty_string());
    }
  }
  return result;
}

// static
Handle<String> StringTable::LookupKey(Isolate* isolate, StringTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  int entry = table->FindEntry(isolate, key);

  // String already in table.
  if (entry != kNotFound) {
    return handle(String::cast(table->KeyAt(entry)), isolate);
  }

  table = StringTable::CautiousShrink(isolate, table);
  // Adding new string. Grow table if needed.
  table = StringTable::EnsureCapacity(isolate, table, 1);
  isolate->heap()->SetRootStringTable(*table);

  return AddKeyNoResize(isolate, key);
}

Handle<String> StringTable::AddKeyNoResize(Isolate* isolate,
                                           StringTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  DCHECK(table->HasSufficientCapacityToAdd(1));
  // Create string object.
  Handle<String> string = key->AsHandle(isolate);
  // There must be no attempts to internalize strings that could throw
  // InvalidStringLength error.
  CHECK(!string.is_null());
  DCHECK(string->HasHashCode());
  DCHECK_EQ(table->FindEntry(isolate, key), kNotFound);

  // Add the new string and return it along with the string table.
  int entry = table->FindInsertionEntry(key->Hash());
  table->set(EntryToIndex(entry), *string);
  table->ElementAdded();

  return Handle<String>::cast(string);
}

Handle<StringTable> StringTable::CautiousShrink(Isolate* isolate,
                                                Handle<StringTable> table) {
  // Only shrink if the table is very empty to avoid performance penalty.
  int capacity = table->Capacity();
  int nof = table->NumberOfElements();
  if (capacity <= StringTable::kMinCapacity) return table;
  if (nof > (capacity / kMaxEmptyFactor)) return table;
  // Keep capacity for at least half of the current nof elements.
  int slack_capacity = nof >> 2;
  return Shrink(isolate, table, slack_capacity);
}

namespace {

class StringTableNoAllocateKey : public StringTableKey {
 public:
  StringTableNoAllocateKey(String* string, uint64_t seed)
      : StringTableKey(0), string_(string) {
    StringShape shape(string);
    one_byte_ = shape.HasOnlyOneByteChars();
    DCHECK(!shape.IsInternalized());
    DCHECK(!shape.IsThin());
    int length = string->length();
    if (shape.IsCons() && length <= String::kMaxHashCalcLength) {
      special_flattening_ = true;
      uint32_t hash_field = 0;
      if (one_byte_) {
        if (V8_LIKELY(length <=
                      static_cast<int>(arraysize(one_byte_buffer_)))) {
          one_byte_content_ = one_byte_buffer_;
        } else {
          one_byte_content_ = new uint8_t[length];
        }
        String::WriteToFlat(string, one_byte_content_, 0, length);
        hash_field =
            StringHasher::HashSequentialString(one_byte_content_, length, seed);
      } else {
        if (V8_LIKELY(length <=
                      static_cast<int>(arraysize(two_byte_buffer_)))) {
          two_byte_content_ = two_byte_buffer_;
        } else {
          two_byte_content_ = new uint16_t[length];
        }
        String::WriteToFlat(string, two_byte_content_, 0, length);
        hash_field =
            StringHasher::HashSequentialString(two_byte_content_, length, seed);
      }
      string->set_hash_field(hash_field);
    } else {
      special_flattening_ = false;
      one_byte_content_ = nullptr;
      string->Hash();
    }

    DCHECK(string->HasHashCode());
    set_hash_field(string->hash_field());
  }

  ~StringTableNoAllocateKey() override {
    if (one_byte_) {
      if (one_byte_content_ != one_byte_buffer_) delete[] one_byte_content_;
    } else {
      if (two_byte_content_ != two_byte_buffer_) delete[] two_byte_content_;
    }
  }

  bool IsMatch(Object* otherstring) override {
    String* other = String::cast(otherstring);
    DCHECK(other->IsInternalizedString());
    DCHECK(other->IsFlat());
    if (Hash() != other->Hash()) return false;
    int len = string_->length();
    if (len != other->length()) return false;

    if (!special_flattening_) {
      if (string_->Get(0) != other->Get(0)) return false;
      if (string_->IsFlat()) {
        StringShape shape1(string_);
        StringShape shape2(other);
        if (shape1.encoding_tag() == kOneByteStringTag &&
            shape2.encoding_tag() == kOneByteStringTag) {
          String::FlatContent flat1 = string_->GetFlatContent();
          String::FlatContent flat2 = other->GetFlatContent();
          return CompareRawStringContents(flat1.ToOneByteVector().start(),
                                          flat2.ToOneByteVector().start(), len);
        }
        if (shape1.encoding_tag() == kTwoByteStringTag &&
            shape2.encoding_tag() == kTwoByteStringTag) {
          String::FlatContent flat1 = string_->GetFlatContent();
          String::FlatContent flat2 = other->GetFlatContent();
          return CompareRawStringContents(flat1.ToUC16Vector().start(),
                                          flat2.ToUC16Vector().start(), len);
        }
      }
      StringComparator comparator;
      return comparator.Equals(string_, other);
    }

    String::FlatContent flat_content = other->GetFlatContent();
    if (one_byte_) {
      if (flat_content.IsOneByte()) {
        return CompareRawStringContents(
            one_byte_content_, flat_content.ToOneByteVector().start(), len);
      } else {
        DCHECK(flat_content.IsTwoByte());
        for (int i = 0; i < len; i++) {
          if (flat_content.Get(i) != one_byte_content_[i]) return false;
        }
        return true;
      }
    } else {
      if (flat_content.IsTwoByte()) {
        return CompareRawStringContents(
            two_byte_content_, flat_content.ToUC16Vector().start(), len);
      } else {
        DCHECK(flat_content.IsOneByte());
        for (int i = 0; i < len; i++) {
          if (flat_content.Get(i) != two_byte_content_[i]) return false;
        }
        return true;
      }
    }
  }

  V8_WARN_UNUSED_RESULT Handle<String> AsHandle(Isolate* isolate) override {
    UNREACHABLE();
  }

 private:
  String* string_;
  bool one_byte_;
  bool special_flattening_;
  union {
    uint8_t* one_byte_content_;
    uint16_t* two_byte_content_;
  };
  union {
    uint8_t one_byte_buffer_[256];
    uint16_t two_byte_buffer_[128];
  };
};

}  // namespace

// static
Object* StringTable::LookupStringIfExists_NoAllocate(Isolate* isolate,
                                                     String* string) {
  DisallowHeapAllocation no_gc;
  Heap* heap = isolate->heap();
  StringTable* table = heap->string_table();

  StringTableNoAllocateKey key(string, heap->HashSeed());

  // String could be an array index.
  uint32_t hash = string->hash_field();

  // Valid array indices are >= 0, so they cannot be mixed up with any of
  // the result sentinels, which are negative.
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kUnsupported));
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kNotFound));

  if (Name::ContainsCachedArrayIndex(hash)) {
    return Smi::FromInt(String::ArrayIndexValueBits::decode(hash));
  }
  if ((hash & Name::kIsNotArrayIndexMask) == 0) {
    // It is an indexed, but it's not cached.
    return Smi::FromInt(ResultSentinel::kUnsupported);
  }

  DCHECK(!string->IsInternalizedString());
  int entry = table->FindEntry(ReadOnlyRoots(isolate), &key, key.Hash());
  if (entry != kNotFound) {
    String* internalized = String::cast(table->KeyAt(entry));
    if (FLAG_thin_strings) {
      MakeStringThin(string, internalized, isolate);
    }
    return internalized;
  }
  // A string that's not an array index, and not in the string table,
  // cannot have been used as a property name before.
  return Smi::FromInt(ResultSentinel::kNotFound);
}

String* StringTable::ForwardStringIfExists(Isolate* isolate,
                                           StringTableKey* key,
                                           String* string) {
  Handle<StringTable> table = isolate->factory()->string_table();
  int entry = table->FindEntry(isolate, key);
  if (entry == kNotFound) return nullptr;

  String* canonical = String::cast(table->KeyAt(entry));
  if (canonical != string) MakeStringThin(string, canonical, isolate);
  return canonical;
}

Handle<StringSet> StringSet::New(Isolate* isolate) {
  return HashTable::New(isolate, 0);
}

Handle<StringSet> StringSet::Add(Isolate* isolate, Handle<StringSet> stringset,
                                 Handle<String> name) {
  if (!stringset->Has(isolate, name)) {
    stringset = EnsureCapacity(isolate, stringset, 1);
    uint32_t hash = ShapeT::Hash(isolate, *name);
    int entry = stringset->FindInsertionEntry(hash);
    stringset->set(EntryToIndex(entry), *name);
    stringset->ElementAdded();
  }
  return stringset;
}

bool StringSet::Has(Isolate* isolate, Handle<String> name) {
  return FindEntry(isolate, *name) != kNotFound;
}

Handle<ObjectHashSet> ObjectHashSet::Add(Isolate* isolate,
                                         Handle<ObjectHashSet> set,
                                         Handle<Object> key) {
  int32_t hash = key->GetOrCreateHash(isolate)->value();
  if (!set->Has(isolate, key, hash)) {
    set = EnsureCapacity(isolate, set, 1);
    int entry = set->FindInsertionEntry(hash);
    set->set(EntryToIndex(entry), *key);
    set->ElementAdded();
  }
  return set;
}

namespace {

const int kLiteralEntryLength = 2;
const int kLiteralInitialLength = 2;
const int kLiteralContextOffset = 0;
const int kLiteralLiteralsOffset = 1;

int SearchLiteralsMapEntry(CompilationCacheTable* cache, int cache_entry,
                           Context* native_context) {
  DisallowHeapAllocation no_gc;
  DCHECK(native_context->IsNativeContext());
  Object* obj = cache->get(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj->IsFixedArray());
  if (obj->IsWeakFixedArray()) {
    WeakFixedArray* literals_map = WeakFixedArray::cast(obj);
    int length = literals_map->length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      DCHECK(literals_map->Get(i + kLiteralContextOffset)->IsWeakOrCleared());
      if (literals_map->Get(i + kLiteralContextOffset) ==
          HeapObjectReference::Weak(native_context)) {
        return i;
      }
    }
  }
  return -1;
}

void AddToFeedbackCellsMap(Handle<CompilationCacheTable> cache, int cache_entry,
                           Handle<Context> native_context,
                           Handle<FeedbackCell> feedback_cell) {
  Isolate* isolate = native_context->GetIsolate();
  DCHECK(native_context->IsNativeContext());
  STATIC_ASSERT(kLiteralEntryLength == 2);
  Handle<WeakFixedArray> new_literals_map;
  int entry;

  Object* obj = cache->get(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj->IsFixedArray());
  if (!obj->IsWeakFixedArray() || WeakFixedArray::cast(obj)->length() == 0) {
    new_literals_map =
        isolate->factory()->NewWeakFixedArray(kLiteralInitialLength, TENURED);
    entry = 0;
  } else {
    Handle<WeakFixedArray> old_literals_map(WeakFixedArray::cast(obj), isolate);
    entry = SearchLiteralsMapEntry(*cache, cache_entry, *native_context);
    if (entry >= 0) {
      // Just set the code of the entry.
      old_literals_map->Set(entry + kLiteralLiteralsOffset,
                            HeapObjectReference::Weak(*feedback_cell));
      return;
    }

    // Can we reuse an entry?
    DCHECK_LT(entry, 0);
    int length = old_literals_map->length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      if (old_literals_map->Get(i + kLiteralContextOffset)->IsCleared()) {
        new_literals_map = old_literals_map;
        entry = i;
        break;
      }
    }

    if (entry < 0) {
      // Copy old optimized code map and append one new entry.
      new_literals_map = isolate->factory()->CopyWeakFixedArrayAndGrow(
          old_literals_map, kLiteralEntryLength, TENURED);
      entry = old_literals_map->length();
    }
  }

  new_literals_map->Set(entry + kLiteralContextOffset,
                        HeapObjectReference::Weak(*native_context));
  new_literals_map->Set(entry + kLiteralLiteralsOffset,
                        HeapObjectReference::Weak(*feedback_cell));

#ifdef DEBUG
  for (int i = 0; i < new_literals_map->length(); i += kLiteralEntryLength) {
    MaybeObject* object = new_literals_map->Get(i + kLiteralContextOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak()->IsNativeContext());
    object = new_literals_map->Get(i + kLiteralLiteralsOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak()->IsFeedbackCell());
  }
#endif

  Object* old_literals_map = cache->get(cache_entry);
  if (old_literals_map != *new_literals_map) {
    cache->set(cache_entry, *new_literals_map);
  }
}

FeedbackCell* SearchLiteralsMap(CompilationCacheTable* cache, int cache_entry,
                                Context* native_context) {
  FeedbackCell* result = nullptr;
  int entry = SearchLiteralsMapEntry(cache, cache_entry, native_context);
  if (entry >= 0) {
    WeakFixedArray* literals_map =
        WeakFixedArray::cast(cache->get(cache_entry));
    DCHECK_LE(entry + kLiteralEntryLength, literals_map->length());
    MaybeObject* object = literals_map->Get(entry + kLiteralLiteralsOffset);

    result = object->IsCleared()
                 ? nullptr
                 : FeedbackCell::cast(object->GetHeapObjectAssumeWeak());
  }
  DCHECK(result == nullptr || result->IsFeedbackCell());
  return result;
}

}  // namespace

MaybeHandle<SharedFunctionInfo> CompilationCacheTable::LookupScript(
    Handle<String> src, Handle<Context> native_context,
    LanguageMode language_mode) {
  // We use the empty function SFI as part of the key. Although the
  // empty_function is native context dependent, the SFI is de-duped on
  // snapshot builds by the PartialSnapshotCache, and so this does not prevent
  // reuse of scripts in the compilation cache across native contexts.
  Handle<SharedFunctionInfo> shared(native_context->empty_function()->shared(),
                                    native_context->GetIsolate());
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  int entry = FindEntry(GetIsolate(), &key);
  if (entry == kNotFound) return MaybeHandle<SharedFunctionInfo>();
  int index = EntryToIndex(entry);
  if (!get(index)->IsFixedArray()) return MaybeHandle<SharedFunctionInfo>();
  Object* obj = get(index + 1);
  if (obj->IsSharedFunctionInfo()) {
    return handle(SharedFunctionInfo::cast(obj), native_context->GetIsolate());
  }
  return MaybeHandle<SharedFunctionInfo>();
}

InfoCellPair CompilationCacheTable::LookupEval(
    Handle<String> src, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> native_context, LanguageMode language_mode, int position) {
  InfoCellPair empty_result;
  StringSharedKey key(src, outer_info, language_mode, position);
  int entry = FindEntry(GetIsolate(), &key);
  if (entry == kNotFound) return empty_result;
  int index = EntryToIndex(entry);
  if (!get(index)->IsFixedArray()) return empty_result;
  Object* obj = get(EntryToIndex(entry) + 1);
  if (obj->IsSharedFunctionInfo()) {
    FeedbackCell* feedback_cell =
        SearchLiteralsMap(this, EntryToIndex(entry) + 2, *native_context);
    return InfoCellPair(SharedFunctionInfo::cast(obj), feedback_cell);
  }
  return empty_result;
}

Handle<Object> CompilationCacheTable::LookupRegExp(Handle<String> src,
                                                   JSRegExp::Flags flags) {
  Isolate* isolate = GetIsolate();
  DisallowHeapAllocation no_allocation;
  RegExpKey key(src, flags);
  int entry = FindEntry(isolate, &key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}

Handle<CompilationCacheTable> CompilationCacheTable::PutScript(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<Context> native_context, LanguageMode language_mode,
    Handle<SharedFunctionInfo> value) {
  Isolate* isolate = native_context->GetIsolate();
  // We use the empty function SFI as part of the key. Although the
  // empty_function is native context dependent, the SFI is de-duped on
  // snapshot builds by the PartialSnapshotCache, and so this does not prevent
  // reuse of scripts in the compilation cache across native contexts.
  Handle<SharedFunctionInfo> shared(native_context->empty_function()->shared(),
                                    isolate);
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  Handle<Object> k = key.AsHandle(isolate);
  cache = EnsureCapacity(isolate, cache, 1);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutEval(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<SharedFunctionInfo> value,
    Handle<Context> native_context, Handle<FeedbackCell> feedback_cell,
    int position) {
  Isolate* isolate = native_context->GetIsolate();
  StringSharedKey key(src, outer_info, value->language_mode(), position);
  {
    Handle<Object> k = key.AsHandle(isolate);
    int entry = cache->FindEntry(isolate, &key);
    if (entry != kNotFound) {
      cache->set(EntryToIndex(entry), *k);
      cache->set(EntryToIndex(entry) + 1, *value);
      // AddToFeedbackCellsMap may allocate a new sub-array to live in the
      // entry, but it won't change the cache array. Therefore EntryToIndex
      // and entry remains correct.
      AddToFeedbackCellsMap(cache, EntryToIndex(entry) + 2, native_context,
                            feedback_cell);
      return cache;
    }
  }

  cache = EnsureCapacity(isolate, cache, 1);
  int entry = cache->FindInsertionEntry(key.Hash());
  Handle<Object> k =
      isolate->factory()->NewNumber(static_cast<double>(key.Hash()));
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, Smi::FromInt(kHashGenerations));
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutRegExp(
    Isolate* isolate, Handle<CompilationCacheTable> cache, Handle<String> src,
    JSRegExp::Flags flags, Handle<FixedArray> value) {
  RegExpKey key(src, flags);
  cache = EnsureCapacity(isolate, cache, 1);
  int entry = cache->FindInsertionEntry(key.Hash());
  // We store the value in the key slot, and compare the search key
  // to the stored value with a custon IsMatch function during lookups.
  cache->set(EntryToIndex(entry), *value);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}


void CompilationCacheTable::Age() {
  DisallowHeapAllocation no_allocation;
  Object* the_hole_value = GetReadOnlyRoots().the_hole_value();
  for (int entry = 0, size = Capacity(); entry < size; entry++) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;

    if (get(entry_index)->IsNumber()) {
      Smi* count = Smi::cast(get(value_index));
      count = Smi::FromInt(count->value() - 1);
      if (count->value() == 0) {
        NoWriteBarrierSet(this, entry_index, the_hole_value);
        NoWriteBarrierSet(this, value_index, the_hole_value);
        ElementRemoved();
      } else {
        NoWriteBarrierSet(this, value_index, count);
      }
    } else if (get(entry_index)->IsFixedArray()) {
      SharedFunctionInfo* info = SharedFunctionInfo::cast(get(value_index));
      if (info->IsInterpreted() && info->GetBytecodeArray()->IsOld()) {
        for (int i = 0; i < kEntrySize; i++) {
          NoWriteBarrierSet(this, entry_index + i, the_hole_value);
        }
        ElementRemoved();
      }
    }
  }
}


void CompilationCacheTable::Remove(Object* value) {
  DisallowHeapAllocation no_allocation;
  Object* the_hole_value = GetReadOnlyRoots().the_hole_value();
  for (int entry = 0, size = Capacity(); entry < size; entry++) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;
    if (get(value_index) == value) {
      for (int i = 0; i < kEntrySize; i++) {
        NoWriteBarrierSet(this, entry_index + i, the_hole_value);
      }
      ElementRemoved();
    }
  }
  return;
}

template <typename Derived, typename Shape>
Handle<Derived> BaseNameDictionary<Derived, Shape>::New(
    Isolate* isolate, int at_least_space_for, PretenureFlag pretenure,
    MinimumCapacity capacity_option) {
  DCHECK_LE(0, at_least_space_for);
  Handle<Derived> dict = Dictionary<Derived, Shape>::New(
      isolate, at_least_space_for, pretenure, capacity_option);
  dict->SetHash(PropertyArray::kNoHashSentinel);
  dict->SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return dict;
}

template <typename Derived, typename Shape>
Handle<Derived> BaseNameDictionary<Derived, Shape>::EnsureCapacity(
    Isolate* isolate, Handle<Derived> dictionary, int n) {
  // Check whether there are enough enumeration indices to add n elements.
  if (!PropertyDetails::IsValidIndex(dictionary->NextEnumerationIndex() + n)) {
    // If not, we generate new indices for the properties.
    int length = dictionary->NumberOfElements();

    Handle<FixedArray> iteration_order = IterationIndices(isolate, dictionary);
    DCHECK_EQ(length, iteration_order->length());

    // Iterate over the dictionary using the enumeration order and update
    // the dictionary with new enumeration indices.
    for (int i = 0; i < length; i++) {
      int index = Smi::ToInt(iteration_order->get(i));
      DCHECK(dictionary->IsKey(dictionary->GetReadOnlyRoots(),
                               dictionary->KeyAt(index)));

      int enum_index = PropertyDetails::kInitialIndex + i;

      PropertyDetails details = dictionary->DetailsAt(index);
      PropertyDetails new_details = details.set_index(enum_index);
      dictionary->DetailsAtPut(isolate, index, new_details);
    }

    // Set the next enumeration index.
    dictionary->SetNextEnumerationIndex(PropertyDetails::kInitialIndex +
                                        length);
  }
  return HashTable<Derived, Shape>::EnsureCapacity(isolate, dictionary, n);
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::DeleteEntry(
    Isolate* isolate, Handle<Derived> dictionary, int entry) {
  DCHECK(Shape::kEntrySize != 3 ||
         dictionary->DetailsAt(entry).IsConfigurable());
  dictionary->ClearEntry(isolate, entry);
  dictionary->ElementRemoved();
  return Shrink(isolate, dictionary);
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::AtPut(Isolate* isolate,
                                                  Handle<Derived> dictionary,
                                                  Key key, Handle<Object> value,
                                                  PropertyDetails details) {
  int entry = dictionary->FindEntry(isolate, key);

  // If the entry is present set the value;
  if (entry == Dictionary::kNotFound) {
    return Derived::Add(isolate, dictionary, key, value, details);
  }

  // We don't need to copy over the enumeration index.
  dictionary->ValueAtPut(entry, *value);
  if (Shape::kEntrySize == 3) dictionary->DetailsAtPut(isolate, entry, details);
  return dictionary;
}

template <typename Derived, typename Shape>
Handle<Derived>
BaseNameDictionary<Derived, Shape>::AddNoUpdateNextEnumerationIndex(
    Isolate* isolate, Handle<Derived> dictionary, Key key, Handle<Object> value,
    PropertyDetails details, int* entry_out) {
  // Insert element at empty or deleted entry
  return Dictionary<Derived, Shape>::Add(isolate, dictionary, key, value,
                                         details, entry_out);
}

// GCC workaround: Explicitly instantiate template method for NameDictionary
// to avoid "undefined reference" issues during linking.
template Handle<NameDictionary>
BaseNameDictionary<NameDictionary, NameDictionaryShape>::
    AddNoUpdateNextEnumerationIndex(Isolate* isolate, Handle<NameDictionary>,
                                    Handle<Name>, Handle<Object>,
                                    PropertyDetails, int*);

template <typename Derived, typename Shape>
Handle<Derived> BaseNameDictionary<Derived, Shape>::Add(
    Isolate* isolate, Handle<Derived> dictionary, Key key, Handle<Object> value,
    PropertyDetails details, int* entry_out) {
  // Insert element at empty or deleted entry
  DCHECK_EQ(0, details.dictionary_index());
  // Assign an enumeration index to the property and update
  // SetNextEnumerationIndex.
  int index = dictionary->NextEnumerationIndex();
  details = details.set_index(index);
  dictionary->SetNextEnumerationIndex(index + 1);
  return AddNoUpdateNextEnumerationIndex(isolate, dictionary, key, value,
                                         details, entry_out);
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::Add(Isolate* isolate,
                                                Handle<Derived> dictionary,
                                                Key key, Handle<Object> value,
                                                PropertyDetails details,
                                                int* entry_out) {
  uint32_t hash = Shape::Hash(isolate, key);
  // Valdate key is absent.
  SLOW_DCHECK((dictionary->FindEntry(isolate, key) == Dictionary::kNotFound));
  // Check whether the dictionary should be extended.
  dictionary = Derived::EnsureCapacity(isolate, dictionary, 1);

  // Compute the key object.
  Handle<Object> k = Shape::AsHandle(isolate, key);

  uint32_t entry = dictionary->FindInsertionEntry(hash);
  dictionary->SetEntry(isolate, entry, *k, *value, details);
  DCHECK(dictionary->KeyAt(entry)->IsNumber() ||
         Shape::Unwrap(dictionary->KeyAt(entry))->IsUniqueName());
  dictionary->ElementAdded();
  if (entry_out) *entry_out = entry;
  return dictionary;
}

// static
Handle<SimpleNumberDictionary> SimpleNumberDictionary::Set(
    Isolate* isolate, Handle<SimpleNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value) {
  return AtPut(isolate, dictionary, key, value, PropertyDetails::Empty());
}

bool NumberDictionary::HasComplexElements() {
  if (!requires_slow_elements()) return false;
  ReadOnlyRoots roots = GetReadOnlyRoots();
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k;
    if (!this->ToKey(roots, i, &k)) continue;
    PropertyDetails details = this->DetailsAt(i);
    if (details.kind() == kAccessor) return true;
    PropertyAttributes attr = details.attributes();
    if (attr & ALL_ATTRIBUTES_MASK) return true;
  }
  return false;
}

void NumberDictionary::UpdateMaxNumberKey(uint32_t key,
                                          Handle<JSObject> dictionary_holder) {
  DisallowHeapAllocation no_allocation;
  // If the dictionary requires slow elements an element has already
  // been added at a high index.
  if (requires_slow_elements()) return;
  // Check if this index is high enough that we should require slow
  // elements.
  if (key > kRequiresSlowElementsLimit) {
    if (!dictionary_holder.is_null()) {
      dictionary_holder->RequireSlowElements(this);
    }
    set_requires_slow_elements();
    return;
  }
  // Update max key value.
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi() || max_number_key() < key) {
    FixedArray::set(kMaxNumberKeyIndex,
                    Smi::FromInt(key << kRequiresSlowElementsTagSize));
  }
}

Handle<NumberDictionary> NumberDictionary::Set(
    Isolate* isolate, Handle<NumberDictionary> dictionary, uint32_t key,
    Handle<Object> value, Handle<JSObject> dictionary_holder,
    PropertyDetails details) {
  dictionary->UpdateMaxNumberKey(key, dictionary_holder);
  return AtPut(isolate, dictionary, key, value, details);
}

void NumberDictionary::CopyValuesTo(FixedArray* elements) {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  int pos = 0;
  int capacity = this->Capacity();
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = elements->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < capacity; i++) {
    Object* k;
    if (this->ToKey(roots, i, &k)) {
      elements->set(pos++, this->ValueAt(i), mode);
    }
  }
  DCHECK_EQ(pos, elements->length());
}

template <typename Derived, typename Shape>
int Dictionary<Derived, Shape>::NumberOfEnumerableProperties() {
  ReadOnlyRoots roots = this->GetReadOnlyRoots();
  int capacity = this->Capacity();
  int result = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k;
    if (!this->ToKey(roots, i, &k)) continue;
    if (k->FilterKey(ENUMERABLE_STRINGS)) continue;
    PropertyDetails details = this->DetailsAt(i);
    PropertyAttributes attr = details.attributes();
    if ((attr & ONLY_ENUMERABLE) == 0) result++;
  }
  return result;
}


template <typename Dictionary>
struct EnumIndexComparator {
  explicit EnumIndexComparator(Dictionary* dict) : dict(dict) {}
  bool operator()(const base::AtomicElement<Smi*>& a,
                  const base::AtomicElement<Smi*>& b) {
    PropertyDetails da(dict->DetailsAt(a.value()->value()));
    PropertyDetails db(dict->DetailsAt(b.value()->value()));
    return da.dictionary_index() < db.dictionary_index();
  }
  Dictionary* dict;
};

template <typename Derived, typename Shape>
void BaseNameDictionary<Derived, Shape>::CopyEnumKeysTo(
    Isolate* isolate, Handle<Derived> dictionary, Handle<FixedArray> storage,
    KeyCollectionMode mode, KeyAccumulator* accumulator) {
  DCHECK_IMPLIES(mode != KeyCollectionMode::kOwnOnly, accumulator != nullptr);
  int length = storage->length();
  int capacity = dictionary->Capacity();
  int properties = 0;
  ReadOnlyRoots roots(isolate);
  for (int i = 0; i < capacity; i++) {
    Object* key;
    if (!dictionary->ToKey(roots, i, &key)) continue;
    bool is_shadowing_key = false;
    if (key->IsSymbol()) continue;
    PropertyDetails details = dictionary->DetailsAt(i);
    if (details.IsDontEnum()) {
      if (mode == KeyCollectionMode::kIncludePrototypes) {
        is_shadowing_key = true;
      } else {
        continue;
      }
    }
    if (is_shadowing_key) {
      accumulator->AddShadowingKey(key);
      continue;
    } else {
      storage->set(properties, Smi::FromInt(i));
    }
    properties++;
    if (mode == KeyCollectionMode::kOwnOnly && properties == length) break;
  }

  CHECK_EQ(length, properties);
  DisallowHeapAllocation no_gc;
  Derived* raw_dictionary = *dictionary;
  FixedArray* raw_storage = *storage;
  EnumIndexComparator<Derived> cmp(raw_dictionary);
  // Use AtomicElement wrapper to ensure that std::sort uses atomic load and
  // store operations that are safe for concurrent marking.
  base::AtomicElement<Smi*>* start =
      reinterpret_cast<base::AtomicElement<Smi*>*>(
          storage->GetFirstElementAddress());
  std::sort(start, start + length, cmp);
  for (int i = 0; i < length; i++) {
    int index = Smi::ToInt(raw_storage->get(i));
    raw_storage->set(i, raw_dictionary->NameAt(index));
  }
}

template <typename Derived, typename Shape>
Handle<FixedArray> BaseNameDictionary<Derived, Shape>::IterationIndices(
    Isolate* isolate, Handle<Derived> dictionary) {
  int capacity = dictionary->Capacity();
  int length = dictionary->NumberOfElements();
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);
  ReadOnlyRoots roots(isolate);
  int array_size = 0;
  {
    DisallowHeapAllocation no_gc;
    Derived* raw_dictionary = *dictionary;
    for (int i = 0; i < capacity; i++) {
      Object* k;
      if (!raw_dictionary->ToKey(roots, i, &k)) continue;
      array->set(array_size++, Smi::FromInt(i));
    }

    DCHECK_EQ(array_size, length);

    EnumIndexComparator<Derived> cmp(raw_dictionary);
    // Use AtomicElement wrapper to ensure that std::sort uses atomic load and
    // store operations that are safe for concurrent marking.
    base::AtomicElement<Smi*>* start =
        reinterpret_cast<base::AtomicElement<Smi*>*>(
            array->GetFirstElementAddress());
    std::sort(start, start + array_size, cmp);
  }
  return FixedArray::ShrinkOrEmpty(isolate, array, array_size);
}

template <typename Derived, typename Shape>
void BaseNameDictionary<Derived, Shape>::CollectKeysTo(
    Handle<Derived> dictionary, KeyAccumulator* keys) {
  Isolate* isolate = keys->isolate();
  ReadOnlyRoots roots(isolate);
  int capacity = dictionary->Capacity();
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(dictionary->NumberOfElements());
  int array_size = 0;
  PropertyFilter filter = keys->filter();
  {
    DisallowHeapAllocation no_gc;
    Derived* raw_dictionary = *dictionary;
    for (int i = 0; i < capacity; i++) {
      Object* k;
      if (!raw_dictionary->ToKey(roots, i, &k)) continue;
      if (k->FilterKey(filter)) continue;
      PropertyDetails details = raw_dictionary->DetailsAt(i);
      if ((details.attributes() & filter) != 0) {
        keys->AddShadowingKey(k);
        continue;
      }
      if (filter & ONLY_ALL_CAN_READ) {
        if (details.kind() != kAccessor) continue;
        Object* accessors = raw_dictionary->ValueAt(i);
        if (!accessors->IsAccessorInfo()) continue;
        if (!AccessorInfo::cast(accessors)->all_can_read()) continue;
      }
      array->set(array_size++, Smi::FromInt(i));
    }

    EnumIndexComparator<Derived> cmp(raw_dictionary);
    // Use AtomicElement wrapper to ensure that std::sort uses atomic load and
    // store operations that are safe for concurrent marking.
    base::AtomicElement<Smi*>* start =
        reinterpret_cast<base::AtomicElement<Smi*>*>(
            array->GetFirstElementAddress());
    std::sort(start, start + array_size, cmp);
  }

  bool has_seen_symbol = false;
  for (int i = 0; i < array_size; i++) {
    int index = Smi::ToInt(array->get(i));
    Object* key = dictionary->NameAt(index);
    if (key->IsSymbol()) {
      has_seen_symbol = true;
      continue;
    }
    keys->AddKey(key, DO_NOT_CONVERT);
  }
  if (has_seen_symbol) {
    for (int i = 0; i < array_size; i++) {
      int index = Smi::ToInt(array->get(i));
      Object* key = dictionary->NameAt(index);
      if (!key->IsSymbol()) continue;
      keys->AddKey(key, DO_NOT_CONVERT);
    }
  }
}

// Backwards lookup (slow).
template <typename Derived, typename Shape>
Object* Dictionary<Derived, Shape>::SlowReverseLookup(Object* value) {
  Derived* dictionary = Derived::cast(this);
  ReadOnlyRoots roots = dictionary->GetReadOnlyRoots();
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k;
    if (!dictionary->ToKey(roots, i, &k)) continue;
    Object* e = dictionary->ValueAt(i);
    if (e == value) return k;
  }
  return roots.undefined_value();
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::FillEntriesWithHoles(
    Handle<Derived> table) {
  int length = table->length();
  for (int i = Derived::EntryToIndex(0); i < length; i++) {
    table->set_the_hole(i);
  }
}

template <typename Derived, typename Shape>
Object* ObjectHashTableBase<Derived, Shape>::Lookup(ReadOnlyRoots roots,
                                                    Handle<Object> key,
                                                    int32_t hash) {
  DisallowHeapAllocation no_gc;
  DCHECK(this->IsKey(roots, *key));

  int entry = this->FindEntry(roots, key, hash);
  if (entry == kNotFound) return roots.the_hole_value();
  return this->get(Derived::EntryToIndex(entry) + 1);
}

template <typename Derived, typename Shape>
Object* ObjectHashTableBase<Derived, Shape>::Lookup(Handle<Object> key) {
  DisallowHeapAllocation no_gc;

  ReadOnlyRoots roots = this->GetReadOnlyRoots();
  DCHECK(this->IsKey(roots, *key));

  // If the object does not have an identity hash, it was never used as a key.
  Object* hash = key->GetHash();
  if (hash->IsUndefined(roots)) {
    return roots.the_hole_value();
  }
  return Lookup(roots, key, Smi::ToInt(hash));
}

template <typename Derived, typename Shape>
Object* ObjectHashTableBase<Derived, Shape>::Lookup(Handle<Object> key,
                                                    int32_t hash) {
  return Lookup(this->GetReadOnlyRoots(), key, hash);
}

template <typename Derived, typename Shape>
Object* ObjectHashTableBase<Derived, Shape>::ValueAt(int entry) {
  return this->get(EntryToValueIndex(entry));
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Put(Handle<Derived> table,
                                                         Handle<Object> key,
                                                         Handle<Object> value) {
  Isolate* isolate = Heap::FromWritableHeapObject(*table)->isolate();
  DCHECK(table->IsKey(ReadOnlyRoots(isolate), *key));
  DCHECK(!value->IsTheHole(ReadOnlyRoots(isolate)));

  // Make sure the key object has an identity hash code.
  int32_t hash = key->GetOrCreateHash(isolate)->value();

  return ObjectHashTableBase<Derived, Shape>::Put(isolate, table, key, value,
                                                  hash);
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Put(Isolate* isolate,
                                                         Handle<Derived> table,
                                                         Handle<Object> key,
                                                         Handle<Object> value,
                                                         int32_t hash) {
  ReadOnlyRoots roots(isolate);
  DCHECK(table->IsKey(roots, *key));
  DCHECK(!value->IsTheHole(roots));

  int entry = table->FindEntry(roots, key, hash);

  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    table->set(Derived::EntryToIndex(entry) + 1, *value);
    return table;
  }

  // Rehash if more than 33% of the entries are deleted entries.
  // TODO(jochen): Consider to shrink the fixed array in place.
  if ((table->NumberOfDeletedElements() << 1) > table->NumberOfElements()) {
    table->Rehash(isolate);
  }
  // If we're out of luck, we didn't get a GC recently, and so rehashing
  // isn't enough to avoid a crash.
  if (!table->HasSufficientCapacityToAdd(1)) {
    int nof = table->NumberOfElements() + 1;
    int capacity = ObjectHashTable::ComputeCapacity(nof * 2);
    if (capacity > ObjectHashTable::kMaxCapacity) {
      for (size_t i = 0; i < 2; ++i) {
        isolate->heap()->CollectAllGarbage(
            Heap::kNoGCFlags, GarbageCollectionReason::kFullHashtable);
      }
      table->Rehash(isolate);
    }
  }

  // Check whether the hash table should be extended.
  table = Derived::EnsureCapacity(isolate, table, 1);
  table->AddEntry(table->FindInsertionEntry(hash), *key, *value);
  return table;
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Remove(
    Isolate* isolate, Handle<Derived> table, Handle<Object> key,
    bool* was_present) {
  DCHECK(table->IsKey(table->GetReadOnlyRoots(), *key));

  Object* hash = key->GetHash();
  if (hash->IsUndefined()) {
    *was_present = false;
    return table;
  }

  return Remove(isolate, table, key, was_present, Smi::ToInt(hash));
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Remove(
    Isolate* isolate, Handle<Derived> table, Handle<Object> key,
    bool* was_present, int32_t hash) {
  ReadOnlyRoots roots = table->GetReadOnlyRoots();
  DCHECK(table->IsKey(roots, *key));

  int entry = table->FindEntry(roots, key, hash);
  if (entry == kNotFound) {
    *was_present = false;
    return table;
  }

  *was_present = true;
  table->RemoveEntry(entry);
  return Derived::Shrink(isolate, table);
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::AddEntry(int entry, Object* key,
                                                   Object* value) {
  this->set(Derived::EntryToIndex(entry), key);
  this->set(Derived::EntryToIndex(entry) + 1, value);
  this->ElementAdded();
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::RemoveEntry(int entry) {
  this->set_the_hole(Derived::EntryToIndex(entry));
  this->set_the_hole(Derived::EntryToIndex(entry) + 1);
  this->ElementRemoved();
}


void JSSet::Initialize(Handle<JSSet> set, Isolate* isolate) {
  Handle<OrderedHashSet> table = isolate->factory()->NewOrderedHashSet();
  set->set_table(*table);
}

void JSSet::Clear(Isolate* isolate, Handle<JSSet> set) {
  Handle<OrderedHashSet> table(OrderedHashSet::cast(set->table()), isolate);
  table = OrderedHashSet::Clear(isolate, table);
  set->set_table(*table);
}


void JSMap::Initialize(Handle<JSMap> map, Isolate* isolate) {
  Handle<OrderedHashMap> table = isolate->factory()->NewOrderedHashMap();
  map->set_table(*table);
}

void JSMap::Clear(Isolate* isolate, Handle<JSMap> map) {
  Handle<OrderedHashMap> table(OrderedHashMap::cast(map->table()), isolate);
  table = OrderedHashMap::Clear(isolate, table);
  map->set_table(*table);
}


void JSWeakCollection::Initialize(Handle<JSWeakCollection> weak_collection,
                                  Isolate* isolate) {
  Handle<EphemeronHashTable> table = EphemeronHashTable::New(isolate, 0);
  weak_collection->set_table(*table);
}


void JSWeakCollection::Set(Handle<JSWeakCollection> weak_collection,
                           Handle<Object> key, Handle<Object> value,
                           int32_t hash) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<EphemeronHashTable> table(
      EphemeronHashTable::cast(weak_collection->table()),
      weak_collection->GetIsolate());
  DCHECK(table->IsKey(weak_collection->GetReadOnlyRoots(), *key));
  Handle<EphemeronHashTable> new_table = EphemeronHashTable::Put(
      weak_collection->GetIsolate(), table, key, value, hash);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    EphemeronHashTable::FillEntriesWithHoles(table);
  }
}


bool JSWeakCollection::Delete(Handle<JSWeakCollection> weak_collection,
                              Handle<Object> key, int32_t hash) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<EphemeronHashTable> table(
      EphemeronHashTable::cast(weak_collection->table()),
      weak_collection->GetIsolate());
  DCHECK(table->IsKey(weak_collection->GetReadOnlyRoots(), *key));
  bool was_present = false;
  Handle<EphemeronHashTable> new_table = EphemeronHashTable::Remove(
      weak_collection->GetIsolate(), table, key, &was_present, hash);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    EphemeronHashTable::FillEntriesWithHoles(table);
  }
  return was_present;
}

Handle<JSArray> JSWeakCollection::GetEntries(Handle<JSWeakCollection> holder,
                                             int max_entries) {
  Isolate* isolate = holder->GetIsolate();
  Handle<EphemeronHashTable> table(EphemeronHashTable::cast(holder->table()),
                                   isolate);
  if (max_entries == 0 || max_entries > table->NumberOfElements()) {
    max_entries = table->NumberOfElements();
  }
  int values_per_entry = holder->IsJSWeakMap() ? 2 : 1;
  Handle<FixedArray> entries =
      isolate->factory()->NewFixedArray(max_entries * values_per_entry);
  // Recompute max_values because GC could have removed elements from the table.
  if (max_entries > table->NumberOfElements()) {
    max_entries = table->NumberOfElements();
  }

  {
    DisallowHeapAllocation no_gc;
    ReadOnlyRoots roots = ReadOnlyRoots(isolate);
    int count = 0;
    for (int i = 0;
         count / values_per_entry < max_entries && i < table->Capacity(); i++) {
      Object* key;
      if (table->ToKey(roots, i, &key)) {
        entries->set(count++, key);
        if (values_per_entry > 1) {
          Object* value = table->Lookup(handle(key, isolate));
          entries->set(count++, value);
        }
      }
    }
    DCHECK_EQ(max_entries * values_per_entry, count);
  }
  return isolate->factory()->NewJSArrayWithElements(entries);
}

// static
MaybeHandle<JSDate> JSDate::New(Handle<JSFunction> constructor,
                                Handle<JSReceiver> new_target, double tv) {
  Isolate* const isolate = constructor->GetIsolate();
  Handle<JSObject> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      JSObject::New(constructor, new_target, Handle<AllocationSite>::null()),
      JSDate);
  if (-DateCache::kMaxTimeInMs <= tv && tv <= DateCache::kMaxTimeInMs) {
    tv = DoubleToInteger(tv) + 0.0;
  } else {
    tv = std::numeric_limits<double>::quiet_NaN();
  }
  Handle<Object> value = isolate->factory()->NewNumber(tv);
  Handle<JSDate>::cast(result)->SetValue(*value, std::isnan(tv));
  return Handle<JSDate>::cast(result);
}


// static
double JSDate::CurrentTimeValue(Isolate* isolate) {
  if (FLAG_log_internal_timer_events) LOG(isolate, CurrentTimeEvent());

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  return Floor(V8::GetCurrentPlatform()->CurrentClockTimeMillis());
}


// static
Object* JSDate::GetField(Object* object, Smi* index) {
  return JSDate::cast(object)->DoGetField(
      static_cast<FieldIndex>(index->value()));
}


Object* JSDate::DoGetField(FieldIndex index) {
  DCHECK_NE(index, kDateValue);

  DateCache* date_cache = GetIsolate()->date_cache();

  if (index < kFirstUncachedField) {
    Object* stamp = cache_stamp();
    if (stamp != date_cache->stamp() && stamp->IsSmi()) {
      // Since the stamp is not NaN, the value is also not NaN.
      int64_t local_time_ms =
          date_cache->ToLocal(static_cast<int64_t>(value()->Number()));
      SetCachedFields(local_time_ms, date_cache);
    }
    switch (index) {
      case kYear: return year();
      case kMonth: return month();
      case kDay: return day();
      case kWeekday: return weekday();
      case kHour: return hour();
      case kMinute: return min();
      case kSecond: return sec();
      default: UNREACHABLE();
    }
  }

  if (index >= kFirstUTCField) {
    return GetUTCField(index, value()->Number(), date_cache);
  }

  double time = value()->Number();
  if (std::isnan(time)) return GetReadOnlyRoots().nan_value();

  int64_t local_time_ms = date_cache->ToLocal(static_cast<int64_t>(time));
  int days = DateCache::DaysFromTime(local_time_ms);

  if (index == kDays) return Smi::FromInt(days);

  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  if (index == kMillisecond) return Smi::FromInt(time_in_day_ms % 1000);
  DCHECK_EQ(index, kTimeInDay);
  return Smi::FromInt(time_in_day_ms);
}


Object* JSDate::GetUTCField(FieldIndex index,
                            double value,
                            DateCache* date_cache) {
  DCHECK_GE(index, kFirstUTCField);

  if (std::isnan(value)) return GetReadOnlyRoots().nan_value();

  int64_t time_ms = static_cast<int64_t>(value);

  if (index == kTimezoneOffset) {
    return Smi::FromInt(date_cache->TimezoneOffset(time_ms));
  }

  int days = DateCache::DaysFromTime(time_ms);

  if (index == kWeekdayUTC) return Smi::FromInt(date_cache->Weekday(days));

  if (index <= kDayUTC) {
    int year, month, day;
    date_cache->YearMonthDayFromDays(days, &year, &month, &day);
    if (index == kYearUTC) return Smi::FromInt(year);
    if (index == kMonthUTC) return Smi::FromInt(month);
    DCHECK_EQ(index, kDayUTC);
    return Smi::FromInt(day);
  }

  int time_in_day_ms = DateCache::TimeInDay(time_ms, days);
  switch (index) {
    case kHourUTC: return Smi::FromInt(time_in_day_ms / (60 * 60 * 1000));
    case kMinuteUTC: return Smi::FromInt((time_in_day_ms / (60 * 1000)) % 60);
    case kSecondUTC: return Smi::FromInt((time_in_day_ms / 1000) % 60);
    case kMillisecondUTC: return Smi::FromInt(time_in_day_ms % 1000);
    case kDaysUTC: return Smi::FromInt(days);
    case kTimeInDayUTC: return Smi::FromInt(time_in_day_ms);
    default: UNREACHABLE();
  }

  UNREACHABLE();
}


// static
Handle<Object> JSDate::SetValue(Handle<JSDate> date, double v) {
  Isolate* const isolate = date->GetIsolate();
  Handle<Object> value = isolate->factory()->NewNumber(v);
  bool value_is_nan = std::isnan(v);
  date->SetValue(*value, value_is_nan);
  return value;
}


void JSDate::SetValue(Object* value, bool is_value_nan) {
  set_value(value);
  if (is_value_nan) {
    HeapNumber* nan = GetReadOnlyRoots().nan_value();
    set_cache_stamp(nan, SKIP_WRITE_BARRIER);
    set_year(nan, SKIP_WRITE_BARRIER);
    set_month(nan, SKIP_WRITE_BARRIER);
    set_day(nan, SKIP_WRITE_BARRIER);
    set_hour(nan, SKIP_WRITE_BARRIER);
    set_min(nan, SKIP_WRITE_BARRIER);
    set_sec(nan, SKIP_WRITE_BARRIER);
    set_weekday(nan, SKIP_WRITE_BARRIER);
  } else {
    set_cache_stamp(Smi::FromInt(DateCache::kInvalidStamp), SKIP_WRITE_BARRIER);
  }
}


void JSDate::SetCachedFields(int64_t local_time_ms, DateCache* date_cache) {
  int days = DateCache::DaysFromTime(local_time_ms);
  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  int year, month, day;
  date_cache->YearMonthDayFromDays(days, &year, &month, &day);
  int weekday = date_cache->Weekday(days);
  int hour = time_in_day_ms / (60 * 60 * 1000);
  int min = (time_in_day_ms / (60 * 1000)) % 60;
  int sec = (time_in_day_ms / 1000) % 60;
  set_cache_stamp(date_cache->stamp());
  set_year(Smi::FromInt(year), SKIP_WRITE_BARRIER);
  set_month(Smi::FromInt(month), SKIP_WRITE_BARRIER);
  set_day(Smi::FromInt(day), SKIP_WRITE_BARRIER);
  set_weekday(Smi::FromInt(weekday), SKIP_WRITE_BARRIER);
  set_hour(Smi::FromInt(hour), SKIP_WRITE_BARRIER);
  set_min(Smi::FromInt(min), SKIP_WRITE_BARRIER);
  set_sec(Smi::FromInt(sec), SKIP_WRITE_BARRIER);
}

int JSMessageObject::GetLineNumber() const {
  if (start_position() == -1) return Message::kNoLineNumberInfo;

  Handle<Script> the_script(script(), GetIsolate());

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return Message::kNoLineNumberInfo;
  }

  return info.line + 1;
}

int JSMessageObject::GetColumnNumber() const {
  if (start_position() == -1) return -1;

  Handle<Script> the_script(script(), GetIsolate());

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return -1;
  }

  return info.column;  // Note: No '+1' in contrast to GetLineNumber.
}

Handle<String> JSMessageObject::GetSourceLine() const {
  Isolate* isolate = GetIsolate();
  Handle<Script> the_script(script(), isolate);

  if (the_script->type() == Script::TYPE_WASM) {
    return isolate->factory()->empty_string();
  }

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return isolate->factory()->empty_string();
  }

  Handle<String> src = handle(String::cast(the_script->source()), isolate);
  return isolate->factory()->NewSubString(src, info.line_start, info.line_end);
}

Handle<PropertyCell> PropertyCell::InvalidateEntry(
    Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry) {
  // Swap with a copy.
  Handle<PropertyCell> cell(dictionary->CellAt(entry), isolate);
  Handle<Name> name(cell->name(), isolate);
  Handle<PropertyCell> new_cell = isolate->factory()->NewPropertyCell(name);
  new_cell->set_value(cell->value());
  dictionary->ValueAtPut(entry, *new_cell);
  bool is_the_hole = cell->value()->IsTheHole(isolate);
  // Cell is officially mutable henceforth.
  PropertyDetails details = cell->property_details();
  details = details.set_cell_type(is_the_hole ? PropertyCellType::kUninitialized
                                              : PropertyCellType::kMutable);
  new_cell->set_property_details(details);
  // Old cell is ready for invalidation.
  if (is_the_hole) {
    cell->set_value(ReadOnlyRoots(isolate).undefined_value());
  } else {
    cell->set_value(ReadOnlyRoots(isolate).the_hole_value());
  }
  details = details.set_cell_type(PropertyCellType::kInvalidated);
  cell->set_property_details(details);
  cell->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kPropertyCellChangedGroup);
  return new_cell;
}


PropertyCellConstantType PropertyCell::GetConstantType() {
  if (value()->IsSmi()) return PropertyCellConstantType::kSmi;
  return PropertyCellConstantType::kStableMap;
}


static bool RemainsConstantType(Handle<PropertyCell> cell,
                                Handle<Object> value) {
  // TODO(dcarney): double->smi and smi->double transition from kConstant
  if (cell->value()->IsSmi() && value->IsSmi()) {
    return true;
  } else if (cell->value()->IsHeapObject() && value->IsHeapObject()) {
    return HeapObject::cast(cell->value())->map() ==
               HeapObject::cast(*value)->map() &&
           HeapObject::cast(*value)->map()->is_stable();
  }
  return false;
}

PropertyCellType PropertyCell::UpdatedType(Isolate* isolate,
                                           Handle<PropertyCell> cell,
                                           Handle<Object> value,
                                           PropertyDetails details) {
  PropertyCellType type = details.cell_type();
  DCHECK(!value->IsTheHole(isolate));
  if (cell->value()->IsTheHole(isolate)) {
    switch (type) {
      // Only allow a cell to transition once into constant state.
      case PropertyCellType::kUninitialized:
        if (value->IsUndefined(isolate)) return PropertyCellType::kUndefined;
        return PropertyCellType::kConstant;
      case PropertyCellType::kInvalidated:
        return PropertyCellType::kMutable;
      default:
        UNREACHABLE();
    }
  }
  switch (type) {
    case PropertyCellType::kUndefined:
      return PropertyCellType::kConstant;
    case PropertyCellType::kConstant:
      if (*value == cell->value()) return PropertyCellType::kConstant;
      V8_FALLTHROUGH;
    case PropertyCellType::kConstantType:
      if (RemainsConstantType(cell, value)) {
        return PropertyCellType::kConstantType;
      }
      V8_FALLTHROUGH;
    case PropertyCellType::kMutable:
      return PropertyCellType::kMutable;
  }
  UNREACHABLE();
}

Handle<PropertyCell> PropertyCell::PrepareForValue(
    Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry,
    Handle<Object> value, PropertyDetails details) {
  DCHECK(!value->IsTheHole(isolate));
  Handle<PropertyCell> cell(dictionary->CellAt(entry), isolate);
  const PropertyDetails original_details = cell->property_details();
  // Data accesses could be cached in ics or optimized code.
  bool invalidate =
      (original_details.kind() == kData && details.kind() == kAccessor) ||
      (!original_details.IsReadOnly() && details.IsReadOnly());
  int index;
  PropertyCellType old_type = original_details.cell_type();
  // Preserve the enumeration index unless the property was deleted or never
  // initialized.
  if (cell->value()->IsTheHole(isolate)) {
    index = dictionary->NextEnumerationIndex();
    dictionary->SetNextEnumerationIndex(index + 1);
  } else {
    index = original_details.dictionary_index();
  }
  DCHECK_LT(0, index);
  details = details.set_index(index);

  PropertyCellType new_type =
      UpdatedType(isolate, cell, value, original_details);
  if (invalidate) {
    cell = PropertyCell::InvalidateEntry(isolate, dictionary, entry);
  }

  // Install new property details.
  details = details.set_cell_type(new_type);
  cell->set_property_details(details);

  if (new_type == PropertyCellType::kConstant ||
      new_type == PropertyCellType::kConstantType) {
    // Store the value now to ensure that the cell contains the constant or
    // type information. Otherwise subsequent store operation will turn
    // the cell to mutable.
    cell->set_value(*value);
  }

  // Deopt when transitioning from a constant type.
  if (!invalidate && (old_type != new_type ||
                      original_details.IsReadOnly() != details.IsReadOnly())) {
    cell->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
  return cell;
}


// static
void PropertyCell::SetValueWithInvalidation(Isolate* isolate,
                                            Handle<PropertyCell> cell,
                                            Handle<Object> new_value) {
  if (cell->value() != *new_value) {
    cell->set_value(*new_value);
    cell->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
}

int JSGeneratorObject::source_position() const {
  CHECK(is_suspended());
  DCHECK(function()->shared()->HasBytecodeArray());

  int code_offset = Smi::ToInt(input_or_debug_pos());

  // The stored bytecode offset is relative to a different base than what
  // is used in the source position table, hence the subtraction.
  code_offset -= BytecodeArray::kHeaderSize - kHeapObjectTag;
  AbstractCode* code =
      AbstractCode::cast(function()->shared()->GetBytecodeArray());
  return code->SourcePosition(code_offset);
}

// static
AccessCheckInfo* AccessCheckInfo::Get(Isolate* isolate,
                                      Handle<JSObject> receiver) {
  DisallowHeapAllocation no_gc;
  DCHECK(receiver->map()->is_access_check_needed());
  Object* maybe_constructor = receiver->map()->GetConstructor();
  if (maybe_constructor->IsFunctionTemplateInfo()) {
    Object* data_obj =
        FunctionTemplateInfo::cast(maybe_constructor)->access_check_info();
    if (data_obj->IsUndefined(isolate)) return nullptr;
    return AccessCheckInfo::cast(data_obj);
  }
  // Might happen for a detached context.
  if (!maybe_constructor->IsJSFunction()) return nullptr;
  JSFunction* constructor = JSFunction::cast(maybe_constructor);
  // Might happen for the debug context.
  if (!constructor->shared()->IsApiFunction()) return nullptr;

  Object* data_obj =
      constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj->IsUndefined(isolate)) return nullptr;

  return AccessCheckInfo::cast(data_obj);
}

bool JSReceiver::HasProxyInPrototype(Isolate* isolate) {
  for (PrototypeIterator iter(isolate, this, kStartAtReceiver,
                              PrototypeIterator::END_AT_NULL);
       !iter.IsAtEnd(); iter.AdvanceIgnoringProxies()) {
    if (iter.GetCurrent<Object>()->IsJSProxy()) return true;
  }
  return false;
}

bool JSReceiver::HasComplexElements() {
  if (IsJSProxy()) return true;
  JSObject* this_object = JSObject::cast(this);
  if (this_object->HasIndexedInterceptor()) {
    return true;
  }
  if (!this_object->HasDictionaryElements()) return false;
  return this_object->element_dictionary()->HasComplexElements();
}

MaybeHandle<Name> FunctionTemplateInfo::TryGetCachedPropertyName(
    Isolate* isolate, Handle<Object> getter) {
  if (getter->IsFunctionTemplateInfo()) {
    Handle<FunctionTemplateInfo> fti =
        Handle<FunctionTemplateInfo>::cast(getter);
    // Check if the accessor uses a cached property.
    if (!fti->cached_property_name()->IsTheHole(isolate)) {
      return handle(Name::cast(fti->cached_property_name()), isolate);
    }
  }
  return MaybeHandle<Name>();
}

Smi* Smi::LexicographicCompare(Isolate* isolate, Smi* x, Smi* y) {
  DisallowHeapAllocation no_allocation;
  DisallowJavascriptExecution no_js(isolate);

  int x_value = Smi::ToInt(x);
  int y_value = Smi::ToInt(y);

  // If the integers are equal so are the string representations.
  if (x_value == y_value) return Smi::FromInt(0);

  // If one of the integers is zero the normal integer order is the
  // same as the lexicographic order of the string representations.
  if (x_value == 0 || y_value == 0)
    return Smi::FromInt(x_value < y_value ? -1 : 1);

  // If only one of the integers is negative the negative number is
  // smallest because the char code of '-' is less than the char code
  // of any digit.  Otherwise, we make both values positive.

  // Use unsigned values otherwise the logic is incorrect for -MIN_INT on
  // architectures using 32-bit Smis.
  uint32_t x_scaled = x_value;
  uint32_t y_scaled = y_value;
  if (x_value < 0 || y_value < 0) {
    if (y_value >= 0) return Smi::FromInt(-1);
    if (x_value >= 0) return Smi::FromInt(1);
    x_scaled = -x_value;
    y_scaled = -y_value;
  }

  // clang-format off
  static const uint32_t kPowersOf10[] = {
      1,                 10,                100,         1000,
      10 * 1000,         100 * 1000,        1000 * 1000, 10 * 1000 * 1000,
      100 * 1000 * 1000, 1000 * 1000 * 1000};
  // clang-format on

  // If the integers have the same number of decimal digits they can be
  // compared directly as the numeric order is the same as the
  // lexicographic order.  If one integer has fewer digits, it is scaled
  // by some power of 10 to have the same number of digits as the longer
  // integer.  If the scaled integers are equal it means the shorter
  // integer comes first in the lexicographic order.

  // From http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
  int x_log2 = 31 - base::bits::CountLeadingZeros(x_scaled);
  int x_log10 = ((x_log2 + 1) * 1233) >> 12;
  x_log10 -= x_scaled < kPowersOf10[x_log10];

  int y_log2 = 31 - base::bits::CountLeadingZeros(y_scaled);
  int y_log10 = ((y_log2 + 1) * 1233) >> 12;
  y_log10 -= y_scaled < kPowersOf10[y_log10];

  int tie = 0;

  if (x_log10 < y_log10) {
    // X has fewer digits.  We would like to simply scale up X but that
    // might overflow, e.g when comparing 9 with 1_000_000_000, 9 would
    // be scaled up to 9_000_000_000. So we scale up by the next
    // smallest power and scale down Y to drop one digit. It is OK to
    // drop one digit from the longer integer since the final digit is
    // past the length of the shorter integer.
    x_scaled *= kPowersOf10[y_log10 - x_log10 - 1];
    y_scaled /= 10;
    tie = -1;
  } else if (y_log10 < x_log10) {
    y_scaled *= kPowersOf10[x_log10 - y_log10 - 1];
    x_scaled /= 10;
    tie = 1;
  }

  if (x_scaled < y_scaled) return Smi::FromInt(-1);
  if (x_scaled > y_scaled) return Smi::FromInt(1);
  return Smi::FromInt(tie);
}

// Force instantiation of template instances class.
// Please note this list is compiler dependent.
// Keep this at the end of this file

template class HashTable<StringTable, StringTableShape>;

template class HashTable<CompilationCacheTable, CompilationCacheShape>;

template class HashTable<ObjectHashTable, ObjectHashTableShape>;

template class HashTable<EphemeronHashTable, EphemeronHashTableShape>;

template class ObjectHashTableBase<ObjectHashTable, ObjectHashTableShape>;

template class ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape>;

template class Dictionary<NameDictionary, NameDictionaryShape>;

template class Dictionary<GlobalDictionary, GlobalDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) HashTable<NumberDictionary, NumberDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Dictionary<NumberDictionary, NumberDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    HashTable<SimpleNumberDictionary, SimpleNumberDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Dictionary<SimpleNumberDictionary, SimpleNumberDictionaryShape>;

template Handle<NameDictionary>
BaseNameDictionary<NameDictionary, NameDictionaryShape>::New(
    Isolate*, int n, PretenureFlag pretenure, MinimumCapacity capacity_option);

template Handle<GlobalDictionary>
BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>::New(
    Isolate*, int n, PretenureFlag pretenure, MinimumCapacity capacity_option);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape>::New(Isolate*, int,
                                                    PretenureFlag,
                                                    MinimumCapacity);

template Handle<ObjectHashSet>
HashTable<ObjectHashSet, ObjectHashSetShape>::New(Isolate*, int n,
                                                  PretenureFlag,
                                                  MinimumCapacity);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape>::Shrink(Isolate* isolate,
                                                       Handle<NameDictionary>,
                                                       int additionalCapacity);

template Handle<NameDictionary>
BaseNameDictionary<NameDictionary, NameDictionaryShape>::Add(
    Isolate* isolate, Handle<NameDictionary>, Handle<Name>, Handle<Object>,
    PropertyDetails, int*);

template Handle<GlobalDictionary>
BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>::Add(
    Isolate* isolate, Handle<GlobalDictionary>, Handle<Name>, Handle<Object>,
    PropertyDetails, int*);

template void HashTable<GlobalDictionary, GlobalDictionaryShape>::Rehash(
    Isolate* isolate);

template Handle<NameDictionary>
BaseNameDictionary<NameDictionary, NameDictionaryShape>::EnsureCapacity(
    Isolate* isolate, Handle<NameDictionary>, int);

template void
BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>::CopyEnumKeysTo(
    Isolate* isolate, Handle<GlobalDictionary> dictionary,
    Handle<FixedArray> storage, KeyCollectionMode mode,
    KeyAccumulator* accumulator);

template void
BaseNameDictionary<NameDictionary, NameDictionaryShape>::CopyEnumKeysTo(
    Isolate* isolate, Handle<NameDictionary> dictionary,
    Handle<FixedArray> storage, KeyCollectionMode mode,
    KeyAccumulator* accumulator);

template Handle<FixedArray>
BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>::IterationIndices(
    Isolate* isolate, Handle<GlobalDictionary> dictionary);
template void
BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>::CollectKeysTo(
    Handle<GlobalDictionary> dictionary, KeyAccumulator* keys);

template Handle<FixedArray>
BaseNameDictionary<NameDictionary, NameDictionaryShape>::IterationIndices(
    Isolate* isolate, Handle<NameDictionary> dictionary);
template void
BaseNameDictionary<NameDictionary, NameDictionaryShape>::CollectKeysTo(
    Handle<NameDictionary> dictionary, KeyAccumulator* keys);

}  // namespace internal
}  // namespace v8
