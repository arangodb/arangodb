// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/contexts.h"

#include "src/ast/modules.h"
#include "src/bootstrapper.h"
#include "src/debug/debug.h"
#include "src/isolate-inl.h"
#include "src/objects/module-inl.h"

namespace v8 {
namespace internal {


Handle<ScriptContextTable> ScriptContextTable::Extend(
    Handle<ScriptContextTable> table, Handle<Context> script_context) {
  Handle<ScriptContextTable> result;
  int used = table->used();
  int length = table->length();
  CHECK(used >= 0 && length > 0 && used < length);
  if (used + kFirstContextSlotIndex == length) {
    CHECK(length < Smi::kMaxValue / 2);
    Isolate* isolate = script_context->GetIsolate();
    Handle<FixedArray> copy =
        isolate->factory()->CopyFixedArrayAndGrow(table, length);
    copy->set_map(ReadOnlyRoots(isolate).script_context_table_map());
    result = Handle<ScriptContextTable>::cast(copy);
  } else {
    result = table;
  }
  result->set_used(used + 1);

  DCHECK(script_context->IsScriptContext());
  result->set(used + kFirstContextSlotIndex, *script_context);
  return result;
}

bool ScriptContextTable::Lookup(Isolate* isolate,
                                Handle<ScriptContextTable> table,
                                Handle<String> name, LookupResult* result) {
  for (int i = 0; i < table->used(); i++) {
    Handle<Context> context = GetContext(isolate, table, i);
    DCHECK(context->IsScriptContext());
    Handle<ScopeInfo> scope_info(context->scope_info(), context->GetIsolate());
    int slot_index = ScopeInfo::ContextSlotIndex(
        scope_info, name, &result->mode, &result->init_flag,
        &result->maybe_assigned_flag);

    if (slot_index >= 0) {
      result->context_index = i;
      result->slot_index = slot_index;
      return true;
    }
  }
  return false;
}


bool Context::is_declaration_context() {
  if (IsFunctionContext() || IsNativeContext() || IsScriptContext() ||
      IsModuleContext()) {
    return true;
  }
  if (IsEvalContext()) {
    return scope_info()->language_mode() == LanguageMode::kStrict;
  }
  if (!IsBlockContext()) return false;
  return scope_info()->is_declaration_scope();
}


Context* Context::declaration_context() {
  Context* current = this;
  while (!current->is_declaration_context()) {
    current = current->previous();
  }
  return current;
}

Context* Context::closure_context() {
  Context* current = this;
  while (!current->IsFunctionContext() && !current->IsScriptContext() &&
         !current->IsModuleContext() && !current->IsNativeContext() &&
         !current->IsEvalContext()) {
    current = current->previous();
  }
  return current;
}

JSObject* Context::extension_object() {
  DCHECK(IsNativeContext() || IsFunctionContext() || IsBlockContext() ||
         IsEvalContext() || IsCatchContext());
  HeapObject* object = extension();
  if (object->IsTheHole()) return nullptr;
  DCHECK(object->IsJSContextExtensionObject() ||
         (IsNativeContext() && object->IsJSGlobalObject()));
  return JSObject::cast(object);
}

JSReceiver* Context::extension_receiver() {
  DCHECK(IsNativeContext() || IsWithContext() || IsEvalContext() ||
         IsFunctionContext() || IsBlockContext());
  return IsWithContext() ? JSReceiver::cast(extension()) : extension_object();
}

ScopeInfo* Context::scope_info() {
  return ScopeInfo::cast(get(SCOPE_INFO_INDEX));
}

Module* Context::module() {
  Context* current = this;
  while (!current->IsModuleContext()) {
    current = current->previous();
  }
  return Module::cast(current->extension());
}

JSGlobalObject* Context::global_object() {
  return JSGlobalObject::cast(native_context()->extension());
}


Context* Context::script_context() {
  Context* current = this;
  while (!current->IsScriptContext()) {
    current = current->previous();
  }
  return current;
}

JSGlobalProxy* Context::global_proxy() {
  return native_context()->global_proxy_object();
}

void Context::set_global_proxy(JSGlobalProxy* object) {
  native_context()->set_global_proxy_object(object);
}


/**
 * Lookups a property in an object environment, taking the unscopables into
 * account. This is used For HasBinding spec algorithms for ObjectEnvironment.
 */
static Maybe<bool> UnscopableLookup(LookupIterator* it) {
  Isolate* isolate = it->isolate();

  Maybe<bool> found = JSReceiver::HasProperty(it);
  if (found.IsNothing() || !found.FromJust()) return found;

  Handle<Object> unscopables;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, unscopables,
      JSReceiver::GetProperty(isolate,
                              Handle<JSReceiver>::cast(it->GetReceiver()),
                              isolate->factory()->unscopables_symbol()),
      Nothing<bool>());
  if (!unscopables->IsJSReceiver()) return Just(true);
  Handle<Object> blacklist;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, blacklist,
      JSReceiver::GetProperty(isolate, Handle<JSReceiver>::cast(unscopables),
                              it->name()),
      Nothing<bool>());
  return Just(!blacklist->BooleanValue(isolate));
}

static PropertyAttributes GetAttributesForMode(VariableMode mode) {
  DCHECK(IsDeclaredVariableMode(mode));
  return mode == VariableMode::kConst ? READ_ONLY : NONE;
}

Handle<Object> Context::Lookup(Handle<String> name, ContextLookupFlags flags,
                               int* index, PropertyAttributes* attributes,
                               InitializationFlag* init_flag,
                               VariableMode* variable_mode,
                               bool* is_sloppy_function_name) {
  Isolate* isolate = GetIsolate();
  Handle<Context> context(this, isolate);

  bool follow_context_chain = (flags & FOLLOW_CONTEXT_CHAIN) != 0;
  bool failed_whitelist = false;
  *index = kNotFound;
  *attributes = ABSENT;
  *init_flag = kCreatedInitialized;
  *variable_mode = VariableMode::kVar;
  if (is_sloppy_function_name != nullptr) {
    *is_sloppy_function_name = false;
  }

  if (FLAG_trace_contexts) {
    PrintF("Context::Lookup(");
    name->ShortPrint();
    PrintF(")\n");
  }

  do {
    if (FLAG_trace_contexts) {
      PrintF(" - looking in context %p", reinterpret_cast<void*>(*context));
      if (context->IsScriptContext()) PrintF(" (script context)");
      if (context->IsNativeContext()) PrintF(" (native context)");
      PrintF("\n");
    }

    // 1. Check global objects, subjects of with, and extension objects.
    DCHECK_IMPLIES(context->IsEvalContext(),
                   context->extension()->IsTheHole(isolate));
    if ((context->IsNativeContext() ||
         (context->IsWithContext() && ((flags & SKIP_WITH_CONTEXT) == 0)) ||
         context->IsFunctionContext() || context->IsBlockContext()) &&
        context->extension_receiver() != nullptr) {
      Handle<JSReceiver> object(context->extension_receiver(), isolate);

      if (context->IsNativeContext()) {
        if (FLAG_trace_contexts) {
          PrintF(" - trying other script contexts\n");
        }
        // Try other script contexts.
        Handle<ScriptContextTable> script_contexts(
            context->global_object()->native_context()->script_context_table(),
            isolate);
        ScriptContextTable::LookupResult r;
        if (ScriptContextTable::Lookup(isolate, script_contexts, name, &r)) {
          if (FLAG_trace_contexts) {
            Handle<Context> c = ScriptContextTable::GetContext(
                isolate, script_contexts, r.context_index);
            PrintF("=> found property in script context %d: %p\n",
                   r.context_index, reinterpret_cast<void*>(*c));
          }
          *index = r.slot_index;
          *variable_mode = r.mode;
          *init_flag = r.init_flag;
          *attributes = GetAttributesForMode(r.mode);
          return ScriptContextTable::GetContext(isolate, script_contexts,
                                                r.context_index);
        }
      }

      // Context extension objects needs to behave as if they have no
      // prototype.  So even if we want to follow prototype chains, we need
      // to only do a local lookup for context extension objects.
      Maybe<PropertyAttributes> maybe = Nothing<PropertyAttributes>();
      if ((flags & FOLLOW_PROTOTYPE_CHAIN) == 0 ||
          object->IsJSContextExtensionObject()) {
        maybe = JSReceiver::GetOwnPropertyAttributes(object, name);
      } else if (context->IsWithContext()) {
        // A with context will never bind "this", but debug-eval may look into
        // a with context when resolving "this". Other synthetic variables such
        // as new.target may be resolved as VariableMode::kDynamicLocal due to
        // bug v8:5405 , skipping them here serves as a workaround until a more
        // thorough fix can be applied.
        // TODO(v8:5405): Replace this check with a DCHECK when resolution of
        // of synthetic variables does not go through this code path.
        if (ScopeInfo::VariableIsSynthetic(*name)) {
          maybe = Just(ABSENT);
        } else {
          LookupIterator it(object, name, object);
          Maybe<bool> found = UnscopableLookup(&it);
          if (found.IsNothing()) {
            maybe = Nothing<PropertyAttributes>();
          } else {
            // Luckily, consumers of |maybe| only care whether the property
            // was absent or not, so we can return a dummy |NONE| value
            // for its attributes when it was present.
            maybe = Just(found.FromJust() ? NONE : ABSENT);
          }
        }
      } else {
        maybe = JSReceiver::GetPropertyAttributes(object, name);
      }

      if (maybe.IsNothing()) return Handle<Object>();
      DCHECK(!isolate->has_pending_exception());
      *attributes = maybe.FromJust();

      if (maybe.FromJust() != ABSENT) {
        if (FLAG_trace_contexts) {
          PrintF("=> found property in context object %p\n",
                 reinterpret_cast<void*>(*object));
        }
        return object;
      }
    }

    // 2. Check the context proper if it has slots.
    if (context->IsFunctionContext() || context->IsBlockContext() ||
        context->IsScriptContext() || context->IsEvalContext() ||
        context->IsModuleContext() || context->IsCatchContext()) {
      // Use serialized scope information of functions and blocks to search
      // for the context index.
      Handle<ScopeInfo> scope_info(context->scope_info(), isolate);
      VariableMode mode;
      InitializationFlag flag;
      MaybeAssignedFlag maybe_assigned_flag;
      int slot_index = ScopeInfo::ContextSlotIndex(scope_info, name, &mode,
                                                   &flag, &maybe_assigned_flag);
      DCHECK(slot_index < 0 || slot_index >= MIN_CONTEXT_SLOTS);
      if (slot_index >= 0) {
        if (FLAG_trace_contexts) {
          PrintF("=> found local in context slot %d (mode = %hhu)\n",
                 slot_index, static_cast<uint8_t>(mode));
        }
        *index = slot_index;
        *variable_mode = mode;
        *init_flag = flag;
        *attributes = GetAttributesForMode(mode);
        return context;
      }

      // Check the slot corresponding to the intermediate context holding
      // only the function name variable. It's conceptually (and spec-wise)
      // in an outer scope of the function's declaration scope.
      if (follow_context_chain && (flags & STOP_AT_DECLARATION_SCOPE) == 0 &&
          context->IsFunctionContext()) {
        int function_index = scope_info->FunctionContextSlotIndex(*name);
        if (function_index >= 0) {
          if (FLAG_trace_contexts) {
            PrintF("=> found intermediate function in context slot %d\n",
                   function_index);
          }
          *index = function_index;
          *attributes = READ_ONLY;
          *init_flag = kCreatedInitialized;
          *variable_mode = VariableMode::kConst;
          if (is_sloppy_function_name != nullptr &&
              is_sloppy(scope_info->language_mode())) {
            *is_sloppy_function_name = true;
          }
          return context;
        }
      }

      // Lookup variable in module imports and exports.
      if (context->IsModuleContext()) {
        VariableMode mode;
        InitializationFlag flag;
        MaybeAssignedFlag maybe_assigned_flag;
        int cell_index =
            scope_info->ModuleIndex(name, &mode, &flag, &maybe_assigned_flag);
        if (cell_index != 0) {
          if (FLAG_trace_contexts) {
            PrintF("=> found in module imports or exports\n");
          }
          *index = cell_index;
          *variable_mode = mode;
          *init_flag = flag;
          *attributes = ModuleDescriptor::GetCellIndexKind(cell_index) ==
                                ModuleDescriptor::kExport
                            ? GetAttributesForMode(mode)
                            : READ_ONLY;
          return handle(context->module(), isolate);
        }
      }
    } else if (context->IsDebugEvaluateContext()) {
      // Check materialized locals.
      Object* ext = context->get(EXTENSION_INDEX);
      if (ext->IsJSReceiver()) {
        Handle<JSReceiver> extension(JSReceiver::cast(ext), isolate);
        LookupIterator it(extension, name, extension);
        Maybe<bool> found = JSReceiver::HasProperty(&it);
        if (found.FromMaybe(false)) {
          *attributes = NONE;
          return extension;
        }
      }
      // Check the original context, but do not follow its context chain.
      Object* obj = context->get(WRAPPED_CONTEXT_INDEX);
      if (obj->IsContext()) {
        Handle<Object> result =
            Context::cast(obj)->Lookup(name, DONT_FOLLOW_CHAINS, index,
                                       attributes, init_flag, variable_mode);
        if (!result.is_null()) return result;
      }
      // Check whitelist. Names that do not pass whitelist shall only resolve
      // to with, script or native contexts up the context chain.
      obj = context->get(WHITE_LIST_INDEX);
      if (obj->IsStringSet()) {
        failed_whitelist =
            failed_whitelist || !StringSet::cast(obj)->Has(isolate, name);
      }
    }

    // 3. Prepare to continue with the previous (next outermost) context.
    if (context->IsNativeContext() ||
        ((flags & STOP_AT_DECLARATION_SCOPE) != 0 &&
         context->is_declaration_context())) {
      follow_context_chain = false;
    } else {
      do {
        context = Handle<Context>(context->previous(), isolate);
        // If we come across a whitelist context, and the name is not
        // whitelisted, then only consider with, script, module or native
        // contexts.
      } while (failed_whitelist && !context->IsScriptContext() &&
               !context->IsNativeContext() && !context->IsWithContext() &&
               !context->IsModuleContext());
    }
  } while (follow_context_chain);

  if (FLAG_trace_contexts) {
    PrintF("=> no property/slot found\n");
  }
  return Handle<Object>::null();
}


void Context::AddOptimizedCode(Code* code) {
  DCHECK(IsNativeContext());
  DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(code->next_code_link()->IsUndefined());
  code->set_next_code_link(get(OPTIMIZED_CODE_LIST));
  set(OPTIMIZED_CODE_LIST, code, UPDATE_WEAK_WRITE_BARRIER);
}


void Context::SetOptimizedCodeListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(OPTIMIZED_CODE_LIST, head, UPDATE_WEAK_WRITE_BARRIER);
}


Object* Context::OptimizedCodeListHead() {
  DCHECK(IsNativeContext());
  return get(OPTIMIZED_CODE_LIST);
}


void Context::SetDeoptimizedCodeListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(DEOPTIMIZED_CODE_LIST, head, UPDATE_WEAK_WRITE_BARRIER);
}


Object* Context::DeoptimizedCodeListHead() {
  DCHECK(IsNativeContext());
  return get(DEOPTIMIZED_CODE_LIST);
}


Handle<Object> Context::ErrorMessageForCodeGenerationFromStrings() {
  Isolate* isolate = GetIsolate();
  Handle<Object> result(error_message_for_code_gen_from_strings(), isolate);
  if (!result->IsUndefined(isolate)) return result;
  return isolate->factory()->NewStringFromStaticChars(
      "Code generation from strings disallowed for this context");
}


#define COMPARE_NAME(index, type, name) \
  if (string->IsOneByteEqualTo(STATIC_CHAR_VECTOR(#name))) return index;

int Context::ImportedFieldIndexForName(Handle<String> string) {
  NATIVE_CONTEXT_IMPORTED_FIELDS(COMPARE_NAME)
  return kNotFound;
}


int Context::IntrinsicIndexForName(Handle<String> string) {
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(COMPARE_NAME);
  return kNotFound;
}

#undef COMPARE_NAME

#define COMPARE_NAME(index, type, name) \
  if (strncmp(string, #name, length) == 0) return index;

int Context::IntrinsicIndexForName(const unsigned char* unsigned_string,
                                   int length) {
  const char* string = reinterpret_cast<const char*>(unsigned_string);
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(COMPARE_NAME);
  return kNotFound;
}

#undef COMPARE_NAME

#ifdef DEBUG

bool Context::IsBootstrappingOrNativeContext(Isolate* isolate, Object* object) {
  // During bootstrapping we allow all objects to pass as global
  // objects. This is necessary to fix circular dependencies.
  return isolate->heap()->gc_state() != Heap::NOT_IN_GC ||
         isolate->bootstrapper()->IsActive() || object->IsNativeContext();
}


bool Context::IsBootstrappingOrValidParentContext(
    Object* object, Context* child) {
  // During bootstrapping we allow all objects to pass as
  // contexts. This is necessary to fix circular dependencies.
  if (child->GetIsolate()->bootstrapper()->IsActive()) return true;
  if (!object->IsContext()) return false;
  Context* context = Context::cast(object);
  return context->IsNativeContext() || context->IsScriptContext() ||
         context->IsModuleContext() || !child->IsModuleContext();
}

#endif

void Context::ResetErrorsThrown() {
  DCHECK(IsNativeContext());
  set_errors_thrown(Smi::FromInt(0));
}

void Context::IncrementErrorsThrown() {
  DCHECK(IsNativeContext());

  int previous_value = errors_thrown()->value();
  set_errors_thrown(Smi::FromInt(previous_value + 1));
}


int Context::GetErrorsThrown() { return errors_thrown()->value(); }

}  // namespace internal
}  // namespace v8
