// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-promise-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-promise.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-promise.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

using Node = compiler::Node;
using IteratorRecord = TorqueStructIteratorRecord;

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateJSPromise(
    TNode<Context> context) {
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<JSFunction> const promise_fun =
      CAST(LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX));
  CSA_ASSERT(this, IsFunctionWithPrototypeSlotMap(LoadMap(promise_fun)));
  TNode<Object> const promise_map =
      LoadObjectField(promise_fun, JSFunction::kPrototypeOrInitialMapOffset);
  TNode<HeapObject> const promise =
      Allocate(JSPromise::kSizeWithEmbedderFields);
  StoreMapNoWriteBarrier(promise, promise_map);
  StoreObjectFieldRoot(promise, JSPromise::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(promise, JSPromise::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  return CAST(promise);
}

void PromiseBuiltinsAssembler::PromiseInit(Node* promise) {
  STATIC_ASSERT(v8::Promise::kPending == 0);
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kReactionsOrResultOffset,
                                 SmiConstant(Smi::zero()));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset,
                                 SmiConstant(Smi::zero()));
  for (int offset = JSPromise::kSize;
       offset < JSPromise::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(promise, offset, SmiConstant(Smi::zero()));
  }
}

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateAndInitJSPromise(
    TNode<Context> context) {
  return AllocateAndInitJSPromise(context, UndefinedConstant());
}

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateAndInitJSPromise(
    TNode<Context> context, TNode<Object> parent) {
  const TNode<JSPromise> instance = AllocateJSPromise(context);
  PromiseInit(instance);

  Label out(this);
  GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance, parent);
  Goto(&out);

  BIND(&out);
  return instance;
}

TNode<JSPromise> PromiseBuiltinsAssembler::AllocateAndSetJSPromise(
    TNode<Context> context, v8::Promise::PromiseState status,
    TNode<Object> result) {
  DCHECK_NE(Promise::kPending, status);

  const TNode<JSPromise> instance = AllocateJSPromise(context);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kReactionsOrResultOffset,
                                 result);
  STATIC_ASSERT(JSPromise::kStatusShift == 0);
  StoreObjectFieldNoWriteBarrier(instance, JSPromise::kFlagsOffset,
                                 SmiConstant(status));
  for (int offset = JSPromise::kSize;
       offset < JSPromise::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(instance, offset, SmiConstant(0));
  }

  Label out(this);
  GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &out);
  CallRuntime(Runtime::kPromiseHookInit, context, instance,
              UndefinedConstant());
  Goto(&out);

  BIND(&out);
  return instance;
}

std::pair<TNode<JSFunction>, TNode<JSFunction>>
PromiseBuiltinsAssembler::CreatePromiseResolvingFunctions(
    TNode<JSPromise> promise, TNode<Object> debug_event,
    TNode<NativeContext> native_context) {
  const TNode<Context> promise_context = CreatePromiseResolvingFunctionsContext(
      promise, debug_event, native_context);
  const TNode<Map> map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  const TNode<SharedFunctionInfo> resolve_info = CAST(LoadContextElement(
      native_context,
      Context::PROMISE_CAPABILITY_DEFAULT_RESOLVE_SHARED_FUN_INDEX));
  const TNode<JSFunction> resolve =
      AllocateFunctionWithMapAndContext(map, resolve_info, promise_context);
  const TNode<SharedFunctionInfo> reject_info = CAST(LoadContextElement(
      native_context,
      Context::PROMISE_CAPABILITY_DEFAULT_REJECT_SHARED_FUN_INDEX));
  const TNode<JSFunction> reject =
      AllocateFunctionWithMapAndContext(map, reject_info, promise_context);
  return std::make_pair(resolve, reject);
}

void PromiseBuiltinsAssembler::ExtractHandlerContext(Node* handler,
                                                     Variable* var_context) {
  VARIABLE(var_handler, MachineRepresentation::kTagged, handler);
  Label loop(this, &var_handler), done(this, Label::kDeferred);
  Goto(&loop);
  BIND(&loop);
  {
    Label if_function(this), if_bound_function(this, Label::kDeferred),
        if_proxy(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(var_handler.value()), &done);

    int32_t case_values[] = {
        JS_FUNCTION_TYPE,
        JS_BOUND_FUNCTION_TYPE,
        JS_PROXY_TYPE,
    };
    Label* case_labels[] = {
        &if_function,
        &if_bound_function,
        &if_proxy,
    };
    static_assert(arraysize(case_values) == arraysize(case_labels), "");
    TNode<Map> handler_map = LoadMap(var_handler.value());
    TNode<Uint16T> handler_type = LoadMapInstanceType(handler_map);
    Switch(handler_type, &done, case_values, case_labels,
           arraysize(case_labels));

    BIND(&if_bound_function);
    {
      // Use the target function's context for JSBoundFunction.
      var_handler.Bind(LoadObjectField(
          var_handler.value(), JSBoundFunction::kBoundTargetFunctionOffset));
      Goto(&loop);
    }

    BIND(&if_proxy);
    {
      // Use the target function's context for JSProxy.
      // If the proxy is revoked, |var_handler| will be undefined and this
      // function will return with unchanged |var_context|.
      var_handler.Bind(
          LoadObjectField(var_handler.value(), JSProxy::kTargetOffset));
      Goto(&loop);
    }

    BIND(&if_function);
    {
      // Use the function's context.
      TNode<Object> handler_context =
          LoadObjectField(var_handler.value(), JSFunction::kContextOffset);
      var_context->Bind(LoadNativeContext(CAST(handler_context)));
      Goto(&done);
    }
  }

  // If no valid context is available, |var_context| is unchanged and the caller
  // will use a fallback context.
  BIND(&done);
}

// ES #sec-newpromisecapability
TF_BUILTIN(NewPromiseCapability, PromiseBuiltinsAssembler) {
  TNode<Context> const context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> const constructor = CAST(Parameter(Descriptor::kConstructor));
  TNode<Object> const debug_event = CAST(Parameter(Descriptor::kDebugEvent));
  TNode<NativeContext> const native_context = LoadNativeContext(context);

  Label if_not_constructor(this, Label::kDeferred),
      if_notcallable(this, Label::kDeferred), if_fast_promise_capability(this),
      if_slow_promise_capability(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(constructor), &if_not_constructor);
  GotoIfNot(IsConstructorMap(LoadMap(CAST(constructor))), &if_not_constructor);
  Branch(TaggedEqual(constructor,
                     LoadContextElement(native_context,
                                        Context::PROMISE_FUNCTION_INDEX)),
         &if_fast_promise_capability, &if_slow_promise_capability);

  BIND(&if_fast_promise_capability);
  {
    TNode<JSPromise> promise =
        AllocateAndInitJSPromise(native_context, UndefinedConstant());

    Node* resolve = nullptr;
    Node* reject = nullptr;
    std::tie(resolve, reject) =
        CreatePromiseResolvingFunctions(promise, debug_event, native_context);

    TNode<HeapObject> capability = Allocate(PromiseCapability::kSize);
    StoreMapNoWriteBarrier(capability, RootIndex::kPromiseCapabilityMap);
    StoreObjectFieldNoWriteBarrier(capability,
                                   PromiseCapability::kPromiseOffset, promise);
    StoreObjectFieldNoWriteBarrier(capability,
                                   PromiseCapability::kResolveOffset, resolve);
    StoreObjectFieldNoWriteBarrier(capability, PromiseCapability::kRejectOffset,
                                   reject);
    Return(capability);
  }

  BIND(&if_slow_promise_capability);
  {
    TNode<HeapObject> capability = Allocate(PromiseCapability::kSize);
    StoreMapNoWriteBarrier(capability, RootIndex::kPromiseCapabilityMap);
    StoreObjectFieldRoot(capability, PromiseCapability::kPromiseOffset,
                         RootIndex::kUndefinedValue);
    StoreObjectFieldRoot(capability, PromiseCapability::kResolveOffset,
                         RootIndex::kUndefinedValue);
    StoreObjectFieldRoot(capability, PromiseCapability::kRejectOffset,
                         RootIndex::kUndefinedValue);

    TNode<Context> executor_context =
        CAST(CreatePromiseGetCapabilitiesExecutorContext(capability,
                                                         native_context));
    TNode<SharedFunctionInfo> executor_info = CAST(LoadContextElement(
        native_context, Context::PROMISE_GET_CAPABILITIES_EXECUTOR_SHARED_FUN));
    TNode<Map> function_map = CAST(LoadContextElement(
        native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
    TNode<JSFunction> executor = AllocateFunctionWithMapAndContext(
        function_map, executor_info, executor_context);

    TNode<JSReceiver> promise =
        Construct(native_context, CAST(constructor), executor);
    StoreObjectField(capability, PromiseCapability::kPromiseOffset, promise);

    TNode<Object> resolve =
        LoadObjectField(capability, PromiseCapability::kResolveOffset);
    GotoIf(TaggedIsSmi(resolve), &if_notcallable);
    GotoIfNot(IsCallable(CAST(resolve)), &if_notcallable);

    TNode<Object> reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    GotoIf(TaggedIsSmi(reject), &if_notcallable);
    GotoIfNot(IsCallable(CAST(reject)), &if_notcallable);
    Return(capability);
  }

  BIND(&if_not_constructor);
  ThrowTypeError(context, MessageTemplate::kNotConstructor, constructor);

  BIND(&if_notcallable);
  ThrowTypeError(context, MessageTemplate::kPromiseNonCallable);
}

TNode<Context> PromiseBuiltinsAssembler::CreatePromiseContext(
    TNode<NativeContext> native_context, int slots) {
  DCHECK_GE(slots, Context::MIN_CONTEXT_SLOTS);

  TNode<HeapObject> const context =
      AllocateInNewSpace(FixedArray::SizeFor(slots));
  InitializeFunctionContext(native_context, context, slots);
  return CAST(context);
}

Node* PromiseBuiltinsAssembler::CreatePromiseAllResolveElementContext(
    Node* promise_capability, Node* native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  // TODO(bmeurer): Manually fold this into a single allocation.
  TNode<Map> array_map = CAST(LoadContextElement(
      native_context, Context::JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX));
  TNode<JSArray> values_array = AllocateJSArray(
      PACKED_ELEMENTS, array_map, IntPtrConstant(0), SmiConstant(0));

  TNode<Context> const context = CreatePromiseContext(
      CAST(native_context), PromiseBuiltins::kPromiseAllResolveElementLength);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
      SmiConstant(1));
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot,
      promise_capability);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot,
      values_array);

  return context;
}

TNode<JSFunction>
PromiseBuiltinsAssembler::CreatePromiseAllResolveElementFunction(
    Node* context, TNode<Smi> index, Node* native_context, int slot_index) {
  CSA_ASSERT(this, SmiGreaterThan(index, SmiConstant(0)));
  CSA_ASSERT(this, SmiLessThanOrEqual(
                       index, SmiConstant(PropertyArray::HashField::kMax)));
  CSA_ASSERT(this, IsNativeContext(native_context));

  const TNode<Map> map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  const TNode<SharedFunctionInfo> resolve_info =
      CAST(LoadContextElement(native_context, slot_index));
  TNode<JSFunction> resolve =
      AllocateFunctionWithMapAndContext(map, resolve_info, CAST(context));

  STATIC_ASSERT(PropertyArray::kNoHashSentinel == 0);
  StoreObjectFieldNoWriteBarrier(resolve, JSFunction::kPropertiesOrHashOffset,
                                 index);

  return resolve;
}

TNode<Context> PromiseBuiltinsAssembler::CreatePromiseResolvingFunctionsContext(
    TNode<JSPromise> promise, TNode<Object> debug_event,
    TNode<NativeContext> native_context) {
  const TNode<Context> context = CreatePromiseContext(
      native_context, PromiseBuiltins::kPromiseContextLength);
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kPromiseSlot,
                                    promise);
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kAlreadyResolvedSlot, FalseConstant());
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kDebugEventSlot,
                                    debug_event);
  return context;
}

Node* PromiseBuiltinsAssembler::CreatePromiseGetCapabilitiesExecutorContext(
    Node* promise_capability, Node* native_context) {
  int kContextLength = PromiseBuiltins::kCapabilitiesContextLength;
  TNode<Context> context =
      CreatePromiseContext(CAST(native_context), kContextLength);
  StoreContextElementNoWriteBarrier(context, PromiseBuiltins::kCapabilitySlot,
                                    promise_capability);
  return context;
}

Node* PromiseBuiltinsAssembler::PromiseHasHandler(Node* promise) {
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  return IsSetWord(SmiUntag(flags), 1 << JSPromise::kHasHandlerBit);
}

void PromiseBuiltinsAssembler::PromiseSetHasHandler(Node* promise) {
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  TNode<Smi> const new_flags =
      SmiOr(flags, SmiConstant(1 << JSPromise::kHasHandlerBit));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset, new_flags);
}

TNode<BoolT> PromiseBuiltinsAssembler::IsPromiseStatus(
    TNode<Word32T> actual, v8::Promise::PromiseState expected) {
  return Word32Equal(actual, Int32Constant(expected));
}

TNode<Word32T> PromiseBuiltinsAssembler::PromiseStatus(Node* promise) {
  STATIC_ASSERT(JSPromise::kStatusShift == 0);
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  return Word32And(SmiToInt32(flags), Int32Constant(JSPromise::kStatusMask));
}

void PromiseBuiltinsAssembler::PromiseSetStatus(
    Node* promise, v8::Promise::PromiseState const status) {
  CSA_ASSERT(this,
             IsPromiseStatus(PromiseStatus(promise), v8::Promise::kPending));
  CHECK_NE(status, v8::Promise::kPending);

  TNode<Smi> mask = SmiConstant(status);
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset,
                                 SmiOr(flags, mask));
}

void PromiseBuiltinsAssembler::PromiseSetHandledHint(Node* promise) {
  TNode<Smi> const flags =
      CAST(LoadObjectField(promise, JSPromise::kFlagsOffset));
  TNode<Smi> const new_flags =
      SmiOr(flags, SmiConstant(1 << JSPromise::kHandledHintBit));
  StoreObjectFieldNoWriteBarrier(promise, JSPromise::kFlagsOffset, new_flags);
}

// ES #sec-performpromisethen
void PromiseBuiltinsAssembler::PerformPromiseThen(
    TNode<Context> context, TNode<JSPromise> promise,
    TNode<HeapObject> on_fulfilled, TNode<HeapObject> on_rejected,
    TNode<HeapObject> result_promise_or_capability) {
  CSA_ASSERT(this,
             Word32Or(IsCallable(on_fulfilled), IsUndefined(on_fulfilled)));
  CSA_ASSERT(this, Word32Or(IsCallable(on_rejected), IsUndefined(on_rejected)));
  CSA_ASSERT(
      this,
      Word32Or(Word32Or(IsJSPromise(result_promise_or_capability),
                        IsPromiseCapability(result_promise_or_capability)),
               IsUndefined(result_promise_or_capability)));

  Label if_pending(this), if_notpending(this), done(this);
  TNode<Word32T> const status = PromiseStatus(promise);
  Branch(IsPromiseStatus(status, v8::Promise::kPending), &if_pending,
         &if_notpending);

  BIND(&if_pending);
  {
    // The {promise} is still in "Pending" state, so we just record a new
    // PromiseReaction holding both the onFulfilled and onRejected callbacks.
    // Once the {promise} is resolved we decide on the concrete handler to
    // push onto the microtask queue.
    const TNode<Object> promise_reactions =
        LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);
    const TNode<PromiseReaction> reaction =
        AllocatePromiseReaction(promise_reactions, result_promise_or_capability,
                                on_fulfilled, on_rejected);
    StoreObjectField(promise, JSPromise::kReactionsOrResultOffset, reaction);
    Goto(&done);
  }

  BIND(&if_notpending);
  {
    TVARIABLE(Map, var_map);
    TVARIABLE(HeapObject, var_handler);
    TVARIABLE(Object, var_handler_context, UndefinedConstant());
    Label if_fulfilled(this), if_rejected(this, Label::kDeferred),
        enqueue(this);
    Branch(IsPromiseStatus(status, v8::Promise::kFulfilled), &if_fulfilled,
           &if_rejected);

    BIND(&if_fulfilled);
    {
      var_map = PromiseFulfillReactionJobTaskMapConstant();
      var_handler = on_fulfilled;

      Label use_fallback(this, Label::kDeferred), done(this);
      ExtractHandlerContext(on_fulfilled, &var_handler_context);
      Branch(IsUndefined(var_handler_context.value()), &use_fallback, &done);

      BIND(&use_fallback);
      var_handler_context = context;
      ExtractHandlerContext(on_rejected, &var_handler_context);
      Goto(&done);

      BIND(&done);
      Goto(&enqueue);
    }

    BIND(&if_rejected);
    {
      CSA_ASSERT(this, IsPromiseStatus(status, v8::Promise::kRejected));
      var_map = PromiseRejectReactionJobTaskMapConstant();
      var_handler = on_rejected;

      Label use_fallback(this, Label::kDeferred), done(this);
      ExtractHandlerContext(on_rejected, &var_handler_context);
      Branch(IsUndefined(var_handler_context.value()), &use_fallback, &done);

      BIND(&use_fallback);
      var_handler_context = context;
      ExtractHandlerContext(on_fulfilled, &var_handler_context);
      Goto(&done);
      BIND(&done);

      GotoIf(PromiseHasHandler(promise), &enqueue);
      CallRuntime(Runtime::kPromiseRevokeReject, context, promise);
      Goto(&enqueue);
    }

    BIND(&enqueue);
    {
      TNode<Object> argument =
          LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);
      TNode<PromiseReactionJobTask> microtask = AllocatePromiseReactionJobTask(
          var_map.value(), CAST(var_handler_context.value()), argument,
          var_handler.value(), result_promise_or_capability);
      CallBuiltin(Builtins::kEnqueueMicrotask, var_handler_context.value(),
                  microtask);
      Goto(&done);
    }
  }

  BIND(&done);
  PromiseSetHasHandler(promise);
}

// ES #sec-performpromisethen
TF_BUILTIN(PerformPromiseThen, PromiseBuiltinsAssembler) {
  const TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  const TNode<JSPromise> promise = CAST(Parameter(Descriptor::kPromise));
  const TNode<HeapObject> on_fulfilled =
      CAST(Parameter(Descriptor::kOnFulfilled));
  const TNode<HeapObject> on_rejected =
      CAST(Parameter(Descriptor::kOnRejected));
  const TNode<HeapObject> result_promise =
      CAST(Parameter(Descriptor::kResultPromise));

  CSA_ASSERT(
      this, Word32Or(IsJSPromise(result_promise), IsUndefined(result_promise)));

  PerformPromiseThen(context, promise, on_fulfilled, on_rejected,
                     result_promise);
  Return(result_promise);
}

TNode<PromiseReaction> PromiseBuiltinsAssembler::AllocatePromiseReaction(
    TNode<Object> next, TNode<HeapObject> promise_or_capability,
    TNode<HeapObject> fulfill_handler, TNode<HeapObject> reject_handler) {
  TNode<HeapObject> const reaction = Allocate(PromiseReaction::kSize);
  StoreMapNoWriteBarrier(reaction, RootIndex::kPromiseReactionMap);
  StoreObjectFieldNoWriteBarrier(reaction, PromiseReaction::kNextOffset, next);
  StoreObjectFieldNoWriteBarrier(reaction,
                                 PromiseReaction::kPromiseOrCapabilityOffset,
                                 promise_or_capability);
  StoreObjectFieldNoWriteBarrier(
      reaction, PromiseReaction::kFulfillHandlerOffset, fulfill_handler);
  StoreObjectFieldNoWriteBarrier(
      reaction, PromiseReaction::kRejectHandlerOffset, reject_handler);
  return CAST(reaction);
}

TNode<PromiseReactionJobTask>
PromiseBuiltinsAssembler::AllocatePromiseReactionJobTask(
    TNode<Map> map, TNode<Context> context, TNode<Object> argument,
    TNode<HeapObject> handler, TNode<HeapObject> promise_or_capability) {
  TNode<HeapObject> const microtask =
      Allocate(PromiseReactionJobTask::kSizeOfAllPromiseReactionJobTasks);
  StoreMapNoWriteBarrier(microtask, map);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kArgumentOffset, argument);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kHandlerOffset, handler);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseReactionJobTask::kPromiseOrCapabilityOffset,
      promise_or_capability);
  return CAST(microtask);
}

TNode<PromiseResolveThenableJobTask>
PromiseBuiltinsAssembler::AllocatePromiseResolveThenableJobTask(
    TNode<JSPromise> promise_to_resolve, TNode<JSReceiver> then,
    TNode<JSReceiver> thenable, TNode<Context> context) {
  const TNode<HeapObject> microtask =
      Allocate(PromiseResolveThenableJobTask::kSize);
  StoreMapNoWriteBarrier(microtask,
                         RootIndex::kPromiseResolveThenableJobTaskMap);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kPromiseToResolveOffset,
      promise_to_resolve);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kThenOffset, then);
  StoreObjectFieldNoWriteBarrier(
      microtask, PromiseResolveThenableJobTask::kThenableOffset, thenable);
  return CAST(microtask);
}

// ES #sec-triggerpromisereactions
Node* PromiseBuiltinsAssembler::TriggerPromiseReactions(
    Node* context, Node* reactions, Node* argument,
    PromiseReaction::Type type) {
  // We need to reverse the {reactions} here, since we record them on the
  // JSPromise in the reverse order.
  {
    VARIABLE(var_current, MachineRepresentation::kTagged, reactions);
    VARIABLE(var_reversed, MachineRepresentation::kTagged,
             SmiConstant(Smi::zero()));

    // As an additional safety net against misuse of the V8 Extras API, we
    // sanity check the {reactions} to make sure that they are actually
    // PromiseReaction instances and not actual JavaScript values (which
    // would indicate that we're rejecting or resolving an already settled
    // promise), see https://crbug.com/931640 for details on this.
    TNode<Map> promise_reaction_map = PromiseReactionMapConstant();

    Label loop(this, {&var_current, &var_reversed}), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* current = var_current.value();
      GotoIf(TaggedIsSmi(current), &done_loop);
      CSA_CHECK(this,
                TaggedEqual(LoadMap(CAST(current)), promise_reaction_map));
      var_current.Bind(LoadObjectField(current, PromiseReaction::kNextOffset));
      StoreObjectField(current, PromiseReaction::kNextOffset,
                       var_reversed.value());
      var_reversed.Bind(current);
      Goto(&loop);
    }
    BIND(&done_loop);
    reactions = var_reversed.value();
  }

  // Morph the {reactions} into PromiseReactionJobTasks and push them
  // onto the microtask queue.
  {
    VARIABLE(var_current, MachineRepresentation::kTagged, reactions);

    Label loop(this, {&var_current}), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* current = var_current.value();
      GotoIf(TaggedIsSmi(current), &done_loop);
      var_current.Bind(LoadObjectField(current, PromiseReaction::kNextOffset));

      VARIABLE(var_context, MachineRepresentation::kTagged,
               UndefinedConstant());

      Node* primary_handler;
      Node* secondary_handler;
      if (type == PromiseReaction::kFulfill) {
        primary_handler =
            LoadObjectField(current, PromiseReaction::kFulfillHandlerOffset);
        secondary_handler =
            LoadObjectField(current, PromiseReaction::kRejectHandlerOffset);
      } else {
        primary_handler =
            LoadObjectField(current, PromiseReaction::kRejectHandlerOffset);
        secondary_handler =
            LoadObjectField(current, PromiseReaction::kFulfillHandlerOffset);
      }

      {
        Label use_fallback(this, Label::kDeferred), done(this);
        ExtractHandlerContext(primary_handler, &var_context);
        Branch(IsUndefined(var_context.value()), &use_fallback, &done);

        BIND(&use_fallback);
        var_context.Bind(context);
        ExtractHandlerContext(secondary_handler, &var_context);
        CSA_ASSERT(this, IsNotUndefined(var_context.value()));
        Goto(&done);

        BIND(&done);
      }

      // Morph {current} from a PromiseReaction into a PromiseReactionJobTask
      // and schedule that on the microtask queue. We try to minimize the number
      // of stores here to avoid screwing up the store buffer.
      STATIC_ASSERT(
          static_cast<int>(PromiseReaction::kSize) ==
          static_cast<int>(
              PromiseReactionJobTask::kSizeOfAllPromiseReactionJobTasks));
      if (type == PromiseReaction::kFulfill) {
        StoreMapNoWriteBarrier(current,
                               RootIndex::kPromiseFulfillReactionJobTaskMap);
        StoreObjectField(current, PromiseReactionJobTask::kArgumentOffset,
                         argument);
        StoreObjectField(current, PromiseReactionJobTask::kContextOffset,
                         var_context.value());
        STATIC_ASSERT(
            static_cast<int>(PromiseReaction::kFulfillHandlerOffset) ==
            static_cast<int>(PromiseReactionJobTask::kHandlerOffset));
        STATIC_ASSERT(
            static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
            static_cast<int>(
                PromiseReactionJobTask::kPromiseOrCapabilityOffset));
      } else {
        StoreMapNoWriteBarrier(current,
                               RootIndex::kPromiseRejectReactionJobTaskMap);
        StoreObjectField(current, PromiseReactionJobTask::kArgumentOffset,
                         argument);
        StoreObjectField(current, PromiseReactionJobTask::kContextOffset,
                         var_context.value());
        StoreObjectField(current, PromiseReactionJobTask::kHandlerOffset,
                         primary_handler);
        STATIC_ASSERT(
            static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
            static_cast<int>(
                PromiseReactionJobTask::kPromiseOrCapabilityOffset));
      }
      CallBuiltin(Builtins::kEnqueueMicrotask, var_context.value(), current);
      Goto(&loop);
    }
    BIND(&done_loop);
  }

  return UndefinedConstant();
}

template <typename... TArgs>
Node* PromiseBuiltinsAssembler::InvokeThen(Node* native_context, Node* receiver,
                                           TArgs... args) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_fast(this), if_slow(this, Label::kDeferred), done(this, &var_result);
  GotoIf(TaggedIsSmi(receiver), &if_slow);
  TNode<Map> const receiver_map = LoadMap(receiver);
  // We can skip the "then" lookup on {receiver} if it's [[Prototype]]
  // is the (initial) Promise.prototype and the Promise#then protector
  // is intact, as that guards the lookup path for the "then" property
  // on JSPromise instances which have the (initial) %PromisePrototype%.
  BranchIfPromiseThenLookupChainIntact(native_context, receiver_map, &if_fast,
                                       &if_slow);

  BIND(&if_fast);
  {
    TNode<Object> const then =
        LoadContextElement(native_context, Context::PROMISE_THEN_INDEX);
    Node* const result =
        CallJS(CodeFactory::CallFunction(
                   isolate(), ConvertReceiverMode::kNotNullOrUndefined),
               native_context, then, receiver, args...);
    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&if_slow);
  {
    TNode<Object> const then = GetProperty(native_context, receiver,
                                           isolate()->factory()->then_string());
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        native_context, then, receiver, args...);
    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

Node* PromiseBuiltinsAssembler::CallResolve(Node* native_context,
                                            Node* constructor, Node* resolve,
                                            Node* value, Label* if_exception,
                                            Variable* var_exception) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  CSA_ASSERT(this, IsConstructor(constructor));
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_fast(this), if_slow(this, Label::kDeferred), done(this, &var_result);

  // Undefined can never be a valid value for the resolve function,
  // instead it is used as a special marker for the fast path.
  Branch(IsUndefined(resolve), &if_fast, &if_slow);

  BIND(&if_fast);
  {
    TNode<Object> const result = CallBuiltin(
        Builtins::kPromiseResolve, native_context, constructor, value);
    GotoIfException(result, if_exception, var_exception);

    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&if_slow);
  {
    CSA_ASSERT(this, IsCallable(resolve));

    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        native_context, resolve, constructor, value);
    GotoIfException(result, if_exception, var_exception);

    var_result.Bind(result);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

void PromiseBuiltinsAssembler::BranchIfPromiseResolveLookupChainIntact(
    Node* native_context, SloppyTNode<Object> constructor, Label* if_fast,
    Label* if_slow) {
  CSA_ASSERT(this, IsNativeContext(native_context));

  GotoIfForceSlowPath(if_slow);
  TNode<Object> promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  GotoIfNot(TaggedEqual(promise_fun, constructor), if_slow);
  Branch(IsPromiseResolveProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::GotoIfNotPromiseResolveLookupChainIntact(
    Node* native_context, SloppyTNode<Object> constructor, Label* if_slow) {
  Label if_fast(this);
  BranchIfPromiseResolveLookupChainIntact(native_context, constructor, &if_fast,
                                          if_slow);
  BIND(&if_fast);
}

void PromiseBuiltinsAssembler::BranchIfPromiseSpeciesLookupChainIntact(
    Node* native_context, Node* promise_map, Label* if_fast, Label* if_slow) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  CSA_ASSERT(this, IsJSPromiseMap(promise_map));

  TNode<Object> promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfForceSlowPath(if_slow);
  GotoIfNot(TaggedEqual(LoadMapPrototype(promise_map), promise_prototype),
            if_slow);
  Branch(IsPromiseSpeciesProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::BranchIfPromiseThenLookupChainIntact(
    Node* native_context, Node* receiver_map, Label* if_fast, Label* if_slow) {
  CSA_ASSERT(this, IsMap(receiver_map));
  CSA_ASSERT(this, IsNativeContext(native_context));

  GotoIfForceSlowPath(if_slow);
  GotoIfNot(IsJSPromiseMap(receiver_map), if_slow);
  TNode<Object> const promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfNot(TaggedEqual(LoadMapPrototype(receiver_map), promise_prototype),
            if_slow);
  Branch(IsPromiseThenProtectorCellInvalid(), if_slow, if_fast);
}

void PromiseBuiltinsAssembler::BranchIfAccessCheckFailed(
    SloppyTNode<Context> context, SloppyTNode<Context> native_context,
    Node* promise_constructor, Node* executor, Label* if_noaccess) {
  VARIABLE(var_executor, MachineRepresentation::kTagged);
  var_executor.Bind(executor);
  Label has_access(this), call_runtime(this, Label::kDeferred);

  // If executor is a bound function, load the bound function until we've
  // reached an actual function.
  Label found_function(this), loop_over_bound_function(this, &var_executor);
  Goto(&loop_over_bound_function);
  BIND(&loop_over_bound_function);
  {
    TNode<Uint16T> executor_type = LoadInstanceType(var_executor.value());
    GotoIf(InstanceTypeEqual(executor_type, JS_FUNCTION_TYPE), &found_function);
    GotoIfNot(InstanceTypeEqual(executor_type, JS_BOUND_FUNCTION_TYPE),
              &call_runtime);
    var_executor.Bind(LoadObjectField(
        var_executor.value(), JSBoundFunction::kBoundTargetFunctionOffset));
    Goto(&loop_over_bound_function);
  }

  // Load the context from the function and compare it to the Promise
  // constructor's context. If they match, everything is fine, otherwise, bail
  // out to the runtime.
  BIND(&found_function);
  {
    TNode<Context> function_context =
        CAST(LoadObjectField(var_executor.value(), JSFunction::kContextOffset));
    TNode<NativeContext> native_function_context =
        LoadNativeContext(function_context);
    Branch(TaggedEqual(native_context, native_function_context), &has_access,
           &call_runtime);
  }

  BIND(&call_runtime);
  {
    Branch(TaggedEqual(CallRuntime(Runtime::kAllowDynamicFunction, context,
                                   promise_constructor),
                       TrueConstant()),
           &has_access, if_noaccess);
  }

  BIND(&has_access);
}

void PromiseBuiltinsAssembler::SetForwardingHandlerIfTrue(
    Node* context, Node* condition, const NodeGenerator& object) {
  Label done(this);
  GotoIfNot(condition, &done);
  SetPropertyStrict(
      CAST(context), CAST(object()),
      HeapConstant(factory()->promise_forwarding_handler_symbol()),
      TrueConstant());
  Goto(&done);
  BIND(&done);
}

void PromiseBuiltinsAssembler::SetPromiseHandledByIfTrue(
    Node* context, Node* condition, Node* promise,
    const NodeGenerator& handled_by) {
  Label done(this);
  GotoIfNot(condition, &done);
  GotoIf(TaggedIsSmi(promise), &done);
  GotoIfNot(HasInstanceType(promise, JS_PROMISE_TYPE), &done);
  SetPropertyStrict(CAST(context), CAST(promise),
                    HeapConstant(factory()->promise_handled_by_symbol()),
                    CAST(handled_by()));
  Goto(&done);
  BIND(&done);
}

// ES #sec-promise-reject-functions
TF_BUILTIN(PromiseCapabilityDefaultReject, PromiseBuiltinsAssembler) {
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. Let promise be F.[[Promise]].
  TNode<Object> const promise =
      LoadContextElement(context, PromiseBuiltins::kPromiseSlot);

  // 3. Let alreadyResolved be F.[[AlreadyResolved]].
  Label if_already_resolved(this, Label::kDeferred);
  TNode<Object> const already_resolved =
      LoadContextElement(context, PromiseBuiltins::kAlreadyResolvedSlot);

  // 4. If alreadyResolved.[[Value]] is true, return undefined.
  GotoIf(IsTrue(already_resolved), &if_already_resolved);

  // 5. Set alreadyResolved.[[Value]] to true.
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kAlreadyResolvedSlot, TrueConstant());

  // 6. Return RejectPromise(promise, reason).
  TNode<Object> const debug_event =
      LoadContextElement(context, PromiseBuiltins::kDebugEventSlot);
  Return(CallBuiltin(Builtins::kRejectPromise, context, promise, reason,
                     debug_event));

  BIND(&if_already_resolved);
  {
    Return(CallRuntime(Runtime::kPromiseRejectAfterResolved, context, promise,
                       reason));
  }
}

// ES #sec-promise-resolve-functions
TF_BUILTIN(PromiseCapabilityDefaultResolve, PromiseBuiltinsAssembler) {
  Node* const resolution = Parameter(Descriptor::kResolution);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. Let promise be F.[[Promise]].
  TNode<Object> const promise =
      LoadContextElement(context, PromiseBuiltins::kPromiseSlot);

  // 3. Let alreadyResolved be F.[[AlreadyResolved]].
  Label if_already_resolved(this, Label::kDeferred);
  TNode<Object> const already_resolved =
      LoadContextElement(context, PromiseBuiltins::kAlreadyResolvedSlot);

  // 4. If alreadyResolved.[[Value]] is true, return undefined.
  GotoIf(IsTrue(already_resolved), &if_already_resolved);

  // 5. Set alreadyResolved.[[Value]] to true.
  StoreContextElementNoWriteBarrier(
      context, PromiseBuiltins::kAlreadyResolvedSlot, TrueConstant());

  // The rest of the logic (and the catch prediction) is
  // encapsulated in the dedicated ResolvePromise builtin.
  Return(CallBuiltin(Builtins::kResolvePromise, context, promise, resolution));

  BIND(&if_already_resolved);
  {
    Return(CallRuntime(Runtime::kPromiseResolveAfterResolved, context, promise,
                       resolution));
  }
}

TF_BUILTIN(PromiseConstructorLazyDeoptContinuation, PromiseBuiltinsAssembler) {
  Node* promise = Parameter(Descriptor::kPromise);
  Node* reject = Parameter(Descriptor::kReject);
  Node* exception = Parameter(Descriptor::kException);
  Node* const context = Parameter(Descriptor::kContext);

  Label finally(this);

  GotoIf(IsTheHole(exception), &finally);
  CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
         context, reject, UndefinedConstant(), exception);
  Goto(&finally);

  BIND(&finally);
  Return(promise);
}

// ES6 #sec-promise-executor
TF_BUILTIN(PromiseConstructor, PromiseBuiltinsAssembler) {
  TNode<Object> executor = CAST(Parameter(Descriptor::kExecutor));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Isolate* isolate = this->isolate();

  Label if_targetisundefined(this, Label::kDeferred);

  GotoIf(IsUndefined(new_target), &if_targetisundefined);

  Label if_notcallable(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(executor), &if_notcallable);

  TNode<Map> const executor_map = LoadMap(CAST(executor));
  GotoIfNot(IsCallableMap(executor_map), &if_notcallable);

  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<JSFunction> const promise_fun =
      CAST(LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX));
  Node* const is_debug_active = IsDebugActive();
  Label if_targetisnotmodified(this),
      if_targetismodified(this, Label::kDeferred), run_executor(this),
      debug_push(this), if_noaccess(this, Label::kDeferred);

  BranchIfAccessCheckFailed(context, native_context, promise_fun, executor,
                            &if_noaccess);

  Branch(TaggedEqual(promise_fun, new_target), &if_targetisnotmodified,
         &if_targetismodified);

  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_reject_call, MachineRepresentation::kTagged);
  VARIABLE(var_reason, MachineRepresentation::kTagged);

  BIND(&if_targetisnotmodified);
  {
    TNode<JSPromise> const instance = AllocateAndInitJSPromise(context);
    var_result.Bind(instance);
    Goto(&debug_push);
  }

  BIND(&if_targetismodified);
  {
    ConstructorBuiltinsAssembler constructor_assembler(this->state());
    TNode<JSObject> instance = constructor_assembler.EmitFastNewObject(
        context, promise_fun, CAST(new_target));
    PromiseInit(instance);
    var_result.Bind(instance);

    GotoIfNot(IsPromiseHookEnabledOrHasAsyncEventDelegate(), &debug_push);
    CallRuntime(Runtime::kPromiseHookInit, context, instance,
                UndefinedConstant());
    Goto(&debug_push);
  }

  BIND(&debug_push);
  {
    GotoIfNot(is_debug_active, &run_executor);
    CallRuntime(Runtime::kDebugPushPromise, context, var_result.value());
    Goto(&run_executor);
  }

  BIND(&run_executor);
  {
    Label out(this), if_rejectpromise(this), debug_pop(this, Label::kDeferred);

    Node *resolve, *reject;
    std::tie(resolve, reject) = CreatePromiseResolvingFunctions(
        CAST(var_result.value()), TrueConstant(), native_context);

    Node* const maybe_exception = CallJS(
        CodeFactory::Call(isolate, ConvertReceiverMode::kNullOrUndefined),
        context, executor, UndefinedConstant(), resolve, reject);

    GotoIfException(maybe_exception, &if_rejectpromise, &var_reason);
    Branch(is_debug_active, &debug_pop, &out);

    BIND(&if_rejectpromise);
    {
      CallJS(CodeFactory::Call(isolate, ConvertReceiverMode::kNullOrUndefined),
             context, reject, UndefinedConstant(), var_reason.value());
      Branch(is_debug_active, &debug_pop, &out);
    }

    BIND(&debug_pop);
    {
      CallRuntime(Runtime::kDebugPopPromise, context);
      Goto(&out);
    }
    BIND(&out);
    Return(var_result.value());
  }

  // 1. If NewTarget is undefined, throw a TypeError exception.
  BIND(&if_targetisundefined);
  ThrowTypeError(context, MessageTemplate::kNotAPromise, new_target);

  // 2. If IsCallable(executor) is false, throw a TypeError exception.
  BIND(&if_notcallable);
  ThrowTypeError(context, MessageTemplate::kResolverNotAFunction, executor);

  // Silently fail if the stack looks fishy.
  BIND(&if_noaccess);
  {
    TNode<Smi> const counter_id =
        SmiConstant(v8::Isolate::kPromiseConstructorReturnedUndefined);
    CallRuntime(Runtime::kIncrementUseCounter, context, counter_id);
    Return(UndefinedConstant());
  }
}

// V8 Extras: v8.createPromise(parent)
TF_BUILTIN(PromiseInternalConstructor, PromiseBuiltinsAssembler) {
  const TNode<Object> parent = CAST(Parameter(Descriptor::kParent));
  const TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Return(AllocateAndInitJSPromise(context, parent));
}

// V8 Extras: v8.rejectPromise(promise, reason)
TF_BUILTIN(PromiseInternalReject, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const context = Parameter(Descriptor::kContext);

  // Main V8 Extras invariant that {promise} is still "pending" at
  // this point, aka that {promise} is not resolved multiple times.
  Label if_promise_is_settled(this, Label::kDeferred);
  GotoIfNot(IsPromiseStatus(PromiseStatus(promise), v8::Promise::kPending),
            &if_promise_is_settled);

  // We pass true to trigger the debugger's on exception handler.
  Return(CallBuiltin(Builtins::kRejectPromise, context, promise, reason,
                     TrueConstant()));

  BIND(&if_promise_is_settled);
  Abort(AbortReason::kPromiseAlreadySettled);
}

// V8 Extras: v8.resolvePromise(promise, resolution)
TF_BUILTIN(PromiseInternalResolve, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const resolution = Parameter(Descriptor::kResolution);
  Node* const context = Parameter(Descriptor::kContext);

  // Main V8 Extras invariant that {promise} is still "pending" at
  // this point, aka that {promise} is not resolved multiple times.
  Label if_promise_is_settled(this, Label::kDeferred);
  GotoIfNot(IsPromiseStatus(PromiseStatus(promise), v8::Promise::kPending),
            &if_promise_is_settled);

  Return(CallBuiltin(Builtins::kResolvePromise, context, promise, resolution));

  BIND(&if_promise_is_settled);
  Abort(AbortReason::kPromiseAlreadySettled);
}

// ES#sec-promise.prototype.then
// Promise.prototype.then ( onFulfilled, onRejected )
TF_BUILTIN(PromisePrototypeThen, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  const TNode<Object> maybe_promise = CAST(Parameter(Descriptor::kReceiver));
  const TNode<Object> on_fulfilled = CAST(Parameter(Descriptor::kOnFulfilled));
  const TNode<Object> on_rejected = CAST(Parameter(Descriptor::kOnRejected));
  const TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // 2. If IsPromise(promise) is false, throw a TypeError exception.
  ThrowIfNotInstanceType(context, maybe_promise, JS_PROMISE_TYPE,
                         "Promise.prototype.then");
  TNode<JSPromise> js_promise = CAST(maybe_promise);

  // 3. Let C be ? SpeciesConstructor(promise, %Promise%).
  Label fast_promise_capability(this), slow_constructor(this, Label::kDeferred),
      slow_promise_capability(this, Label::kDeferred);
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<JSFunction> promise_fun =
      CAST(LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX));
  TNode<Map> const promise_map = LoadMap(js_promise);
  BranchIfPromiseSpeciesLookupChainIntact(
      native_context, promise_map, &fast_promise_capability, &slow_constructor);

  BIND(&slow_constructor);
  TNode<JSReceiver> constructor =
      SpeciesConstructor(native_context, js_promise, promise_fun);
  Branch(TaggedEqual(constructor, promise_fun), &fast_promise_capability,
         &slow_promise_capability);

  // 4. Let resultCapability be ? NewPromiseCapability(C).
  Label perform_promise_then(this);
  TVARIABLE(Object, var_result_promise);
  TVARIABLE(HeapObject, var_result_promise_or_capability);

  BIND(&fast_promise_capability);
  {
    const TNode<JSPromise> result_promise =
        AllocateAndInitJSPromise(context, js_promise);
    var_result_promise_or_capability = result_promise;
    var_result_promise = result_promise;
    Goto(&perform_promise_then);
  }

  BIND(&slow_promise_capability);
  {
    TNode<Oddball> const debug_event = TrueConstant();
    TNode<PromiseCapability> const capability = CAST(CallBuiltin(
        Builtins::kNewPromiseCapability, context, constructor, debug_event));
    var_result_promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    var_result_promise_or_capability = capability;
    Goto(&perform_promise_then);
  }

  // 5. Return PerformPromiseThen(promise, onFulfilled, onRejected,
  //    resultCapability).
  BIND(&perform_promise_then);
  {
    // We do some work of the PerformPromiseThen operation here, in that
    // we check the handlers and turn non-callable handlers into undefined.
    // This is because this is the one and only callsite of PerformPromiseThen
    // that has to do this.

    // 3. If IsCallable(onFulfilled) is false, then
    //    a. Set onFulfilled to undefined.
    TVARIABLE(Object, var_on_fulfilled, on_fulfilled);
    Label if_fulfilled_done(this), if_fulfilled_notcallable(this);
    GotoIf(TaggedIsSmi(on_fulfilled), &if_fulfilled_notcallable);
    Branch(IsCallable(CAST(on_fulfilled)), &if_fulfilled_done,
           &if_fulfilled_notcallable);
    BIND(&if_fulfilled_notcallable);
    var_on_fulfilled = UndefinedConstant();
    Goto(&if_fulfilled_done);
    BIND(&if_fulfilled_done);

    // 4. If IsCallable(onRejected) is false, then
    //    a. Set onRejected to undefined.
    TVARIABLE(Object, var_on_rejected, on_rejected);
    Label if_rejected_done(this), if_rejected_notcallable(this);
    GotoIf(TaggedIsSmi(on_rejected), &if_rejected_notcallable);
    Branch(IsCallable(CAST(on_rejected)), &if_rejected_done,
           &if_rejected_notcallable);
    BIND(&if_rejected_notcallable);
    var_on_rejected = UndefinedConstant();
    Goto(&if_rejected_done);
    BIND(&if_rejected_done);

    PerformPromiseThen(context, js_promise, CAST(var_on_fulfilled.value()),
                       CAST(var_on_rejected.value()),
                       var_result_promise_or_capability.value());
    Return(var_result_promise.value());
  }
}

// ES#sec-promise.prototype.catch
// Promise.prototype.catch ( onRejected )
TF_BUILTIN(PromisePrototypeCatch, PromiseBuiltinsAssembler) {
  // 1. Let promise be the this value.
  Node* const receiver = Parameter(Descriptor::kReceiver);
  TNode<Oddball> const on_fulfilled = UndefinedConstant();
  Node* const on_rejected = Parameter(Descriptor::kOnRejected);
  Node* const context = Parameter(Descriptor::kContext);

  // 2. Return ? Invoke(promise, "then", « undefined, onRejected »).
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  Return(InvokeThen(native_context, receiver, on_fulfilled, on_rejected));
}

// ES #sec-promiseresolvethenablejob
TF_BUILTIN(PromiseResolveThenableJob, PromiseBuiltinsAssembler) {
  TNode<NativeContext> native_context = CAST(Parameter(Descriptor::kContext));
  TNode<JSPromise> promise_to_resolve =
      CAST(Parameter(Descriptor::kPromiseToResolve));
  TNode<JSReceiver> thenable = CAST(Parameter(Descriptor::kThenable));
  TNode<Object> then = CAST(Parameter(Descriptor::kThen));

  // We can use a simple optimization here if we know that {then} is the initial
  // Promise.prototype.then method, and {thenable} is a JSPromise whose
  // @@species lookup chain is intact: We can connect {thenable} and
  // {promise_to_resolve} directly in that case and avoid the allocation of a
  // temporary JSPromise and the closures plus context.
  //
  // We take the generic (slow-)path if a PromiseHook is enabled or the debugger
  // is active, to make sure we expose spec compliant behavior.
  Label if_fast(this), if_slow(this, Label::kDeferred);
  TNode<Object> promise_then =
      LoadContextElement(native_context, Context::PROMISE_THEN_INDEX);
  GotoIfNot(TaggedEqual(then, promise_then), &if_slow);
  TNode<Map> const thenable_map = LoadMap(thenable);
  GotoIfNot(IsJSPromiseMap(thenable_map), &if_slow);
  GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_slow);
  BranchIfPromiseSpeciesLookupChainIntact(native_context, thenable_map,
                                          &if_fast, &if_slow);

  BIND(&if_fast);
  {
    // We know that the {thenable} is a JSPromise, which doesn't require
    // any special treatment and that {then} corresponds to the initial
    // Promise.prototype.then method. So instead of allocating a temporary
    // JSPromise to connect the {thenable} with the {promise_to_resolve},
    // we can directly schedule the {promise_to_resolve} with default
    // handlers onto the {thenable} promise. This does not only save the
    // JSPromise allocation, but also avoids the allocation of the two
    // resolving closures and the shared context.
    //
    // What happens normally in this case is
    //
    //   resolve, reject = CreateResolvingFunctions(promise_to_resolve)
    //   result_capability = NewPromiseCapability(%Promise%)
    //   PerformPromiseThen(thenable, resolve, reject, result_capability)
    //
    // which means that PerformPromiseThen will either schedule a new
    // PromiseReaction with resolve and reject or a PromiseReactionJob
    // with resolve or reject based on the state of {thenable}. And
    // resolve or reject will just invoke the default [[Resolve]] or
    // [[Reject]] functions on the {promise_to_resolve}.
    //
    // This is the same as just doing
    //
    //   PerformPromiseThen(thenable, undefined, undefined, promise_to_resolve)
    //
    // which performs exactly the same (observable) steps.
    TailCallBuiltin(Builtins::kPerformPromiseThen, native_context, thenable,
                    UndefinedConstant(), UndefinedConstant(),
                    promise_to_resolve);
  }

  BIND(&if_slow);
  {
    Node* resolve = nullptr;
    Node* reject = nullptr;
    std::tie(resolve, reject) = CreatePromiseResolvingFunctions(
        promise_to_resolve, FalseConstant(), native_context);

    Label if_exception(this, Label::kDeferred);
    VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        native_context, then, thenable, resolve, reject);
    GotoIfException(result, &if_exception, &var_exception);
    Return(result);

    BIND(&if_exception);
    {
      // We need to reject the {thenable}.
      Node* const result = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          native_context, reject, UndefinedConstant(), var_exception.value());
      Return(result);
    }
  }
}

// ES #sec-promisereactionjob
void PromiseBuiltinsAssembler::PromiseReactionJob(Node* context, Node* argument,
                                                  Node* handler,
                                                  Node* promise_or_capability,
                                                  PromiseReaction::Type type) {
  CSA_ASSERT(this, TaggedIsNotSmi(handler));
  CSA_ASSERT(this, Word32Or(IsUndefined(handler), IsCallable(handler)));
  CSA_ASSERT(this, TaggedIsNotSmi(promise_or_capability));
  CSA_ASSERT(this,
             Word32Or(Word32Or(IsJSPromise(promise_or_capability),
                               IsPromiseCapability(promise_or_capability)),
                      IsUndefined(promise_or_capability)));

  VARIABLE(var_handler_result, MachineRepresentation::kTagged, argument);
  Label if_handler_callable(this), if_fulfill(this), if_reject(this),
      if_internal(this);
  Branch(IsUndefined(handler),
         type == PromiseReaction::kFulfill ? &if_fulfill : &if_reject,
         &if_handler_callable);

  BIND(&if_handler_callable);
  {
    Node* const result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
        context, handler, UndefinedConstant(), argument);
    GotoIfException(result, &if_reject, &var_handler_result);
    var_handler_result.Bind(result);
    Branch(IsUndefined(promise_or_capability), &if_internal, &if_fulfill);
  }

  BIND(&if_internal);
  {
    // There's no [[Capability]] for this promise reaction job, which
    // means that this is a specification-internal operation (aka await)
    // where the result does not matter (see the specification change in
    // https://github.com/tc39/ecma262/pull/1146 for details).
    Return(UndefinedConstant());
  }

  BIND(&if_fulfill);
  {
    Label if_promise(this), if_promise_capability(this, Label::kDeferred);
    Node* const value = var_handler_result.value();
    Branch(IsPromiseCapability(promise_or_capability), &if_promise_capability,
           &if_promise);

    BIND(&if_promise);
    {
      // For fast native promises we can skip the indirection
      // via the promiseCapability.[[Resolve]] function and
      // run the resolve logic directly from here.
      TailCallBuiltin(Builtins::kResolvePromise, context, promise_or_capability,
                      value);
    }

    BIND(&if_promise_capability);
    {
      // In the general case we need to call the (user provided)
      // promiseCapability.[[Resolve]] function.
      TNode<Object> const resolve = LoadObjectField(
          promise_or_capability, PromiseCapability::kResolveOffset);
      Node* const result = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          context, resolve, UndefinedConstant(), value);
      GotoIfException(result, &if_reject, &var_handler_result);
      Return(result);
    }
  }

  BIND(&if_reject);
  if (type == PromiseReaction::kReject) {
    Label if_promise(this), if_promise_capability(this, Label::kDeferred);
    Node* const reason = var_handler_result.value();
    Branch(IsPromiseCapability(promise_or_capability), &if_promise_capability,
           &if_promise);

    BIND(&if_promise);
    {
      // For fast native promises we can skip the indirection
      // via the promiseCapability.[[Reject]] function and
      // run the resolve logic directly from here.
      TailCallBuiltin(Builtins::kRejectPromise, context, promise_or_capability,
                      reason, FalseConstant());
    }

    BIND(&if_promise_capability);
    {
      // In the general case we need to call the (user provided)
      // promiseCapability.[[Reject]] function.
      Label if_exception(this, Label::kDeferred);
      VARIABLE(var_exception, MachineRepresentation::kTagged,
               TheHoleConstant());
      TNode<Object> const reject = LoadObjectField(
          promise_or_capability, PromiseCapability::kRejectOffset);
      Node* const result = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          context, reject, UndefinedConstant(), reason);
      GotoIfException(result, &if_exception, &var_exception);
      Return(result);

      // Swallow the exception here.
      BIND(&if_exception);
      TailCallRuntime(Runtime::kReportMessage, context, var_exception.value());
    }
  } else {
    // We have to call out to the dedicated PromiseRejectReactionJob builtin
    // here, instead of just doing the work inline, as otherwise the catch
    // predictions in the debugger will be wrong, which just walks the stack
    // and checks for certain builtins.
    TailCallBuiltin(Builtins::kPromiseRejectReactionJob, context,
                    var_handler_result.value(), UndefinedConstant(),
                    promise_or_capability);
  }
}

// ES #sec-promisereactionjob
TF_BUILTIN(PromiseFulfillReactionJob, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const handler = Parameter(Descriptor::kHandler);
  Node* const promise_or_capability =
      Parameter(Descriptor::kPromiseOrCapability);

  PromiseReactionJob(context, value, handler, promise_or_capability,
                     PromiseReaction::kFulfill);
}

// ES #sec-promisereactionjob
TF_BUILTIN(PromiseRejectReactionJob, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const handler = Parameter(Descriptor::kHandler);
  Node* const promise_or_capability =
      Parameter(Descriptor::kPromiseOrCapability);

  PromiseReactionJob(context, reason, handler, promise_or_capability,
                     PromiseReaction::kReject);
}

TF_BUILTIN(PromiseResolveTrampoline, PromiseBuiltinsAssembler) {
  //  1. Let C be the this value.
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // 2. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, CAST(receiver),
                       MessageTemplate::kCalledOnNonObject, "PromiseResolve");

  // 3. Return ? PromiseResolve(C, x).
  Return(CallBuiltin(Builtins::kPromiseResolve, context, receiver, value));
}

TF_BUILTIN(PromiseResolve, PromiseBuiltinsAssembler) {
  TNode<JSReceiver> constructor = CAST(Parameter(Descriptor::kConstructor));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<JSFunction> promise_fun =
      CAST(LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX));

  Label if_slow_constructor(this, Label::kDeferred), if_need_to_allocate(this);

  // Check if {value} is a JSPromise.
  GotoIf(TaggedIsSmi(value), &if_need_to_allocate);
  TNode<Map> const value_map = LoadMap(CAST(value));
  GotoIfNot(IsJSPromiseMap(value_map), &if_need_to_allocate);

  // We can skip the "constructor" lookup on {value} if it's [[Prototype]]
  // is the (initial) Promise.prototype and the @@species protector is
  // intact, as that guards the lookup path for "constructor" on
  // JSPromise instances which have the (initial) Promise.prototype.
  TNode<Object> promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  GotoIfNot(TaggedEqual(LoadMapPrototype(value_map), promise_prototype),
            &if_slow_constructor);
  GotoIf(IsPromiseSpeciesProtectorCellInvalid(), &if_slow_constructor);

  // If the {constructor} is the Promise function, we just immediately
  // return the {value} here and don't bother wrapping it into a
  // native Promise.
  GotoIfNot(TaggedEqual(promise_fun, constructor), &if_slow_constructor);
  Return(value);

  // At this point, value or/and constructor are not native promises, but
  // they could be of the same subclass.
  BIND(&if_slow_constructor);
  {
    TNode<Object> value_constructor =
        GetProperty(context, value, isolate()->factory()->constructor_string());
    GotoIfNot(TaggedEqual(value_constructor, constructor),
              &if_need_to_allocate);
    Return(value);
  }

  BIND(&if_need_to_allocate);
  {
    Label if_nativepromise(this), if_notnativepromise(this, Label::kDeferred);
    Branch(TaggedEqual(promise_fun, constructor), &if_nativepromise,
           &if_notnativepromise);

    // This adds a fast path for native promises that don't need to
    // create NewPromiseCapability.
    BIND(&if_nativepromise);
    {
      TNode<JSPromise> const result = AllocateAndInitJSPromise(context);
      CallBuiltin(Builtins::kResolvePromise, context, result, value);
      Return(result);
    }

    BIND(&if_notnativepromise);
    {
      TNode<Oddball> const debug_event = TrueConstant();
      TNode<PromiseCapability> const capability = CAST(CallBuiltin(
          Builtins::kNewPromiseCapability, context, constructor, debug_event));

      TNode<Object> const resolve =
          LoadObjectField(capability, PromiseCapability::kResolveOffset);
      CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          context, resolve, UndefinedConstant(), value);

      TNode<Object> const result =
          LoadObjectField(capability, PromiseCapability::kPromiseOffset);
      Return(result);
    }
  }
}

// ES6 #sec-getcapabilitiesexecutor-functions
TF_BUILTIN(PromiseGetCapabilitiesExecutor, PromiseBuiltinsAssembler) {
  Node* const resolve = Parameter(Descriptor::kResolve);
  Node* const reject = Parameter(Descriptor::kReject);
  Node* const context = Parameter(Descriptor::kContext);

  TNode<PromiseCapability> const capability =
      CAST(LoadContextElement(context, PromiseBuiltins::kCapabilitySlot));

  Label if_alreadyinvoked(this, Label::kDeferred);
  GotoIfNot(IsUndefined(
                LoadObjectField(capability, PromiseCapability::kResolveOffset)),
            &if_alreadyinvoked);
  GotoIfNot(IsUndefined(
                LoadObjectField(capability, PromiseCapability::kRejectOffset)),
            &if_alreadyinvoked);

  StoreObjectField(capability, PromiseCapability::kResolveOffset, resolve);
  StoreObjectField(capability, PromiseCapability::kRejectOffset, reject);

  Return(UndefinedConstant());

  BIND(&if_alreadyinvoked);
  ThrowTypeError(context, MessageTemplate::kPromiseExecutorAlreadyInvoked);
}

TF_BUILTIN(PromiseReject, PromiseBuiltinsAssembler) {
  // 1. Let C be the this value.
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> reason = CAST(Parameter(Descriptor::kReason));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // 2. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "PromiseReject");

  Label if_nativepromise(this), if_custompromise(this, Label::kDeferred);
  TNode<NativeContext> const native_context = LoadNativeContext(context);

  TNode<Object> promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  Branch(TaggedEqual(promise_fun, receiver), &if_nativepromise,
         &if_custompromise);

  BIND(&if_nativepromise);
  {
    TNode<JSPromise> const promise =
        AllocateAndSetJSPromise(context, v8::Promise::kRejected, reason);
    CallRuntime(Runtime::kPromiseRejectEventFromStack, context, promise,
                reason);
    Return(promise);
  }

  BIND(&if_custompromise);
  {
    // 3. Let promiseCapability be ? NewPromiseCapability(C).
    TNode<Oddball> const debug_event = TrueConstant();
    TNode<PromiseCapability> const capability = CAST(CallBuiltin(
        Builtins::kNewPromiseCapability, context, receiver, debug_event));

    // 4. Perform ? Call(promiseCapability.[[Reject]], undefined, « r »).
    TNode<Object> const reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
           context, reject, UndefinedConstant(), reason);

    // 5. Return promiseCapability.[[Promise]].
    TNode<Object> const promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

std::pair<Node*, Node*> PromiseBuiltinsAssembler::CreatePromiseFinallyFunctions(
    Node* on_finally, Node* constructor, Node* native_context) {
  const TNode<Context> promise_context = CreatePromiseContext(
      CAST(native_context), PromiseBuiltins::kPromiseFinallyContextLength);
  StoreContextElementNoWriteBarrier(
      promise_context, PromiseBuiltins::kOnFinallySlot, on_finally);
  StoreContextElementNoWriteBarrier(
      promise_context, PromiseBuiltins::kConstructorSlot, constructor);
  const TNode<Map> map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  const TNode<SharedFunctionInfo> then_finally_info = CAST(LoadContextElement(
      native_context, Context::PROMISE_THEN_FINALLY_SHARED_FUN));
  TNode<JSFunction> const then_finally = AllocateFunctionWithMapAndContext(
      map, then_finally_info, promise_context);
  const TNode<SharedFunctionInfo> catch_finally_info = CAST(LoadContextElement(
      native_context, Context::PROMISE_CATCH_FINALLY_SHARED_FUN));
  TNode<JSFunction> const catch_finally = AllocateFunctionWithMapAndContext(
      map, catch_finally_info, promise_context);
  return std::make_pair(then_finally, catch_finally);
}

TF_BUILTIN(PromiseValueThunkFinally, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);

  TNode<Object> const value =
      LoadContextElement(context, PromiseBuiltins::kValueSlot);
  Return(value);
}

Node* PromiseBuiltinsAssembler::CreateValueThunkFunction(Node* value,
                                                         Node* native_context) {
  const TNode<Context> value_thunk_context = CreatePromiseContext(
      CAST(native_context),
      PromiseBuiltins::kPromiseValueThunkOrReasonContextLength);
  StoreContextElementNoWriteBarrier(value_thunk_context,
                                    PromiseBuiltins::kValueSlot, value);
  const TNode<Map> map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  const TNode<SharedFunctionInfo> value_thunk_info = CAST(LoadContextElement(
      native_context, Context::PROMISE_VALUE_THUNK_FINALLY_SHARED_FUN));
  TNode<JSFunction> const value_thunk = AllocateFunctionWithMapAndContext(
      map, value_thunk_info, value_thunk_context);
  return value_thunk;
}

TF_BUILTIN(PromiseThenFinally, PromiseBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);

  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  // 1. Let onFinally be F.[[OnFinally]].
  TNode<HeapObject> const on_finally =
      CAST(LoadContextElement(context, PromiseBuiltins::kOnFinallySlot));

  // 2.  Assert: IsCallable(onFinally) is true.
  CSA_ASSERT(this, IsCallable(on_finally));

  // 3. Let result be ?  Call(onFinally).
  Node* const result = CallJS(
      CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
      context, on_finally, UndefinedConstant());

  // 4. Let C be F.[[Constructor]].
  TNode<JSFunction> const constructor =
      CAST(LoadContextElement(context, PromiseBuiltins::kConstructorSlot));

  // 5. Assert: IsConstructor(C) is true.
  CSA_ASSERT(this, IsConstructor(constructor));

  // 6. Let promise be ? PromiseResolve(C, result).
  TNode<Object> const promise =
      CallBuiltin(Builtins::kPromiseResolve, context, constructor, result);

  // 7. Let valueThunk be equivalent to a function that returns value.
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  Node* const value_thunk = CreateValueThunkFunction(value, native_context);

  // 8. Return ? Invoke(promise, "then", « valueThunk »).
  Return(InvokeThen(native_context, promise, value_thunk));
}

TF_BUILTIN(PromiseThrowerFinally, PromiseBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);

  TNode<Object> const reason =
      LoadContextElement(context, PromiseBuiltins::kValueSlot);
  CallRuntime(Runtime::kThrow, context, reason);
  Unreachable();
}

Node* PromiseBuiltinsAssembler::CreateThrowerFunction(Node* reason,
                                                      Node* native_context) {
  const TNode<Context> thrower_context = CreatePromiseContext(
      CAST(native_context),
      PromiseBuiltins::kPromiseValueThunkOrReasonContextLength);
  StoreContextElementNoWriteBarrier(thrower_context,
                                    PromiseBuiltins::kValueSlot, reason);
  const TNode<Map> map = CAST(LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
  const TNode<SharedFunctionInfo> thrower_info = CAST(LoadContextElement(
      native_context, Context::PROMISE_THROWER_FINALLY_SHARED_FUN));
  TNode<JSFunction> const thrower =
      AllocateFunctionWithMapAndContext(map, thrower_info, thrower_context);
  return thrower;
}

TF_BUILTIN(PromiseCatchFinally, PromiseBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);

  Node* const reason = Parameter(Descriptor::kReason);
  Node* const context = Parameter(Descriptor::kContext);

  // 1. Let onFinally be F.[[OnFinally]].
  TNode<HeapObject> const on_finally =
      CAST(LoadContextElement(context, PromiseBuiltins::kOnFinallySlot));

  // 2. Assert: IsCallable(onFinally) is true.
  CSA_ASSERT(this, IsCallable(on_finally));

  // 3. Let result be ? Call(onFinally).
  Node* result = CallJS(
      CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
      context, on_finally, UndefinedConstant());

  // 4. Let C be F.[[Constructor]].
  TNode<JSFunction> const constructor =
      CAST(LoadContextElement(context, PromiseBuiltins::kConstructorSlot));

  // 5. Assert: IsConstructor(C) is true.
  CSA_ASSERT(this, IsConstructor(constructor));

  // 6. Let promise be ? PromiseResolve(C, result).
  TNode<Object> const promise =
      CallBuiltin(Builtins::kPromiseResolve, context, constructor, result);

  // 7. Let thrower be equivalent to a function that throws reason.
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  Node* const thrower = CreateThrowerFunction(reason, native_context);

  // 8. Return ? Invoke(promise, "then", « thrower »).
  Return(InvokeThen(native_context, promise, thrower));
}

TF_BUILTIN(PromisePrototypeFinally, PromiseBuiltinsAssembler) {
  CSA_ASSERT_JS_ARGC_EQ(this, 1);

  // 1.  Let promise be the this value.
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const on_finally = Parameter(Descriptor::kOnFinally);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // 2. If Type(promise) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, CAST(receiver),
                       MessageTemplate::kCalledOnNonObject,
                       "Promise.prototype.finally");

  // 3. Let C be ? SpeciesConstructor(promise, %Promise%).
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  TNode<Object> const promise_fun =
      LoadContextElement(native_context, Context::PROMISE_FUNCTION_INDEX);
  VARIABLE(var_constructor, MachineRepresentation::kTagged, promise_fun);
  Label slow_constructor(this, Label::kDeferred), done_constructor(this);
  TNode<Map> const receiver_map = LoadMap(receiver);
  GotoIfNot(IsJSPromiseMap(receiver_map), &slow_constructor);
  BranchIfPromiseSpeciesLookupChainIntact(native_context, receiver_map,
                                          &done_constructor, &slow_constructor);
  BIND(&slow_constructor);
  {
    TNode<JSReceiver> const constructor =
        SpeciesConstructor(context, receiver, CAST(promise_fun));
    var_constructor.Bind(constructor);
    Goto(&done_constructor);
  }
  BIND(&done_constructor);
  Node* const constructor = var_constructor.value();

  // 4. Assert: IsConstructor(C) is true.
  CSA_ASSERT(this, IsConstructor(constructor));

  VARIABLE(var_then_finally, MachineRepresentation::kTagged);
  VARIABLE(var_catch_finally, MachineRepresentation::kTagged);

  Label if_notcallable(this, Label::kDeferred), perform_finally(this);

  GotoIf(TaggedIsSmi(on_finally), &if_notcallable);
  GotoIfNot(IsCallable(on_finally), &if_notcallable);

  // 6. Else,
  //   a. Let thenFinally be a new built-in function object as defined
  //   in ThenFinally Function.
  //   b. Let catchFinally be a new built-in function object as
  //   defined in CatchFinally Function.
  //   c. Set thenFinally and catchFinally's [[Constructor]] internal
  //   slots to C.
  //   d. Set thenFinally and catchFinally's [[OnFinally]] internal
  //   slots to onFinally.
  Node* then_finally = nullptr;
  Node* catch_finally = nullptr;
  std::tie(then_finally, catch_finally) =
      CreatePromiseFinallyFunctions(on_finally, constructor, native_context);
  var_then_finally.Bind(then_finally);
  var_catch_finally.Bind(catch_finally);
  Goto(&perform_finally);

  // 5. If IsCallable(onFinally) is not true,
  //    a. Let thenFinally be onFinally.
  //    b. Let catchFinally be onFinally.
  BIND(&if_notcallable);
  {
    var_then_finally.Bind(on_finally);
    var_catch_finally.Bind(on_finally);
    Goto(&perform_finally);
  }

  // 7. Return ? Invoke(promise, "then", « thenFinally, catchFinally »).
  BIND(&perform_finally);
  Return(InvokeThen(native_context, receiver, var_then_finally.value(),
                    var_catch_finally.value()));
}

// ES #sec-fulfillpromise
TF_BUILTIN(FulfillPromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, TaggedIsNotSmi(promise));
  CSA_ASSERT(this, IsJSPromise(promise));

  // 2. Let reactions be promise.[[PromiseFulfillReactions]].
  TNode<Object> const reactions =
      LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);

  // 3. Set promise.[[PromiseResult]] to value.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  StoreObjectField(promise, JSPromise::kReactionsOrResultOffset, value);

  // 6. Set promise.[[PromiseState]] to "fulfilled".
  PromiseSetStatus(promise, Promise::kFulfilled);

  // 7. Return TriggerPromiseReactions(reactions, value).
  Return(TriggerPromiseReactions(context, reactions, value,
                                 PromiseReaction::kFulfill));
}

// ES #sec-rejectpromise
TF_BUILTIN(RejectPromise, PromiseBuiltinsAssembler) {
  Node* const promise = Parameter(Descriptor::kPromise);
  Node* const reason = Parameter(Descriptor::kReason);
  Node* const debug_event = Parameter(Descriptor::kDebugEvent);
  Node* const context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, TaggedIsNotSmi(promise));
  CSA_ASSERT(this, IsJSPromise(promise));
  CSA_ASSERT(this, IsBoolean(debug_event));
  Label if_runtime(this, Label::kDeferred);

  // If promise hook is enabled or the debugger is active, let
  // the runtime handle this operation, which greatly reduces
  // the complexity here and also avoids a couple of back and
  // forth between JavaScript and C++ land.
  GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_runtime);

  // 7. If promise.[[PromiseIsHandled]] is false, perform
  //    HostPromiseRejectionTracker(promise, "reject").
  // We don't try to handle rejecting {promise} without handler
  // here, but we let the C++ code take care of this completely.
  GotoIfNot(PromiseHasHandler(promise), &if_runtime);

  // 2. Let reactions be promise.[[PromiseRejectReactions]].
  TNode<Object> reactions =
      LoadObjectField(promise, JSPromise::kReactionsOrResultOffset);

  // 3. Set promise.[[PromiseResult]] to reason.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  StoreObjectField(promise, JSPromise::kReactionsOrResultOffset, reason);

  // 6. Set promise.[[PromiseState]] to "rejected".
  PromiseSetStatus(promise, Promise::kRejected);

  // 7. Return TriggerPromiseReactions(reactions, reason).
  Return(TriggerPromiseReactions(context, reactions, reason,
                                 PromiseReaction::kReject));

  BIND(&if_runtime);
  TailCallRuntime(Runtime::kRejectPromise, context, promise, reason,
                  debug_event);
}

// ES #sec-promise-resolve-functions
TF_BUILTIN(ResolvePromise, PromiseBuiltinsAssembler) {
  TNode<JSPromise> const promise = CAST(Parameter(Descriptor::kPromise));
  TNode<Object> const resolution = CAST(Parameter(Descriptor::kResolution));
  TNode<Context> const context = CAST(Parameter(Descriptor::kContext));

  Label do_enqueue(this), if_fulfill(this), if_reject(this, Label::kDeferred),
      if_runtime(this, Label::kDeferred);
  TVARIABLE(Object, var_reason);
  TVARIABLE(JSReceiver, var_then);

  // If promise hook is enabled or the debugger is active, let
  // the runtime handle this operation, which greatly reduces
  // the complexity here and also avoids a couple of back and
  // forth between JavaScript and C++ land.
  GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
         &if_runtime);

  // 6. If SameValue(resolution, promise) is true, then
  // We can use pointer comparison here, since the {promise} is guaranteed
  // to be a JSPromise inside this function and thus is reference comparable.
  GotoIf(TaggedEqual(promise, resolution), &if_runtime);

  // 7. If Type(resolution) is not Object, then
  GotoIf(TaggedIsSmi(resolution), &if_fulfill);
  TNode<Map> resolution_map = LoadMap(CAST(resolution));
  GotoIfNot(IsJSReceiverMap(resolution_map), &if_fulfill);

  // We can skip the "then" lookup on {resolution} if its [[Prototype]]
  // is the (initial) Promise.prototype and the Promise#then protector
  // is intact, as that guards the lookup path for the "then" property
  // on JSPromise instances which have the (initial) %PromisePrototype%.
  Label if_fast(this), if_receiver(this), if_slow(this, Label::kDeferred);
  TNode<NativeContext> const native_context = LoadNativeContext(context);
  GotoIfForceSlowPath(&if_slow);
  GotoIf(IsPromiseThenProtectorCellInvalid(), &if_slow);
  GotoIfNot(IsJSPromiseMap(resolution_map), &if_receiver);
  TNode<Object> const promise_prototype =
      LoadContextElement(native_context, Context::PROMISE_PROTOTYPE_INDEX);
  Branch(TaggedEqual(LoadMapPrototype(resolution_map), promise_prototype),
         &if_fast, &if_slow);

  BIND(&if_fast);
  {
    // The {resolution} is a native Promise in this case.
    var_then =
        CAST(LoadContextElement(native_context, Context::PROMISE_THEN_INDEX));
    Goto(&do_enqueue);
  }

  BIND(&if_receiver);
  {
    // We can skip the lookup of "then" if the {resolution} is a (newly
    // created) IterResultObject, as the Promise#then() protector also
    // ensures that the intrinsic %ObjectPrototype% doesn't contain any
    // "then" property. This helps to avoid negative lookups on iterator
    // results from async generators.
    CSA_ASSERT(this, IsJSReceiverMap(resolution_map));
    CSA_ASSERT(this, Word32BinaryNot(IsPromiseThenProtectorCellInvalid()));
    TNode<Object> const iterator_result_map =
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
    Branch(TaggedEqual(resolution_map, iterator_result_map), &if_fulfill,
           &if_slow);
  }

  BIND(&if_slow);
  {
    // 8. Let then be Get(resolution, "then").
    TNode<Object> then =
        GetProperty(context, resolution, isolate()->factory()->then_string());

    // 9. If then is an abrupt completion, then
    GotoIfException(then, &if_reject, &var_reason);

    // 11. If IsCallable(thenAction) is false, then
    GotoIf(TaggedIsSmi(then), &if_fulfill);
    TNode<Map> const then_map = LoadMap(CAST(then));
    GotoIfNot(IsCallableMap(then_map), &if_fulfill);
    var_then = CAST(then);
    Goto(&do_enqueue);
  }

  BIND(&do_enqueue);
  {
    // 12. Perform EnqueueJob("PromiseJobs", PromiseResolveThenableJob,
    //                        «promise, resolution, thenAction»).
    const TNode<PromiseResolveThenableJobTask> task =
        AllocatePromiseResolveThenableJobTask(promise, var_then.value(),
                                              CAST(resolution), native_context);
    TailCallBuiltin(Builtins::kEnqueueMicrotask, native_context, task);
  }

  BIND(&if_fulfill);
  {
    // 7.b Return FulfillPromise(promise, resolution).
    TailCallBuiltin(Builtins::kFulfillPromise, context, promise, resolution);
  }

  BIND(&if_runtime);
  Return(CallRuntime(Runtime::kResolvePromise, context, promise, resolution));

  BIND(&if_reject);
  {
    // 9.a Return RejectPromise(promise, then.[[Value]]).
    TailCallBuiltin(Builtins::kRejectPromise, context, promise,
                    var_reason.value(), FalseConstant());
  }
}

Node* PromiseBuiltinsAssembler::PerformPromiseAll(
    Node* context, Node* constructor, Node* capability,
    const IteratorRecord& iterator,
    const PromiseAllResolvingElementFunction& create_resolve_element_function,
    const PromiseAllResolvingElementFunction& create_reject_element_function,
    Label* if_exception, Variable* var_exception) {
  IteratorBuiltinsAssembler iter_assembler(state());

  TNode<NativeContext> native_context = LoadNativeContext(context);

  // For catch prediction, don't treat the .then calls as handling it;
  // instead, recurse outwards.
  SetForwardingHandlerIfTrue(
      native_context, IsDebugActive(),
      LoadObjectField(capability, PromiseCapability::kRejectOffset));

  TNode<Context> resolve_element_context =
      Cast(CreatePromiseAllResolveElementContext(capability, native_context));

  TVARIABLE(Smi, var_index, SmiConstant(1));
  Label loop(this, &var_index), done_loop(this),
      too_many_elements(this, Label::kDeferred),
      close_iterator(this, Label::kDeferred), if_slow(this, Label::kDeferred);

  // We can skip the "resolve" lookup on {constructor} if it's the
  // Promise constructor and the Promise.resolve protector is intact,
  // as that guards the lookup path for the "resolve" property on the
  // Promise constructor.
  TVARIABLE(Object, var_promise_resolve_function, UndefinedConstant());
  GotoIfNotPromiseResolveLookupChainIntact(native_context, constructor,
                                           &if_slow);
  Goto(&loop);

  BIND(&if_slow);
  {
    // 5. Let _promiseResolve_ be ? Get(_constructor_, `"resolve"`).
    TNode<Object> resolve =
        GetProperty(native_context, constructor, factory()->resolve_string());
    GotoIfException(resolve, &close_iterator, var_exception);

    // 6. If IsCallable(_promiseResolve_) is *false*, throw a *TypeError*
    // exception.
    ThrowIfNotCallable(CAST(context), resolve, "resolve");

    var_promise_resolve_function = resolve;
    Goto(&loop);
  }

  BIND(&loop);
  {
    // Let next be IteratorStep(iteratorRecord.[[Iterator]]).
    // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
    // ReturnIfAbrupt(next).
    TNode<Map> const fast_iterator_result_map = CAST(
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX));
    TNode<JSReceiver> const next = iter_assembler.IteratorStep(
        native_context, iterator, &done_loop, fast_iterator_result_map,
        if_exception, var_exception);

    // Let nextValue be IteratorValue(next).
    // If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to
    //     true.
    // ReturnIfAbrupt(nextValue).
    TNode<Object> const next_value = iter_assembler.IteratorValue(
        native_context, next, fast_iterator_result_map, if_exception,
        var_exception);

    // Check if we reached the limit.
    TNode<Smi> const index = var_index.value();
    GotoIf(SmiEqual(index, SmiConstant(PropertyArray::HashField::kMax)),
           &too_many_elements);

    // Set index to index + 1.
    var_index = SmiAdd(index, SmiConstant(1));

    // Set remainingElementsCount.[[Value]] to
    //     remainingElementsCount.[[Value]] + 1.
    TNode<Smi> const remaining_elements_count = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
    StoreContextElementNoWriteBarrier(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
        SmiAdd(remaining_elements_count, SmiConstant(1)));

    // Let resolveElement be CreateBuiltinFunction(steps,
    //                                             « [[AlreadyCalled]],
    //                                               [[Index]],
    //                                               [[Values]],
    //                                               [[Capability]],
    //                                               [[RemainingElements]] »).
    // Set resolveElement.[[AlreadyCalled]] to a Record { [[Value]]: false }.
    // Set resolveElement.[[Index]] to index.
    // Set resolveElement.[[Values]] to values.
    // Set resolveElement.[[Capability]] to resultCapability.
    // Set resolveElement.[[RemainingElements]] to remainingElementsCount.
    TNode<Object> const resolve_element_fun = create_resolve_element_function(
        resolve_element_context, index, native_context, Cast(capability));
    TNode<Object> const reject_element_fun = create_reject_element_function(
        resolve_element_context, index, native_context, Cast(capability));

    // We can skip the "resolve" lookup on the {constructor} as well as the
    // "then" lookup on the result of the "resolve" call, and immediately
    // chain continuation onto the {next_value} if:
    //
    //   (a) The {constructor} is the intrinsic %Promise% function, and
    //       looking up "resolve" on {constructor} yields the initial
    //       Promise.resolve() builtin, and
    //   (b) the promise @@species protector cell is valid, meaning that
    //       no one messed with the Symbol.species property on any
    //       intrinsic promise or on the Promise.prototype, and
    //   (c) the {next_value} is a JSPromise whose [[Prototype]] field
    //       contains the intrinsic %PromisePrototype%, and
    //   (d) we're not running with async_hooks or DevTools enabled.
    //
    // In that case we also don't need to allocate a chained promise for
    // the PromiseReaction (aka we can pass undefined to PerformPromiseThen),
    // since this is only necessary for DevTools and PromiseHooks.
    Label if_fast(this), if_slow(this);
    GotoIfNot(IsUndefined(var_promise_resolve_function.value()), &if_slow);
    GotoIf(IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate(),
           &if_slow);
    GotoIf(IsPromiseSpeciesProtectorCellInvalid(), &if_slow);
    GotoIf(TaggedIsSmi(next_value), &if_slow);
    TNode<Map> const next_value_map = LoadMap(CAST(next_value));
    BranchIfPromiseThenLookupChainIntact(native_context, next_value_map,
                                         &if_fast, &if_slow);

    BIND(&if_fast);
    {
      // Register the PromiseReaction immediately on the {next_value}, not
      // passing any chained promise since neither async_hooks nor DevTools
      // are enabled, so there's no use of the resulting promise.
      PerformPromiseThen(native_context, CAST(next_value),
                         CAST(resolve_element_fun), CAST(reject_element_fun),
                         UndefinedConstant());
      Goto(&loop);
    }

    BIND(&if_slow);
    {
      // Let nextPromise be ? Call(constructor, _promiseResolve_, « nextValue
      // »).
      Node* const next_promise = CallResolve(
          native_context, constructor, var_promise_resolve_function.value(),
          next_value, &close_iterator, var_exception);

      // Perform ? Invoke(nextPromise, "then", « resolveElement,
      //                  resultCapability.[[Reject]] »).
      TNode<Object> const then =
          GetProperty(native_context, next_promise, factory()->then_string());
      GotoIfException(then, &close_iterator, var_exception);

      Node* const then_call =
          CallJS(CodeFactory::Call(isolate(),
                                   ConvertReceiverMode::kNotNullOrUndefined),
                 native_context, then, next_promise, resolve_element_fun,
                 reject_element_fun);
      GotoIfException(then_call, &close_iterator, var_exception);

      // For catch prediction, mark that rejections here are semantically
      // handled by the combined Promise.
      SetPromiseHandledByIfTrue(
          native_context, IsDebugActive(), then_call, [=]() {
            // Load promiseCapability.[[Promise]]
            return LoadObjectField(capability,
                                   PromiseCapability::kPromiseOffset);
          });

      Goto(&loop);
    }
  }

  BIND(&too_many_elements);
  {
    // If there are too many elements (currently more than 2**21-1), raise a
    // RangeError here (which is caught directly and turned into a rejection)
    // of the resulting promise. We could gracefully handle this case as well
    // and support more than this number of elements by going to a separate
    // function and pass the larger indices via a separate context, but it
    // doesn't seem likely that we need this, and it's unclear how the rest
    // of the system deals with 2**21 live Promises anyways.
    TNode<Object> const result =
        CallRuntime(Runtime::kThrowRangeError, native_context,
                    SmiConstant(MessageTemplate::kTooManyElementsInPromiseAll));
    GotoIfException(result, &close_iterator, var_exception);
    Unreachable();
  }

  BIND(&close_iterator);
  {
    // Exception must be bound to a JS value.
    CSA_ASSERT(this, IsNotTheHole(var_exception->value()));
    iter_assembler.IteratorCloseOnException(native_context, iterator,
                                            if_exception, var_exception);
  }

  BIND(&done_loop);
  {
    Label resolve_promise(this, Label::kDeferred), return_promise(this);
    // Set iteratorRecord.[[Done]] to true.
    // Set remainingElementsCount.[[Value]] to
    //    remainingElementsCount.[[Value]] - 1.
    TNode<Smi> remaining_elements_count = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
    remaining_elements_count = SmiSub(remaining_elements_count, SmiConstant(1));
    StoreContextElementNoWriteBarrier(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
        remaining_elements_count);
    GotoIf(SmiEqual(remaining_elements_count, SmiConstant(0)),
           &resolve_promise);

    // Pre-allocate the backing store for the {values_array} to the desired
    // capacity here. We may already have elements here in case of some
    // fancy Thenable that calls the resolve callback immediately, so we need
    // to handle that correctly here.
    TNode<JSArray> const values_array = CAST(LoadContextElement(
        resolve_element_context,
        PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot));
    TNode<FixedArrayBase> const old_elements = LoadElements(values_array);
    TNode<Smi> const old_capacity = LoadFixedArrayBaseLength(old_elements);
    TNode<Smi> const new_capacity = var_index.value();
    GotoIf(SmiGreaterThanOrEqual(old_capacity, new_capacity), &return_promise);
    TNode<FixedArrayBase> const new_elements =
        AllocateFixedArray(PACKED_ELEMENTS, new_capacity, SMI_PARAMETERS,
                           AllocationFlag::kAllowLargeObjectAllocation);
    CopyFixedArrayElements(PACKED_ELEMENTS, old_elements, PACKED_ELEMENTS,
                           new_elements, SmiConstant(0), old_capacity,
                           new_capacity, UPDATE_WRITE_BARRIER, SMI_PARAMETERS);
    StoreObjectField(values_array, JSArray::kElementsOffset, new_elements);
    Goto(&return_promise);

    // If remainingElementsCount.[[Value]] is 0, then
    //     Let valuesArray be CreateArrayFromList(values).
    //     Perform ? Call(resultCapability.[[Resolve]], undefined,
    //                    « valuesArray »).
    BIND(&resolve_promise);
    {
      TNode<Object> const resolve =
          LoadObjectField(capability, PromiseCapability::kResolveOffset);
      TNode<Object> const values_array = LoadContextElement(
          resolve_element_context,
          PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot);
      Node* const resolve_call = CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          native_context, resolve, UndefinedConstant(), values_array);
      GotoIfException(resolve_call, if_exception, var_exception);
      Goto(&return_promise);
    }

    // Return resultCapability.[[Promise]].
    BIND(&return_promise);
  }

  TNode<Object> const promise =
      LoadObjectField(capability, PromiseCapability::kPromiseOffset);
  return promise;
}

void PromiseBuiltinsAssembler::Generate_PromiseAll(
    TNode<Context> context, TNode<Object> receiver, TNode<Object> iterable,
    const PromiseAllResolvingElementFunction& create_resolve_element_function,
    const PromiseAllResolvingElementFunction& create_reject_element_function) {
  IteratorBuiltinsAssembler iter_assembler(state());

  // Let C be the this value.
  // If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver, MessageTemplate::kCalledOnNonObject,
                       "Promise.all");

  // Let promiseCapability be ? NewPromiseCapability(C).
  // Don't fire debugEvent so that forwarding the rejection through all does not
  // trigger redundant ExceptionEvents
  TNode<Oddball> const debug_event = FalseConstant();
  TNode<PromiseCapability> const capability = CAST(CallBuiltin(
      Builtins::kNewPromiseCapability, context, receiver, debug_event));

  VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());
  Label reject_promise(this, &var_exception, Label::kDeferred);

  // Let iterator be GetIterator(iterable).
  // IfAbruptRejectPromise(iterator, promiseCapability).
  IteratorRecord iterator = iter_assembler.GetIterator(
      context, iterable, &reject_promise, &var_exception);

  // Let result be PerformPromiseAll(iteratorRecord, C, promiseCapability).
  // If result is an abrupt completion, then
  //   If iteratorRecord.[[Done]] is false, let result be
  //       IteratorClose(iterator, result).
  //    IfAbruptRejectPromise(result, promiseCapability).
  Node* const result = PerformPromiseAll(
      context, receiver, capability, iterator, create_resolve_element_function,
      create_reject_element_function, &reject_promise, &var_exception);

  Return(result);

  BIND(&reject_promise);
  {
    // Exception must be bound to a JS value.
    CSA_SLOW_ASSERT(this, IsNotTheHole(var_exception.value()));
    TNode<Object> const reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
           context, reject, UndefinedConstant(), var_exception.value());

    TNode<Object> const promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

// ES#sec-promise.all
// Promise.all ( iterable )
TF_BUILTIN(PromiseAll, PromiseBuiltinsAssembler) {
  TNode<Object> receiver = Cast(Parameter(Descriptor::kReceiver));
  TNode<Context> context = Cast(Parameter(Descriptor::kContext));
  TNode<Object> iterable = Cast(Parameter(Descriptor::kIterable));
  Generate_PromiseAll(
      context, receiver, iterable,
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return CreatePromiseAllResolveElementFunction(
            context, index, native_context,
            Context::PROMISE_ALL_RESOLVE_ELEMENT_SHARED_FUN);
      },
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return LoadObjectField(capability, PromiseCapability::kRejectOffset);
      });
}

// ES#sec-promise.allsettled
// Promise.allSettled ( iterable )
TF_BUILTIN(PromiseAllSettled, PromiseBuiltinsAssembler) {
  TNode<Object> receiver = Cast(Parameter(Descriptor::kReceiver));
  TNode<Context> context = Cast(Parameter(Descriptor::kContext));
  TNode<Object> iterable = Cast(Parameter(Descriptor::kIterable));
  Generate_PromiseAll(
      context, receiver, iterable,
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return CreatePromiseAllResolveElementFunction(
            context, index, native_context,
            Context::PROMISE_ALL_SETTLED_RESOLVE_ELEMENT_SHARED_FUN);
      },
      [this](TNode<Context> context, TNode<Smi> index,
             TNode<NativeContext> native_context,
             TNode<PromiseCapability> capability) {
        return CreatePromiseAllResolveElementFunction(
            context, index, native_context,
            Context::PROMISE_ALL_SETTLED_REJECT_ELEMENT_SHARED_FUN);
      });
}

void PromiseBuiltinsAssembler::Generate_PromiseAllResolveElementClosure(
    TNode<Context> context, TNode<Object> value, TNode<JSFunction> function,
    const CreatePromiseAllResolveElementFunctionValue& callback) {
  Label already_called(this, Label::kDeferred), resolve_promise(this);

  // We use the {function}s context as the marker to remember whether this
  // resolve element closure was already called. It points to the resolve
  // element context (which is a FunctionContext) until it was called the
  // first time, in which case we make it point to the native context here
  // to mark this resolve element closure as done.
  GotoIf(IsNativeContext(context), &already_called);
  CSA_ASSERT(
      this,
      SmiEqual(LoadObjectField<Smi>(context, Context::kLengthOffset),
               SmiConstant(PromiseBuiltins::kPromiseAllResolveElementLength)));
  TNode<NativeContext> native_context = LoadNativeContext(context);
  StoreObjectField(function, JSFunction::kContextOffset, native_context);

  // Update the value depending on whether Promise.all or
  // Promise.allSettled is called.
  value = callback(context, native_context, value);

  // Determine the index from the {function}.
  Label unreachable(this, Label::kDeferred);
  STATIC_ASSERT(PropertyArray::kNoHashSentinel == 0);
  TNode<IntPtrT> identity_hash =
      LoadJSReceiverIdentityHash(function, &unreachable);
  CSA_ASSERT(this, IntPtrGreaterThan(identity_hash, IntPtrConstant(0)));
  TNode<IntPtrT> index = IntPtrSub(identity_hash, IntPtrConstant(1));

  // Check if we need to grow the [[ValuesArray]] to store {value} at {index}.
  TNode<JSArray> values_array = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementValuesArraySlot));
  TNode<FixedArray> elements = CAST(LoadElements(values_array));
  TNode<IntPtrT> values_length =
      LoadAndUntagObjectField(values_array, JSArray::kLengthOffset);
  Label if_inbounds(this), if_outofbounds(this), done(this);
  Branch(IntPtrLessThan(index, values_length), &if_inbounds, &if_outofbounds);

  BIND(&if_outofbounds);
  {
    // Check if we need to grow the backing store.
    TNode<IntPtrT> new_length = IntPtrAdd(index, IntPtrConstant(1));
    TNode<IntPtrT> elements_length =
        LoadAndUntagObjectField(elements, FixedArray::kLengthOffset);
    Label if_grow(this, Label::kDeferred), if_nogrow(this);
    Branch(IntPtrLessThan(index, elements_length), &if_nogrow, &if_grow);

    BIND(&if_grow);
    {
      // We need to grow the backing store to fit the {index} as well.
      TNode<IntPtrT> new_elements_length =
          IntPtrMin(CalculateNewElementsCapacity(new_length),
                    IntPtrConstant(PropertyArray::HashField::kMax + 1));
      CSA_ASSERT(this, IntPtrLessThan(index, new_elements_length));
      CSA_ASSERT(this, IntPtrLessThan(elements_length, new_elements_length));
      TNode<FixedArray> new_elements =
          CAST(AllocateFixedArray(PACKED_ELEMENTS, new_elements_length,
                                  AllocationFlag::kAllowLargeObjectAllocation));
      CopyFixedArrayElements(PACKED_ELEMENTS, elements, PACKED_ELEMENTS,
                             new_elements, elements_length,
                             new_elements_length);
      StoreFixedArrayElement(new_elements, index, value);

      // Update backing store and "length" on {values_array}.
      StoreObjectField(values_array, JSArray::kElementsOffset, new_elements);
      StoreObjectFieldNoWriteBarrier(values_array, JSArray::kLengthOffset,
                                     SmiTag(new_length));
      Goto(&done);
    }

    BIND(&if_nogrow);
    {
      // The {index} is within bounds of the {elements} backing store, so
      // just store the {value} and update the "length" of the {values_array}.
      StoreObjectFieldNoWriteBarrier(values_array, JSArray::kLengthOffset,
                                     SmiTag(new_length));
      StoreFixedArrayElement(elements, index, value);
      Goto(&done);
    }
  }

  BIND(&if_inbounds);
  {
    // The {index} is in bounds of the {values_array},
    // just store the {value} and continue.
    StoreFixedArrayElement(elements, index, value);
    Goto(&done);
  }

  BIND(&done);
  TNode<Smi> remaining_elements_count = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementRemainingSlot));
  remaining_elements_count = SmiSub(remaining_elements_count, SmiConstant(1));
  StoreContextElement(context,
                      PromiseBuiltins::kPromiseAllResolveElementRemainingSlot,
                      remaining_elements_count);
  GotoIf(SmiEqual(remaining_elements_count, SmiConstant(0)), &resolve_promise);
  Return(UndefinedConstant());

  BIND(&resolve_promise);
  TNode<PromiseCapability> capability = CAST(LoadContextElement(
      context, PromiseBuiltins::kPromiseAllResolveElementCapabilitySlot));
  TNode<Object> resolve =
      LoadObjectField(capability, PromiseCapability::kResolveOffset);
  CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
         context, resolve, UndefinedConstant(), values_array);
  Return(UndefinedConstant());

  BIND(&already_called);
  Return(UndefinedConstant());

  BIND(&unreachable);
  Unreachable();
}

TF_BUILTIN(PromiseAllResolveElementClosure, PromiseBuiltinsAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  Generate_PromiseAllResolveElementClosure(
      context, value, function,
      [](TNode<Object>, TNode<NativeContext>, TNode<Object> value) {
        return value;
      });
}

TF_BUILTIN(PromiseAllSettledResolveElementClosure, PromiseBuiltinsAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  Generate_PromiseAllResolveElementClosure(
      context, value, function,
      [this](TNode<Context> context, TNode<NativeContext> native_context,
             TNode<Object> value) {
        // TODO(gsathya): Optimize the creation using a cached map to
        // prevent transitions here.
        // 9. Let obj be ! ObjectCreate(%ObjectPrototype%).
        TNode<HeapObject> object_function = Cast(
            LoadContextElement(native_context, Context::OBJECT_FUNCTION_INDEX));
        TNode<Map> object_function_map = Cast(LoadObjectField(
            object_function, JSFunction::kPrototypeOrInitialMapOffset));
        TNode<JSObject> obj = AllocateJSObjectFromMap(object_function_map);

        // 10. Perform ! CreateDataProperty(obj, "status", "fulfilled").
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("status"), StringConstant("fulfilled"));

        // 11. Perform ! CreateDataProperty(obj, "value", x).
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("value"), value);

        return obj;
      });
}

TF_BUILTIN(PromiseAllSettledRejectElementClosure, PromiseBuiltinsAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  Generate_PromiseAllResolveElementClosure(
      context, value, function,
      [this](TNode<Context> context, TNode<NativeContext> native_context,
             TNode<Object> value) {
        // TODO(gsathya): Optimize the creation using a cached map to
        // prevent transitions here.
        // 9. Let obj be ! ObjectCreate(%ObjectPrototype%).
        TNode<HeapObject> object_function = Cast(
            LoadContextElement(native_context, Context::OBJECT_FUNCTION_INDEX));
        TNode<Map> object_function_map = Cast(LoadObjectField(
            object_function, JSFunction::kPrototypeOrInitialMapOffset));
        TNode<JSObject> obj = AllocateJSObjectFromMap(object_function_map);

        // 10. Perform ! CreateDataProperty(obj, "status", "rejected").
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("status"), StringConstant("rejected"));

        // 11. Perform ! CreateDataProperty(obj, "reason", x).
        CallBuiltin(Builtins::kFastCreateDataProperty, context, obj,
                    StringConstant("reason"), value);

        return obj;
      });
}

// ES#sec-promise.race
// Promise.race ( iterable )
TF_BUILTIN(PromiseRace, PromiseBuiltinsAssembler) {
  IteratorBuiltinsAssembler iter_assembler(state());
  VARIABLE(var_exception, MachineRepresentation::kTagged, TheHoleConstant());

  Node* const receiver = Parameter(Descriptor::kReceiver);
  TNode<Context> const context = CAST(Parameter(Descriptor::kContext));
  ThrowIfNotJSReceiver(context, CAST(receiver),
                       MessageTemplate::kCalledOnNonObject, "Promise.race");

  // Let promiseCapability be ? NewPromiseCapability(C).
  // Don't fire debugEvent so that forwarding the rejection through all does not
  // trigger redundant ExceptionEvents
  TNode<Oddball> const debug_event = FalseConstant();
  TNode<PromiseCapability> const capability = CAST(CallBuiltin(
      Builtins::kNewPromiseCapability, context, receiver, debug_event));

  TNode<Object> const resolve =
      LoadObjectField(capability, PromiseCapability::kResolveOffset);
  TNode<Object> const reject =
      LoadObjectField(capability, PromiseCapability::kRejectOffset);

  Label close_iterator(this, Label::kDeferred);
  Label reject_promise(this, Label::kDeferred);

  // For catch prediction, don't treat the .then calls as handling it;
  // instead, recurse outwards.
  SetForwardingHandlerIfTrue(context, IsDebugActive(), reject);

  // Let iterator be GetIterator(iterable).
  // IfAbruptRejectPromise(iterator, promiseCapability).
  Node* const iterable = Parameter(Descriptor::kIterable);
  IteratorRecord iterator = iter_assembler.GetIterator(
      context, iterable, &reject_promise, &var_exception);

  // Let result be PerformPromiseRace(iteratorRecord, C, promiseCapability).
  {
    // We can skip the "resolve" lookup on {constructor} if it's the
    // Promise constructor and the Promise.resolve protector is intact,
    // as that guards the lookup path for the "resolve" property on the
    // Promise constructor.
    Label loop(this), break_loop(this), if_slow(this, Label::kDeferred);
    TNode<NativeContext> const native_context = LoadNativeContext(context);
    TVARIABLE(Object, var_promise_resolve_function, UndefinedConstant());
    GotoIfNotPromiseResolveLookupChainIntact(native_context, receiver,
                                             &if_slow);
    Goto(&loop);

    BIND(&if_slow);
    {
      // 3. Let _promiseResolve_ be ? Get(_constructor_, `"resolve"`).
      TNode<Object> resolve =
          GetProperty(native_context, receiver, factory()->resolve_string());
      GotoIfException(resolve, &close_iterator, &var_exception);

      // 4. If IsCallable(_promiseResolve_) is *false*, throw a *TypeError*
      // exception.
      ThrowIfNotCallable(context, resolve, "resolve");

      var_promise_resolve_function = resolve;
      Goto(&loop);
    }

    BIND(&loop);
    {
      TNode<Map> const fast_iterator_result_map = CAST(LoadContextElement(
          native_context, Context::ITERATOR_RESULT_MAP_INDEX));

      // Let next be IteratorStep(iteratorRecord.[[Iterator]]).
      // If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
      // ReturnIfAbrupt(next).
      TNode<JSReceiver> const next = iter_assembler.IteratorStep(
          context, iterator, &break_loop, fast_iterator_result_map,
          &reject_promise, &var_exception);

      // Let nextValue be IteratorValue(next).
      // If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to
      //     true.
      // ReturnIfAbrupt(nextValue).
      TNode<Object> const next_value =
          iter_assembler.IteratorValue(context, next, fast_iterator_result_map,
                                       &reject_promise, &var_exception);

      // Let nextPromise be ? Call(constructor, _promiseResolve_, « nextValue
      // »).
      Node* const next_promise = CallResolve(
          native_context, receiver, var_promise_resolve_function.value(),
          next_value, &close_iterator, &var_exception);

      // Perform ? Invoke(nextPromise, "then", « resolveElement,
      //                  resultCapability.[[Reject]] »).
      TNode<Object> const then =
          GetProperty(context, next_promise, factory()->then_string());
      GotoIfException(then, &close_iterator, &var_exception);

      Node* const then_call =
          CallJS(CodeFactory::Call(isolate(),
                                   ConvertReceiverMode::kNotNullOrUndefined),
                 context, then, next_promise, resolve, reject);
      GotoIfException(then_call, &close_iterator, &var_exception);

      // For catch prediction, mark that rejections here are semantically
      // handled by the combined Promise.
      SetPromiseHandledByIfTrue(context, IsDebugActive(), then_call, [=]() {
        // Load promiseCapability.[[Promise]]
        return LoadObjectField(capability, PromiseCapability::kPromiseOffset);
      });
      Goto(&loop);
    }

    BIND(&break_loop);
    Return(LoadObjectField(capability, PromiseCapability::kPromiseOffset));
  }

  BIND(&close_iterator);
  {
    CSA_ASSERT(this, IsNotTheHole(var_exception.value()));
    iter_assembler.IteratorCloseOnException(context, iterator, &reject_promise,
                                            &var_exception);
  }

  BIND(&reject_promise);
  {
    TNode<Object> const reject =
        LoadObjectField(capability, PromiseCapability::kRejectOffset);
    CallJS(CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
           context, reject, UndefinedConstant(), var_exception.value());

    TNode<Object> const promise =
        LoadObjectField(capability, PromiseCapability::kPromiseOffset);
    Return(promise);
  }
}

}  // namespace internal
}  // namespace v8
