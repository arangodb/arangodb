// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/objects-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#endif

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.1 Number Objects

// ES6 section 20.1.3.2 Number.prototype.toExponential ( fractionDigits )
BUILTIN(NumberPrototypeToExponential) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> fraction_digits = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSPrimitiveWrapper()) {
    value = handle(Handle<JSPrimitiveWrapper>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toExponential"),
                              isolate->factory()->Number_string()));
  }
  double const value_number = value->Number();

  // Convert the {fraction_digits} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, fraction_digits, Object::ToInteger(isolate, fraction_digits));
  double const fraction_digits_number = fraction_digits->Number();

  if (std::isnan(value_number)) return ReadOnlyRoots(isolate).NaN_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? ReadOnlyRoots(isolate).minus_Infinity_string()
                                : ReadOnlyRoots(isolate).Infinity_string();
  }
  if (fraction_digits_number < 0.0 ||
      fraction_digits_number > kMaxFractionDigits) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kNumberFormatRange,
                               isolate->factory()->NewStringFromAsciiChecked(
                                   "toExponential()")));
  }
  int const f = args.atOrUndefined(isolate, 1)->IsUndefined(isolate)
                    ? -1
                    : static_cast<int>(fraction_digits_number);
  char* const str = DoubleToExponentialCString(value_number, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

// ES6 section 20.1.3.3 Number.prototype.toFixed ( fractionDigits )
BUILTIN(NumberPrototypeToFixed) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> fraction_digits = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSPrimitiveWrapper()) {
    value = handle(Handle<JSPrimitiveWrapper>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toFixed"),
                              isolate->factory()->Number_string()));
  }
  double const value_number = value->Number();

  // Convert the {fraction_digits} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, fraction_digits, Object::ToInteger(isolate, fraction_digits));
  double const fraction_digits_number = fraction_digits->Number();

  // Check if the {fraction_digits} are in the supported range.
  if (fraction_digits_number < 0.0 ||
      fraction_digits_number > kMaxFractionDigits) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kNumberFormatRange,
                               isolate->factory()->NewStringFromAsciiChecked(
                                   "toFixed() digits")));
  }

  if (std::isnan(value_number)) return ReadOnlyRoots(isolate).NaN_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? ReadOnlyRoots(isolate).minus_Infinity_string()
                                : ReadOnlyRoots(isolate).Infinity_string();
  }
  char* const str = DoubleToFixedCString(
      value_number, static_cast<int>(fraction_digits_number));
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

// ES6 section 20.1.3.4 Number.prototype.toLocaleString ( [ r1 [ , r2 ] ] )
BUILTIN(NumberPrototypeToLocaleString) {
  HandleScope scope(isolate);
  const char* method = "Number.prototype.toLocaleString";

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kNumberToLocaleString);

  Handle<Object> value = args.at(0);

  // Unwrap the receiver {value}.
  if (value->IsJSPrimitiveWrapper()) {
    value = handle(Handle<JSPrimitiveWrapper>::cast(value)->value(), isolate);
  }
  // 1. Let x be ? thisNumberValue(this value)
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kNotGeneric,
                     isolate->factory()->NewStringFromAsciiChecked(method),
                     isolate->factory()->Number_string()));
  }

#ifdef V8_INTL_SUPPORT
  RETURN_RESULT_OR_FAILURE(
      isolate,
      Intl::NumberToLocaleString(isolate, value, args.atOrUndefined(isolate, 1),
                                 args.atOrUndefined(isolate, 2), method));
#else
  // Turn the {value} into a String.
  return *isolate->factory()->NumberToString(value);
#endif  // V8_INTL_SUPPORT
}

// ES6 section 20.1.3.5 Number.prototype.toPrecision ( precision )
BUILTIN(NumberPrototypeToPrecision) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> precision = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSPrimitiveWrapper()) {
    value = handle(Handle<JSPrimitiveWrapper>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toPrecision"),
                              isolate->factory()->Number_string()));
  }
  double const value_number = value->Number();

  // If no {precision} was specified, just return ToString of {value}.
  if (precision->IsUndefined(isolate)) {
    return *isolate->factory()->NumberToString(value);
  }

  // Convert the {precision} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, precision,
                                     Object::ToInteger(isolate, precision));
  double const precision_number = precision->Number();

  if (std::isnan(value_number)) return ReadOnlyRoots(isolate).NaN_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? ReadOnlyRoots(isolate).minus_Infinity_string()
                                : ReadOnlyRoots(isolate).Infinity_string();
  }
  if (precision_number < 1.0 || precision_number > kMaxFractionDigits) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kToPrecisionFormatRange));
  }
  char* const str = DoubleToPrecisionCString(
      value_number, static_cast<int>(precision_number));
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

// ES6 section 20.1.3.6 Number.prototype.toString ( [ radix ] )
BUILTIN(NumberPrototypeToString) {
  HandleScope scope(isolate);
  Handle<Object> value = args.at(0);
  Handle<Object> radix = args.atOrUndefined(isolate, 1);

  // Unwrap the receiver {value}.
  if (value->IsJSPrimitiveWrapper()) {
    value = handle(Handle<JSPrimitiveWrapper>::cast(value)->value(), isolate);
  }
  if (!value->IsNumber()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotGeneric,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Number.prototype.toString"),
                              isolate->factory()->Number_string()));
  }
  double const value_number = value->Number();

  // If no {radix} was specified, just return ToString of {value}.
  if (radix->IsUndefined(isolate)) {
    return *isolate->factory()->NumberToString(value);
  }

  // Convert the {radix} to an integer first.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, radix,
                                     Object::ToInteger(isolate, radix));
  double const radix_number = radix->Number();

  // If {radix} is 10, just return ToString of {value}.
  if (radix_number == 10.0) return *isolate->factory()->NumberToString(value);

  // Make sure the {radix} is within the valid range.
  if (radix_number < 2.0 || radix_number > 36.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kToRadixFormatRange));
  }

  // Fast case where the result is a one character string.
  if ((IsUint32Double(value_number) && value_number < radix_number) ||
      IsMinusZero(value_number)) {
    // Character array used for conversion.
    static const char kCharTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    return *isolate->factory()->LookupSingleCharacterStringFromCode(
        kCharTable[static_cast<uint32_t>(value_number)]);
  }

  // Slow case.
  if (std::isnan(value_number)) return ReadOnlyRoots(isolate).NaN_string();
  if (std::isinf(value_number)) {
    return (value_number < 0.0) ? ReadOnlyRoots(isolate).minus_Infinity_string()
                                : ReadOnlyRoots(isolate).Infinity_string();
  }
  char* const str =
      DoubleToRadixCString(value_number, static_cast<int>(radix_number));
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

}  // namespace internal
}  // namespace v8
