// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"

namespace v8 {
namespace internal {

class ConversionBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ConversionBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_NonPrimitiveToPrimitive(TNode<Context> context,
                                        TNode<Object> input,
                                        ToPrimitiveHint hint);

  void Generate_OrdinaryToPrimitive(TNode<Context> context, TNode<Object> input,
                                    OrdinaryToPrimitiveHint hint);
};

// ES6 section 7.1.1 ToPrimitive ( input [ , PreferredType ] )
void ConversionBuiltinsAssembler::Generate_NonPrimitiveToPrimitive(
    TNode<Context> context, TNode<Object> input, ToPrimitiveHint hint) {
  // Lookup the @@toPrimitive property on the {input}.
  TNode<Object> exotic_to_prim =
      GetProperty(context, input, factory()->to_primitive_symbol());

  // Check if {exotic_to_prim} is neither null nor undefined.
  Label ordinary_to_primitive(this);
  GotoIf(IsNullOrUndefined(exotic_to_prim), &ordinary_to_primitive);
  {
    // Invoke the {exotic_to_prim} method on the {input} with a string
    // representation of the {hint}.
    Callable callable =
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined);
    TNode<String> hint_string =
        HeapConstant(factory()->ToPrimitiveHintString(hint));
    TNode<Object> result =
        CallJS(callable, context, exotic_to_prim, input, hint_string);

    // Verify that the {result} is actually a primitive.
    Label if_resultisprimitive(this),
        if_resultisnotprimitive(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(result), &if_resultisprimitive);
    TNode<Uint16T> result_instance_type = LoadInstanceType(CAST(result));
    Branch(IsPrimitiveInstanceType(result_instance_type), &if_resultisprimitive,
           &if_resultisnotprimitive);

    BIND(&if_resultisprimitive);
    {
      // Just return the {result}.
      Return(result);
    }

    BIND(&if_resultisnotprimitive);
    {
      // Somehow the @@toPrimitive method on {input} didn't yield a primitive.
      ThrowTypeError(context, MessageTemplate::kCannotConvertToPrimitive);
    }
  }

  // Convert using the OrdinaryToPrimitive algorithm instead.
  BIND(&ordinary_to_primitive);
  {
    Callable callable = CodeFactory::OrdinaryToPrimitive(
        isolate(), (hint == ToPrimitiveHint::kString)
                       ? OrdinaryToPrimitiveHint::kString
                       : OrdinaryToPrimitiveHint::kNumber);
    TailCallStub(callable, context, input);
  }
}

TF_BUILTIN(NonPrimitiveToPrimitive_Default, ConversionBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Generate_NonPrimitiveToPrimitive(context, input, ToPrimitiveHint::kDefault);
}

TF_BUILTIN(NonPrimitiveToPrimitive_Number, ConversionBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Generate_NonPrimitiveToPrimitive(context, input, ToPrimitiveHint::kNumber);
}

TF_BUILTIN(NonPrimitiveToPrimitive_String, ConversionBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Generate_NonPrimitiveToPrimitive(context, input, ToPrimitiveHint::kString);
}

TF_BUILTIN(StringToNumber, CodeStubAssembler) {
  TNode<String> input = CAST(Parameter(Descriptor::kArgument));

  Return(StringToNumber(input));
}

TF_BUILTIN(ToName, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  TVARIABLE(Object, var_input, input);
  Label loop(this, &var_input);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {input} value.
    TNode<Object> input = var_input.value();

    // Dispatch based on the type of the {input.}
    Label if_inputisbigint(this), if_inputisname(this), if_inputisnumber(this),
        if_inputisoddball(this), if_inputisreceiver(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(input), &if_inputisnumber);
    TNode<Uint16T> input_instance_type = LoadInstanceType(CAST(input));
    STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
    GotoIf(IsNameInstanceType(input_instance_type), &if_inputisname);
    GotoIf(IsJSReceiverInstanceType(input_instance_type), &if_inputisreceiver);
    GotoIf(IsHeapNumberInstanceType(input_instance_type), &if_inputisnumber);
    Branch(IsBigIntInstanceType(input_instance_type), &if_inputisbigint,
           &if_inputisoddball);

    BIND(&if_inputisbigint);
    {
      // We don't have a fast-path for BigInt currently, so just
      // tail call to the %ToString runtime function here for now.
      TailCallRuntime(Runtime::kToStringRT, context, input);
    }

    BIND(&if_inputisname);
    {
      // The {input} is already a Name.
      Return(input);
    }

    BIND(&if_inputisnumber);
    {
      // Convert the String {input} to a Number.
      TailCallBuiltin(Builtins::kNumberToString, context, input);
    }

    BIND(&if_inputisoddball);
    {
      // Just return the {input}'s string representation.
      CSA_ASSERT(this, IsOddballInstanceType(input_instance_type));
      Return(LoadObjectField(CAST(input), Oddball::kToStringOffset));
    }

    BIND(&if_inputisreceiver);
    {
      // Convert the JSReceiver {input} to a primitive first,
      // and then run the loop again with the new {input},
      // which is then a primitive value.
      var_input = CallBuiltin(Builtins::kNonPrimitiveToPrimitive_String,
                              context, input);
      Goto(&loop);
    }
  }
}

TF_BUILTIN(NonNumberToNumber, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<HeapObject> input = CAST(Parameter(Descriptor::kArgument));

  Return(NonNumberToNumber(context, input));
}

TF_BUILTIN(NonNumberToNumeric, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<HeapObject> input = CAST(Parameter(Descriptor::kArgument));

  Return(NonNumberToNumeric(context, input));
}

TF_BUILTIN(ToNumeric, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Return(Select<Numeric>(
      IsNumber(input), [=] { return CAST(input); },
      [=] { return NonNumberToNumeric(context, CAST(input)); }));
}

// ES6 section 7.1.3 ToNumber ( argument )
TF_BUILTIN(ToNumber, CodeStubAssembler) {
  // TODO(solanes, v8:6949): Changing this to a TNode<Context> crashes with the
  // empty context. Context might not be needed, but it is propagated all over
  // the place and hard to pull out.
  Node* context = Parameter(Descriptor::kContext);
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Return(ToNumber(context, input));
}

// Like ToNumber, but also converts BigInts.
TF_BUILTIN(ToNumberConvertBigInt, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Return(ToNumber(context, input, BigIntHandling::kConvertToNumber));
}

// ES section #sec-tostring-applied-to-the-number-type
TF_BUILTIN(NumberToString, CodeStubAssembler) {
  TNode<Number> input = CAST(Parameter(Descriptor::kArgument));

  Return(NumberToString(input));
}

// 7.1.1.1 OrdinaryToPrimitive ( O, hint )
void ConversionBuiltinsAssembler::Generate_OrdinaryToPrimitive(
    TNode<Context> context, TNode<Object> input, OrdinaryToPrimitiveHint hint) {
  TVARIABLE(Object, var_result);
  Label return_result(this, &var_result);

  Handle<String> method_names[2];
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      method_names[0] = factory()->valueOf_string();
      method_names[1] = factory()->toString_string();
      break;
    case OrdinaryToPrimitiveHint::kString:
      method_names[0] = factory()->toString_string();
      method_names[1] = factory()->valueOf_string();
      break;
  }
  for (Handle<String> name : method_names) {
    // Lookup the {name} on the {input}.
    TNode<Object> method = GetProperty(context, input, name);

    // Check if the {method} is callable.
    Label if_methodiscallable(this),
        if_methodisnotcallable(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(method), &if_methodisnotcallable);
    TNode<Map> method_map = LoadMap(CAST(method));
    Branch(IsCallableMap(method_map), &if_methodiscallable,
           &if_methodisnotcallable);

    BIND(&if_methodiscallable);
    {
      // Call the {method} on the {input}.
      Callable callable = CodeFactory::Call(
          isolate(), ConvertReceiverMode::kNotNullOrUndefined);
      TNode<Object> result = CallJS(callable, context, method, input);
      var_result = result;

      // Return the {result} if it is a primitive.
      GotoIf(TaggedIsSmi(result), &return_result);
      TNode<Uint16T> result_instance_type = LoadInstanceType(CAST(result));
      GotoIf(IsPrimitiveInstanceType(result_instance_type), &return_result);
    }

    // Just continue with the next {name} if the {method} is not callable.
    Goto(&if_methodisnotcallable);
    BIND(&if_methodisnotcallable);
  }

  ThrowTypeError(context, MessageTemplate::kCannotConvertToPrimitive);

  BIND(&return_result);
  Return(var_result.value());
}

TF_BUILTIN(OrdinaryToPrimitive_Number, ConversionBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));
  Generate_OrdinaryToPrimitive(context, input,
                               OrdinaryToPrimitiveHint::kNumber);
}

TF_BUILTIN(OrdinaryToPrimitive_String, ConversionBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));
  Generate_OrdinaryToPrimitive(context, input,
                               OrdinaryToPrimitiveHint::kString);
}

// ES6 section 7.1.2 ToBoolean ( argument )
TF_BUILTIN(ToBoolean, CodeStubAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kArgument));

  Label return_true(this), return_false(this);
  BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// ES6 section 7.1.2 ToBoolean ( argument )
// Requires parameter on stack so that it can be used as a continuation from a
// LAZY deopt.
TF_BUILTIN(ToBooleanLazyDeoptContinuation, CodeStubAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kArgument));

  Label return_true(this), return_false(this);
  BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

TF_BUILTIN(ToLength, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // We might need to loop once for ToNumber conversion.
  TVARIABLE(Object, var_len, CAST(Parameter(Descriptor::kArgument)));
  Label loop(this, &var_len);
  Goto(&loop);
  BIND(&loop);
  {
    // Shared entry points.
    Label return_len(this), return_two53minus1(this, Label::kDeferred),
        return_zero(this, Label::kDeferred);

    // Load the current {len} value.
    TNode<Object> len = var_len.value();

    // Check if {len} is a positive Smi.
    GotoIf(TaggedIsPositiveSmi(len), &return_len);

    // Check if {len} is a (negative) Smi.
    GotoIf(TaggedIsSmi(len), &return_zero);

    // Check if {len} is a HeapNumber.
    TNode<HeapObject> len_heap_object = CAST(len);
    Label if_lenisheapnumber(this),
        if_lenisnotheapnumber(this, Label::kDeferred);
    Branch(IsHeapNumber(len_heap_object), &if_lenisheapnumber,
           &if_lenisnotheapnumber);

    BIND(&if_lenisheapnumber);
    {
      // Load the floating-point value of {len}.
      TNode<Float64T> len_value = LoadHeapNumberValue(len_heap_object);

      // Check if {len} is not greater than zero.
      GotoIfNot(Float64GreaterThan(len_value, Float64Constant(0.0)),
                &return_zero);

      // Check if {len} is greater than or equal to 2^53-1.
      GotoIf(Float64GreaterThanOrEqual(len_value,
                                       Float64Constant(kMaxSafeInteger)),
             &return_two53minus1);

      // Round the {len} towards -Infinity.
      TNode<Float64T> value = Float64Floor(len_value);
      TNode<Number> result = ChangeFloat64ToTagged(value);
      Return(result);
    }

    BIND(&if_lenisnotheapnumber);
    {
      // Need to convert {len} to a Number first.
      var_len = CallBuiltin(Builtins::kNonNumberToNumber, context, len);
      Goto(&loop);
    }

    BIND(&return_len);
    Return(var_len.value());

    BIND(&return_two53minus1);
    Return(NumberConstant(kMaxSafeInteger));

    BIND(&return_zero);
    Return(SmiConstant(0));
  }
}

TF_BUILTIN(ToInteger, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Return(ToInteger(context, input, kNoTruncation));
}

TF_BUILTIN(ToInteger_TruncateMinusZero, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Return(ToInteger(context, input, kTruncateMinusZero));
}

// ES6 section 7.1.13 ToObject (argument)
TF_BUILTIN(ToObject, CodeStubAssembler) {
  Label if_smi(this, Label::kDeferred), if_jsreceiver(this),
      if_noconstructor(this, Label::kDeferred),
      if_wrapjs_primitive_wrapper(this);

  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> object = CAST(Parameter(Descriptor::kArgument));

  TVARIABLE(IntPtrT, constructor_function_index_var);

  GotoIf(TaggedIsSmi(object), &if_smi);

  TNode<Map> map = LoadMap(CAST(object));
  TNode<Uint16T> instance_type = LoadMapInstanceType(map);
  GotoIf(IsJSReceiverInstanceType(instance_type), &if_jsreceiver);

  TNode<IntPtrT> constructor_function_index =
      LoadMapConstructorFunctionIndex(map);
  GotoIf(WordEqual(constructor_function_index,
                   IntPtrConstant(Map::kNoConstructorFunctionIndex)),
         &if_noconstructor);
  constructor_function_index_var = constructor_function_index;
  Goto(&if_wrapjs_primitive_wrapper);

  BIND(&if_smi);
  constructor_function_index_var =
      IntPtrConstant(Context::NUMBER_FUNCTION_INDEX);
  Goto(&if_wrapjs_primitive_wrapper);

  BIND(&if_wrapjs_primitive_wrapper);
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<JSFunction> constructor = CAST(LoadContextElement(
      native_context, constructor_function_index_var.value()));
  TNode<Object> initial_map =
      LoadObjectField(constructor, JSFunction::kPrototypeOrInitialMapOffset);
  TNode<HeapObject> js_primitive_wrapper = Allocate(JSPrimitiveWrapper::kSize);
  StoreMapNoWriteBarrier(js_primitive_wrapper, initial_map);
  StoreObjectFieldRoot(js_primitive_wrapper,
                       JSPrimitiveWrapper::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(js_primitive_wrapper,
                       JSPrimitiveWrapper::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectField(js_primitive_wrapper, JSPrimitiveWrapper::kValueOffset,
                   object);
  Return(js_primitive_wrapper);

  BIND(&if_noconstructor);
  ThrowTypeError(context, MessageTemplate::kUndefinedOrNullToObject,
                 "ToObject");

  BIND(&if_jsreceiver);
  Return(object);
}

// ES6 section 12.5.5 typeof operator
TF_BUILTIN(Typeof, CodeStubAssembler) {
  TNode<Object> object = CAST(Parameter(Descriptor::kObject));

  Return(Typeof(object));
}

}  // namespace internal
}  // namespace v8
