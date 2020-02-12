// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.3 Date Objects

class DateBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit DateBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_DatePrototype_GetField(TNode<Context> context,
                                       TNode<Object> receiver, int field_index);
};

void DateBuiltinsAssembler::Generate_DatePrototype_GetField(
    TNode<Context> context, TNode<Object> receiver, int field_index) {
  Label receiver_not_date(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &receiver_not_date);
  TNode<Uint16T> receiver_instance_type = LoadInstanceType(CAST(receiver));
  GotoIfNot(InstanceTypeEqual(receiver_instance_type, JS_DATE_TYPE),
            &receiver_not_date);

  TNode<JSDate> date_receiver = CAST(receiver);
  // Load the specified date field, falling back to the runtime as necessary.
  if (field_index == JSDate::kDateValue) {
    Return(LoadObjectField(date_receiver, JSDate::kValueOffset));
  } else {
    if (field_index < JSDate::kFirstUncachedField) {
      Label stamp_mismatch(this, Label::kDeferred);
      TNode<Object> date_cache_stamp = Load<Object>(
          ExternalConstant(ExternalReference::date_cache_stamp(isolate())));

      TNode<Object> cache_stamp =
          LoadObjectField(date_receiver, JSDate::kCacheStampOffset);
      GotoIf(TaggedNotEqual(date_cache_stamp, cache_stamp), &stamp_mismatch);
      Return(LoadObjectField(date_receiver,
                             JSDate::kValueOffset + field_index * kTaggedSize));

      BIND(&stamp_mismatch);
    }

    TNode<Smi> field_index_smi = SmiConstant(field_index);
    TNode<ExternalReference> function =
        ExternalConstant(ExternalReference::get_date_field_function());
    TNode<Object> result = CAST(CallCFunction(
        function, MachineType::AnyTagged(),
        std::make_pair(MachineType::AnyTagged(), date_receiver),
        std::make_pair(MachineType::AnyTagged(), field_index_smi)));
    Return(result);
  }

  // Raise a TypeError if the receiver is not a date.
  BIND(&receiver_not_date);
  { ThrowTypeError(context, MessageTemplate::kNotDateObject); }
}

TF_BUILTIN(DatePrototypeGetDate, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDay);
}

TF_BUILTIN(DatePrototypeGetDay, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kWeekday);
}

TF_BUILTIN(DatePrototypeGetFullYear, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kYear);
}

TF_BUILTIN(DatePrototypeGetHours, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kHour);
}

TF_BUILTIN(DatePrototypeGetMilliseconds, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMillisecond);
}

TF_BUILTIN(DatePrototypeGetMinutes, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMinute);
}

TF_BUILTIN(DatePrototypeGetMonth, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMonth);
}

TF_BUILTIN(DatePrototypeGetSeconds, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kSecond);
}

TF_BUILTIN(DatePrototypeGetTime, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDateValue);
}

TF_BUILTIN(DatePrototypeGetTimezoneOffset, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kTimezoneOffset);
}

TF_BUILTIN(DatePrototypeGetUTCDate, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDayUTC);
}

TF_BUILTIN(DatePrototypeGetUTCDay, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kWeekdayUTC);
}

TF_BUILTIN(DatePrototypeGetUTCFullYear, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kYearUTC);
}

TF_BUILTIN(DatePrototypeGetUTCHours, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kHourUTC);
}

TF_BUILTIN(DatePrototypeGetUTCMilliseconds, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMillisecondUTC);
}

TF_BUILTIN(DatePrototypeGetUTCMinutes, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMinuteUTC);
}

TF_BUILTIN(DatePrototypeGetUTCMonth, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMonthUTC);
}

TF_BUILTIN(DatePrototypeGetUTCSeconds, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kSecondUTC);
}

TF_BUILTIN(DatePrototypeValueOf, DateBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDateValue);
}

TF_BUILTIN(DatePrototypeToPrimitive, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> hint = CAST(Parameter(Descriptor::kHint));

  // Check if the {receiver} is actually a JSReceiver.
  Label receiver_is_invalid(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &receiver_is_invalid);
  GotoIfNot(IsJSReceiver(CAST(receiver)), &receiver_is_invalid);

  // Dispatch to the appropriate OrdinaryToPrimitive builtin.
  Label hint_is_number(this), hint_is_string(this),
      hint_is_invalid(this, Label::kDeferred);

  // Fast cases for internalized strings.
  TNode<String> number_string = numberStringConstant();
  GotoIf(TaggedEqual(hint, number_string), &hint_is_number);
  TNode<String> default_string = DefaultStringConstant();
  GotoIf(TaggedEqual(hint, default_string), &hint_is_string);
  TNode<String> string_string = StringStringConstant();
  GotoIf(TaggedEqual(hint, string_string), &hint_is_string);

  // Slow-case with actual string comparisons.
  GotoIf(TaggedIsSmi(hint), &hint_is_invalid);
  GotoIfNot(IsString(CAST(hint)), &hint_is_invalid);
  GotoIf(TaggedEqual(
             CallBuiltin(Builtins::kStringEqual, context, hint, number_string),
             TrueConstant()),
         &hint_is_number);
  GotoIf(TaggedEqual(
             CallBuiltin(Builtins::kStringEqual, context, hint, default_string),
             TrueConstant()),
         &hint_is_string);
  GotoIf(TaggedEqual(
             CallBuiltin(Builtins::kStringEqual, context, hint, string_string),
             TrueConstant()),
         &hint_is_string);
  Goto(&hint_is_invalid);

  // Use the OrdinaryToPrimitive builtin to convert to a Number.
  BIND(&hint_is_number);
  {
    Callable callable = CodeFactory::OrdinaryToPrimitive(
        isolate(), OrdinaryToPrimitiveHint::kNumber);
    TNode<Object> result = CallStub(callable, context, receiver);
    Return(result);
  }

  // Use the OrdinaryToPrimitive builtin to convert to a String.
  BIND(&hint_is_string);
  {
    Callable callable = CodeFactory::OrdinaryToPrimitive(
        isolate(), OrdinaryToPrimitiveHint::kString);
    TNode<Object> result = CallStub(callable, context, receiver);
    Return(result);
  }

  // Raise a TypeError if the {hint} is invalid.
  BIND(&hint_is_invalid);
  { ThrowTypeError(context, MessageTemplate::kInvalidHint, hint); }

  // Raise a TypeError if the {receiver} is not a JSReceiver instance.
  BIND(&receiver_is_invalid);
  {
    ThrowTypeError(context, MessageTemplate::kIncompatibleMethodReceiver,
                   StringConstant("Date.prototype [ @@toPrimitive ]"),
                   receiver);
  }
}

}  // namespace internal
}  // namespace v8
