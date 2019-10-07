// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"

#include "src/accessors.h"
#include "src/api-inl.h"
#include "src/api-natives.h"
#include "src/base/ieee754.h"
#include "src/code-stubs.h"
#include "src/compiler.h"
#include "src/debug/debug.h"
#include "src/extensions/externalize-string-extension.h"
#include "src/extensions/free-buffer-extension.h"
#include "src/extensions/gc-extension.h"
#include "src/extensions/ignition-statistics-extension.h"
#include "src/extensions/statistics-extension.h"
#include "src/extensions/trigger-failure-extension.h"
#include "src/heap/heap.h"
#include "src/isolate-inl.h"
#include "src/math-random.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments.h"
#include "src/objects/builtin-function-id.h"
#include "src/objects/hash-table-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#include "src/objects/js-date-time-format.h"
#include "src/objects/js-list-format.h"
#include "src/objects/js-locale.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-string-iterator.h"
#include "src/objects/js-regexp.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segmenter.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/templates.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-js.h"

namespace v8 {
namespace internal {

void SourceCodeCache::Initialize(Isolate* isolate, bool create_heap_objects) {
  cache_ = create_heap_objects ? ReadOnlyRoots(isolate).empty_fixed_array()
                               : nullptr;
}

bool SourceCodeCache::Lookup(Isolate* isolate, Vector<const char> name,
                             Handle<SharedFunctionInfo>* handle) {
  for (int i = 0; i < cache_->length(); i += 2) {
    SeqOneByteString* str = SeqOneByteString::cast(cache_->get(i));
    if (str->IsUtf8EqualTo(name)) {
      *handle = Handle<SharedFunctionInfo>(
          SharedFunctionInfo::cast(cache_->get(i + 1)), isolate);
      return true;
    }
  }
  return false;
}

void SourceCodeCache::Add(Isolate* isolate, Vector<const char> name,
                          Handle<SharedFunctionInfo> shared) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  int length = cache_->length();
  Handle<FixedArray> new_array = factory->NewFixedArray(length + 2, TENURED);
  cache_->CopyTo(0, *new_array, 0, cache_->length());
  cache_ = *new_array;
  Handle<String> str =
      factory->NewStringFromOneByte(Vector<const uint8_t>::cast(name), TENURED)
          .ToHandleChecked();
  DCHECK(!str.is_null());
  cache_->set(length, *str);
  cache_->set(length + 1, *shared);
  Script::cast(shared->script())->set_type(type_);
}

Bootstrapper::Bootstrapper(Isolate* isolate)
    : isolate_(isolate),
      nesting_(0),
      extensions_cache_(Script::TYPE_EXTENSION) {}

Handle<String> Bootstrapper::GetNativeSource(NativeType type, int index) {
  NativesExternalStringResource* resource =
      new NativesExternalStringResource(type, index);
  Handle<ExternalOneByteString> source_code =
      isolate_->factory()->NewNativeSourceString(resource);
  DCHECK(source_code->is_uncached());
  return source_code;
}

void Bootstrapper::Initialize(bool create_heap_objects) {
  extensions_cache_.Initialize(isolate_, create_heap_objects);
}


static const char* GCFunctionName() {
  bool flag_given =
      FLAG_expose_gc_as != nullptr && strlen(FLAG_expose_gc_as) != 0;
  return flag_given ? FLAG_expose_gc_as : "gc";
}

v8::Extension* Bootstrapper::free_buffer_extension_ = nullptr;
v8::Extension* Bootstrapper::gc_extension_ = nullptr;
v8::Extension* Bootstrapper::externalize_string_extension_ = nullptr;
v8::Extension* Bootstrapper::statistics_extension_ = nullptr;
v8::Extension* Bootstrapper::trigger_failure_extension_ = nullptr;
v8::Extension* Bootstrapper::ignition_statistics_extension_ = nullptr;

void Bootstrapper::InitializeOncePerProcess() {
  free_buffer_extension_ = new FreeBufferExtension;
  v8::RegisterExtension(free_buffer_extension_);
  gc_extension_ = new GCExtension(GCFunctionName());
  v8::RegisterExtension(gc_extension_);
  externalize_string_extension_ = new ExternalizeStringExtension;
  v8::RegisterExtension(externalize_string_extension_);
  statistics_extension_ = new StatisticsExtension;
  v8::RegisterExtension(statistics_extension_);
  trigger_failure_extension_ = new TriggerFailureExtension;
  v8::RegisterExtension(trigger_failure_extension_);
  ignition_statistics_extension_ = new IgnitionStatisticsExtension;
  v8::RegisterExtension(ignition_statistics_extension_);
}


void Bootstrapper::TearDownExtensions() {
  delete free_buffer_extension_;
  free_buffer_extension_ = nullptr;
  delete gc_extension_;
  gc_extension_ = nullptr;
  delete externalize_string_extension_;
  externalize_string_extension_ = nullptr;
  delete statistics_extension_;
  statistics_extension_ = nullptr;
  delete trigger_failure_extension_;
  trigger_failure_extension_ = nullptr;
  delete ignition_statistics_extension_;
  ignition_statistics_extension_ = nullptr;
}

void Bootstrapper::TearDown() {
  extensions_cache_.Initialize(isolate_, false);  // Yes, symmetrical
}

class Genesis {
 public:
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template,
          size_t context_snapshot_index,
          v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
          GlobalContextType context_type);
  Genesis(Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
          v8::Local<v8::ObjectTemplate> global_proxy_template);
  ~Genesis() = default;

  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate_->factory(); }
  Builtins* builtins() const { return isolate_->builtins(); }
  Heap* heap() const { return isolate_->heap(); }

  Handle<Context> result() { return result_; }

  Handle<JSGlobalProxy> global_proxy() { return global_proxy_; }

 private:
  Handle<NativeContext> native_context() { return native_context_; }

  // Creates some basic objects. Used for creating a context from scratch.
  void CreateRoots();
  // Creates the empty function.  Used for creating a context from scratch.
  Handle<JSFunction> CreateEmptyFunction();
  // Returns the %ThrowTypeError% intrinsic function.
  // See ES#sec-%throwtypeerror% for details.
  Handle<JSFunction> GetThrowTypeErrorIntrinsic();

  void CreateSloppyModeFunctionMaps(Handle<JSFunction> empty);
  void CreateStrictModeFunctionMaps(Handle<JSFunction> empty);
  void CreateObjectFunction(Handle<JSFunction> empty);
  void CreateIteratorMaps(Handle<JSFunction> empty);
  void CreateAsyncIteratorMaps(Handle<JSFunction> empty);
  void CreateAsyncFunctionMaps(Handle<JSFunction> empty);
  void CreateJSProxyMaps();

  // Make the "arguments" and "caller" properties throw a TypeError on access.
  void AddRestrictedFunctionProperties(Handle<JSFunction> empty);

  // Creates the global objects using the global proxy and the template passed
  // in through the API.  We call this regardless of whether we are building a
  // context from scratch or using a deserialized one from the partial snapshot
  // but in the latter case we don't use the objects it produces directly, as
  // we have to use the deserialized ones that are linked together with the
  // rest of the context snapshot. At the end we link the global proxy and the
  // context to each other.
  Handle<JSGlobalObject> CreateNewGlobals(
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      Handle<JSGlobalProxy> global_proxy);
  // Similarly, we want to use the global that has been created by the templates
  // passed through the API.  The global from the snapshot is detached from the
  // other objects in the snapshot.
  void HookUpGlobalObject(Handle<JSGlobalObject> global_object);
  // Hooks the given global proxy into the context in the case we do not
  // replace the global object from the deserialized native context.
  void HookUpGlobalProxy(Handle<JSGlobalProxy> global_proxy);
  // The native context has a ScriptContextTable that store declarative bindings
  // made in script scopes.  Add a "this" binding to that table pointing to the
  // global proxy.
  void InstallGlobalThisBinding();
  // New context initialization.  Used for creating a context from scratch.
  void InitializeGlobal(Handle<JSGlobalObject> global_object,
                        Handle<JSFunction> empty_function,
                        GlobalContextType context_type);
  void InitializeExperimentalGlobal();
  // Depending on the situation, expose and/or get rid of the utils object.
  void ConfigureUtilsObject(GlobalContextType context_type);

#define DECLARE_FEATURE_INITIALIZATION(id, descr) \
  void InitializeGlobal_##id();

  HARMONY_INPROGRESS(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_STAGED(DECLARE_FEATURE_INITIALIZATION)
  HARMONY_SHIPPING(DECLARE_FEATURE_INITIALIZATION)
#undef DECLARE_FEATURE_INITIALIZATION

  enum ArrayBufferKind {
    ARRAY_BUFFER,
    SHARED_ARRAY_BUFFER,
  };
  Handle<JSFunction> CreateArrayBuffer(Handle<String> name,
                                       ArrayBufferKind array_buffer_kind);
  Handle<JSFunction> InstallInternalArray(Handle<JSObject> target,
                                          const char* name,
                                          ElementsKind elements_kind);
  bool InstallNatives(GlobalContextType context_type);

  Handle<JSFunction> InstallTypedArray(const char* name,
                                       ElementsKind elements_kind);
  bool InstallExtraNatives();
  bool InstallExperimentalExtraNatives();
  bool InstallDebuggerNatives();
  void InstallBuiltinFunctionIds();
  void InstallExperimentalBuiltinFunctionIds();
  void InitializeNormalizedMapCaches();

  enum ExtensionTraversalState {
    UNVISITED, VISITED, INSTALLED
  };

  class ExtensionStates {
   public:
    ExtensionStates();
    ExtensionTraversalState get_state(RegisteredExtension* extension);
    void set_state(RegisteredExtension* extension,
                   ExtensionTraversalState state);
   private:
    base::HashMap map_;
    DISALLOW_COPY_AND_ASSIGN(ExtensionStates);
  };

  // Used both for deserialized and from-scratch contexts to add the extensions
  // provided.
  static bool InstallExtensions(Isolate* isolate,
                                Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions);
  static bool InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states);
  static bool InstallRequestedExtensions(Isolate* isolate,
                                         v8::ExtensionConfiguration* extensions,
                                         ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate,
                               const char* name,
                               ExtensionStates* extension_states);
  static bool InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states);
  static bool InstallSpecialObjects(Isolate* isolate,
                                    Handle<Context> native_context);
  bool ConfigureApiObject(Handle<JSObject> object,
                          Handle<ObjectTemplateInfo> object_template);
  bool ConfigureGlobalObjects(
      v8::Local<v8::ObjectTemplate> global_proxy_template);

  // Migrates all properties from the 'from' object to the 'to'
  // object and overrides the prototype in 'to' with the one from
  // 'from'.
  void TransferObject(Handle<JSObject> from, Handle<JSObject> to);
  void TransferNamedProperties(Handle<JSObject> from, Handle<JSObject> to);
  void TransferIndexedProperties(Handle<JSObject> from, Handle<JSObject> to);

  static bool CallUtilsFunction(Isolate* isolate, const char* name);

  static bool CompileExtension(Isolate* isolate, v8::Extension* extension);

  Isolate* isolate_;
  Handle<Context> result_;
  Handle<NativeContext> native_context_;
  Handle<JSGlobalProxy> global_proxy_;

  // Temporary function maps needed only during bootstrapping.
  Handle<Map> strict_function_with_home_object_map_;
  Handle<Map> strict_function_with_name_and_home_object_map_;

  // %ThrowTypeError%. See ES#sec-%throwtypeerror% for details.
  Handle<JSFunction> restricted_properties_thrower_;

  BootstrapperActive active_;
  friend class Bootstrapper;
};

void Bootstrapper::Iterate(RootVisitor* v) {
  extensions_cache_.Iterate(v);
  v->Synchronize(VisitorSynchronization::kExtensions);
}

Handle<Context> Bootstrapper::CreateEnvironment(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
    GlobalContextType context_type) {
  HandleScope scope(isolate_);
  Handle<Context> env;
  {
    Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template,
                    context_snapshot_index, embedder_fields_deserializer,
                    context_type);
    env = genesis.result();
    if (env.is_null() || !InstallExtensions(env, extensions)) {
      return Handle<Context>();
    }
  }
  // Log all maps created during bootstrapping.
  if (FLAG_trace_maps) LOG(isolate_, LogMaps());
  return scope.CloseAndEscape(env);
}

Handle<JSGlobalProxy> Bootstrapper::NewRemoteContext(
    MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template) {
  HandleScope scope(isolate_);
  Handle<JSGlobalProxy> global_proxy;
  {
    Genesis genesis(isolate_, maybe_global_proxy, global_proxy_template);
    global_proxy = genesis.global_proxy();
    if (global_proxy.is_null()) return Handle<JSGlobalProxy>();
  }
  // Log all maps created during bootstrapping.
  if (FLAG_trace_maps) LOG(isolate_, LogMaps());
  return scope.CloseAndEscape(global_proxy);
}

void Bootstrapper::DetachGlobal(Handle<Context> env) {
  isolate_->counters()->errors_thrown_per_context()->AddSample(
      env->GetErrorsThrown());

  ReadOnlyRoots roots(isolate_);
  Handle<JSGlobalProxy> global_proxy(JSGlobalProxy::cast(env->global_proxy()),
                                     isolate_);
  global_proxy->set_native_context(roots.null_value());
  JSObject::ForceSetPrototype(global_proxy, isolate_->factory()->null_value());
  global_proxy->map()->SetConstructor(roots.null_value());
  if (FLAG_track_detached_contexts) {
    isolate_->AddDetachedContext(env);
  }
}

namespace {

V8_NOINLINE Handle<SharedFunctionInfo> SimpleCreateSharedFunctionInfo(
    Isolate* isolate, Builtins::Name builtin_id, Handle<String> name, int len,
    FunctionKind kind = FunctionKind::kNormalFunction) {
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(name, builtin_id,
                                                          kind);
  shared->set_internal_formal_parameter_count(len);
  shared->set_length(len);
  return shared;
}

V8_NOINLINE Handle<SharedFunctionInfo> SimpleCreateBuiltinSharedFunctionInfo(
    Isolate* isolate, Builtins::Name builtin_id, Handle<String> name, int len) {
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(name, builtin_id,
                                                          kNormalFunction);
  shared->set_internal_formal_parameter_count(len);
  shared->set_length(len);
  return shared;
}

V8_NOINLINE void InstallFunction(Isolate* isolate, Handle<JSObject> target,
                                 Handle<Name> property_name,
                                 Handle<JSFunction> function,
                                 Handle<String> function_name,
                                 PropertyAttributes attributes = DONT_ENUM) {
  JSObject::AddProperty(isolate, target, property_name, function, attributes);
}

V8_NOINLINE void InstallFunction(Isolate* isolate, Handle<JSObject> target,
                                 Handle<JSFunction> function, Handle<Name> name,
                                 PropertyAttributes attributes = DONT_ENUM) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate, name).ToHandleChecked();
  InstallFunction(isolate, target, name, function, name_string, attributes);
}

V8_NOINLINE Handle<JSFunction> CreateFunction(
    Isolate* isolate, Handle<String> name, InstanceType type, int instance_size,
    int inobject_properties, MaybeHandle<Object> maybe_prototype,
    Builtins::Name builtin_id) {
  Handle<Object> prototype;
  Handle<JSFunction> result;

  if (maybe_prototype.ToHandle(&prototype)) {
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
        name, prototype, type, instance_size, inobject_properties, builtin_id,
        IMMUTABLE);

    result = isolate->factory()->NewFunction(args);
    // Make the JSFunction's prototype object fast.
    JSObject::MakePrototypesFast(handle(result->prototype(), isolate),
                                 kStartAtReceiver, isolate);
  } else {
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithoutPrototype(
        name, builtin_id, LanguageMode::kStrict);
    result = isolate->factory()->NewFunction(args);
  }

  // Make the resulting JSFunction object fast.
  JSObject::MakePrototypesFast(result, kStartAtReceiver, isolate);
  result->shared()->set_native(true);
  return result;
}

V8_NOINLINE Handle<JSFunction> InstallFunction(
    Isolate* isolate, Handle<JSObject> target, Handle<Name> name,
    InstanceType type, int instance_size, int inobject_properties,
    MaybeHandle<Object> maybe_prototype, Builtins::Name call,
    PropertyAttributes attributes) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate, name).ToHandleChecked();
  Handle<JSFunction> function =
      CreateFunction(isolate, name_string, type, instance_size,
                     inobject_properties, maybe_prototype, call);
  InstallFunction(isolate, target, name, function, name_string, attributes);
  return function;
}

V8_NOINLINE Handle<JSFunction> InstallFunction(
    Isolate* isolate, Handle<JSObject> target, const char* name,
    InstanceType type, int instance_size, int inobject_properties,
    MaybeHandle<Object> maybe_prototype, Builtins::Name call) {
  PropertyAttributes attributes = DONT_ENUM;
  return InstallFunction(
      isolate, target, isolate->factory()->InternalizeUtf8String(name), type,
      instance_size, inobject_properties, maybe_prototype, call, attributes);
}

V8_NOINLINE Handle<JSFunction> SimpleCreateFunction(Isolate* isolate,
                                                    Handle<String> name,
                                                    Builtins::Name call,
                                                    int len, bool adapt) {
  Handle<JSFunction> fun =
      CreateFunction(isolate, name, JS_OBJECT_TYPE, JSObject::kHeaderSize, 0,
                     MaybeHandle<JSObject>(), call);
  if (adapt) {
    fun->shared()->set_internal_formal_parameter_count(len);
  } else {
    fun->shared()->DontAdaptArguments();
  }
  fun->shared()->set_length(len);
  return fun;
}

V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, Handle<Name> property_name,
    Handle<String> function_name, Builtins::Name call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM,
    BuiltinFunctionId id = BuiltinFunctionId::kInvalidBuiltinFunctionId) {
  Handle<JSFunction> fun =
      SimpleCreateFunction(isolate, function_name, call, len, adapt);
  if (id != BuiltinFunctionId::kInvalidBuiltinFunctionId) {
    fun->shared()->set_builtin_function_id(id);
  }
  InstallFunction(isolate, base, fun, property_name, attrs);
  return fun;
}

V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, Handle<String> name,
    Builtins::Name call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM,
    BuiltinFunctionId id = BuiltinFunctionId::kInvalidBuiltinFunctionId) {
  return SimpleInstallFunction(isolate, base, name, name, call, len, adapt,
                               attrs, id);
}

V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, Handle<Name> property_name,
    const char* function_name, Builtins::Name call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM,
    BuiltinFunctionId id = BuiltinFunctionId::kInvalidBuiltinFunctionId) {
  return SimpleInstallFunction(
      isolate, base, property_name,
      isolate->factory()->InternalizeUtf8String(function_name), call, len,
      adapt, attrs, id);
}

V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, const char* name,
    Builtins::Name call, int len, bool adapt,
    PropertyAttributes attrs = DONT_ENUM,
    BuiltinFunctionId id = BuiltinFunctionId::kInvalidBuiltinFunctionId) {
  // Although function name does not have to be internalized the property name
  // will be internalized during property addition anyway, so do it here now.
  return SimpleInstallFunction(isolate, base,
                               isolate->factory()->InternalizeUtf8String(name),
                               call, len, adapt, attrs, id);
}

V8_NOINLINE Handle<JSFunction> SimpleInstallFunction(
    Isolate* isolate, Handle<JSObject> base, const char* name,
    Builtins::Name call, int len, bool adapt, BuiltinFunctionId id) {
  return SimpleInstallFunction(isolate, base, name, call, len, adapt, DONT_ENUM,
                               id);
}

V8_NOINLINE void SimpleInstallGetterSetter(Isolate* isolate,
                                           Handle<JSObject> base,
                                           Handle<String> name,
                                           Builtins::Name call_getter,
                                           Builtins::Name call_setter,
                                           PropertyAttributes attribs) {
  Handle<String> getter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call_getter, 0, true);

  Handle<String> setter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->set_string())
          .ToHandleChecked();
  Handle<JSFunction> setter =
      SimpleCreateFunction(isolate, setter_name, call_setter, 1, true);

  JSObject::DefineAccessor(base, name, getter, setter, attribs).Check();
}

V8_NOINLINE Handle<JSFunction> SimpleInstallGetter(
    Isolate* isolate, Handle<JSObject> base, Handle<Name> name,
    Handle<Name> property_name, Builtins::Name call, bool adapt) {
  Handle<String> getter_name =
      Name::ToFunctionName(isolate, name, isolate->factory()->get_string())
          .ToHandleChecked();
  Handle<JSFunction> getter =
      SimpleCreateFunction(isolate, getter_name, call, 0, adapt);

  Handle<Object> setter = isolate->factory()->undefined_value();

  JSObject::DefineAccessor(base, property_name, getter, setter, DONT_ENUM)
      .Check();

  return getter;
}

V8_NOINLINE Handle<JSFunction> SimpleInstallGetter(Isolate* isolate,
                                                   Handle<JSObject> base,
                                                   Handle<Name> name,
                                                   Builtins::Name call,
                                                   bool adapt) {
  return SimpleInstallGetter(isolate, base, name, name, call, adapt);
}

V8_NOINLINE Handle<JSFunction> SimpleInstallGetter(
    Isolate* isolate, Handle<JSObject> base, Handle<Name> name,
    Builtins::Name call, bool adapt, BuiltinFunctionId id) {
  Handle<JSFunction> fun =
      SimpleInstallGetter(isolate, base, name, call, adapt);
  fun->shared()->set_builtin_function_id(id);
  return fun;
}

V8_NOINLINE void InstallConstant(Isolate* isolate, Handle<JSObject> holder,
                                 const char* name, Handle<Object> value) {
  JSObject::AddProperty(
      isolate, holder, isolate->factory()->InternalizeUtf8String(name), value,
      static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
}

V8_NOINLINE void InstallSpeciesGetter(Isolate* isolate,
                                      Handle<JSFunction> constructor) {
  Factory* factory = isolate->factory();
  // TODO(adamk): We should be able to share a SharedFunctionInfo
  // between all these JSFunctins.
  SimpleInstallGetter(isolate, constructor, factory->symbol_species_string(),
                      factory->species_symbol(), Builtins::kReturnReceiver,
                      true);
}

}  // namespace

Handle<JSFunction> Genesis::CreateEmptyFunction() {
  // Allocate the function map first and then patch the prototype later.
  Handle<Map> empty_function_map = factory()->CreateSloppyFunctionMap(
      FUNCTION_WITHOUT_PROTOTYPE, MaybeHandle<JSFunction>());
  empty_function_map->set_is_prototype_map(true);
  DCHECK(!empty_function_map->is_dictionary_map());

  // Allocate ScopeInfo for the empty function.
  Handle<ScopeInfo> scope_info = ScopeInfo::CreateForEmptyFunction(isolate());

  // Allocate the empty function as the prototype for function according to
  // ES#sec-properties-of-the-function-prototype-object
  NewFunctionArgs args = NewFunctionArgs::ForBuiltin(
      factory()->empty_string(), empty_function_map, Builtins::kEmptyFunction);
  Handle<JSFunction> empty_function = factory()->NewFunction(args);
  native_context()->set_empty_function(*empty_function);

  // --- E m p t y ---
  Handle<String> source = factory()->NewStringFromStaticChars("() {}");
  Handle<Script> script = factory()->NewScript(source);
  script->set_type(Script::TYPE_NATIVE);
  Handle<WeakFixedArray> infos = factory()->NewWeakFixedArray(2);
  script->set_shared_function_infos(*infos);
  empty_function->shared()->set_scope_info(*scope_info);
  empty_function->shared()->DontAdaptArguments();
  SharedFunctionInfo::SetScript(handle(empty_function->shared(), isolate()),
                                script, 1);

  return empty_function;
}

void Genesis::CreateSloppyModeFunctionMaps(Handle<JSFunction> empty) {
  Factory* factory = isolate_->factory();
  Handle<Map> map;

  //
  // Allocate maps for sloppy functions without prototype.
  //
  map = factory->CreateSloppyFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_sloppy_function_without_prototype_map(*map);

  //
  // Allocate maps for sloppy functions with readonly prototype.
  //
  map =
      factory->CreateSloppyFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_sloppy_function_with_readonly_prototype_map(*map);

  //
  // Allocate maps for sloppy functions with writable prototype.
  //
  map = factory->CreateSloppyFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE,
                                         empty);
  native_context()->set_sloppy_function_map(*map);

  map = factory->CreateSloppyFunctionMap(
      FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE, empty);
  native_context()->set_sloppy_function_with_name_map(*map);
}

Handle<JSFunction> Genesis::GetThrowTypeErrorIntrinsic() {
  if (!restricted_properties_thrower_.is_null()) {
    return restricted_properties_thrower_;
  }
  Handle<String> name = factory()->empty_string();
  NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithoutPrototype(
      name, Builtins::kStrictPoisonPillThrower, i::LanguageMode::kStrict);
  Handle<JSFunction> function = factory()->NewFunction(args);
  function->shared()->DontAdaptArguments();

  // %ThrowTypeError% must not have a name property.
  if (JSReceiver::DeleteProperty(function, factory()->name_string())
          .IsNothing()) {
    DCHECK(false);
  }

  // length needs to be non configurable.
  Handle<Object> value(Smi::FromInt(function->shared()->GetLength()),
                       isolate());
  JSObject::SetOwnPropertyIgnoreAttributes(
      function, factory()->length_string(), value,
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY))
      .Assert();

  if (JSObject::PreventExtensions(function, kThrowOnError).IsNothing()) {
    DCHECK(false);
  }

  JSObject::MigrateSlowToFast(function, 0, "Bootstrapping");

  restricted_properties_thrower_ = function;
  return function;
}

void Genesis::CreateStrictModeFunctionMaps(Handle<JSFunction> empty) {
  Factory* factory = isolate_->factory();
  Handle<Map> map;

  //
  // Allocate maps for strict functions without prototype.
  //
  map = factory->CreateStrictFunctionMap(FUNCTION_WITHOUT_PROTOTYPE, empty);
  native_context()->set_strict_function_without_prototype_map(*map);

  map = factory->CreateStrictFunctionMap(METHOD_WITH_NAME, empty);
  native_context()->set_method_with_name_map(*map);

  map = factory->CreateStrictFunctionMap(METHOD_WITH_HOME_OBJECT, empty);
  native_context()->set_method_with_home_object_map(*map);

  map =
      factory->CreateStrictFunctionMap(METHOD_WITH_NAME_AND_HOME_OBJECT, empty);
  native_context()->set_method_with_name_and_home_object_map(*map);

  //
  // Allocate maps for strict functions with writable prototype.
  //
  map = factory->CreateStrictFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE,
                                         empty);
  native_context()->set_strict_function_map(*map);

  map = factory->CreateStrictFunctionMap(
      FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE, empty);
  native_context()->set_strict_function_with_name_map(*map);

  strict_function_with_home_object_map_ = factory->CreateStrictFunctionMap(
      FUNCTION_WITH_HOME_OBJECT_AND_WRITEABLE_PROTOTYPE, empty);
  strict_function_with_name_and_home_object_map_ =
      factory->CreateStrictFunctionMap(
          FUNCTION_WITH_NAME_AND_HOME_OBJECT_AND_WRITEABLE_PROTOTYPE, empty);

  //
  // Allocate maps for strict functions with readonly prototype.
  //
  map =
      factory->CreateStrictFunctionMap(FUNCTION_WITH_READONLY_PROTOTYPE, empty);
  native_context()->set_strict_function_with_readonly_prototype_map(*map);

  //
  // Allocate map for class functions.
  //
  map = factory->CreateClassFunctionMap(empty);
  native_context()->set_class_function_map(*map);

  // Now that the strict mode function map is available, set up the
  // restricted "arguments" and "caller" getters.
  AddRestrictedFunctionProperties(empty);
}

void Genesis::CreateObjectFunction(Handle<JSFunction> empty_function) {
  Factory* factory = isolate_->factory();

  // --- O b j e c t ---
  int inobject_properties = JSObject::kInitialGlobalObjectUnusedPropertiesCount;
  int instance_size =
      JSObject::kHeaderSize + kPointerSize * inobject_properties;

  Handle<JSFunction> object_fun = CreateFunction(
      isolate_, factory->Object_string(), JS_OBJECT_TYPE, instance_size,
      inobject_properties, factory->null_value(), Builtins::kObjectConstructor);
  object_fun->shared()->set_length(1);
  object_fun->shared()->DontAdaptArguments();
  native_context()->set_object_function(*object_fun);

  {
    // Finish setting up Object function's initial map.
    Map* initial_map = object_fun->initial_map();
    initial_map->set_elements_kind(HOLEY_ELEMENTS);
  }

  // Allocate a new prototype for the object function.
  Handle<JSObject> object_function_prototype =
      factory->NewFunctionPrototype(object_fun);

  Handle<Map> map =
      Map::Copy(isolate(), handle(object_function_prototype->map(), isolate()),
                "EmptyObjectPrototype");
  map->set_is_prototype_map(true);
  // Ban re-setting Object.prototype.__proto__ to prevent Proxy security bug
  map->set_is_immutable_proto(true);
  object_function_prototype->set_map(*map);

  // Complete setting up empty function.
  {
    Handle<Map> empty_function_map(empty_function->map(), isolate_);
    Map::SetPrototype(isolate(), empty_function_map, object_function_prototype);
  }

  native_context()->set_initial_object_prototype(*object_function_prototype);
  JSFunction::SetPrototype(object_fun, object_function_prototype);

  {
    // Set up slow map for Object.create(null) instances without in-object
    // properties.
    Handle<Map> map(object_fun->initial_map(), isolate_);
    map = Map::CopyInitialMapNormalized(isolate(), map);
    Map::SetPrototype(isolate(), map, factory->null_value());
    native_context()->set_slow_object_with_null_prototype_map(*map);

    // Set up slow map for literals with too many properties.
    map = Map::Copy(isolate(), map, "slow_object_with_object_prototype_map");
    Map::SetPrototype(isolate(), map, object_function_prototype);
    native_context()->set_slow_object_with_object_prototype_map(*map);
  }
}

namespace {

Handle<Map> CreateNonConstructorMap(Isolate* isolate, Handle<Map> source_map,
                                    Handle<JSObject> prototype,
                                    const char* reason) {
  Handle<Map> map = Map::Copy(isolate, source_map, reason);
  // Ensure the resulting map has prototype slot (it is necessary for storing
  // inital map even when the prototype property is not required).
  if (!map->has_prototype_slot()) {
    // Re-set the unused property fields after changing the instance size.
    // TODO(ulan): Do not change instance size after map creation.
    int unused_property_fields = map->UnusedPropertyFields();
    map->set_instance_size(map->instance_size() + kPointerSize);
    // The prototype slot shifts the in-object properties area by one slot.
    map->SetInObjectPropertiesStartInWords(
        map->GetInObjectPropertiesStartInWords() + 1);
    map->set_has_prototype_slot(true);
    map->SetInObjectUnusedPropertyFields(unused_property_fields);
  }
  map->set_is_constructor(false);
  Map::SetPrototype(isolate, map, prototype);
  return map;
}

}  // namespace

void Genesis::CreateIteratorMaps(Handle<JSFunction> empty) {
  // Create iterator-related meta-objects.
  Handle<JSObject> iterator_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);

  SimpleInstallFunction(isolate(), iterator_prototype,
                        factory()->iterator_symbol(), "[Symbol.iterator]",
                        Builtins::kReturnReceiver, 0, true);
  native_context()->set_initial_iterator_prototype(*iterator_prototype);

  Handle<JSObject> generator_object_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  native_context()->set_initial_generator_prototype(
      *generator_object_prototype);
  JSObject::ForceSetPrototype(generator_object_prototype, iterator_prototype);
  Handle<JSObject> generator_function_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::ForceSetPrototype(generator_function_prototype, empty);

  JSObject::AddProperty(isolate(), generator_function_prototype,
                        factory()->to_string_tag_symbol(),
                        factory()->InternalizeUtf8String("GeneratorFunction"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(isolate(), generator_function_prototype,
                        factory()->prototype_string(),
                        generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  JSObject::AddProperty(isolate(), generator_object_prototype,
                        factory()->constructor_string(),
                        generator_function_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(isolate(), generator_object_prototype,
                        factory()->to_string_tag_symbol(),
                        factory()->InternalizeUtf8String("Generator"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  SimpleInstallFunction(isolate(), generator_object_prototype, "next",
                        Builtins::kGeneratorPrototypeNext, 1, false);
  SimpleInstallFunction(isolate(), generator_object_prototype, "return",
                        Builtins::kGeneratorPrototypeReturn, 1, false);
  SimpleInstallFunction(isolate(), generator_object_prototype, "throw",
                        Builtins::kGeneratorPrototypeThrow, 1, false);

  // Internal version of generator_prototype_next, flagged as non-native such
  // that it doesn't show up in Error traces.
  Handle<JSFunction> generator_next_internal =
      SimpleCreateFunction(isolate(), factory()->next_string(),
                           Builtins::kGeneratorPrototypeNext, 1, false);
  generator_next_internal->shared()->set_native(false);
  native_context()->set_generator_next_internal(*generator_next_internal);

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  // Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> map;
  map = CreateNonConstructorMap(isolate(), isolate()->strict_function_map(),
                                generator_function_prototype,
                                "GeneratorFunction");
  native_context()->set_generator_function_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->strict_function_with_name_map(),
      generator_function_prototype, "GeneratorFunction with name");
  native_context()->set_generator_function_with_name_map(*map);

  map = CreateNonConstructorMap(
      isolate(), strict_function_with_home_object_map_,
      generator_function_prototype, "GeneratorFunction with home object");
  native_context()->set_generator_function_with_home_object_map(*map);

  map = CreateNonConstructorMap(isolate(),
                                strict_function_with_name_and_home_object_map_,
                                generator_function_prototype,
                                "GeneratorFunction with name and home object");
  native_context()->set_generator_function_with_name_and_home_object_map(*map);

  Handle<JSFunction> object_function(native_context()->object_function(),
                                     isolate());
  Handle<Map> generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(isolate(), generator_object_prototype_map,
                    generator_object_prototype);
  native_context()->set_generator_object_prototype_map(
      *generator_object_prototype_map);
}

void Genesis::CreateAsyncIteratorMaps(Handle<JSFunction> empty) {
  // %AsyncIteratorPrototype%
  // proposal-async-iteration/#sec-asynciteratorprototype
  Handle<JSObject> async_iterator_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);

  SimpleInstallFunction(
      isolate(), async_iterator_prototype, factory()->async_iterator_symbol(),
      "[Symbol.asyncIterator]", Builtins::kReturnReceiver, 0, true);

  // %AsyncFromSyncIteratorPrototype%
  // proposal-async-iteration/#sec-%asyncfromsynciteratorprototype%-object
  Handle<JSObject> async_from_sync_iterator_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  SimpleInstallFunction(isolate(), async_from_sync_iterator_prototype,
                        factory()->next_string(),
                        Builtins::kAsyncFromSyncIteratorPrototypeNext, 1, true);
  SimpleInstallFunction(
      isolate(), async_from_sync_iterator_prototype, factory()->return_string(),
      Builtins::kAsyncFromSyncIteratorPrototypeReturn, 1, true);
  SimpleInstallFunction(
      isolate(), async_from_sync_iterator_prototype, factory()->throw_string(),
      Builtins::kAsyncFromSyncIteratorPrototypeThrow, 1, true);

  JSObject::AddProperty(
      isolate(), async_from_sync_iterator_prototype,
      factory()->to_string_tag_symbol(),
      factory()->InternalizeUtf8String("Async-from-Sync Iterator"),
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  JSObject::ForceSetPrototype(async_from_sync_iterator_prototype,
                              async_iterator_prototype);

  Handle<Map> async_from_sync_iterator_map = factory()->NewMap(
      JS_ASYNC_FROM_SYNC_ITERATOR_TYPE, JSAsyncFromSyncIterator::kSize);
  Map::SetPrototype(isolate(), async_from_sync_iterator_map,
                    async_from_sync_iterator_prototype);
  native_context()->set_async_from_sync_iterator_map(
      *async_from_sync_iterator_map);

  // Async Generators
  Handle<String> AsyncGeneratorFunction_string =
      factory()->NewStringFromAsciiChecked("AsyncGeneratorFunction", TENURED);

  Handle<JSObject> async_generator_object_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Handle<JSObject> async_generator_function_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);

  // %AsyncGenerator% / %AsyncGeneratorFunction%.prototype
  JSObject::ForceSetPrototype(async_generator_function_prototype, empty);

  // The value of AsyncGeneratorFunction.prototype.prototype is the
  //     %AsyncGeneratorPrototype% intrinsic object.
  // This property has the attributes
  //     { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
  JSObject::AddProperty(isolate(), async_generator_function_prototype,
                        factory()->prototype_string(),
                        async_generator_object_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(isolate(), async_generator_object_prototype,
                        factory()->constructor_string(),
                        async_generator_function_prototype,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  JSObject::AddProperty(isolate(), async_generator_function_prototype,
                        factory()->to_string_tag_symbol(),
                        AsyncGeneratorFunction_string,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  // %AsyncGeneratorPrototype%
  JSObject::ForceSetPrototype(async_generator_object_prototype,
                              async_iterator_prototype);
  native_context()->set_initial_async_generator_prototype(
      *async_generator_object_prototype);

  JSObject::AddProperty(isolate(), async_generator_object_prototype,
                        factory()->to_string_tag_symbol(),
                        factory()->InternalizeUtf8String("AsyncGenerator"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "next",
                        Builtins::kAsyncGeneratorPrototypeNext, 1, false);
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "return",
                        Builtins::kAsyncGeneratorPrototypeReturn, 1, false);
  SimpleInstallFunction(isolate(), async_generator_object_prototype, "throw",
                        Builtins::kAsyncGeneratorPrototypeThrow, 1, false);

  // Create maps for generator functions and their prototypes.  Store those
  // maps in the native context. The "prototype" property descriptor is
  // writable, non-enumerable, and non-configurable (as per ES6 draft
  // 04-14-15, section 25.2.4.3).
  // Async Generator functions do not have "caller" or "arguments" accessors.
  Handle<Map> map;
  map = CreateNonConstructorMap(isolate(), isolate()->strict_function_map(),
                                async_generator_function_prototype,
                                "AsyncGeneratorFunction");
  native_context()->set_async_generator_function_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->strict_function_with_name_map(),
      async_generator_function_prototype, "AsyncGeneratorFunction with name");
  native_context()->set_async_generator_function_with_name_map(*map);

  map =
      CreateNonConstructorMap(isolate(), strict_function_with_home_object_map_,
                              async_generator_function_prototype,
                              "AsyncGeneratorFunction with home object");
  native_context()->set_async_generator_function_with_home_object_map(*map);

  map = CreateNonConstructorMap(
      isolate(), strict_function_with_name_and_home_object_map_,
      async_generator_function_prototype,
      "AsyncGeneratorFunction with name and home object");
  native_context()->set_async_generator_function_with_name_and_home_object_map(
      *map);

  Handle<JSFunction> object_function(native_context()->object_function(),
                                     isolate());
  Handle<Map> async_generator_object_prototype_map = Map::Create(isolate(), 0);
  Map::SetPrototype(isolate(), async_generator_object_prototype_map,
                    async_generator_object_prototype);
  native_context()->set_async_generator_object_prototype_map(
      *async_generator_object_prototype_map);
}

void Genesis::CreateAsyncFunctionMaps(Handle<JSFunction> empty) {
  // %AsyncFunctionPrototype% intrinsic
  Handle<JSObject> async_function_prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::ForceSetPrototype(async_function_prototype, empty);

  JSObject::AddProperty(isolate(), async_function_prototype,
                        factory()->to_string_tag_symbol(),
                        factory()->InternalizeUtf8String("AsyncFunction"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  Handle<Map> map;
  map = CreateNonConstructorMap(
      isolate(), isolate()->strict_function_without_prototype_map(),
      async_function_prototype, "AsyncFunction");
  native_context()->set_async_function_map(*map);

  map = CreateNonConstructorMap(isolate(), isolate()->method_with_name_map(),
                                async_function_prototype,
                                "AsyncFunction with name");
  native_context()->set_async_function_with_name_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->method_with_home_object_map(),
      async_function_prototype, "AsyncFunction with home object");
  native_context()->set_async_function_with_home_object_map(*map);

  map = CreateNonConstructorMap(
      isolate(), isolate()->method_with_name_and_home_object_map(),
      async_function_prototype, "AsyncFunction with name and home object");
  native_context()->set_async_function_with_name_and_home_object_map(*map);
}

void Genesis::CreateJSProxyMaps() {
  // Allocate maps for all Proxy types.
  // Next to the default proxy, we need maps indicating callable and
  // constructable proxies.
  Handle<Map> proxy_map = factory()->NewMap(JS_PROXY_TYPE, JSProxy::kSize,
                                            TERMINAL_FAST_ELEMENTS_KIND);
  proxy_map->set_is_dictionary_map(true);
  proxy_map->set_may_have_interesting_symbols(true);
  native_context()->set_proxy_map(*proxy_map);

  Handle<Map> proxy_callable_map =
      Map::Copy(isolate_, proxy_map, "callable Proxy");
  proxy_callable_map->set_is_callable(true);
  native_context()->set_proxy_callable_map(*proxy_callable_map);
  proxy_callable_map->SetConstructor(native_context()->function_function());

  Handle<Map> proxy_constructor_map =
      Map::Copy(isolate_, proxy_callable_map, "constructor Proxy");
  proxy_constructor_map->set_is_constructor(true);
  native_context()->set_proxy_constructor_map(*proxy_constructor_map);

  {
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSProxyRevocableResult::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 2);
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // proxy
      Descriptor d = Descriptor::DataField(isolate(), factory()->proxy_string(),
                                           JSProxyRevocableResult::kProxyIndex,
                                           NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // revoke
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->revoke_string(),
          JSProxyRevocableResult::kRevokeIndex, NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_proxy_revocable_result_map(*map);
  }
}

namespace {
void ReplaceAccessors(Isolate* isolate, Handle<Map> map, Handle<String> name,
                      PropertyAttributes attributes,
                      Handle<AccessorPair> accessor_pair) {
  DescriptorArray* descriptors = map->instance_descriptors();
  int idx = descriptors->SearchWithCache(isolate, *name, *map);
  Descriptor d = Descriptor::AccessorConstant(name, accessor_pair, attributes);
  descriptors->Replace(idx, &d);
}
}  // namespace

void Genesis::AddRestrictedFunctionProperties(Handle<JSFunction> empty) {
  PropertyAttributes rw_attribs = static_cast<PropertyAttributes>(DONT_ENUM);
  Handle<JSFunction> thrower = GetThrowTypeErrorIntrinsic();
  Handle<AccessorPair> accessors = factory()->NewAccessorPair();
  accessors->set_getter(*thrower);
  accessors->set_setter(*thrower);

  Handle<Map> map(empty->map(), isolate());
  ReplaceAccessors(isolate(), map, factory()->arguments_string(), rw_attribs,
                   accessors);
  ReplaceAccessors(isolate(), map, factory()->caller_string(), rw_attribs,
                   accessors);
}

static void AddToWeakNativeContextList(Isolate* isolate, Context* context) {
  DCHECK(context->IsNativeContext());
  Heap* heap = isolate->heap();
#ifdef DEBUG
  { // NOLINT
    DCHECK(context->next_context_link()->IsUndefined(isolate));
    // Check that context is not in the list yet.
    for (Object* current = heap->native_contexts_list();
         !current->IsUndefined(isolate);
         current = Context::cast(current)->next_context_link()) {
      DCHECK(current != context);
    }
  }
#endif
  context->set(Context::NEXT_CONTEXT_LINK, heap->native_contexts_list(),
               UPDATE_WEAK_WRITE_BARRIER);
  heap->set_native_contexts_list(context);
}


void Genesis::CreateRoots() {
  // Allocate the native context FixedArray first and then patch the
  // closure and extension object later (we need the empty function
  // and the global object, but in order to create those, we need the
  // native context).
  native_context_ = factory()->NewNativeContext();
  AddToWeakNativeContextList(isolate(), *native_context());
  isolate()->set_context(*native_context());

  // Allocate the message listeners object.
  {
    Handle<TemplateList> list = TemplateList::New(isolate(), 1);
    native_context()->set_message_listeners(*list);
  }
}


void Genesis::InstallGlobalThisBinding() {
  Handle<ScriptContextTable> script_contexts(
      native_context()->script_context_table(), isolate());
  Handle<ScopeInfo> scope_info = ScopeInfo::CreateGlobalThisBinding(isolate());
  Handle<Context> context =
      factory()->NewScriptContext(native_context(), scope_info);

  // Go ahead and hook it up while we're at it.
  int slot = scope_info->ReceiverContextSlotIndex();
  DCHECK_EQ(slot, Context::MIN_CONTEXT_SLOTS);
  context->set(slot, native_context()->global_proxy());

  Handle<ScriptContextTable> new_script_contexts =
      ScriptContextTable::Extend(script_contexts, context);
  native_context()->set_script_context_table(*new_script_contexts);
}


Handle<JSGlobalObject> Genesis::CreateNewGlobals(
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    Handle<JSGlobalProxy> global_proxy) {
  // The argument global_proxy_template aka data is an ObjectTemplateInfo.
  // It has a constructor pointer that points at global_constructor which is a
  // FunctionTemplateInfo.
  // The global_proxy_constructor is used to (re)initialize the
  // global_proxy. The global_proxy_constructor also has a prototype_template
  // pointer that points at js_global_object_template which is an
  // ObjectTemplateInfo.
  // That in turn has a constructor pointer that points at
  // js_global_object_constructor which is a FunctionTemplateInfo.
  // js_global_object_constructor is used to make js_global_object_function
  // js_global_object_function is used to make the new global_object.
  //
  // --- G l o b a l ---
  // Step 1: Create a fresh JSGlobalObject.
  Handle<JSFunction> js_global_object_function;
  Handle<ObjectTemplateInfo> js_global_object_template;
  if (!global_proxy_template.IsEmpty()) {
    // Get prototype template of the global_proxy_template.
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor =
        Handle<FunctionTemplateInfo>(
            FunctionTemplateInfo::cast(data->constructor()), isolate());
    Handle<Object> proto_template(global_constructor->prototype_template(),
                                  isolate());
    if (!proto_template->IsUndefined(isolate())) {
      js_global_object_template =
          Handle<ObjectTemplateInfo>::cast(proto_template);
    }
  }

  if (js_global_object_template.is_null()) {
    Handle<String> name = factory()->empty_string();
    Handle<JSObject> prototype =
        factory()->NewFunctionPrototype(isolate()->object_function());
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
        name, prototype, JS_GLOBAL_OBJECT_TYPE, JSGlobalObject::kSize, 0,
        Builtins::kIllegal, MUTABLE);
    js_global_object_function = factory()->NewFunction(args);
#ifdef DEBUG
    LookupIterator it(isolate(), prototype, factory()->constructor_string(),
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    Handle<Object> value = Object::GetProperty(&it).ToHandleChecked();
    DCHECK(it.IsFound());
    DCHECK_EQ(*isolate()->object_function(), *value);
#endif
  } else {
    Handle<FunctionTemplateInfo> js_global_object_constructor(
        FunctionTemplateInfo::cast(js_global_object_template->constructor()),
        isolate());
    js_global_object_function = ApiNatives::CreateApiFunction(
        isolate(), js_global_object_constructor, factory()->the_hole_value(),
        JS_GLOBAL_OBJECT_TYPE);
  }

  js_global_object_function->initial_map()->set_is_prototype_map(true);
  js_global_object_function->initial_map()->set_is_dictionary_map(true);
  js_global_object_function->initial_map()->set_may_have_interesting_symbols(
      true);
  Handle<JSGlobalObject> global_object =
      factory()->NewJSGlobalObject(js_global_object_function);

  // Step 2: (re)initialize the global proxy object.
  Handle<JSFunction> global_proxy_function;
  if (global_proxy_template.IsEmpty()) {
    Handle<String> name = factory()->empty_string();
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
        name, factory()->the_hole_value(), JS_GLOBAL_PROXY_TYPE,
        JSGlobalProxy::SizeWithEmbedderFields(0), 0, Builtins::kIllegal,
        MUTABLE);
    global_proxy_function = factory()->NewFunction(args);
  } else {
    Handle<ObjectTemplateInfo> data =
        v8::Utils::OpenHandle(*global_proxy_template);
    Handle<FunctionTemplateInfo> global_constructor(
        FunctionTemplateInfo::cast(data->constructor()), isolate());
    global_proxy_function = ApiNatives::CreateApiFunction(
        isolate(), global_constructor, factory()->the_hole_value(),
        JS_GLOBAL_PROXY_TYPE);
  }
  global_proxy_function->initial_map()->set_is_access_check_needed(true);
  global_proxy_function->initial_map()->set_has_hidden_prototype(true);
  global_proxy_function->initial_map()->set_may_have_interesting_symbols(true);
  native_context()->set_global_proxy_function(*global_proxy_function);

  // Set global_proxy.__proto__ to js_global after ConfigureGlobalObjects
  // Return the global proxy.

  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);

  // Set the native context for the global object.
  global_object->set_native_context(*native_context());
  global_object->set_global_proxy(*global_proxy);
  // Set the native context of the global proxy.
  global_proxy->set_native_context(*native_context());
  // Set the global proxy of the native context. If the native context has been
  // deserialized, the global proxy is already correctly set up by the
  // deserializer. Otherwise it's undefined.
  DCHECK(native_context()
             ->get(Context::GLOBAL_PROXY_INDEX)
             ->IsUndefined(isolate()) ||
         native_context()->global_proxy() == *global_proxy);
  native_context()->set_global_proxy(*global_proxy);

  return global_object;
}

void Genesis::HookUpGlobalProxy(Handle<JSGlobalProxy> global_proxy) {
  // Re-initialize the global proxy with the global proxy function from the
  // snapshot, and then set up the link to the native context.
  Handle<JSFunction> global_proxy_function(
      native_context()->global_proxy_function(), isolate());
  factory()->ReinitializeJSGlobalProxy(global_proxy, global_proxy_function);
  Handle<JSObject> global_object(
      JSObject::cast(native_context()->global_object()), isolate());
  JSObject::ForceSetPrototype(global_proxy, global_object);
  global_proxy->set_native_context(*native_context());
  DCHECK(native_context()->global_proxy() == *global_proxy);
}

void Genesis::HookUpGlobalObject(Handle<JSGlobalObject> global_object) {
  Handle<JSGlobalObject> global_object_from_snapshot(
      JSGlobalObject::cast(native_context()->extension()), isolate());
  native_context()->set_extension(*global_object);
  native_context()->set_security_token(*global_object);

  TransferNamedProperties(global_object_from_snapshot, global_object);
  TransferIndexedProperties(global_object_from_snapshot, global_object);
}

static void InstallWithIntrinsicDefaultProto(Isolate* isolate,
                                             Handle<JSFunction> function,
                                             int context_index) {
  Handle<Smi> index(Smi::FromInt(context_index), isolate);
  JSObject::AddProperty(isolate, function,
                        isolate->factory()->native_context_index_symbol(),
                        index, NONE);
  isolate->native_context()->set(context_index, *function);
}

static void InstallError(Isolate* isolate, Handle<JSObject> global,
                         Handle<String> name, int context_index) {
  Factory* factory = isolate->factory();

  Handle<JSFunction> error_fun = InstallFunction(
      isolate, global, name, JS_ERROR_TYPE, JSObject::kHeaderSize, 0,
      factory->the_hole_value(), Builtins::kErrorConstructor, DONT_ENUM);
  error_fun->shared()->DontAdaptArguments();
  error_fun->shared()->set_length(1);

  if (context_index == Context::ERROR_FUNCTION_INDEX) {
    SimpleInstallFunction(isolate, error_fun, "captureStackTrace",
                          Builtins::kErrorCaptureStackTrace, 2, false);
  }

  InstallWithIntrinsicDefaultProto(isolate, error_fun, context_index);

  {
    // Setup %XXXErrorPrototype%.
    Handle<JSObject> prototype(JSObject::cast(error_fun->instance_prototype()),
                               isolate);

    JSObject::AddProperty(isolate, prototype, factory->name_string(), name,
                          DONT_ENUM);
    JSObject::AddProperty(isolate, prototype, factory->message_string(),
                          factory->empty_string(), DONT_ENUM);

    if (context_index == Context::ERROR_FUNCTION_INDEX) {
      Handle<JSFunction> to_string_fun =
          SimpleInstallFunction(isolate, prototype, factory->toString_string(),
                                Builtins::kErrorPrototypeToString, 0, true);
      isolate->native_context()->set_error_to_string(*to_string_fun);
      isolate->native_context()->set_initial_error_prototype(*prototype);
    } else {
      DCHECK(isolate->native_context()->error_to_string()->IsJSFunction());

      InstallFunction(isolate, prototype, isolate->error_to_string(),
                      factory->toString_string(), DONT_ENUM);

      Handle<JSFunction> global_error = isolate->error_function();
      CHECK(JSReceiver::SetPrototype(error_fun, global_error, false,
                                     kThrowOnError)
                .FromMaybe(false));
      CHECK(JSReceiver::SetPrototype(prototype,
                                     handle(global_error->prototype(), isolate),
                                     false, kThrowOnError)
                .FromMaybe(false));
    }
  }

  Handle<Map> initial_map(error_fun->initial_map(), isolate);
  Map::EnsureDescriptorSlack(isolate, initial_map, 1);

  {
    Handle<AccessorInfo> info = factory->error_stack_accessor();
    Descriptor d = Descriptor::AccessorConstant(handle(info->name(), isolate),
                                                info, DONT_ENUM);
    initial_map->AppendDescriptor(&d);
  }
}

namespace {

void InstallMakeError(Isolate* isolate, int builtin_id, int context_index) {
  NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
      isolate->factory()->empty_string(), isolate->factory()->the_hole_value(),
      JS_OBJECT_TYPE, JSObject::kHeaderSize, 0, builtin_id, MUTABLE);

  Handle<JSFunction> function = isolate->factory()->NewFunction(args);
  function->shared()->DontAdaptArguments();
  isolate->native_context()->set(context_index, *function);
}

}  // namespace

// This is only called if we are not using snapshots.  The equivalent
// work in the snapshot case is done in HookUpGlobalObject.
void Genesis::InitializeGlobal(Handle<JSGlobalObject> global_object,
                               Handle<JSFunction> empty_function,
                               GlobalContextType context_type) {
  // --- N a t i v e   C o n t e x t ---
  // Use the empty scope info.
  native_context()->set_scope_info(empty_function->shared()->scope_info());
  native_context()->set_previous(nullptr);
  // Set extension and global object.
  native_context()->set_extension(*global_object);
  // Security setup: Set the security token of the native context to the global
  // object. This makes the security check between two different contexts fail
  // by default even in case of global object reinitialization.
  native_context()->set_security_token(*global_object);

  Factory* factory = isolate_->factory();

  Handle<ScriptContextTable> script_context_table =
      factory->NewScriptContextTable();
  native_context()->set_script_context_table(*script_context_table);
  InstallGlobalThisBinding();

  {  // --- O b j e c t ---
    Handle<String> object_name = factory->Object_string();
    Handle<JSFunction> object_function = isolate_->object_function();
    JSObject::AddProperty(isolate_, global_object, object_name, object_function,
                          DONT_ENUM);

    SimpleInstallFunction(isolate_, object_function, factory->assign_string(),
                          Builtins::kObjectAssign, 2, false);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertyDescriptor",
                          Builtins::kObjectGetOwnPropertyDescriptor, 2, false);
    SimpleInstallFunction(isolate_, object_function,
                          factory->getOwnPropertyDescriptors_string(),
                          Builtins::kObjectGetOwnPropertyDescriptors, 1, false);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertyNames",
                          Builtins::kObjectGetOwnPropertyNames, 1, true);
    SimpleInstallFunction(isolate_, object_function, "getOwnPropertySymbols",
                          Builtins::kObjectGetOwnPropertySymbols, 1, false);
    SimpleInstallFunction(isolate_, object_function, "is", Builtins::kObjectIs,
                          2, true);
    SimpleInstallFunction(isolate_, object_function, "preventExtensions",
                          Builtins::kObjectPreventExtensions, 1, false);
    SimpleInstallFunction(isolate_, object_function, "seal",
                          Builtins::kObjectSeal, 1, false);

    Handle<JSFunction> object_create = SimpleInstallFunction(
        isolate_, object_function, factory->create_string(),
        Builtins::kObjectCreate, 2, false);
    native_context()->set_object_create(*object_create);

    Handle<JSFunction> object_define_properties =
        SimpleInstallFunction(isolate_, object_function, "defineProperties",
                              Builtins::kObjectDefineProperties, 2, true);
    native_context()->set_object_define_properties(*object_define_properties);

    Handle<JSFunction> object_define_property = SimpleInstallFunction(
        isolate_, object_function, factory->defineProperty_string(),
        Builtins::kObjectDefineProperty, 3, true);
    native_context()->set_object_define_property(*object_define_property);

    SimpleInstallFunction(isolate_, object_function, "freeze",
                          Builtins::kObjectFreeze, 1, false);

    Handle<JSFunction> object_get_prototype_of =
        SimpleInstallFunction(isolate_, object_function, "getPrototypeOf",
                              Builtins::kObjectGetPrototypeOf, 1, false);
    native_context()->set_object_get_prototype_of(*object_get_prototype_of);
    SimpleInstallFunction(isolate_, object_function, "setPrototypeOf",
                          Builtins::kObjectSetPrototypeOf, 2, false);

    SimpleInstallFunction(isolate_, object_function, "isExtensible",
                          Builtins::kObjectIsExtensible, 1, false);
    SimpleInstallFunction(isolate_, object_function, "isFrozen",
                          Builtins::kObjectIsFrozen, 1, false);

    Handle<JSFunction> object_is_sealed =
        SimpleInstallFunction(isolate_, object_function, "isSealed",
                              Builtins::kObjectIsSealed, 1, false);
    native_context()->set_object_is_sealed(*object_is_sealed);

    Handle<JSFunction> object_keys = SimpleInstallFunction(
        isolate_, object_function, "keys", Builtins::kObjectKeys, 1, true);
    native_context()->set_object_keys(*object_keys);
    SimpleInstallFunction(isolate_, object_function, factory->entries_string(),
                          Builtins::kObjectEntries, 1, true);
    SimpleInstallFunction(isolate_, object_function, factory->values_string(),
                          Builtins::kObjectValues, 1, true);

    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__defineGetter__", Builtins::kObjectDefineGetter, 2,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__defineSetter__", Builtins::kObjectDefineSetter, 2,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "hasOwnProperty",
                          Builtins::kObjectPrototypeHasOwnProperty, 1, true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__lookupGetter__", Builtins::kObjectLookupGetter, 1,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "__lookupSetter__", Builtins::kObjectLookupSetter, 1,
                          true);
    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "isPrototypeOf",
                          Builtins::kObjectPrototypeIsPrototypeOf, 1, true);
    SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "propertyIsEnumerable",
        Builtins::kObjectPrototypePropertyIsEnumerable, 1, false);
    Handle<JSFunction> object_to_string =
        SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                              factory->toString_string(),
                              Builtins::kObjectPrototypeToString, 0, true);
    native_context()->set_object_to_string(*object_to_string);
    Handle<JSFunction> object_value_of = SimpleInstallFunction(
        isolate_, isolate_->initial_object_prototype(), "valueOf",
        Builtins::kObjectPrototypeValueOf, 0, true);
    native_context()->set_object_value_of(*object_value_of);

    SimpleInstallGetterSetter(isolate_, isolate_->initial_object_prototype(),
                              factory->proto_string(),
                              Builtins::kObjectPrototypeGetProto,
                              Builtins::kObjectPrototypeSetProto, DONT_ENUM);

    SimpleInstallFunction(isolate_, isolate_->initial_object_prototype(),
                          "toLocaleString",
                          Builtins::kObjectPrototypeToLocaleString, 0, true);
  }

  Handle<JSObject> global(native_context()->global_object(), isolate());

  {  // --- F u n c t i o n ---
    Handle<JSFunction> prototype = empty_function;
    Handle<JSFunction> function_fun =
        InstallFunction(isolate_, global, "Function", JS_FUNCTION_TYPE,
                        JSFunction::kSizeWithPrototype, 0, prototype,
                        Builtins::kFunctionConstructor);
    // Function instances are sloppy by default.
    function_fun->set_prototype_or_initial_map(
        *isolate_->sloppy_function_map());
    function_fun->shared()->DontAdaptArguments();
    function_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, function_fun,
                                     Context::FUNCTION_FUNCTION_INDEX);

    // Setup the methods on the %FunctionPrototype%.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          function_fun, DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, factory->apply_string(),
                          Builtins::kFunctionPrototypeApply, 2, false);
    SimpleInstallFunction(isolate_, prototype, factory->bind_string(),
                          Builtins::kFastFunctionPrototypeBind, 1, false);
    SimpleInstallFunction(isolate_, prototype, factory->call_string(),
                          Builtins::kFunctionPrototypeCall, 1, false);
    SimpleInstallFunction(isolate_, prototype, factory->toString_string(),
                          Builtins::kFunctionPrototypeToString, 0, false);

    // Install the @@hasInstance function.
    Handle<JSFunction> has_instance = SimpleInstallFunction(
        isolate_, prototype, factory->has_instance_symbol(),
        "[Symbol.hasInstance]", Builtins::kFunctionPrototypeHasInstance, 1,
        true,
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY),
        BuiltinFunctionId::kFunctionHasInstance);
    native_context()->set_function_has_instance(*has_instance);

    // Complete setting up function maps.
    {
      isolate_->sloppy_function_map()->SetConstructor(*function_fun);
      isolate_->sloppy_function_with_name_map()->SetConstructor(*function_fun);
      isolate_->sloppy_function_with_readonly_prototype_map()->SetConstructor(
          *function_fun);

      isolate_->strict_function_map()->SetConstructor(*function_fun);
      isolate_->strict_function_with_name_map()->SetConstructor(*function_fun);
      strict_function_with_home_object_map_->SetConstructor(*function_fun);
      strict_function_with_name_and_home_object_map_->SetConstructor(
          *function_fun);
      isolate_->strict_function_with_readonly_prototype_map()->SetConstructor(
          *function_fun);

      isolate_->class_function_map()->SetConstructor(*function_fun);
    }
  }

  {  // --- A s y n c F r o m S y n c I t e r a t o r
    Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncIteratorValueUnwrap, factory->empty_string(),
        1);
    native_context()->set_async_iterator_value_unwrap_shared_fun(*info);
  }

  {  // --- A s y n c G e n e r a t o r ---
    Handle<JSFunction> await_caught =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtins::kAsyncGeneratorAwaitCaught, 1, false);
    native_context()->set_async_generator_await_caught(*await_caught);

    Handle<JSFunction> await_uncaught =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtins::kAsyncGeneratorAwaitUncaught, 1, false);
    native_context()->set_async_generator_await_uncaught(*await_uncaught);

    Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorAwaitResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_await_resolve_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorAwaitRejectClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_await_reject_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorYieldResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_yield_resolve_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorReturnResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_return_resolve_shared_fun(*info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorReturnClosedResolveClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_return_closed_resolve_shared_fun(
        *info);

    info = SimpleCreateSharedFunctionInfo(
        isolate_, Builtins::kAsyncGeneratorReturnClosedRejectClosure,
        factory->empty_string(), 1);
    native_context()->set_async_generator_return_closed_reject_shared_fun(
        *info);
  }

  {  // --- A r r a y ---
    Handle<JSFunction> array_function = InstallFunction(
        isolate_, global, "Array", JS_ARRAY_TYPE, JSArray::kSize, 0,
        isolate_->initial_object_prototype(), Builtins::kArrayConstructor);
    array_function->shared()->DontAdaptArguments();
    array_function->shared()->set_builtin_function_id(
        BuiltinFunctionId::kArrayConstructor);

    // This seems a bit hackish, but we need to make sure Array.length
    // is 1.
    array_function->shared()->set_length(1);

    Handle<Map> initial_map(array_function->initial_map(), isolate());

    // This assert protects an optimization in
    // HGraphBuilder::JSArrayBuilder::EmitMapCode()
    DCHECK(initial_map->elements_kind() == GetInitialFastElementsKind());
    Map::EnsureDescriptorSlack(isolate_, initial_map, 1);

    PropertyAttributes attribs = static_cast<PropertyAttributes>(
        DONT_ENUM | DONT_DELETE);

    STATIC_ASSERT(JSArray::kLengthDescriptorIndex == 0);
    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->array_length_accessor(), attribs);
      initial_map->AppendDescriptor(&d);
    }

    InstallWithIntrinsicDefaultProto(isolate_, array_function,
                                     Context::ARRAY_FUNCTION_INDEX);
    InstallSpeciesGetter(isolate_, array_function);

    // Cache the array maps, needed by ArrayConstructorStub
    CacheInitialJSArrayMaps(native_context(), initial_map);

    // Set up %ArrayPrototype%.
    // The %ArrayPrototype% has TERMINAL_FAST_ELEMENTS_KIND in order to ensure
    // that constant functions stay constant after turning prototype to setup
    // mode and back when constant field tracking is enabled.
    Handle<JSArray> proto =
        factory->NewJSArray(0, TERMINAL_FAST_ELEMENTS_KIND, TENURED);
    JSFunction::SetPrototype(array_function, proto);
    native_context()->set_initial_array_prototype(*proto);

    Handle<JSFunction> is_arraylike = SimpleInstallFunction(
        isolate_, array_function, "isArray", Builtins::kArrayIsArray, 1, true);
    native_context()->set_is_arraylike(*is_arraylike);

    SimpleInstallFunction(isolate_, array_function, "from",
                          Builtins::kArrayFrom, 1, false);
    SimpleInstallFunction(isolate_, array_function, "of", Builtins::kArrayOf, 0,
                          false);

    JSObject::AddProperty(isolate_, proto, factory->constructor_string(),
                          array_function, DONT_ENUM);

    SimpleInstallFunction(isolate_, proto, "concat", Builtins::kArrayConcat, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "copyWithin",
                          Builtins::kArrayPrototypeCopyWithin, 2, false);
    SimpleInstallFunction(isolate_, proto, "fill",
                          Builtins::kArrayPrototypeFill, 1, false);
    SimpleInstallFunction(isolate_, proto, "find",
                          Builtins::kArrayPrototypeFind, 1, false);
    SimpleInstallFunction(isolate_, proto, "findIndex",
                          Builtins::kArrayPrototypeFindIndex, 1, false);
    SimpleInstallFunction(isolate_, proto, "lastIndexOf",
                          Builtins::kArrayPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, proto, "pop", Builtins::kArrayPrototypePop,
                          0, false);
    SimpleInstallFunction(isolate_, proto, "push",
                          Builtins::kArrayPrototypePush, 1, false);
    SimpleInstallFunction(isolate_, proto, "reverse",
                          Builtins::kArrayPrototypeReverse, 0, false);
    SimpleInstallFunction(isolate_, proto, "shift",
                          Builtins::kArrayPrototypeShift, 0, false);
    SimpleInstallFunction(isolate_, proto, "unshift",
                          Builtins::kArrayPrototypeUnshift, 1, false);
    SimpleInstallFunction(isolate_, proto, "slice",
                          Builtins::kArrayPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, proto, "sort",
                          Builtins::kArrayPrototypeSort, 1, false);
    SimpleInstallFunction(isolate_, proto, "splice", Builtins::kArraySplice, 2,
                          false);
    SimpleInstallFunction(isolate_, proto, "includes", Builtins::kArrayIncludes,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "indexOf", Builtins::kArrayIndexOf,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "keys",
                          Builtins::kArrayPrototypeKeys, 0, true,
                          BuiltinFunctionId::kArrayKeys);
    SimpleInstallFunction(isolate_, proto, "entries",
                          Builtins::kArrayPrototypeEntries, 0, true,
                          BuiltinFunctionId::kArrayEntries);
    SimpleInstallFunction(isolate_, proto, factory->iterator_symbol(), "values",
                          Builtins::kArrayPrototypeValues, 0, true, DONT_ENUM,
                          BuiltinFunctionId::kArrayValues);
    SimpleInstallFunction(isolate_, proto, "forEach", Builtins::kArrayForEach,
                          1, false);
    SimpleInstallFunction(isolate_, proto, "filter", Builtins::kArrayFilter, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "map", Builtins::kArrayMap, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "every", Builtins::kArrayEvery, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "some", Builtins::kArraySome, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "reduce", Builtins::kArrayReduce, 1,
                          false);
    SimpleInstallFunction(isolate_, proto, "reduceRight",
                          Builtins::kArrayReduceRight, 1, false);
  }

  {  // --- A r r a y I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> array_iterator_prototype =
        factory->NewJSObject(isolate_->object_function(), TENURED);
    JSObject::ForceSetPrototype(array_iterator_prototype, iterator_prototype);

    JSObject::AddProperty(
        isolate_, array_iterator_prototype, factory->to_string_tag_symbol(),
        factory->ArrayIterator_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    SimpleInstallFunction(isolate_, array_iterator_prototype, "next",
                          Builtins::kArrayIteratorPrototypeNext, 0, true,
                          BuiltinFunctionId::kArrayIteratorNext);

    Handle<JSFunction> array_iterator_function =
        CreateFunction(isolate_, factory->ArrayIterator_string(),
                       JS_ARRAY_ITERATOR_TYPE, JSArrayIterator::kSize, 0,
                       array_iterator_prototype, Builtins::kIllegal);
    array_iterator_function->shared()->set_native(false);

    native_context()->set_initial_array_iterator_map(
        array_iterator_function->initial_map());
    native_context()->set_initial_array_iterator_prototype(
        *array_iterator_prototype);
  }

  {  // --- N u m b e r ---
    Handle<JSFunction> number_fun = InstallFunction(
        isolate_, global, "Number", JS_VALUE_TYPE, JSValue::kSize, 0,
        isolate_->initial_object_prototype(), Builtins::kNumberConstructor);
    number_fun->shared()->set_builtin_function_id(
        BuiltinFunctionId::kNumberConstructor);
    number_fun->shared()->DontAdaptArguments();
    number_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, number_fun,
                                     Context::NUMBER_FUNCTION_INDEX);

    // Create the %NumberPrototype%
    Handle<JSValue> prototype =
        Handle<JSValue>::cast(factory->NewJSObject(number_fun, TENURED));
    prototype->set_value(Smi::kZero);
    JSFunction::SetPrototype(number_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          number_fun, DONT_ENUM);

    // Install the Number.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toExponential",
                          Builtins::kNumberPrototypeToExponential, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toFixed",
                          Builtins::kNumberPrototypeToFixed, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toPrecision",
                          Builtins::kNumberPrototypeToPrecision, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kNumberPrototypeToString, 1, false);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kNumberPrototypeValueOf, 0, true);

    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kNumberPrototypeToLocaleString, 0, false);

    // Install the Number functions.
    SimpleInstallFunction(isolate_, number_fun, "isFinite",
                          Builtins::kNumberIsFinite, 1, true);
    SimpleInstallFunction(isolate_, number_fun, "isInteger",
                          Builtins::kNumberIsInteger, 1, true);
    SimpleInstallFunction(isolate_, number_fun, "isNaN", Builtins::kNumberIsNaN,
                          1, true);
    SimpleInstallFunction(isolate_, number_fun, "isSafeInteger",
                          Builtins::kNumberIsSafeInteger, 1, true);

    // Install Number.parseFloat and Global.parseFloat.
    Handle<JSFunction> parse_float_fun =
        SimpleInstallFunction(isolate_, number_fun, "parseFloat",
                              Builtins::kNumberParseFloat, 1, true);
    JSObject::AddProperty(isolate_, global_object,
                          factory->InternalizeUtf8String("parseFloat"),
                          parse_float_fun, DONT_ENUM);

    // Install Number.parseInt and Global.parseInt.
    Handle<JSFunction> parse_int_fun = SimpleInstallFunction(
        isolate_, number_fun, "parseInt", Builtins::kNumberParseInt, 2, true);
    JSObject::AddProperty(isolate_, global_object,
                          factory->InternalizeUtf8String("parseInt"),
                          parse_int_fun, DONT_ENUM);

    // Install Number constants
    double kMaxValue = 1.7976931348623157e+308;
    double kMinValue = 5e-324;
    double kMinSafeInteger = -kMaxSafeInteger;
    double kEPS = 2.220446049250313e-16;

    Handle<Object> infinity = factory->infinity_value();
    Handle<Object> nan = factory->nan_value();
    Handle<String> nan_name = factory->InternalizeUtf8String("NaN");

    JSObject::AddProperty(
        isolate_, number_fun, factory->InternalizeUtf8String("MAX_VALUE"),
        factory->NewNumber(kMaxValue),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, number_fun, factory->InternalizeUtf8String("MIN_VALUE"),
        factory->NewNumber(kMinValue),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, number_fun, nan_name, nan,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, number_fun,
        factory->InternalizeUtf8String("NEGATIVE_INFINITY"),
        factory->NewNumber(-V8_INFINITY),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, number_fun,
        factory->InternalizeUtf8String("POSITIVE_INFINITY"), infinity,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, number_fun,
        factory->InternalizeUtf8String("MAX_SAFE_INTEGER"),
        factory->NewNumber(kMaxSafeInteger),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, number_fun,
        factory->InternalizeUtf8String("MIN_SAFE_INTEGER"),
        factory->NewNumber(kMinSafeInteger),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, number_fun, factory->InternalizeUtf8String("EPSILON"),
        factory->NewNumber(kEPS),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));

    JSObject::AddProperty(
        isolate_, global, factory->InternalizeUtf8String("Infinity"), infinity,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, global, nan_name, nan,
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
    JSObject::AddProperty(
        isolate_, global, factory->InternalizeUtf8String("undefined"),
        factory->undefined_value(),
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY));
  }

  {  // --- B o o l e a n ---
    Handle<JSFunction> boolean_fun = InstallFunction(
        isolate_, global, "Boolean", JS_VALUE_TYPE, JSValue::kSize, 0,
        isolate_->initial_object_prototype(), Builtins::kBooleanConstructor);
    boolean_fun->shared()->DontAdaptArguments();
    boolean_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, boolean_fun,
                                     Context::BOOLEAN_FUNCTION_INDEX);

    // Create the %BooleanPrototype%
    Handle<JSValue> prototype =
        Handle<JSValue>::cast(factory->NewJSObject(boolean_fun, TENURED));
    prototype->set_value(ReadOnlyRoots(isolate_).false_value());
    JSFunction::SetPrototype(boolean_fun, prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          boolean_fun, DONT_ENUM);

    // Install the Boolean.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kBooleanPrototypeToString, 0, true);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kBooleanPrototypeValueOf, 0, true);
  }

  {  // --- S t r i n g ---
    Handle<JSFunction> string_fun = InstallFunction(
        isolate_, global, "String", JS_VALUE_TYPE, JSValue::kSize, 0,
        isolate_->initial_object_prototype(), Builtins::kStringConstructor);
    string_fun->shared()->set_builtin_function_id(
        BuiltinFunctionId::kStringConstructor);
    string_fun->shared()->DontAdaptArguments();
    string_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, string_fun,
                                     Context::STRING_FUNCTION_INDEX);

    Handle<Map> string_map = Handle<Map>(
        native_context()->string_function()->initial_map(), isolate());
    string_map->set_elements_kind(FAST_STRING_WRAPPER_ELEMENTS);
    Map::EnsureDescriptorSlack(isolate_, string_map, 1);

    PropertyAttributes attribs = static_cast<PropertyAttributes>(
        DONT_ENUM | DONT_DELETE | READ_ONLY);

    {  // Add length.
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->string_length_accessor(), attribs);
      string_map->AppendDescriptor(&d);
    }

    // Install the String.fromCharCode function.
    SimpleInstallFunction(isolate_, string_fun, "fromCharCode",
                          Builtins::kStringFromCharCode, 1, false);

    // Install the String.fromCodePoint function.
    SimpleInstallFunction(isolate_, string_fun, "fromCodePoint",
                          Builtins::kStringFromCodePoint, 1, false);

    // Install the String.raw function.
    SimpleInstallFunction(isolate_, string_fun, "raw", Builtins::kStringRaw, 1,
                          false);

    // Create the %StringPrototype%
    Handle<JSValue> prototype =
        Handle<JSValue>::cast(factory->NewJSObject(string_fun, TENURED));
    prototype->set_value(ReadOnlyRoots(isolate_).empty_string());
    JSFunction::SetPrototype(string_fun, prototype);
    native_context()->set_initial_string_prototype(*prototype);

    // Install the "constructor" property on the {prototype}.
    JSObject::AddProperty(isolate_, prototype, factory->constructor_string(),
                          string_fun, DONT_ENUM);

    // Install the String.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "anchor",
                          Builtins::kStringPrototypeAnchor, 1, true);
    SimpleInstallFunction(isolate_, prototype, "big",
                          Builtins::kStringPrototypeBig, 0, true);
    SimpleInstallFunction(isolate_, prototype, "blink",
                          Builtins::kStringPrototypeBlink, 0, true);
    SimpleInstallFunction(isolate_, prototype, "bold",
                          Builtins::kStringPrototypeBold, 0, true);
    SimpleInstallFunction(isolate_, prototype, "charAt",
                          Builtins::kStringPrototypeCharAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "charCodeAt",
                          Builtins::kStringPrototypeCharCodeAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "codePointAt",
                          Builtins::kStringPrototypeCodePointAt, 1, true);
    SimpleInstallFunction(isolate_, prototype, "concat",
                          Builtins::kStringPrototypeConcat, 1, false);
    SimpleInstallFunction(isolate_, prototype, "endsWith",
                          Builtins::kStringPrototypeEndsWith, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fontcolor",
                          Builtins::kStringPrototypeFontcolor, 1, true);
    SimpleInstallFunction(isolate_, prototype, "fontsize",
                          Builtins::kStringPrototypeFontsize, 1, true);
    SimpleInstallFunction(isolate_, prototype, "fixed",
                          Builtins::kStringPrototypeFixed, 0, true);
    SimpleInstallFunction(isolate_, prototype, "includes",
                          Builtins::kStringPrototypeIncludes, 1, false);
    SimpleInstallFunction(isolate_, prototype, "indexOf",
                          Builtins::kStringPrototypeIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "italics",
                          Builtins::kStringPrototypeItalics, 0, true);
    SimpleInstallFunction(isolate_, prototype, "lastIndexOf",
                          Builtins::kStringPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "link",
                          Builtins::kStringPrototypeLink, 1, true);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "localeCompare",
                          Builtins::kStringPrototypeLocaleCompare, 1, false);
#else
    SimpleInstallFunction(isolate_, prototype, "localeCompare",
                          Builtins::kStringPrototypeLocaleCompare, 1, true);
#endif  // V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "match",
                          Builtins::kStringPrototypeMatch, 1, true);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "normalize",
                          Builtins::kStringPrototypeNormalizeIntl, 0, false);
#else
    SimpleInstallFunction(isolate_, prototype, "normalize",
                          Builtins::kStringPrototypeNormalize, 0, false);
#endif  // V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "padEnd",
                          Builtins::kStringPrototypePadEnd, 1, false);
    SimpleInstallFunction(isolate_, prototype, "padStart",
                          Builtins::kStringPrototypePadStart, 1, false);
    SimpleInstallFunction(isolate_, prototype, "repeat",
                          Builtins::kStringPrototypeRepeat, 1, true);
    SimpleInstallFunction(isolate_, prototype, "replace",
                          Builtins::kStringPrototypeReplace, 2, true);
    SimpleInstallFunction(isolate_, prototype, "search",
                          Builtins::kStringPrototypeSearch, 1, true);
    SimpleInstallFunction(isolate_, prototype, "slice",
                          Builtins::kStringPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, prototype, "small",
                          Builtins::kStringPrototypeSmall, 0, true);
    SimpleInstallFunction(isolate_, prototype, "split",
                          Builtins::kStringPrototypeSplit, 2, false);
    SimpleInstallFunction(isolate_, prototype, "strike",
                          Builtins::kStringPrototypeStrike, 0, true);
    SimpleInstallFunction(isolate_, prototype, "sub",
                          Builtins::kStringPrototypeSub, 0, true);
    SimpleInstallFunction(isolate_, prototype, "substr",
                          Builtins::kStringPrototypeSubstr, 2, false);
    SimpleInstallFunction(isolate_, prototype, "substring",
                          Builtins::kStringPrototypeSubstring, 2, false);
    SimpleInstallFunction(isolate_, prototype, "sup",
                          Builtins::kStringPrototypeSup, 0, true);
    SimpleInstallFunction(isolate_, prototype, "startsWith",
                          Builtins::kStringPrototypeStartsWith, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kStringPrototypeToString, 0, true);
    SimpleInstallFunction(isolate_, prototype, "trim",
                          Builtins::kStringPrototypeTrim, 0, false);

    // Install `String.prototype.trimStart` with `trimLeft` alias.
    Handle<JSFunction> trim_start_fun =
        SimpleInstallFunction(isolate_, prototype, "trimStart",
                              Builtins::kStringPrototypeTrimStart, 0, false);
    JSObject::AddProperty(isolate_, prototype,
                          factory->InternalizeUtf8String("trimLeft"),
                          trim_start_fun, DONT_ENUM);

    // Install `String.prototype.trimEnd` with `trimRight` alias.
    Handle<JSFunction> trim_end_fun =
        SimpleInstallFunction(isolate_, prototype, "trimEnd",
                              Builtins::kStringPrototypeTrimEnd, 0, false);
    JSObject::AddProperty(isolate_, prototype,
                          factory->InternalizeUtf8String("trimRight"),
                          trim_end_fun, DONT_ENUM);

    SimpleInstallFunction(isolate_, prototype, "toLocaleLowerCase",
                          Builtins::kStringPrototypeToLocaleLowerCase, 0,
                          false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleUpperCase",
                          Builtins::kStringPrototypeToLocaleUpperCase, 0,
                          false);
#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "toLowerCase",
                          Builtins::kStringPrototypeToLowerCaseIntl, 0, true);
    SimpleInstallFunction(isolate_, prototype, "toUpperCase",
                          Builtins::kStringPrototypeToUpperCaseIntl, 0, false);
#else
    SimpleInstallFunction(isolate_, prototype, "toLowerCase",
                          Builtins::kStringPrototypeToLowerCase, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toUpperCase",
                          Builtins::kStringPrototypeToUpperCase, 0, false);
#endif
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kStringPrototypeValueOf, 0, true);

    SimpleInstallFunction(isolate_, prototype, factory->iterator_symbol(),
                          "[Symbol.iterator]",
                          Builtins::kStringPrototypeIterator, 0, true,
                          DONT_ENUM, BuiltinFunctionId::kStringIterator);
  }

  {  // --- S t r i n g I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> string_iterator_prototype =
        factory->NewJSObject(isolate_->object_function(), TENURED);
    JSObject::ForceSetPrototype(string_iterator_prototype, iterator_prototype);

    JSObject::AddProperty(
        isolate_, string_iterator_prototype, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("String Iterator"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    SimpleInstallFunction(isolate_, string_iterator_prototype, "next",
                          Builtins::kStringIteratorPrototypeNext, 0, true,
                          BuiltinFunctionId::kStringIteratorNext);

    Handle<JSFunction> string_iterator_function = CreateFunction(
        isolate_, factory->InternalizeUtf8String("StringIterator"),
        JS_STRING_ITERATOR_TYPE, JSStringIterator::kSize, 0,
        string_iterator_prototype, Builtins::kIllegal);
    string_iterator_function->shared()->set_native(false);
    native_context()->set_initial_string_iterator_map(
        string_iterator_function->initial_map());
    native_context()->set_initial_string_iterator_prototype(
        *string_iterator_prototype);
  }

  {  // --- S y m b o l ---
    Handle<JSFunction> symbol_fun = InstallFunction(
        isolate_, global, "Symbol", JS_VALUE_TYPE, JSValue::kSize, 0,
        factory->the_hole_value(), Builtins::kSymbolConstructor);
    symbol_fun->shared()->set_builtin_function_id(
        BuiltinFunctionId::kSymbolConstructor);
    symbol_fun->shared()->set_length(0);
    symbol_fun->shared()->DontAdaptArguments();
    native_context()->set_symbol_function(*symbol_fun);

    // Install the Symbol.for and Symbol.keyFor functions.
    SimpleInstallFunction(isolate_, symbol_fun, "for", Builtins::kSymbolFor, 1,
                          false);
    SimpleInstallFunction(isolate_, symbol_fun, "keyFor",
                          Builtins::kSymbolKeyFor, 1, false);

    // Install well-known symbols.
    InstallConstant(isolate_, symbol_fun, "asyncIterator",
                    factory->async_iterator_symbol());
    InstallConstant(isolate_, symbol_fun, "hasInstance",
                    factory->has_instance_symbol());
    InstallConstant(isolate_, symbol_fun, "isConcatSpreadable",
                    factory->is_concat_spreadable_symbol());
    InstallConstant(isolate_, symbol_fun, "iterator",
                    factory->iterator_symbol());
    InstallConstant(isolate_, symbol_fun, "match", factory->match_symbol());
    InstallConstant(isolate_, symbol_fun, "replace", factory->replace_symbol());
    InstallConstant(isolate_, symbol_fun, "search", factory->search_symbol());
    InstallConstant(isolate_, symbol_fun, "species", factory->species_symbol());
    InstallConstant(isolate_, symbol_fun, "split", factory->split_symbol());
    InstallConstant(isolate_, symbol_fun, "toPrimitive",
                    factory->to_primitive_symbol());
    InstallConstant(isolate_, symbol_fun, "toStringTag",
                    factory->to_string_tag_symbol());
    InstallConstant(isolate_, symbol_fun, "unscopables",
                    factory->unscopables_symbol());

    // Setup %SymbolPrototype%.
    Handle<JSObject> prototype(JSObject::cast(symbol_fun->instance_prototype()),
                               isolate());

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("Symbol"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Install the Symbol.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kSymbolPrototypeToString, 0, true,
                          BuiltinFunctionId::kSymbolPrototypeToString);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kSymbolPrototypeValueOf, 0, true,
                          BuiltinFunctionId::kSymbolPrototypeValueOf);

    // Install the @@toPrimitive function.
    Handle<JSFunction> to_primitive = InstallFunction(
        isolate_, prototype, factory->to_primitive_symbol(), JS_OBJECT_TYPE,
        JSObject::kHeaderSize, 0, MaybeHandle<JSObject>(),
        Builtins::kSymbolPrototypeToPrimitive,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Set the expected parameters for @@toPrimitive to 1; required by builtin.
    to_primitive->shared()->set_internal_formal_parameter_count(1);

    // Set the length for the function to satisfy ECMA-262.
    to_primitive->shared()->set_length(1);
  }

  {  // --- D a t e ---
    Handle<JSFunction> date_fun = InstallFunction(
        isolate_, global, "Date", JS_DATE_TYPE, JSDate::kSize, 0,
        factory->the_hole_value(), Builtins::kDateConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, date_fun,
                                     Context::DATE_FUNCTION_INDEX);
    date_fun->shared()->set_length(7);
    date_fun->shared()->DontAdaptArguments();

    // Install the Date.now, Date.parse and Date.UTC functions.
    SimpleInstallFunction(isolate_, date_fun, "now", Builtins::kDateNow, 0,
                          false);
    SimpleInstallFunction(isolate_, date_fun, "parse", Builtins::kDateParse, 1,
                          false);
    SimpleInstallFunction(isolate_, date_fun, "UTC", Builtins::kDateUTC, 7,
                          false);

    // Setup %DatePrototype%.
    Handle<JSObject> prototype(JSObject::cast(date_fun->instance_prototype()),
                               isolate());

    // Install the Date.prototype methods.
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toDateString",
                          Builtins::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toTimeString",
                          Builtins::kDatePrototypeToTimeString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toISOString",
                          Builtins::kDatePrototypeToISOString, 0, false);
    Handle<JSFunction> to_utc_string =
        SimpleInstallFunction(isolate_, prototype, "toUTCString",
                              Builtins::kDatePrototypeToUTCString, 0, false);
    InstallFunction(isolate_, prototype, to_utc_string,
                    factory->InternalizeUtf8String("toGMTString"), DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, "getDate",
                          Builtins::kDatePrototypeGetDate, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setDate",
                          Builtins::kDatePrototypeSetDate, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getDay",
                          Builtins::kDatePrototypeGetDay, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getFullYear",
                          Builtins::kDatePrototypeGetFullYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setFullYear",
                          Builtins::kDatePrototypeSetFullYear, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getHours",
                          Builtins::kDatePrototypeGetHours, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setHours",
                          Builtins::kDatePrototypeSetHours, 4, false);
    SimpleInstallFunction(isolate_, prototype, "getMilliseconds",
                          Builtins::kDatePrototypeGetMilliseconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMilliseconds",
                          Builtins::kDatePrototypeSetMilliseconds, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getMinutes",
                          Builtins::kDatePrototypeGetMinutes, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMinutes",
                          Builtins::kDatePrototypeSetMinutes, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getMonth",
                          Builtins::kDatePrototypeGetMonth, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setMonth",
                          Builtins::kDatePrototypeSetMonth, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getSeconds",
                          Builtins::kDatePrototypeGetSeconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setSeconds",
                          Builtins::kDatePrototypeSetSeconds, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getTime",
                          Builtins::kDatePrototypeGetTime, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setTime",
                          Builtins::kDatePrototypeSetTime, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getTimezoneOffset",
                          Builtins::kDatePrototypeGetTimezoneOffset, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getUTCDate",
                          Builtins::kDatePrototypeGetUTCDate, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCDate",
                          Builtins::kDatePrototypeSetUTCDate, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCDay",
                          Builtins::kDatePrototypeGetUTCDay, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getUTCFullYear",
                          Builtins::kDatePrototypeGetUTCFullYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCFullYear",
                          Builtins::kDatePrototypeSetUTCFullYear, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCHours",
                          Builtins::kDatePrototypeGetUTCHours, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCHours",
                          Builtins::kDatePrototypeSetUTCHours, 4, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMilliseconds",
                          Builtins::kDatePrototypeGetUTCMilliseconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMilliseconds",
                          Builtins::kDatePrototypeSetUTCMilliseconds, 1, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMinutes",
                          Builtins::kDatePrototypeGetUTCMinutes, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMinutes",
                          Builtins::kDatePrototypeSetUTCMinutes, 3, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCMonth",
                          Builtins::kDatePrototypeGetUTCMonth, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCMonth",
                          Builtins::kDatePrototypeSetUTCMonth, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUTCSeconds",
                          Builtins::kDatePrototypeGetUTCSeconds, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setUTCSeconds",
                          Builtins::kDatePrototypeSetUTCSeconds, 2, false);
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kDatePrototypeValueOf, 0, true);
    SimpleInstallFunction(isolate_, prototype, "getYear",
                          Builtins::kDatePrototypeGetYear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "setYear",
                          Builtins::kDatePrototypeSetYear, 1, false);
    SimpleInstallFunction(isolate_, prototype, "toJSON",
                          Builtins::kDatePrototypeToJson, 1, false);

#ifdef V8_INTL_SUPPORT
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kDatePrototypeToLocaleString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleDateString",
                          Builtins::kDatePrototypeToLocaleDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleTimeString",
                          Builtins::kDatePrototypeToLocaleTimeString, 0, false);
#else
    // Install Intl fallback functions.
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kDatePrototypeToString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleDateString",
                          Builtins::kDatePrototypeToDateString, 0, false);
    SimpleInstallFunction(isolate_, prototype, "toLocaleTimeString",
                          Builtins::kDatePrototypeToTimeString, 0, false);
#endif  // V8_INTL_SUPPORT

    // Install the @@toPrimitive function.
    Handle<JSFunction> to_primitive = InstallFunction(
        isolate_, prototype, factory->to_primitive_symbol(), JS_OBJECT_TYPE,
        JSObject::kHeaderSize, 0, MaybeHandle<JSObject>(),
        Builtins::kDatePrototypeToPrimitive,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Set the expected parameters for @@toPrimitive to 1; required by builtin.
    to_primitive->shared()->set_internal_formal_parameter_count(1);

    // Set the length for the function to satisfy ECMA-262.
    to_primitive->shared()->set_length(1);
  }

  {
    Handle<SharedFunctionInfo> info = SimpleCreateBuiltinSharedFunctionInfo(
        isolate_, Builtins::kPromiseGetCapabilitiesExecutor,
        factory->empty_string(), 2);
    native_context()->set_promise_get_capabilities_executor_shared_fun(*info);
  }

  {  // -- P r o m i s e
    Handle<JSFunction> promise_fun = InstallFunction(
        isolate_, global, "Promise", JS_PROMISE_TYPE,
        JSPromise::kSizeWithEmbedderFields, 0, factory->the_hole_value(),
        Builtins::kPromiseConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, promise_fun,
                                     Context::PROMISE_FUNCTION_INDEX);

    Handle<SharedFunctionInfo> shared(promise_fun->shared(), isolate_);
    shared->set_internal_formal_parameter_count(1);
    shared->set_length(1);

    InstallSpeciesGetter(isolate_, promise_fun);

    SimpleInstallFunction(isolate_, promise_fun, "all", Builtins::kPromiseAll,
                          1, true);

    SimpleInstallFunction(isolate_, promise_fun, "race", Builtins::kPromiseRace,
                          1, true);

    SimpleInstallFunction(isolate_, promise_fun, "resolve",
                          Builtins::kPromiseResolveTrampoline, 1, true);

    SimpleInstallFunction(isolate_, promise_fun, "reject",
                          Builtins::kPromiseReject, 1, true);

    // Setup %PromisePrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(promise_fun->instance_prototype()), isolate());
    native_context()->set_promise_prototype(*prototype);

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->Promise_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<JSFunction> promise_then = SimpleInstallFunction(
        isolate_, prototype, isolate_->factory()->then_string(),
        Builtins::kPromisePrototypeThen, 2, true);
    native_context()->set_promise_then(*promise_then);

    Handle<JSFunction> promise_catch =
        SimpleInstallFunction(isolate_, prototype, "catch",
                              Builtins::kPromisePrototypeCatch, 1, true);
    native_context()->set_promise_catch(*promise_catch);

    SimpleInstallFunction(isolate_, prototype, "finally",
                          Builtins::kPromisePrototypeFinally, 1, true,
                          DONT_ENUM);

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseThenFinally,
          isolate_->factory()->empty_string(), 1);
      info->set_native(true);
      native_context()->set_promise_then_finally_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseCatchFinally,
          isolate_->factory()->empty_string(), 1);
      info->set_native(true);
      native_context()->set_promise_catch_finally_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseValueThunkFinally,
          isolate_->factory()->empty_string(), 0);
      native_context()->set_promise_value_thunk_finally_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate(), Builtins::kPromiseThrowerFinally,
          isolate_->factory()->empty_string(), 0);
      native_context()->set_promise_thrower_finally_shared_fun(*info);
    }

    // Force the Promise constructor to fast properties, so that we can use the
    // fast paths for various things like
    //
    //   x instanceof Promise
    //
    // etc. We should probably come up with a more principled approach once
    // the JavaScript builtins are gone.
    JSObject::MigrateSlowToFast(Handle<JSObject>::cast(promise_fun), 0,
                                "Bootstrapping");

    Handle<Map> prototype_map(prototype->map(), isolate());
    Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate_);

    {  // Internal: IsPromise
      Handle<JSFunction> function = SimpleCreateFunction(
          isolate_, factory->empty_string(), Builtins::kIsPromise, 1, false);
      native_context()->set_is_promise(*function);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kPromiseCapabilityDefaultResolve,
          factory->empty_string(), 1, FunctionKind::kConciseMethod);
      info->set_native(true);
      info->set_function_map_index(
          Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
      native_context()->set_promise_capability_default_resolve_shared_fun(
          *info);

      info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kPromiseCapabilityDefaultReject,
          factory->empty_string(), 1, FunctionKind::kConciseMethod);
      info->set_native(true);
      info->set_function_map_index(
          Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
      native_context()->set_promise_capability_default_reject_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kPromiseAllResolveElementClosure,
          factory->empty_string(), 1);
      native_context()->set_promise_all_resolve_element_shared_fun(*info);
    }

    // Force the Promise constructor to fast properties, so that we can use the
    // fast paths for various things like
    //
    //   x instanceof Promise
    //
    // etc. We should probably come up with a more principled approach once
    // the JavaScript builtins are gone.
    JSObject::MigrateSlowToFast(promise_fun, 0, "Bootstrapping");
  }

  {  // -- R e g E x p
    // Builtin functions for RegExp.prototype.
    Handle<JSFunction> regexp_fun = InstallFunction(
        isolate_, global, "RegExp", JS_REGEXP_TYPE,
        JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize,
        JSRegExp::kInObjectFieldCount, factory->the_hole_value(),
        Builtins::kRegExpConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, regexp_fun,
                                     Context::REGEXP_FUNCTION_INDEX);

    Handle<SharedFunctionInfo> shared(regexp_fun->shared(), isolate_);
    shared->set_internal_formal_parameter_count(2);
    shared->set_length(2);

    {
      // Setup %RegExpPrototype%.
      Handle<JSObject> prototype(
          JSObject::cast(regexp_fun->instance_prototype()), isolate());

      {
        Handle<JSFunction> fun = SimpleInstallFunction(
            isolate_, prototype, factory->exec_string(),
            Builtins::kRegExpPrototypeExec, 1, true, DONT_ENUM);
        native_context()->set_regexp_exec_function(*fun);
      }

      SimpleInstallGetter(isolate_, prototype, factory->dotAll_string(),
                          Builtins::kRegExpPrototypeDotAllGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->flags_string(),
                          Builtins::kRegExpPrototypeFlagsGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->global_string(),
                          Builtins::kRegExpPrototypeGlobalGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->ignoreCase_string(),
                          Builtins::kRegExpPrototypeIgnoreCaseGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->multiline_string(),
                          Builtins::kRegExpPrototypeMultilineGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->source_string(),
                          Builtins::kRegExpPrototypeSourceGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->sticky_string(),
                          Builtins::kRegExpPrototypeStickyGetter, true);
      SimpleInstallGetter(isolate_, prototype, factory->unicode_string(),
                          Builtins::kRegExpPrototypeUnicodeGetter, true);

      SimpleInstallFunction(isolate_, prototype, "compile",
                            Builtins::kRegExpPrototypeCompile, 2, true,
                            DONT_ENUM);
      SimpleInstallFunction(isolate_, prototype, factory->toString_string(),
                            Builtins::kRegExpPrototypeToString, 0, false,
                            DONT_ENUM);
      SimpleInstallFunction(isolate_, prototype, "test",
                            Builtins::kRegExpPrototypeTest, 1, true, DONT_ENUM);

      SimpleInstallFunction(isolate_, prototype, factory->match_symbol(),
                            "[Symbol.match]", Builtins::kRegExpPrototypeMatch,
                            1, true);

      SimpleInstallFunction(isolate_, prototype, factory->replace_symbol(),
                            "[Symbol.replace]",
                            Builtins::kRegExpPrototypeReplace, 2, false);

      SimpleInstallFunction(isolate_, prototype, factory->search_symbol(),
                            "[Symbol.search]", Builtins::kRegExpPrototypeSearch,
                            1, true);

      SimpleInstallFunction(isolate_, prototype, factory->split_symbol(),
                            "[Symbol.split]", Builtins::kRegExpPrototypeSplit,
                            2, false);

      Handle<Map> prototype_map(prototype->map(), isolate());
      Map::SetShouldBeFastPrototypeMap(prototype_map, true, isolate_);

      // Store the initial RegExp.prototype map. This is used in fast-path
      // checks. Do not alter the prototype after this point.
      native_context()->set_regexp_prototype_map(*prototype_map);
    }

    {
      // RegExp getters and setters.

      InstallSpeciesGetter(isolate_, regexp_fun);

      // Static properties set by a successful match.

      const PropertyAttributes no_enum = DONT_ENUM;
      SimpleInstallGetterSetter(isolate_, regexp_fun, factory->input_string(),
                                Builtins::kRegExpInputGetter,
                                Builtins::kRegExpInputSetter, no_enum);
      SimpleInstallGetterSetter(
          isolate_, regexp_fun, factory->InternalizeUtf8String("$_"),
          Builtins::kRegExpInputGetter, Builtins::kRegExpInputSetter, no_enum);

      SimpleInstallGetterSetter(
          isolate_, regexp_fun, factory->InternalizeUtf8String("lastMatch"),
          Builtins::kRegExpLastMatchGetter, Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(
          isolate_, regexp_fun, factory->InternalizeUtf8String("$&"),
          Builtins::kRegExpLastMatchGetter, Builtins::kEmptyFunction, no_enum);

      SimpleInstallGetterSetter(
          isolate_, regexp_fun, factory->InternalizeUtf8String("lastParen"),
          Builtins::kRegExpLastParenGetter, Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(
          isolate_, regexp_fun, factory->InternalizeUtf8String("$+"),
          Builtins::kRegExpLastParenGetter, Builtins::kEmptyFunction, no_enum);

      SimpleInstallGetterSetter(isolate_, regexp_fun,
                                factory->InternalizeUtf8String("leftContext"),
                                Builtins::kRegExpLeftContextGetter,
                                Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(isolate_, regexp_fun,
                                factory->InternalizeUtf8String("$`"),
                                Builtins::kRegExpLeftContextGetter,
                                Builtins::kEmptyFunction, no_enum);

      SimpleInstallGetterSetter(isolate_, regexp_fun,
                                factory->InternalizeUtf8String("rightContext"),
                                Builtins::kRegExpRightContextGetter,
                                Builtins::kEmptyFunction, no_enum);
      SimpleInstallGetterSetter(isolate_, regexp_fun,
                                factory->InternalizeUtf8String("$'"),
                                Builtins::kRegExpRightContextGetter,
                                Builtins::kEmptyFunction, no_enum);

#define INSTALL_CAPTURE_GETTER(i)                                   \
  SimpleInstallGetterSetter(                                        \
      isolate_, regexp_fun, factory->InternalizeUtf8String("$" #i), \
      Builtins::kRegExpCapture##i##Getter, Builtins::kEmptyFunction, no_enum)
      INSTALL_CAPTURE_GETTER(1);
      INSTALL_CAPTURE_GETTER(2);
      INSTALL_CAPTURE_GETTER(3);
      INSTALL_CAPTURE_GETTER(4);
      INSTALL_CAPTURE_GETTER(5);
      INSTALL_CAPTURE_GETTER(6);
      INSTALL_CAPTURE_GETTER(7);
      INSTALL_CAPTURE_GETTER(8);
      INSTALL_CAPTURE_GETTER(9);
#undef INSTALL_CAPTURE_GETTER
    }

    DCHECK(regexp_fun->has_initial_map());
    Handle<Map> initial_map(regexp_fun->initial_map(), isolate());

    DCHECK_EQ(1, initial_map->GetInObjectProperties());

    Map::EnsureDescriptorSlack(isolate_, initial_map, 1);

    // ECMA-262, section 15.10.7.5.
    PropertyAttributes writable =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
    Descriptor d = Descriptor::DataField(isolate(), factory->lastIndex_string(),
                                         JSRegExp::kLastIndexFieldIndex,
                                         writable, Representation::Tagged());
    initial_map->AppendDescriptor(&d);

    {  // Internal: RegExpInternalMatch
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate_, isolate_->factory()->empty_string(),
                               Builtins::kRegExpInternalMatch, 2, true);
      native_context()->set(Context::REGEXP_INTERNAL_MATCH, *function);
    }

    // Create the last match info. One for external use, and one for internal
    // use when we don't want to modify the externally visible match info.
    Handle<RegExpMatchInfo> last_match_info = factory->NewRegExpMatchInfo();
    native_context()->set_regexp_last_match_info(*last_match_info);
    Handle<RegExpMatchInfo> internal_match_info = factory->NewRegExpMatchInfo();
    native_context()->set_regexp_internal_match_info(*internal_match_info);

    // Force the RegExp constructor to fast properties, so that we can use the
    // fast paths for various things like
    //
    //   x instanceof RegExp
    //
    // etc. We should probably come up with a more principled approach once
    // the JavaScript builtins are gone.
    JSObject::MigrateSlowToFast(regexp_fun, 0, "Bootstrapping");
  }

  {  // -- E r r o r
    InstallError(isolate_, global, factory->Error_string(),
                 Context::ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeError, Context::MAKE_ERROR_INDEX);
  }

  {  // -- E v a l E r r o r
    InstallError(isolate_, global, factory->EvalError_string(),
                 Context::EVAL_ERROR_FUNCTION_INDEX);
  }

  {  // -- R a n g e E r r o r
    InstallError(isolate_, global, factory->RangeError_string(),
                 Context::RANGE_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeRangeError,
                     Context::MAKE_RANGE_ERROR_INDEX);
  }

  {  // -- R e f e r e n c e E r r o r
    InstallError(isolate_, global, factory->ReferenceError_string(),
                 Context::REFERENCE_ERROR_FUNCTION_INDEX);
  }

  {  // -- S y n t a x E r r o r
    InstallError(isolate_, global, factory->SyntaxError_string(),
                 Context::SYNTAX_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeSyntaxError,
                     Context::MAKE_SYNTAX_ERROR_INDEX);
  }

  {  // -- T y p e E r r o r
    InstallError(isolate_, global, factory->TypeError_string(),
                 Context::TYPE_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeTypeError,
                     Context::MAKE_TYPE_ERROR_INDEX);
  }

  {  // -- U R I E r r o r
    InstallError(isolate_, global, factory->URIError_string(),
                 Context::URI_ERROR_FUNCTION_INDEX);
    InstallMakeError(isolate_, Builtins::kMakeURIError,
                     Context::MAKE_URI_ERROR_INDEX);
  }

  {  // -- C o m p i l e E r r o r
    Handle<JSObject> dummy = factory->NewJSObject(isolate_->object_function());
    InstallError(isolate_, dummy, factory->CompileError_string(),
                 Context::WASM_COMPILE_ERROR_FUNCTION_INDEX);

    // -- L i n k E r r o r
    InstallError(isolate_, dummy, factory->LinkError_string(),
                 Context::WASM_LINK_ERROR_FUNCTION_INDEX);

    // -- R u n t i m e E r r o r
    InstallError(isolate_, dummy, factory->RuntimeError_string(),
                 Context::WASM_RUNTIME_ERROR_FUNCTION_INDEX);
  }

  // Initialize the embedder data slot.
  native_context()->set_embedder_data(*factory->empty_fixed_array());

  {  // -- J S O N
    Handle<String> name = factory->InternalizeUtf8String("JSON");
    Handle<JSObject> json_object =
        factory->NewJSObject(isolate_->object_function(), TENURED);
    JSObject::AddProperty(isolate_, global, name, json_object, DONT_ENUM);
    SimpleInstallFunction(isolate_, json_object, "parse", Builtins::kJsonParse,
                          2, false);
    SimpleInstallFunction(isolate_, json_object, "stringify",
                          Builtins::kJsonStringify, 3, true);
    JSObject::AddProperty(
        isolate_, json_object, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("JSON"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // -- M a t h
    Handle<String> name = factory->InternalizeUtf8String("Math");
    Handle<JSObject> math =
        factory->NewJSObject(isolate_->object_function(), TENURED);
    JSObject::AddProperty(isolate_, global, name, math, DONT_ENUM);
    SimpleInstallFunction(isolate_, math, "abs", Builtins::kMathAbs, 1, true);
    SimpleInstallFunction(isolate_, math, "acos", Builtins::kMathAcos, 1, true);
    SimpleInstallFunction(isolate_, math, "acosh", Builtins::kMathAcosh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "asin", Builtins::kMathAsin, 1, true);
    SimpleInstallFunction(isolate_, math, "asinh", Builtins::kMathAsinh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "atan", Builtins::kMathAtan, 1, true);
    SimpleInstallFunction(isolate_, math, "atanh", Builtins::kMathAtanh, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "atan2", Builtins::kMathAtan2, 2,
                          true);
    SimpleInstallFunction(isolate_, math, "ceil", Builtins::kMathCeil, 1, true);
    SimpleInstallFunction(isolate_, math, "cbrt", Builtins::kMathCbrt, 1, true);
    SimpleInstallFunction(isolate_, math, "expm1", Builtins::kMathExpm1, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "clz32", Builtins::kMathClz32, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "cos", Builtins::kMathCos, 1, true);
    SimpleInstallFunction(isolate_, math, "cosh", Builtins::kMathCosh, 1, true);
    SimpleInstallFunction(isolate_, math, "exp", Builtins::kMathExp, 1, true);
    Handle<JSFunction> math_floor = SimpleInstallFunction(
        isolate_, math, "floor", Builtins::kMathFloor, 1, true);
    native_context()->set_math_floor(*math_floor);
    SimpleInstallFunction(isolate_, math, "fround", Builtins::kMathFround, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "hypot", Builtins::kMathHypot, 2,
                          false);
    SimpleInstallFunction(isolate_, math, "imul", Builtins::kMathImul, 2, true);
    SimpleInstallFunction(isolate_, math, "log", Builtins::kMathLog, 1, true);
    SimpleInstallFunction(isolate_, math, "log1p", Builtins::kMathLog1p, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "log2", Builtins::kMathLog2, 1, true);
    SimpleInstallFunction(isolate_, math, "log10", Builtins::kMathLog10, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "max", Builtins::kMathMax, 2, false);
    SimpleInstallFunction(isolate_, math, "min", Builtins::kMathMin, 2, false);
    Handle<JSFunction> math_pow = SimpleInstallFunction(
        isolate_, math, "pow", Builtins::kMathPow, 2, true);
    native_context()->set_math_pow(*math_pow);
    SimpleInstallFunction(isolate_, math, "random", Builtins::kMathRandom, 0,
                          true);
    SimpleInstallFunction(isolate_, math, "round", Builtins::kMathRound, 1,
                          true);
    SimpleInstallFunction(isolate_, math, "sign", Builtins::kMathSign, 1, true);
    SimpleInstallFunction(isolate_, math, "sin", Builtins::kMathSin, 1, true);
    SimpleInstallFunction(isolate_, math, "sinh", Builtins::kMathSinh, 1, true);
    SimpleInstallFunction(isolate_, math, "sqrt", Builtins::kMathSqrt, 1, true);
    SimpleInstallFunction(isolate_, math, "tan", Builtins::kMathTan, 1, true);
    SimpleInstallFunction(isolate_, math, "tanh", Builtins::kMathTanh, 1, true);
    SimpleInstallFunction(isolate_, math, "trunc", Builtins::kMathTrunc, 1,
                          true);

    // Install math constants.
    double const kE = base::ieee754::exp(1.0);
    double const kPI = 3.1415926535897932;
    InstallConstant(isolate_, math, "E", factory->NewNumber(kE));
    InstallConstant(isolate_, math, "LN10",
                    factory->NewNumber(base::ieee754::log(10.0)));
    InstallConstant(isolate_, math, "LN2",
                    factory->NewNumber(base::ieee754::log(2.0)));
    InstallConstant(isolate_, math, "LOG10E",
                    factory->NewNumber(base::ieee754::log10(kE)));
    InstallConstant(isolate_, math, "LOG2E",
                    factory->NewNumber(base::ieee754::log2(kE)));
    InstallConstant(isolate_, math, "PI", factory->NewNumber(kPI));
    InstallConstant(isolate_, math, "SQRT1_2",
                    factory->NewNumber(std::sqrt(0.5)));
    InstallConstant(isolate_, math, "SQRT2",
                    factory->NewNumber(std::sqrt(2.0)));
    JSObject::AddProperty(
        isolate_, math, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("Math"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // -- C o n s o l e
    Handle<String> name = factory->InternalizeUtf8String("console");
    NewFunctionArgs args = NewFunctionArgs::ForFunctionWithoutCode(
        name, isolate_->strict_function_map(), LanguageMode::kStrict);
    Handle<JSFunction> cons = factory->NewFunction(args);

    Handle<JSObject> empty = factory->NewJSObject(isolate_->object_function());
    JSFunction::SetPrototype(cons, empty);

    Handle<JSObject> console = factory->NewJSObject(cons, TENURED);
    DCHECK(console->IsJSObject());
    JSObject::AddProperty(isolate_, global, name, console, DONT_ENUM);
    SimpleInstallFunction(isolate_, console, "debug", Builtins::kConsoleDebug,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "error", Builtins::kConsoleError,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "info", Builtins::kConsoleInfo, 1,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "log", Builtins::kConsoleLog, 1,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "warn", Builtins::kConsoleWarn, 1,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "dir", Builtins::kConsoleDir, 1,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "dirxml", Builtins::kConsoleDirXml,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "table", Builtins::kConsoleTable,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "trace", Builtins::kConsoleTrace,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "group", Builtins::kConsoleGroup,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "groupCollapsed",
                          Builtins::kConsoleGroupCollapsed, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "groupEnd",
                          Builtins::kConsoleGroupEnd, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "clear", Builtins::kConsoleClear,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "count", Builtins::kConsoleCount,
                          1, false, NONE);
    SimpleInstallFunction(isolate_, console, "countReset",
                          Builtins::kConsoleCountReset, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "assert",
                          Builtins::kFastConsoleAssert, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "profile",
                          Builtins::kConsoleProfile, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "profileEnd",
                          Builtins::kConsoleProfileEnd, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "time", Builtins::kConsoleTime, 1,
                          false, NONE);
    SimpleInstallFunction(isolate_, console, "timeLog",
                          Builtins::kConsoleTimeLog, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "timeEnd",
                          Builtins::kConsoleTimeEnd, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "timeStamp",
                          Builtins::kConsoleTimeStamp, 1, false, NONE);
    SimpleInstallFunction(isolate_, console, "context",
                          Builtins::kConsoleContext, 1, true, NONE);
    JSObject::AddProperty(
        isolate_, console, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("Object"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

#ifdef V8_INTL_SUPPORT
  {  // -- I n t l
    Handle<String> name = factory->InternalizeUtf8String("Intl");
    Handle<JSObject> intl =
        factory->NewJSObject(isolate_->object_function(), TENURED);
    JSObject::AddProperty(isolate_, global, name, intl, DONT_ENUM);

    {
      Handle<JSFunction> date_time_format_constructor = InstallFunction(
          isolate_, intl, "DateTimeFormat", JS_INTL_DATE_TIME_FORMAT_TYPE,
          JSDateTimeFormat::kSize, 0, factory->the_hole_value(),
          Builtins::kDateTimeFormatConstructor);
      date_time_format_constructor->shared()->set_length(0);
      date_time_format_constructor->shared()->DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, date_time_format_constructor,
          Context::INTL_DATE_TIME_FORMAT_FUNCTION_INDEX);

      native_context()->set_intl_date_time_format_function(
          *date_time_format_constructor);

      SimpleInstallFunction(
          isolate(), date_time_format_constructor, "supportedLocalesOf",
          Builtins::kDateTimeFormatSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(date_time_format_constructor->prototype()), isolate_);

      // Install the @@toStringTag property on the {prototype}.
      JSObject::AddProperty(
          isolate_, prototype, factory->to_string_tag_symbol(),
          factory->Object_string(),
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kDateTimeFormatPrototypeResolvedOptions,
                            0, false);

      SimpleInstallFunction(isolate_, prototype, "formatToParts",
                            Builtins::kDateTimeFormatPrototypeFormatToParts, 1,
                            false);

      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("format"),
                          Builtins::kDateTimeFormatPrototypeFormat, false);
    }

    {
      Handle<JSFunction> number_format_constructor = InstallFunction(
          isolate_, intl, "NumberFormat", JS_INTL_NUMBER_FORMAT_TYPE,
          JSNumberFormat::kSize, 0, factory->the_hole_value(),
          Builtins::kNumberFormatConstructor);
      number_format_constructor->shared()->set_length(0);
      number_format_constructor->shared()->DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, number_format_constructor,
          Context::INTL_NUMBER_FORMAT_FUNCTION_INDEX);

      SimpleInstallFunction(
          isolate(), number_format_constructor, "supportedLocalesOf",
          Builtins::kNumberFormatSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(number_format_constructor->prototype()), isolate_);

      // Install the @@toStringTag property on the {prototype}.
      JSObject::AddProperty(
          isolate_, prototype, factory->to_string_tag_symbol(),
          factory->Object_string(),
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kNumberFormatPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallFunction(isolate_, prototype, "formatToParts",
                            Builtins::kNumberFormatPrototypeFormatToParts, 1,
                            false);
      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("format"),
                          Builtins::kNumberFormatPrototypeFormatNumber, false);
    }

    {
      Handle<JSFunction> collator_constructor = InstallFunction(
          isolate_, intl, "Collator", JS_INTL_COLLATOR_TYPE, JSCollator::kSize,
          0, factory->the_hole_value(), Builtins::kCollatorConstructor);
      collator_constructor->shared()->DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(isolate_, collator_constructor,
                                       Context::INTL_COLLATOR_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), collator_constructor,
                            "supportedLocalesOf",
                            Builtins::kCollatorSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(collator_constructor->prototype()), isolate_);

      // Install the @@toStringTag property on the {prototype}.
      JSObject::AddProperty(
          isolate_, prototype, factory->to_string_tag_symbol(),
          factory->Object_string(),
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kCollatorPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("compare"),
                          Builtins::kCollatorPrototypeCompare, false);
    }

    {
      Handle<JSFunction> v8_break_iterator_constructor = InstallFunction(
          isolate_, intl, "v8BreakIterator", JS_INTL_V8_BREAK_ITERATOR_TYPE,
          JSV8BreakIterator::kSize, 0, factory->the_hole_value(),
          Builtins::kV8BreakIteratorConstructor);
      v8_break_iterator_constructor->shared()->DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, v8_break_iterator_constructor,
          Context::INTL_V8_BREAK_ITERATOR_FUNCTION_INDEX);

      SimpleInstallFunction(
          isolate_, v8_break_iterator_constructor, "supportedLocalesOf",
          Builtins::kV8BreakIteratorSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(v8_break_iterator_constructor->prototype()), isolate_);

      // Install the @@toStringTag property on the {prototype}.
      JSObject::AddProperty(
          isolate_, prototype, factory->to_string_tag_symbol(),
          factory->Object_string(),
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kV8BreakIteratorPrototypeResolvedOptions,
                            0, false);

      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("adoptText"),
                          Builtins::kV8BreakIteratorPrototypeAdoptText, false);

      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("first"),
                          Builtins::kV8BreakIteratorPrototypeFirst, false);

      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("next"),
                          Builtins::kV8BreakIteratorPrototypeNext, false);

      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("current"),
                          Builtins::kV8BreakIteratorPrototypeCurrent, false);

      SimpleInstallGetter(isolate_, prototype,
                          factory->InternalizeUtf8String("breakType"),
                          Builtins::kV8BreakIteratorPrototypeBreakType, false);
    }

    {
      Handle<JSFunction> plural_rules_constructor = InstallFunction(
          isolate_, intl, "PluralRules", JS_INTL_PLURAL_RULES_TYPE,
          JSPluralRules::kSize, 0, factory->the_hole_value(),
          Builtins::kPluralRulesConstructor);
      plural_rules_constructor->shared()->DontAdaptArguments();
      InstallWithIntrinsicDefaultProto(
          isolate_, plural_rules_constructor,
          Context::INTL_PLURAL_RULES_FUNCTION_INDEX);

      SimpleInstallFunction(isolate(), plural_rules_constructor,
                            "supportedLocalesOf",
                            Builtins::kPluralRulesSupportedLocalesOf, 1, false);

      Handle<JSObject> prototype(
          JSObject::cast(plural_rules_constructor->prototype()), isolate_);

      // Install the @@toStringTag property on the {prototype}.
      JSObject::AddProperty(
          isolate_, prototype, factory->to_string_tag_symbol(),
          factory->Object_string(),
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

      SimpleInstallFunction(isolate_, prototype, "resolvedOptions",
                            Builtins::kPluralRulesPrototypeResolvedOptions, 0,
                            false);

      SimpleInstallFunction(isolate_, prototype, "select",
                            Builtins::kPluralRulesPrototypeSelect, 1, false);
    }
  }
#endif  // V8_INTL_SUPPORT

  {  // -- A r r a y B u f f e r
    Handle<String> name = factory->ArrayBuffer_string();
    Handle<JSFunction> array_buffer_fun = CreateArrayBuffer(name, ARRAY_BUFFER);
    JSObject::AddProperty(isolate_, global, name, array_buffer_fun, DONT_ENUM);
    InstallWithIntrinsicDefaultProto(isolate_, array_buffer_fun,
                                     Context::ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, array_buffer_fun);

    Handle<JSFunction> array_buffer_noinit_fun = SimpleCreateFunction(
        isolate_,
        factory->InternalizeUtf8String(
            "arrayBufferConstructor_DoNotInitialize"),
        Builtins::kArrayBufferConstructor_DoNotInitialize, 1, false);
    native_context()->set_array_buffer_noinit_fun(*array_buffer_noinit_fun);
  }

  {  // -- S h a r e d A r r a y B u f f e r
    Handle<String> name = factory->SharedArrayBuffer_string();
    Handle<JSFunction> shared_array_buffer_fun =
        CreateArrayBuffer(name, SHARED_ARRAY_BUFFER);
    InstallWithIntrinsicDefaultProto(isolate_, shared_array_buffer_fun,
                                     Context::SHARED_ARRAY_BUFFER_FUN_INDEX);
    InstallSpeciesGetter(isolate_, shared_array_buffer_fun);
  }

  {  // -- A t o m i c s
    Handle<JSObject> atomics_object =
        factory->NewJSObject(isolate_->object_function(), TENURED);
    native_context()->set_atomics_object(*atomics_object);

    SimpleInstallFunction(isolate_, atomics_object, "load",
                          Builtins::kAtomicsLoad, 2, true);
    SimpleInstallFunction(isolate_, atomics_object, "store",
                          Builtins::kAtomicsStore, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "add",
                          Builtins::kAtomicsAdd, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "sub",
                          Builtins::kAtomicsSub, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "and",
                          Builtins::kAtomicsAnd, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "or", Builtins::kAtomicsOr,
                          3, true);
    SimpleInstallFunction(isolate_, atomics_object, "xor",
                          Builtins::kAtomicsXor, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "exchange",
                          Builtins::kAtomicsExchange, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "compareExchange",
                          Builtins::kAtomicsCompareExchange, 4, true);
    SimpleInstallFunction(isolate_, atomics_object, "isLockFree",
                          Builtins::kAtomicsIsLockFree, 1, true);
    SimpleInstallFunction(isolate_, atomics_object, "wait",
                          Builtins::kAtomicsWait, 4, true);
    SimpleInstallFunction(isolate_, atomics_object, "wake",
                          Builtins::kAtomicsWake, 3, true);
    SimpleInstallFunction(isolate_, atomics_object, "notify",
                          Builtins::kAtomicsNotify, 3, true);
  }

  {  // -- T y p e d A r r a y
    Handle<JSFunction> typed_array_fun = CreateFunction(
        isolate_, factory->InternalizeUtf8String("TypedArray"),
        JS_TYPED_ARRAY_TYPE, JSTypedArray::kSize, 0, factory->the_hole_value(),
        Builtins::kTypedArrayBaseConstructor);
    typed_array_fun->shared()->set_native(false);
    typed_array_fun->shared()->set_length(0);
    InstallSpeciesGetter(isolate_, typed_array_fun);
    native_context()->set_typed_array_function(*typed_array_fun);

    SimpleInstallFunction(isolate_, typed_array_fun, "of",
                          Builtins::kTypedArrayOf, 0, false);
    SimpleInstallFunction(isolate_, typed_array_fun, "from",
                          Builtins::kTypedArrayFrom, 1, false);

    // Setup %TypedArrayPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(typed_array_fun->instance_prototype()), isolate());
    native_context()->set_typed_array_prototype(*prototype);

    // Install the "buffer", "byteOffset", "byteLength", "length"
    // and @@toStringTag getters on the {prototype}.
    SimpleInstallGetter(isolate_, prototype, factory->buffer_string(),
                        Builtins::kTypedArrayPrototypeBuffer, false);
    SimpleInstallGetter(isolate_, prototype, factory->byte_length_string(),
                        Builtins::kTypedArrayPrototypeByteLength, true,
                        BuiltinFunctionId::kTypedArrayByteLength);
    SimpleInstallGetter(isolate_, prototype, factory->byte_offset_string(),
                        Builtins::kTypedArrayPrototypeByteOffset, true,
                        BuiltinFunctionId::kTypedArrayByteOffset);
    SimpleInstallGetter(isolate_, prototype, factory->length_string(),
                        Builtins::kTypedArrayPrototypeLength, true,
                        BuiltinFunctionId::kTypedArrayLength);
    SimpleInstallGetter(isolate_, prototype, factory->to_string_tag_symbol(),
                        Builtins::kTypedArrayPrototypeToStringTag, true,
                        BuiltinFunctionId::kTypedArrayToStringTag);

    // Install "keys", "values" and "entries" methods on the {prototype}.
    SimpleInstallFunction(isolate_, prototype, "entries",
                          Builtins::kTypedArrayPrototypeEntries, 0, true,
                          BuiltinFunctionId::kTypedArrayEntries);

    SimpleInstallFunction(isolate_, prototype, "keys",
                          Builtins::kTypedArrayPrototypeKeys, 0, true,
                          BuiltinFunctionId::kTypedArrayKeys);

    Handle<JSFunction> values = SimpleInstallFunction(
        isolate_, prototype, "values", Builtins::kTypedArrayPrototypeValues, 0,
        true, BuiltinFunctionId::kTypedArrayValues);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          values, DONT_ENUM);

    // TODO(caitp): alphasort accessors/methods
    SimpleInstallFunction(isolate_, prototype, "copyWithin",
                          Builtins::kTypedArrayPrototypeCopyWithin, 2, false);
    SimpleInstallFunction(isolate_, prototype, "every",
                          Builtins::kTypedArrayPrototypeEvery, 1, false);
    SimpleInstallFunction(isolate_, prototype, "fill",
                          Builtins::kTypedArrayPrototypeFill, 1, false);
    SimpleInstallFunction(isolate_, prototype, "filter",
                          Builtins::kTypedArrayPrototypeFilter, 1, false);
    SimpleInstallFunction(isolate_, prototype, "find",
                          Builtins::kTypedArrayPrototypeFind, 1, false);
    SimpleInstallFunction(isolate_, prototype, "findIndex",
                          Builtins::kTypedArrayPrototypeFindIndex, 1, false);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtins::kTypedArrayPrototypeForEach, 1, false);
    SimpleInstallFunction(isolate_, prototype, "includes",
                          Builtins::kTypedArrayPrototypeIncludes, 1, false);
    SimpleInstallFunction(isolate_, prototype, "indexOf",
                          Builtins::kTypedArrayPrototypeIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "lastIndexOf",
                          Builtins::kTypedArrayPrototypeLastIndexOf, 1, false);
    SimpleInstallFunction(isolate_, prototype, "map",
                          Builtins::kTypedArrayPrototypeMap, 1, false);
    SimpleInstallFunction(isolate_, prototype, "reverse",
                          Builtins::kTypedArrayPrototypeReverse, 0, false);
    SimpleInstallFunction(isolate_, prototype, "reduce",
                          Builtins::kTypedArrayPrototypeReduce, 1, false);
    SimpleInstallFunction(isolate_, prototype, "reduceRight",
                          Builtins::kTypedArrayPrototypeReduceRight, 1, false);
    SimpleInstallFunction(isolate_, prototype, "set",
                          Builtins::kTypedArrayPrototypeSet, 1, false);
    SimpleInstallFunction(isolate_, prototype, "slice",
                          Builtins::kTypedArrayPrototypeSlice, 2, false);
    SimpleInstallFunction(isolate_, prototype, "some",
                          Builtins::kTypedArrayPrototypeSome, 1, false);
    SimpleInstallFunction(isolate_, prototype, "sort",
                          Builtins::kTypedArrayPrototypeSort, 1, false);
    SimpleInstallFunction(isolate_, prototype, "subarray",
                          Builtins::kTypedArrayPrototypeSubArray, 2, false);
  }

  {  // -- T y p e d A r r a y s
#define INSTALL_TYPED_ARRAY(Type, type, TYPE, ctype)                   \
  {                                                                    \
    Handle<JSFunction> fun =                                           \
        InstallTypedArray(#Type "Array", TYPE##_ELEMENTS);             \
    InstallWithIntrinsicDefaultProto(isolate_, fun,                    \
                                     Context::TYPE##_ARRAY_FUN_INDEX); \
  }
    TYPED_ARRAYS(INSTALL_TYPED_ARRAY)
#undef INSTALL_TYPED_ARRAY
  }

  {  // -- D a t a V i e w
    Handle<JSFunction> data_view_fun = InstallFunction(
        isolate_, global, "DataView", JS_DATA_VIEW_TYPE,
        JSDataView::kSizeWithEmbedderFields, 0, factory->the_hole_value(),
        Builtins::kDataViewConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, data_view_fun,
                                     Context::DATA_VIEW_FUN_INDEX);
    data_view_fun->shared()->set_length(1);
    data_view_fun->shared()->DontAdaptArguments();

    // Setup %DataViewPrototype%.
    Handle<JSObject> prototype(
        JSObject::cast(data_view_fun->instance_prototype()), isolate());

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("DataView"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Install the "buffer", "byteOffset" and "byteLength" getters
    // on the {prototype}.
    SimpleInstallGetter(isolate_, prototype, factory->buffer_string(),
                        Builtins::kDataViewPrototypeGetBuffer, false,
                        BuiltinFunctionId::kDataViewBuffer);
    SimpleInstallGetter(isolate_, prototype, factory->byte_length_string(),
                        Builtins::kDataViewPrototypeGetByteLength, false,
                        BuiltinFunctionId::kDataViewByteLength);
    SimpleInstallGetter(isolate_, prototype, factory->byte_offset_string(),
                        Builtins::kDataViewPrototypeGetByteOffset, false,
                        BuiltinFunctionId::kDataViewByteOffset);

    SimpleInstallFunction(isolate_, prototype, "getInt8",
                          Builtins::kDataViewPrototypeGetInt8, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt8",
                          Builtins::kDataViewPrototypeSetInt8, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint8",
                          Builtins::kDataViewPrototypeGetUint8, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint8",
                          Builtins::kDataViewPrototypeSetUint8, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getInt16",
                          Builtins::kDataViewPrototypeGetInt16, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt16",
                          Builtins::kDataViewPrototypeSetInt16, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint16",
                          Builtins::kDataViewPrototypeGetUint16, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint16",
                          Builtins::kDataViewPrototypeSetUint16, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getInt32",
                          Builtins::kDataViewPrototypeGetInt32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setInt32",
                          Builtins::kDataViewPrototypeSetInt32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getUint32",
                          Builtins::kDataViewPrototypeGetUint32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setUint32",
                          Builtins::kDataViewPrototypeSetUint32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getFloat32",
                          Builtins::kDataViewPrototypeGetFloat32, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setFloat32",
                          Builtins::kDataViewPrototypeSetFloat32, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getFloat64",
                          Builtins::kDataViewPrototypeGetFloat64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setFloat64",
                          Builtins::kDataViewPrototypeSetFloat64, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getBigInt64",
                          Builtins::kDataViewPrototypeGetBigInt64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setBigInt64",
                          Builtins::kDataViewPrototypeSetBigInt64, 2, false);
    SimpleInstallFunction(isolate_, prototype, "getBigUint64",
                          Builtins::kDataViewPrototypeGetBigUint64, 1, false);
    SimpleInstallFunction(isolate_, prototype, "setBigUint64",
                          Builtins::kDataViewPrototypeSetBigUint64, 2, false);
  }

  {  // -- M a p
    Handle<JSFunction> js_map_fun =
        InstallFunction(isolate_, global, "Map", JS_MAP_TYPE, JSMap::kSize, 0,
                        factory->the_hole_value(), Builtins::kMapConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, js_map_fun,
                                     Context::JS_MAP_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(js_map_fun->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %MapPrototype%.
    Handle<JSObject> prototype(JSObject::cast(js_map_fun->instance_prototype()),
                               isolate());

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->Map_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<JSFunction> map_get = SimpleInstallFunction(
        isolate_, prototype, "get", Builtins::kMapPrototypeGet, 1, true);
    native_context()->set_map_get(*map_get);

    Handle<JSFunction> map_set = SimpleInstallFunction(
        isolate_, prototype, "set", Builtins::kMapPrototypeSet, 2, true);
    native_context()->set_map_set(*map_set);

    Handle<JSFunction> map_has = SimpleInstallFunction(
        isolate_, prototype, "has", Builtins::kMapPrototypeHas, 1, true);
    native_context()->set_map_has(*map_has);

    Handle<JSFunction> map_delete = SimpleInstallFunction(
        isolate_, prototype, "delete", Builtins::kMapPrototypeDelete, 1, true);
    native_context()->set_map_delete(*map_delete);

    SimpleInstallFunction(isolate_, prototype, "clear",
                          Builtins::kMapPrototypeClear, 0, true);
    Handle<JSFunction> entries =
        SimpleInstallFunction(isolate_, prototype, "entries",
                              Builtins::kMapPrototypeEntries, 0, true);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          entries, DONT_ENUM);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtins::kMapPrototypeForEach, 1, false);
    SimpleInstallFunction(isolate_, prototype, "keys",
                          Builtins::kMapPrototypeKeys, 0, true);
    SimpleInstallGetter(
        isolate_, prototype, factory->InternalizeUtf8String("size"),
        Builtins::kMapPrototypeGetSize, true, BuiltinFunctionId::kMapSize);
    SimpleInstallFunction(isolate_, prototype, "values",
                          Builtins::kMapPrototypeValues, 0, true);

    native_context()->set_initial_map_prototype_map(prototype->map());

    InstallSpeciesGetter(isolate_, js_map_fun);
  }

  {  // -- B i g I n t
    Handle<JSFunction> bigint_fun = InstallFunction(
        isolate_, global, "BigInt", JS_VALUE_TYPE, JSValue::kSize, 0,
        factory->the_hole_value(), Builtins::kBigIntConstructor);
    bigint_fun->shared()->set_builtin_function_id(
        BuiltinFunctionId::kBigIntConstructor);
    bigint_fun->shared()->DontAdaptArguments();
    bigint_fun->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(isolate_, bigint_fun,
                                     Context::BIGINT_FUNCTION_INDEX);

    // Install the properties of the BigInt constructor.
    // asUintN(bits, bigint)
    SimpleInstallFunction(isolate_, bigint_fun, "asUintN",
                          Builtins::kBigIntAsUintN, 2, false);
    // asIntN(bits, bigint)
    SimpleInstallFunction(isolate_, bigint_fun, "asIntN",
                          Builtins::kBigIntAsIntN, 2, false);

    // Set up the %BigIntPrototype%.
    Handle<JSObject> prototype(JSObject::cast(bigint_fun->instance_prototype()),
                               isolate_);
    JSFunction::SetPrototype(bigint_fun, prototype);

    // Install the properties of the BigInt.prototype.
    // "constructor" is created implicitly by InstallFunction() above.
    // toLocaleString([reserved1 [, reserved2]])
    SimpleInstallFunction(isolate_, prototype, "toLocaleString",
                          Builtins::kBigIntPrototypeToLocaleString, 0, false);
    // toString([radix])
    SimpleInstallFunction(isolate_, prototype, "toString",
                          Builtins::kBigIntPrototypeToString, 0, false);
    // valueOf()
    SimpleInstallFunction(isolate_, prototype, "valueOf",
                          Builtins::kBigIntPrototypeValueOf, 0, false);
    // @@toStringTag
    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->BigInt_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }

  {  // -- S e t
    Handle<JSFunction> js_set_fun =
        InstallFunction(isolate_, global, "Set", JS_SET_TYPE, JSSet::kSize, 0,
                        factory->the_hole_value(), Builtins::kSetConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, js_set_fun,
                                     Context::JS_SET_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(js_set_fun->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %SetPrototype%.
    Handle<JSObject> prototype(JSObject::cast(js_set_fun->instance_prototype()),
                               isolate());

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->Set_string(),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    Handle<JSFunction> set_has = SimpleInstallFunction(
        isolate_, prototype, "has", Builtins::kSetPrototypeHas, 1, true);
    native_context()->set_set_has(*set_has);

    Handle<JSFunction> set_add = SimpleInstallFunction(
        isolate_, prototype, "add", Builtins::kSetPrototypeAdd, 1, true);
    native_context()->set_set_add(*set_add);

    Handle<JSFunction> set_delete = SimpleInstallFunction(
        isolate_, prototype, "delete", Builtins::kSetPrototypeDelete, 1, true);
    native_context()->set_set_delete(*set_delete);

    SimpleInstallFunction(isolate_, prototype, "clear",
                          Builtins::kSetPrototypeClear, 0, true);
    SimpleInstallFunction(isolate_, prototype, "entries",
                          Builtins::kSetPrototypeEntries, 0, true);
    SimpleInstallFunction(isolate_, prototype, "forEach",
                          Builtins::kSetPrototypeForEach, 1, false);
    SimpleInstallGetter(
        isolate_, prototype, factory->InternalizeUtf8String("size"),
        Builtins::kSetPrototypeGetSize, true, BuiltinFunctionId::kSetSize);
    Handle<JSFunction> values = SimpleInstallFunction(
        isolate_, prototype, "values", Builtins::kSetPrototypeValues, 0, true);
    JSObject::AddProperty(isolate_, prototype, factory->keys_string(), values,
                          DONT_ENUM);
    JSObject::AddProperty(isolate_, prototype, factory->iterator_symbol(),
                          values, DONT_ENUM);

    native_context()->set_initial_set_prototype_map(prototype->map());

    InstallSpeciesGetter(isolate_, js_set_fun);
  }

  {  // -- J S M o d u l e N a m e s p a c e
    Handle<Map> map = factory->NewMap(
        JS_MODULE_NAMESPACE_TYPE, JSModuleNamespace::kSize,
        TERMINAL_FAST_ELEMENTS_KIND, JSModuleNamespace::kInObjectFieldCount);
    Map::SetPrototype(isolate(), map, isolate_->factory()->null_value());
    Map::EnsureDescriptorSlack(isolate_, map, 1);
    native_context()->set_js_module_namespace_map(*map);

    {  // Install @@toStringTag.
      PropertyAttributes attribs =
          static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY);
      Descriptor d =
          Descriptor::DataField(isolate(), factory->to_string_tag_symbol(),
                                JSModuleNamespace::kToStringTagFieldIndex,
                                attribs, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
  }

  {  // -- I t e r a t o r R e s u l t
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSIteratorResult::kSize,
                                      TERMINAL_FAST_ELEMENTS_KIND, 2);
    Map::SetPrototype(isolate(), map, isolate_->initial_object_prototype());
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // value
      Descriptor d = Descriptor::DataField(isolate(), factory->value_string(),
                                           JSIteratorResult::kValueIndex, NONE,
                                           Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    {  // done
      Descriptor d = Descriptor::DataField(isolate(), factory->done_string(),
                                           JSIteratorResult::kDoneIndex, NONE,
                                           Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    map->SetConstructor(native_context()->object_function());
    native_context()->set_iterator_result_map(*map);
  }

  {  // -- W e a k M a p
    Handle<JSFunction> cons = InstallFunction(
        isolate_, global, "WeakMap", JS_WEAK_MAP_TYPE, JSWeakMap::kSize, 0,
        factory->the_hole_value(), Builtins::kWeakMapConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, cons,
                                     Context::JS_WEAK_MAP_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(cons->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %WeakMapPrototype%.
    Handle<JSObject> prototype(JSObject::cast(cons->instance_prototype()),
                               isolate());

    SimpleInstallFunction(isolate_, prototype, "delete",
                          Builtins::kWeakMapPrototypeDelete, 1, true);
    Handle<JSFunction> weakmap_get = SimpleInstallFunction(
        isolate_, prototype, "get", Builtins::kWeakMapGet, 1, true);
    native_context()->set_weakmap_get(*weakmap_get);
    SimpleInstallFunction(isolate_, prototype, "has", Builtins::kWeakMapHas, 1,
                          true);
    Handle<JSFunction> weakmap_set = SimpleInstallFunction(
        isolate_, prototype, "set", Builtins::kWeakMapPrototypeSet, 2, true);
    native_context()->set_weakmap_set(*weakmap_set);

    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("WeakMap"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context()->set_initial_weakmap_prototype_map(prototype->map());
  }

  {  // -- W e a k S e t
    Handle<JSFunction> cons = InstallFunction(
        isolate_, global, "WeakSet", JS_WEAK_SET_TYPE, JSWeakSet::kSize, 0,
        factory->the_hole_value(), Builtins::kWeakSetConstructor);
    InstallWithIntrinsicDefaultProto(isolate_, cons,
                                     Context::JS_WEAK_SET_FUN_INDEX);

    Handle<SharedFunctionInfo> shared(cons->shared(), isolate_);
    shared->DontAdaptArguments();
    shared->set_length(0);

    // Setup %WeakSetPrototype%.
    Handle<JSObject> prototype(JSObject::cast(cons->instance_prototype()),
                               isolate());

    SimpleInstallFunction(isolate_, prototype, "delete",
                          Builtins::kWeakSetPrototypeDelete, 1, true);
    SimpleInstallFunction(isolate_, prototype, "has", Builtins::kWeakSetHas, 1,
                          true);
    Handle<JSFunction> weakset_add = SimpleInstallFunction(
        isolate_, prototype, "add", Builtins::kWeakSetPrototypeAdd, 1, true);
    native_context()->set_weakset_add(*weakset_add);

    JSObject::AddProperty(
        isolate_, prototype, factory->to_string_tag_symbol(),
        factory->InternalizeUtf8String("WeakSet"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context()->set_initial_weakset_prototype_map(prototype->map());
  }

  {  // -- P r o x y
    CreateJSProxyMaps();
    // Proxy function map has prototype slot for storing initial map but does
    // not have a prototype property.
    Handle<Map> proxy_function_map = Map::Copy(
        isolate_, isolate_->strict_function_without_prototype_map(), "Proxy");
    proxy_function_map->set_is_constructor(true);

    Handle<String> name = factory->Proxy_string();

    NewFunctionArgs args = NewFunctionArgs::ForBuiltin(
        name, proxy_function_map, Builtins::kProxyConstructor);
    Handle<JSFunction> proxy_function = factory->NewFunction(args);

    isolate_->proxy_map()->SetConstructor(*proxy_function);

    proxy_function->shared()->set_internal_formal_parameter_count(2);
    proxy_function->shared()->set_length(2);

    native_context()->set_proxy_function(*proxy_function);
    InstallFunction(isolate_, global, name, proxy_function,
                    factory->Object_string());

    DCHECK(!proxy_function->has_prototype_property());

    SimpleInstallFunction(isolate_, proxy_function, "revocable",
                          Builtins::kProxyRevocable, 2, true);

    {  // Internal: ProxyRevoke
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate_, Builtins::kProxyRevoke, factory->empty_string(), 0);
      native_context()->set_proxy_revoke_shared_fun(*info);
    }
  }

  {  // -- R e f l e c t
    Handle<String> reflect_string = factory->InternalizeUtf8String("Reflect");
    Handle<JSObject> reflect =
        factory->NewJSObject(isolate_->object_function(), TENURED);
    JSObject::AddProperty(isolate_, global, reflect_string, reflect, DONT_ENUM);

    Handle<JSFunction> define_property = SimpleInstallFunction(
        isolate_, reflect, factory->defineProperty_string(),
        Builtins::kReflectDefineProperty, 3, true);
    native_context()->set_reflect_define_property(*define_property);

    Handle<JSFunction> delete_property = SimpleInstallFunction(
        isolate_, reflect, factory->deleteProperty_string(),
        Builtins::kReflectDeleteProperty, 2, true);
    native_context()->set_reflect_delete_property(*delete_property);

    Handle<JSFunction> apply =
        SimpleInstallFunction(isolate_, reflect, factory->apply_string(),
                              Builtins::kReflectApply, 3, false);
    native_context()->set_reflect_apply(*apply);

    Handle<JSFunction> construct =
        SimpleInstallFunction(isolate_, reflect, factory->construct_string(),
                              Builtins::kReflectConstruct, 2, false);
    native_context()->set_reflect_construct(*construct);

    SimpleInstallFunction(isolate_, reflect, factory->get_string(),
                          Builtins::kReflectGet, 2, false);
    SimpleInstallFunction(isolate_, reflect,
                          factory->getOwnPropertyDescriptor_string(),
                          Builtins::kReflectGetOwnPropertyDescriptor, 2, true);
    SimpleInstallFunction(isolate_, reflect, factory->getPrototypeOf_string(),
                          Builtins::kReflectGetPrototypeOf, 1, true);
    SimpleInstallFunction(isolate_, reflect, factory->has_string(),
                          Builtins::kReflectHas, 2, true);
    SimpleInstallFunction(isolate_, reflect, factory->isExtensible_string(),
                          Builtins::kReflectIsExtensible, 1, true);
    SimpleInstallFunction(isolate_, reflect, factory->ownKeys_string(),
                          Builtins::kReflectOwnKeys, 1, true);
    SimpleInstallFunction(isolate_, reflect,
                          factory->preventExtensions_string(),
                          Builtins::kReflectPreventExtensions, 1, true);
    SimpleInstallFunction(isolate_, reflect, factory->set_string(),
                          Builtins::kReflectSet, 3, false);
    SimpleInstallFunction(isolate_, reflect, factory->setPrototypeOf_string(),
                          Builtins::kReflectSetPrototypeOf, 2, true);
  }

  {  // --- B o u n d F u n c t i o n
    Handle<Map> map =
        factory->NewMap(JS_BOUND_FUNCTION_TYPE, JSBoundFunction::kSize,
                        TERMINAL_FAST_ELEMENTS_KIND, 0);
    map->SetConstructor(native_context()->object_function());
    map->set_is_callable(true);
    Map::SetPrototype(isolate(), map, empty_function);

    PropertyAttributes roc_attribs =
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d = Descriptor::AccessorConstant(
          factory->length_string(), factory->bound_function_length_accessor(),
          roc_attribs);
      map->AppendDescriptor(&d);
    }

    {  // name
      Descriptor d = Descriptor::AccessorConstant(
          factory->name_string(), factory->bound_function_name_accessor(),
          roc_attribs);
      map->AppendDescriptor(&d);
    }
    native_context()->set_bound_function_without_constructor_map(*map);

    map = Map::Copy(isolate_, map, "IsConstructor");
    map->set_is_constructor(true);
    native_context()->set_bound_function_with_constructor_map(*map);
  }

  {  // --- sloppy arguments map
    Handle<String> arguments_string = factory->Arguments_string();
    NewFunctionArgs args = NewFunctionArgs::ForBuiltinWithPrototype(
        arguments_string, isolate_->initial_object_prototype(),
        JS_ARGUMENTS_TYPE, JSSloppyArgumentsObject::kSize, 2,
        Builtins::kIllegal, MUTABLE);
    Handle<JSFunction> function = factory->NewFunction(args);
    Handle<Map> map(function->initial_map(), isolate());

    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d =
          Descriptor::DataField(isolate(), factory->length_string(),
                                JSSloppyArgumentsObject::kLengthIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      Descriptor d =
          Descriptor::DataField(isolate(), factory->callee_string(),
                                JSSloppyArgumentsObject::kCalleeIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    // @@iterator method is added later.

    native_context()->set_sloppy_arguments_map(*map);

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsObjectElementsKind(map->elements_kind()));
  }

  {  // --- fast and slow aliased arguments map
    Handle<Map> map = isolate_->sloppy_arguments_map();
    map = Map::Copy(isolate_, map, "FastAliasedArguments");
    map->set_elements_kind(FAST_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_fast_aliased_arguments_map(*map);

    map = Map::Copy(isolate_, map, "SlowAliasedArguments");
    map->set_elements_kind(SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    DCHECK_EQ(2, map->GetInObjectProperties());
    native_context()->set_slow_aliased_arguments_map(*map);
  }

  {  // --- strict mode arguments map
    const PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

    // Create the ThrowTypeError function.
    Handle<AccessorPair> callee = factory->NewAccessorPair();

    Handle<JSFunction> poison = GetThrowTypeErrorIntrinsic();

    // Install the ThrowTypeError function.
    callee->set_getter(*poison);
    callee->set_setter(*poison);

    // Create the map. Allocate one in-object field for length.
    Handle<Map> map = factory->NewMap(
        JS_ARGUMENTS_TYPE, JSStrictArgumentsObject::kSize, PACKED_ELEMENTS, 1);
    // Create the descriptor array for the arguments object.
    Map::EnsureDescriptorSlack(isolate_, map, 2);

    {  // length
      Descriptor d =
          Descriptor::DataField(isolate(), factory->length_string(),
                                JSStrictArgumentsObject::kLengthIndex,
                                DONT_ENUM, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // callee
      Descriptor d = Descriptor::AccessorConstant(factory->callee_string(),
                                                  callee, attributes);
      map->AppendDescriptor(&d);
    }
    // @@iterator method is added later.

    DCHECK_EQ(native_context()->object_function()->prototype(),
              *isolate_->initial_object_prototype());
    Map::SetPrototype(isolate(), map, isolate_->initial_object_prototype());

    // Copy constructor from the sloppy arguments boilerplate.
    map->SetConstructor(
        native_context()->sloppy_arguments_map()->GetConstructor());

    native_context()->set_strict_arguments_map(*map);

    DCHECK(!map->is_dictionary_map());
    DCHECK(IsObjectElementsKind(map->elements_kind()));
  }

  {  // --- context extension
    // Create a function for the context extension objects.
    Handle<JSFunction> context_extension_fun =
        CreateFunction(isolate_, factory->empty_string(),
                       JS_CONTEXT_EXTENSION_OBJECT_TYPE, JSObject::kHeaderSize,
                       0, factory->the_hole_value(), Builtins::kIllegal);
    native_context()->set_context_extension_function(*context_extension_fun);
  }

  {
    // Set up the call-as-function delegate.
    Handle<JSFunction> delegate =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtins::kHandleApiCallAsFunction, 0, false);
    native_context()->set_call_as_function_delegate(*delegate);
  }

  {
    // Set up the call-as-constructor delegate.
    Handle<JSFunction> delegate =
        SimpleCreateFunction(isolate_, factory->empty_string(),
                             Builtins::kHandleApiCallAsConstructor, 0, false);
    native_context()->set_call_as_constructor_delegate(*delegate);
  }
}  // NOLINT(readability/fn_size)

Handle<JSFunction> Genesis::InstallTypedArray(const char* name,
                                              ElementsKind elements_kind) {
  Handle<JSObject> global =
      Handle<JSObject>(native_context()->global_object(), isolate());

  Handle<JSObject> typed_array_prototype = isolate()->typed_array_prototype();
  Handle<JSFunction> typed_array_function = isolate()->typed_array_function();

  Handle<JSFunction> result = InstallFunction(
      isolate(), global, name, JS_TYPED_ARRAY_TYPE,
      JSTypedArray::kSizeWithEmbedderFields, 0, factory()->the_hole_value(),
      Builtins::kTypedArrayConstructor);
  result->initial_map()->set_elements_kind(elements_kind);

  result->shared()->DontAdaptArguments();
  result->shared()->set_length(3);

  CHECK(JSObject::SetPrototype(result, typed_array_function, false, kDontThrow)
            .FromJust());

  Handle<Smi> bytes_per_element(
      Smi::FromInt(1 << ElementsKindToShiftSize(elements_kind)), isolate());

  InstallConstant(isolate(), result, "BYTES_PER_ELEMENT", bytes_per_element);

  // Setup prototype object.
  DCHECK(result->prototype()->IsJSObject());
  Handle<JSObject> prototype(JSObject::cast(result->prototype()), isolate());

  CHECK(JSObject::SetPrototype(prototype, typed_array_prototype, false,
                               kDontThrow)
            .FromJust());

  InstallConstant(isolate(), prototype, "BYTES_PER_ELEMENT", bytes_per_element);
  return result;
}


void Genesis::InitializeExperimentalGlobal() {
#define FEATURE_INITIALIZE_GLOBAL(id, descr) InitializeGlobal_##id();

  HARMONY_INPROGRESS(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_STAGED(FEATURE_INITIALIZE_GLOBAL)
  HARMONY_SHIPPING(FEATURE_INITIALIZE_GLOBAL)
#undef FEATURE_INITIALIZE_GLOBAL
}

bool Bootstrapper::CompileBuiltin(Isolate* isolate, int index) {
  Vector<const char> name = Natives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->GetNativeSource(CORE, index);

  // We pass in extras_utils so that builtin code can set it up for later use
  // by actual extras code, compiled with CompileExtraBuiltin.
  Handle<Object> global = isolate->global_object();
  Handle<Object> utils = isolate->natives_utils_object();
  Handle<Object> extras_utils = isolate->extras_utils_object();
  Handle<Object> args[] = {global, utils, extras_utils};

  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, NATIVES_CODE);
}


bool Bootstrapper::CompileExtraBuiltin(Isolate* isolate, int index) {
  HandleScope scope(isolate);
  Vector<const char> name = ExtraNatives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->GetNativeSource(EXTRAS, index);
  Handle<Object> global = isolate->global_object();
  Handle<Object> binding = isolate->extras_binding_object();
  Handle<Object> extras_utils = isolate->extras_utils_object();
  Handle<Object> args[] = {global, binding, extras_utils};
  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, EXTENSION_CODE);
}


bool Bootstrapper::CompileExperimentalExtraBuiltin(Isolate* isolate,
                                                   int index) {
  HandleScope scope(isolate);
  Vector<const char> name = ExperimentalExtraNatives::GetScriptName(index);
  Handle<String> source_code =
      isolate->bootstrapper()->GetNativeSource(EXPERIMENTAL_EXTRAS, index);
  Handle<Object> global = isolate->global_object();
  Handle<Object> binding = isolate->extras_binding_object();
  Handle<Object> extras_utils = isolate->extras_utils_object();
  Handle<Object> args[] = {global, binding, extras_utils};
  return Bootstrapper::CompileNative(isolate, name, source_code,
                                     arraysize(args), args, EXTENSION_CODE);
}

bool Bootstrapper::CompileNative(Isolate* isolate, Vector<const char> name,
                                 Handle<String> source, int argc,
                                 Handle<Object> argv[],
                                 NativesFlag natives_flag) {
  SuppressDebug compiling_natives(isolate->debug());

  Handle<Context> context(isolate->context(), isolate);
  Handle<String> script_name =
      isolate->factory()->NewStringFromUtf8(name).ToHandleChecked();
  MaybeHandle<SharedFunctionInfo> maybe_function_info =
      Compiler::GetSharedFunctionInfoForScript(
          isolate, source, Compiler::ScriptDetails(script_name),
          ScriptOriginOptions(), nullptr, nullptr,
          ScriptCompiler::kNoCompileOptions, ScriptCompiler::kNoCacheNoReason,
          natives_flag);
  Handle<SharedFunctionInfo> function_info;
  if (!maybe_function_info.ToHandle(&function_info)) return false;

  DCHECK(context->IsNativeContext());

  Handle<JSFunction> fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(function_info,
                                                            context);
  Handle<Object> receiver = isolate->factory()->undefined_value();

  // For non-extension scripts, run script to get the function wrapper.
  Handle<Object> wrapper;
  if (!Execution::TryCall(isolate, fun, receiver, 0, nullptr,
                          Execution::MessageHandling::kKeepPending, nullptr)
           .ToHandle(&wrapper)) {
    return false;
  }
  // Then run the function wrapper.
  return !Execution::TryCall(isolate, Handle<JSFunction>::cast(wrapper),
                             receiver, argc, argv,
                             Execution::MessageHandling::kKeepPending, nullptr)
              .is_null();
}


bool Genesis::CallUtilsFunction(Isolate* isolate, const char* name) {
  Handle<JSObject> utils =
      Handle<JSObject>::cast(isolate->natives_utils_object());
  Handle<String> name_string = isolate->factory()->InternalizeUtf8String(name);
  Handle<Object> fun = JSObject::GetDataProperty(utils, name_string);
  Handle<Object> receiver = isolate->factory()->undefined_value();
  Handle<Object> args[] = {utils};
  return !Execution::TryCall(isolate, fun, receiver, 1, args,
                             Execution::MessageHandling::kKeepPending, nullptr)
              .is_null();
}


bool Genesis::CompileExtension(Isolate* isolate, v8::Extension* extension) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<SharedFunctionInfo> function_info;

  Handle<String> source =
      isolate->factory()
          ->NewExternalStringFromOneByte(extension->source())
          .ToHandleChecked();
  DCHECK(source->IsOneByteRepresentation());

  // If we can't find the function in the cache, we compile a new
  // function and insert it into the cache.
  Vector<const char> name = CStrVector(extension->name());
  SourceCodeCache* cache = isolate->bootstrapper()->extensions_cache();
  Handle<Context> context(isolate->context(), isolate);
  DCHECK(context->IsNativeContext());

  if (!cache->Lookup(isolate, name, &function_info)) {
    Handle<String> script_name =
        factory->NewStringFromUtf8(name).ToHandleChecked();
    MaybeHandle<SharedFunctionInfo> maybe_function_info =
        Compiler::GetSharedFunctionInfoForScript(
            isolate, source, Compiler::ScriptDetails(script_name),
            ScriptOriginOptions(), extension, nullptr,
            ScriptCompiler::kNoCompileOptions,
            ScriptCompiler::kNoCacheBecauseV8Extension, EXTENSION_CODE);
    if (!maybe_function_info.ToHandle(&function_info)) return false;
    cache->Add(isolate, name, function_info);
  }

  // Set up the function context. Conceptually, we should clone the
  // function before overwriting the context but since we're in a
  // single-threaded environment it is not strictly necessary.
  Handle<JSFunction> fun =
      factory->NewFunctionFromSharedFunctionInfo(function_info, context);

  // Call function using either the runtime object or the global
  // object as the receiver. Provide no parameters.
  Handle<Object> receiver = isolate->global_object();
  return !Execution::TryCall(isolate, fun, receiver, 0, nullptr,
                             Execution::MessageHandling::kKeepPending, nullptr)
              .is_null();
}

static Handle<JSObject> ResolveBuiltinIdHolder(Isolate* isolate,
                                               Handle<Context> native_context,
                                               const char* holder_expr) {
  Factory* factory = isolate->factory();
  Handle<JSGlobalObject> global(native_context->global_object(), isolate);
  const char* period_pos = strchr(holder_expr, '.');
  if (period_pos == nullptr) {
    return Handle<JSObject>::cast(
        Object::GetPropertyOrElement(
            isolate, global, factory->InternalizeUtf8String(holder_expr))
            .ToHandleChecked());
  }
  const char* inner = period_pos + 1;
  DCHECK(!strchr(inner, '.'));
  Vector<const char> property(holder_expr,
                              static_cast<int>(period_pos - holder_expr));
  Handle<String> property_string = factory->InternalizeUtf8String(property);
  DCHECK(!property_string.is_null());
  Handle<JSObject> object = Handle<JSObject>::cast(
      JSReceiver::GetProperty(isolate, global, property_string)
          .ToHandleChecked());
  if (strcmp("prototype", inner) == 0) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(object);
    return Handle<JSObject>(JSObject::cast(function->prototype()), isolate);
  }
  Handle<String> inner_string = factory->InternalizeUtf8String(inner);
  DCHECK(!inner_string.is_null());
  Handle<Object> value =
      JSReceiver::GetProperty(isolate, object, inner_string).ToHandleChecked();
  return Handle<JSObject>::cast(value);
}

void Genesis::ConfigureUtilsObject(GlobalContextType context_type) {
  switch (context_type) {
    // We still need the utils object to find debug functions.
    case DEBUG_CONTEXT:
      return;
    // Expose the natives in global if a valid name for it is specified.
    case FULL_CONTEXT: {
      // We still need the utils object after deserialization.
      if (isolate()->serializer_enabled()) return;
      if (FLAG_expose_natives_as == nullptr) break;
      if (strlen(FLAG_expose_natives_as) == 0) break;
      HandleScope scope(isolate());
      Handle<String> natives_key =
          factory()->InternalizeUtf8String(FLAG_expose_natives_as);
      uint32_t dummy_index;
      if (natives_key->AsArrayIndex(&dummy_index)) break;
      Handle<Object> utils = isolate()->natives_utils_object();
      Handle<JSObject> global = isolate()->global_object();
      JSObject::AddProperty(isolate(), global, natives_key, utils, DONT_ENUM);
      break;
    }
  }

  // The utils object can be removed for cases that reach this point.
  HeapObject* undefined = ReadOnlyRoots(heap()).undefined_value();
  native_context()->set_natives_utils_object(undefined);
  native_context()->set_extras_utils_object(undefined);
}


void Bootstrapper::ExportFromRuntime(Isolate* isolate,
                                     Handle<JSObject> container) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<NativeContext> native_context = isolate->native_context();
#define EXPORT_PRIVATE_SYMBOL(_, NAME)                                \
  Handle<String> NAME##_name = factory->InternalizeUtf8String(#NAME); \
  JSObject::AddProperty(isolate, container, NAME##_name, factory->NAME(), NONE);
  PRIVATE_SYMBOL_LIST_GENERATOR(EXPORT_PRIVATE_SYMBOL, /* not used */)
#undef EXPORT_PRIVATE_SYMBOL

#define EXPORT_PUBLIC_SYMBOL(_, NAME, DESCRIPTION)                    \
  Handle<String> NAME##_name = factory->InternalizeUtf8String(#NAME); \
  JSObject::AddProperty(isolate, container, NAME##_name, factory->NAME(), NONE);
  PUBLIC_SYMBOL_LIST_GENERATOR(EXPORT_PUBLIC_SYMBOL, /* not used */)
  WELL_KNOWN_SYMBOL_LIST_GENERATOR(EXPORT_PUBLIC_SYMBOL, /* not used */)
#undef EXPORT_PUBLIC_SYMBOL

  Handle<JSObject> iterator_prototype(
      native_context->initial_iterator_prototype(), isolate);

  JSObject::AddProperty(isolate, container,
                        factory->InternalizeUtf8String("IteratorPrototype"),
                        iterator_prototype, NONE);

  {
    PrototypeIterator iter(isolate, native_context->generator_function_map());
    Handle<JSObject> generator_function_prototype(iter.GetCurrent<JSObject>(),
                                                  isolate);

    JSObject::AddProperty(
        isolate, container,
        factory->InternalizeUtf8String("GeneratorFunctionPrototype"),
        generator_function_prototype, NONE);

    Handle<JSFunction> generator_function_function = InstallFunction(
        isolate, container, "GeneratorFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, generator_function_prototype,
        Builtins::kGeneratorFunctionConstructor);
    generator_function_function->set_prototype_or_initial_map(
        native_context->generator_function_map());
    generator_function_function->shared()->DontAdaptArguments();
    generator_function_function->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, generator_function_function,
        Context::GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(generator_function_function,
                                isolate->function_function());
    JSObject::AddProperty(
        isolate, generator_function_prototype, factory->constructor_string(),
        generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->generator_function_map()->SetConstructor(
        *generator_function_function);
  }

  {
    PrototypeIterator iter(isolate,
                           native_context->async_generator_function_map());
    Handle<JSObject> async_generator_function_prototype(
        iter.GetCurrent<JSObject>(), isolate);

    Handle<JSFunction> async_generator_function_function = InstallFunction(
        isolate, container, "AsyncGeneratorFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, async_generator_function_prototype,
        Builtins::kAsyncGeneratorFunctionConstructor);
    async_generator_function_function->set_prototype_or_initial_map(
        native_context->async_generator_function_map());
    async_generator_function_function->shared()->DontAdaptArguments();
    async_generator_function_function->shared()->set_length(1);
    InstallWithIntrinsicDefaultProto(
        isolate, async_generator_function_function,
        Context::ASYNC_GENERATOR_FUNCTION_FUNCTION_INDEX);

    JSObject::ForceSetPrototype(async_generator_function_function,
                                isolate->function_function());

    JSObject::AddProperty(
        isolate, async_generator_function_prototype,
        factory->constructor_string(), async_generator_function_function,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    native_context->async_generator_function_map()->SetConstructor(
        *async_generator_function_function);
  }

  {  // -- S e t I t e r a t o r
    Handle<String> name = factory->SetIterator_string();

    // Setup %SetIteratorPrototype%.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    JSObject::ForceSetPrototype(prototype, iterator_prototype);

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        isolate, prototype, factory->to_string_tag_symbol(), name,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Install the next function on the {prototype}.
    SimpleInstallFunction(isolate, prototype, "next",
                          Builtins::kSetIteratorPrototypeNext, 0, true,
                          BuiltinFunctionId::kSetIteratorNext);

    // Setup SetIterator constructor.
    Handle<JSFunction> set_iterator_function = InstallFunction(
        isolate, container, "SetIterator", JS_SET_VALUE_ITERATOR_TYPE,
        JSSetIterator::kSize, 0, prototype, Builtins::kIllegal);
    set_iterator_function->shared()->set_native(false);

    Handle<Map> set_value_iterator_map(set_iterator_function->initial_map(),
                                       isolate);
    native_context->set_set_value_iterator_map(*set_value_iterator_map);

    Handle<Map> set_key_value_iterator_map = Map::Copy(
        isolate, set_value_iterator_map, "JS_SET_KEY_VALUE_ITERATOR_TYPE");
    set_key_value_iterator_map->set_instance_type(
        JS_SET_KEY_VALUE_ITERATOR_TYPE);
    native_context->set_set_key_value_iterator_map(*set_key_value_iterator_map);
  }

  {  // -- M a p I t e r a t o r
    Handle<String> name = factory->MapIterator_string();

    // Setup %MapIteratorPrototype%.
    Handle<JSObject> prototype =
        factory->NewJSObject(isolate->object_function(), TENURED);
    JSObject::ForceSetPrototype(prototype, iterator_prototype);

    // Install the @@toStringTag property on the {prototype}.
    JSObject::AddProperty(
        isolate, prototype, factory->to_string_tag_symbol(), name,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    // Install the next function on the {prototype}.
    SimpleInstallFunction(isolate, prototype, "next",
                          Builtins::kMapIteratorPrototypeNext, 0, true,
                          BuiltinFunctionId::kMapIteratorNext);

    // Setup MapIterator constructor.
    Handle<JSFunction> map_iterator_function = InstallFunction(
        isolate, container, "MapIterator", JS_MAP_KEY_ITERATOR_TYPE,
        JSMapIterator::kSize, 0, prototype, Builtins::kIllegal);
    map_iterator_function->shared()->set_native(false);

    Handle<Map> map_key_iterator_map(map_iterator_function->initial_map(),
                                     isolate);
    native_context->set_map_key_iterator_map(*map_key_iterator_map);

    Handle<Map> map_key_value_iterator_map = Map::Copy(
        isolate, map_key_iterator_map, "JS_MAP_KEY_VALUE_ITERATOR_TYPE");
    map_key_value_iterator_map->set_instance_type(
        JS_MAP_KEY_VALUE_ITERATOR_TYPE);
    native_context->set_map_key_value_iterator_map(*map_key_value_iterator_map);

    Handle<Map> map_value_iterator_map =
        Map::Copy(isolate, map_key_iterator_map, "JS_MAP_VALUE_ITERATOR_TYPE");
    map_value_iterator_map->set_instance_type(JS_MAP_VALUE_ITERATOR_TYPE);
    native_context->set_map_value_iterator_map(*map_value_iterator_map);
  }

  {  // -- A s y n c F u n c t i o n
    // Builtin functions for AsyncFunction.
    PrototypeIterator iter(isolate, native_context->async_function_map());
    Handle<JSObject> async_function_prototype(iter.GetCurrent<JSObject>(),
                                              isolate);

    Handle<JSFunction> async_function_constructor = InstallFunction(
        isolate, container, "AsyncFunction", JS_FUNCTION_TYPE,
        JSFunction::kSizeWithPrototype, 0, async_function_prototype,
        Builtins::kAsyncFunctionConstructor);
    async_function_constructor->set_prototype_or_initial_map(
        native_context->async_function_map());
    async_function_constructor->shared()->DontAdaptArguments();
    async_function_constructor->shared()->set_length(1);
    native_context->set_async_function_constructor(*async_function_constructor);
    JSObject::ForceSetPrototype(async_function_constructor,
                                isolate->function_function());

    JSObject::AddProperty(
        isolate, async_function_prototype, factory->constructor_string(),
        async_function_constructor,
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    JSFunction::SetPrototype(async_function_constructor,
                             async_function_prototype);

    {
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kAsyncFunctionAwaitCaught, 2, false);
      native_context->set_async_function_await_caught(*function);
    }

    {
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kAsyncFunctionAwaitUncaught, 2, false);
      native_context->set_async_function_await_uncaught(*function);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate, Builtins::kAsyncFunctionAwaitRejectClosure,
          factory->empty_string(), 1);
      native_context->set_async_function_await_reject_shared_fun(*info);
    }

    {
      Handle<SharedFunctionInfo> info = SimpleCreateSharedFunctionInfo(
          isolate, Builtins::kAsyncFunctionAwaitResolveClosure,
          factory->empty_string(), 1);
      native_context->set_async_function_await_resolve_shared_fun(*info);
    }

    {
      Handle<JSFunction> function =
          SimpleCreateFunction(isolate, factory->empty_string(),
                               Builtins::kAsyncFunctionPromiseCreate, 0, false);
      native_context->set_async_function_promise_create(*function);
    }

    {
      Handle<JSFunction> function = SimpleCreateFunction(
          isolate, factory->empty_string(),
          Builtins::kAsyncFunctionPromiseRelease, 2, false);
      native_context->set_async_function_promise_release(*function);
    }
  }

  {  // -- C a l l S i t e
    // Builtin functions for CallSite.

    // CallSites are a special case; the constructor is for our private use
    // only, therefore we set it up as a builtin that throws. Internally, we use
    // CallSiteUtils::Construct to create CallSite objects.

    Handle<JSFunction> callsite_fun = InstallFunction(
        isolate, container, "CallSite", JS_OBJECT_TYPE, JSObject::kHeaderSize,
        0, factory->the_hole_value(), Builtins::kUnsupportedThrower);
    callsite_fun->shared()->DontAdaptArguments();
    isolate->native_context()->set_callsite_function(*callsite_fun);

    {
      // Setup CallSite.prototype.
      Handle<JSObject> prototype(
          JSObject::cast(callsite_fun->instance_prototype()), isolate);

      struct FunctionInfo {
        const char* name;
        Builtins::Name id;
      };

      FunctionInfo infos[] = {
          {"getColumnNumber", Builtins::kCallSitePrototypeGetColumnNumber},
          {"getEvalOrigin", Builtins::kCallSitePrototypeGetEvalOrigin},
          {"getFileName", Builtins::kCallSitePrototypeGetFileName},
          {"getFunction", Builtins::kCallSitePrototypeGetFunction},
          {"getFunctionName", Builtins::kCallSitePrototypeGetFunctionName},
          {"getLineNumber", Builtins::kCallSitePrototypeGetLineNumber},
          {"getMethodName", Builtins::kCallSitePrototypeGetMethodName},
          {"getPosition", Builtins::kCallSitePrototypeGetPosition},
          {"getScriptNameOrSourceURL",
           Builtins::kCallSitePrototypeGetScriptNameOrSourceURL},
          {"getThis", Builtins::kCallSitePrototypeGetThis},
          {"getTypeName", Builtins::kCallSitePrototypeGetTypeName},
          {"isAsync", Builtins::kCallSitePrototypeIsAsync},
          {"isConstructor", Builtins::kCallSitePrototypeIsConstructor},
          {"isEval", Builtins::kCallSitePrototypeIsEval},
          {"isNative", Builtins::kCallSitePrototypeIsNative},
          {"isToplevel", Builtins::kCallSitePrototypeIsToplevel},
          {"toString", Builtins::kCallSitePrototypeToString}};

      PropertyAttributes attrs =
          static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE | READ_ONLY);

      Handle<JSFunction> fun;
      for (const FunctionInfo& info : infos) {
        SimpleInstallFunction(isolate, prototype, info.name, info.id, 0, true,
                              attrs);
      }
    }
  }
}


#define EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(id) \
  void Genesis::InitializeGlobal_##id() {}

EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_do_expressions)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_public_fields)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_private_fields)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_static_fields)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_class_fields)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_dynamic_import)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_import_meta)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_numeric_separator)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_json_stringify)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_regexp_sequence)
EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE(harmony_await_optimization)

#undef EMPTY_INITIALIZE_GLOBAL_FOR_FEATURE

void Genesis::InitializeGlobal_harmony_global() {
  if (!FLAG_harmony_global) return;

  Factory* factory = isolate()->factory();
  Handle<JSGlobalObject> global(native_context()->global_object(), isolate());
  Handle<JSGlobalProxy> global_proxy(native_context()->global_proxy(),
                                     isolate());
  JSObject::AddProperty(isolate_, global, factory->globalThis_string(),
                        global_proxy, DONT_ENUM);
}

void Genesis::InitializeGlobal_harmony_sharedarraybuffer() {
  if (!FLAG_harmony_sharedarraybuffer) return;

  Handle<JSGlobalObject> global(native_context()->global_object(), isolate());
  Factory* factory = isolate()->factory();

  {
    Handle<String> name = factory->InternalizeUtf8String("SharedArrayBuffer");
    JSObject::AddProperty(isolate_, global, name,
                          isolate()->shared_array_buffer_fun(), DONT_ENUM);
  }

  {
    Handle<String> name = factory->InternalizeUtf8String("Atomics");
    JSObject::AddProperty(isolate_, global, name, isolate()->atomics_object(),
                          DONT_ENUM);
    JSObject::AddProperty(
        isolate_, isolate()->atomics_object(), factory->to_string_tag_symbol(),
        name, static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));
  }
}

void Genesis::InitializeGlobal_harmony_array_prototype_values() {
  if (!FLAG_harmony_array_prototype_values) return;
  Handle<JSFunction> array_constructor(native_context()->array_function(),
                                       isolate());
  Handle<JSObject> array_prototype(
      JSObject::cast(array_constructor->instance_prototype()), isolate());
  Handle<Object> values_iterator =
      JSObject::GetProperty(isolate(), array_prototype,
                            factory()->iterator_symbol())
          .ToHandleChecked();
  DCHECK(values_iterator->IsJSFunction());
  JSObject::AddProperty(isolate(), array_prototype, factory()->values_string(),
                        values_iterator, DONT_ENUM);

  Handle<Object> unscopables =
      JSObject::GetProperty(isolate(), array_prototype,
                            factory()->unscopables_symbol())
          .ToHandleChecked();
  DCHECK(unscopables->IsJSObject());
  JSObject::AddProperty(isolate(), Handle<JSObject>::cast(unscopables),
                        factory()->values_string(), factory()->true_value(),
                        NONE);
}

void Genesis::InitializeGlobal_harmony_array_flat() {
  if (!FLAG_harmony_array_flat) return;
  Handle<JSFunction> array_constructor(native_context()->array_function(),
                                       isolate());
  Handle<JSObject> array_prototype(
      JSObject::cast(array_constructor->instance_prototype()), isolate());
  SimpleInstallFunction(isolate(), array_prototype, "flat",
                        Builtins::kArrayPrototypeFlat, 0, false, DONT_ENUM);
  SimpleInstallFunction(isolate(), array_prototype, "flatMap",
                        Builtins::kArrayPrototypeFlatMap, 1, false, DONT_ENUM);
}

void Genesis::InitializeGlobal_harmony_symbol_description() {
  if (!FLAG_harmony_symbol_description) return;

  // Symbol.prototype.description
  Handle<JSFunction> symbol_fun(native_context()->symbol_function(), isolate());
  Handle<JSObject> symbol_prototype(
      JSObject::cast(symbol_fun->instance_prototype()), isolate());
  SimpleInstallGetter(isolate(), symbol_prototype,
                      factory()->InternalizeUtf8String("description"),
                      Builtins::kSymbolPrototypeDescriptionGetter, true);
}

void Genesis::InitializeGlobal_harmony_string_matchall() {
  if (!FLAG_harmony_string_matchall) return;

  {  // String.prototype.matchAll
    Handle<JSFunction> string_fun(native_context()->string_function(),
                                  isolate());
    Handle<JSObject> string_prototype(
        JSObject::cast(string_fun->instance_prototype()), isolate());

    SimpleInstallFunction(isolate(), string_prototype, "matchAll",
                          Builtins::kStringPrototypeMatchAll, 1, true);
  }

  {  // RegExp.prototype[@@matchAll]
    Handle<JSFunction> regexp_fun(native_context()->regexp_function(),
                                  isolate());
    Handle<JSObject> regexp_prototype(
        JSObject::cast(regexp_fun->instance_prototype()), isolate());
    SimpleInstallFunction(isolate(), regexp_prototype,
                          factory()->match_all_symbol(), "[Symbol.matchAll]",
                          Builtins::kRegExpPrototypeMatchAll, 1, true);
    Handle<Map> regexp_prototype_map(regexp_prototype->map(), isolate());
    Map::SetShouldBeFastPrototypeMap(regexp_prototype_map, true, isolate());
    native_context()->set_regexp_prototype_map(*regexp_prototype_map);
  }

  {  // --- R e g E x p S t r i n g  I t e r a t o r ---
    Handle<JSObject> iterator_prototype(
        native_context()->initial_iterator_prototype(), isolate());

    Handle<JSObject> regexp_string_iterator_prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    JSObject::ForceSetPrototype(regexp_string_iterator_prototype,
                                iterator_prototype);

    JSObject::AddProperty(
        isolate(), regexp_string_iterator_prototype,
        factory()->to_string_tag_symbol(),
        factory()->InternalizeUtf8String("RegExp String Iterator"),
        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

    SimpleInstallFunction(isolate(), regexp_string_iterator_prototype, "next",
                          Builtins::kRegExpStringIteratorPrototypeNext, 0,
                          true);

    Handle<JSFunction> regexp_string_iterator_function = CreateFunction(
        isolate(), factory()->InternalizeUtf8String("RegExpStringIterator"),
        JS_REGEXP_STRING_ITERATOR_TYPE, JSRegExpStringIterator::kSize, 0,
        regexp_string_iterator_prototype, Builtins::kIllegal);
    regexp_string_iterator_function->shared()->set_native(false);
    native_context()->set_initial_regexp_string_iterator_prototype_map(
        regexp_string_iterator_function->initial_map());
  }

  {  // @@matchAll Symbol
    Handle<JSFunction> symbol_fun(native_context()->symbol_function(),
                                  isolate());
    InstallConstant(isolate(), symbol_fun, "matchAll",
                    factory()->match_all_symbol());
  }
}

#ifdef V8_INTL_SUPPORT
void Genesis::InitializeGlobal_harmony_intl_list_format() {
  if (!FLAG_harmony_intl_list_format) return;
  Handle<JSObject> intl = Handle<JSObject>::cast(
      JSReceiver::GetProperty(
          isolate(),
          Handle<JSReceiver>(native_context()->global_object(), isolate()),
          factory()->InternalizeUtf8String("Intl"))
          .ToHandleChecked());

  Handle<JSFunction> list_format_fun =
      InstallFunction(isolate(), intl, "ListFormat", JS_INTL_LIST_FORMAT_TYPE,
                      JSListFormat::kSize, 0, factory()->the_hole_value(),
                      Builtins::kListFormatConstructor);
  list_format_fun->shared()->set_length(0);
  list_format_fun->shared()->DontAdaptArguments();

  SimpleInstallFunction(isolate(), list_format_fun, "supportedLocalesOf",
                        Builtins::kListFormatSupportedLocalesOf, 1, false);

  // Setup %ListFormatPrototype%.
  Handle<JSObject> prototype(
      JSObject::cast(list_format_fun->instance_prototype()), isolate());

  // Install the @@toStringTag property on the {prototype}.
  JSObject::AddProperty(isolate(), prototype, factory()->to_string_tag_symbol(),
                        factory()->InternalizeUtf8String("Intl.ListFormat"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                        Builtins::kListFormatPrototypeResolvedOptions, 0,
                        false);
  SimpleInstallFunction(isolate(), prototype, "format",
                        Builtins::kListFormatPrototypeFormat, 1, false);
  SimpleInstallFunction(isolate(), prototype, "formatToParts",
                        Builtins::kListFormatPrototypeFormatToParts, 1, false);
}

void Genesis::InitializeGlobal_harmony_locale() {
  if (!FLAG_harmony_locale) return;

  Handle<JSObject> intl = Handle<JSObject>::cast(
      JSReceiver::GetProperty(
          isolate(),
          Handle<JSReceiver>(native_context()->global_object(), isolate()),
          factory()->InternalizeUtf8String("Intl"))
          .ToHandleChecked());

  Handle<JSFunction> locale_fun = InstallFunction(
      isolate(), intl, "Locale", JS_INTL_LOCALE_TYPE, JSLocale::kSize, 0,
      factory()->the_hole_value(), Builtins::kLocaleConstructor);
  InstallWithIntrinsicDefaultProto(isolate(), locale_fun,
                                   Context::INTL_LOCALE_FUNCTION_INDEX);
  locale_fun->shared()->set_length(1);
  locale_fun->shared()->DontAdaptArguments();

  // Setup %LocalePrototype%.
  Handle<JSObject> prototype(JSObject::cast(locale_fun->instance_prototype()),
                             isolate());

  // Install the @@toStringTag property on the {prototype}.
  JSObject::AddProperty(isolate(), prototype, factory()->to_string_tag_symbol(),
                        factory()->InternalizeUtf8String("Intl.Locale"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  SimpleInstallFunction(isolate(), prototype, "toString",
                        Builtins::kLocalePrototypeToString, 0, false);
  SimpleInstallFunction(isolate(), prototype, "maximize",
                        Builtins::kLocalePrototypeMaximize, 0, false);
  SimpleInstallFunction(isolate(), prototype, "minimize",
                        Builtins::kLocalePrototypeMinimize, 0, false);
  // Base locale getters.
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("language"),
                      Builtins::kLocalePrototypeLanguage, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("script"),
                      Builtins::kLocalePrototypeScript, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("region"),
                      Builtins::kLocalePrototypeRegion, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("baseName"),
                      Builtins::kLocalePrototypeBaseName, true);
  // Unicode extension getters.
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("calendar"),
                      Builtins::kLocalePrototypeCalendar, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("caseFirst"),
                      Builtins::kLocalePrototypeCaseFirst, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("collation"),
                      Builtins::kLocalePrototypeCollation, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("hourCycle"),
                      Builtins::kLocalePrototypeHourCycle, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("numeric"),
                      Builtins::kLocalePrototypeNumeric, true);
  SimpleInstallGetter(isolate(), prototype,
                      factory()->InternalizeUtf8String("numberingSystem"),
                      Builtins::kLocalePrototypeNumberingSystem, true);
}

void Genesis::InitializeGlobal_harmony_intl_relative_time_format() {
  if (!FLAG_harmony_intl_relative_time_format) return;
  Handle<JSObject> intl = Handle<JSObject>::cast(
      JSReceiver::GetProperty(
          isolate(),
          Handle<JSReceiver>(native_context()->global_object(), isolate()),
          factory()->InternalizeUtf8String("Intl"))
          .ToHandleChecked());

  Handle<JSFunction> relative_time_format_fun = InstallFunction(
      isolate(), intl, "RelativeTimeFormat", JS_INTL_RELATIVE_TIME_FORMAT_TYPE,
      JSRelativeTimeFormat::kSize, 0, factory()->the_hole_value(),
      Builtins::kRelativeTimeFormatConstructor);
  relative_time_format_fun->shared()->set_length(0);
  relative_time_format_fun->shared()->DontAdaptArguments();

  SimpleInstallFunction(
      isolate(), relative_time_format_fun, "supportedLocalesOf",
      Builtins::kRelativeTimeFormatSupportedLocalesOf, 1, false);

  // Setup %RelativeTimeFormatPrototype%.
  Handle<JSObject> prototype(
      JSObject::cast(relative_time_format_fun->instance_prototype()),
      isolate());

  // Install the @@toStringTag property on the {prototype}.
  JSObject::AddProperty(
      isolate(), prototype, factory()->to_string_tag_symbol(),
      factory()->InternalizeUtf8String("Intl.RelativeTimeFormat"),
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                        Builtins::kRelativeTimeFormatPrototypeResolvedOptions,
                        0, false);
  SimpleInstallFunction(isolate(), prototype, "format",
                        Builtins::kRelativeTimeFormatPrototypeFormat, 2, false);
  SimpleInstallFunction(isolate(), prototype, "formatToParts",
                        Builtins::kRelativeTimeFormatPrototypeFormatToParts, 2,
                        false);
}

void Genesis::InitializeGlobal_harmony_intl_segmenter() {
  if (!FLAG_harmony_intl_segmenter) return;
  Handle<JSObject> intl = Handle<JSObject>::cast(
      JSReceiver::GetProperty(
          isolate(),
          Handle<JSReceiver>(native_context()->global_object(), isolate()),
          factory()->InternalizeUtf8String("Intl"))
          .ToHandleChecked());

  Handle<JSFunction> segmenter_fun = InstallFunction(
      isolate(), intl, "Segmenter", JS_INTL_SEGMENTER_TYPE, JSSegmenter::kSize,
      0, factory()->the_hole_value(), Builtins::kSegmenterConstructor);
  segmenter_fun->shared()->set_length(0);
  segmenter_fun->shared()->DontAdaptArguments();

  SimpleInstallFunction(isolate(), segmenter_fun, "supportedLocalesOf",
                        Builtins::kSegmenterSupportedLocalesOf, 1, false);

  // Setup %SegmenterPrototype%.
  Handle<JSObject> prototype(
      JSObject::cast(segmenter_fun->instance_prototype()), isolate());

  // Install the @@toStringTag property on the {prototype}.
  JSObject::AddProperty(isolate(), prototype, factory()->to_string_tag_symbol(),
                        factory()->NewStringFromStaticChars("Intl.Segmenter"),
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  SimpleInstallFunction(isolate(), prototype, "resolvedOptions",
                        Builtins::kSegmenterPrototypeResolvedOptions, 0, false);
}

#endif  // V8_INTL_SUPPORT

Handle<JSFunction> Genesis::CreateArrayBuffer(
    Handle<String> name, ArrayBufferKind array_buffer_kind) {
  // Create the %ArrayBufferPrototype%
  // Setup the {prototype} with the given {name} for @@toStringTag.
  Handle<JSObject> prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  JSObject::AddProperty(isolate(), prototype, factory()->to_string_tag_symbol(),
                        name,
                        static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY));

  // Allocate the constructor with the given {prototype}.
  Handle<JSFunction> array_buffer_fun =
      CreateFunction(isolate(), name, JS_ARRAY_BUFFER_TYPE,
                     JSArrayBuffer::kSizeWithEmbedderFields, 0, prototype,
                     Builtins::kArrayBufferConstructor);
  array_buffer_fun->shared()->DontAdaptArguments();
  array_buffer_fun->shared()->set_length(1);

  // Install the "constructor" property on the {prototype}.
  JSObject::AddProperty(isolate(), prototype, factory()->constructor_string(),
                        array_buffer_fun, DONT_ENUM);

  switch (array_buffer_kind) {
    case ARRAY_BUFFER:
      SimpleInstallFunction(isolate(), array_buffer_fun,
                            factory()->isView_string(),
                            Builtins::kArrayBufferIsView, 1, true, DONT_ENUM,
                            BuiltinFunctionId::kArrayBufferIsView);

      // Install the "byteLength" getter on the {prototype}.
      SimpleInstallGetter(isolate(), prototype, factory()->byte_length_string(),
                          Builtins::kArrayBufferPrototypeGetByteLength, false,
                          BuiltinFunctionId::kArrayBufferByteLength);

      SimpleInstallFunction(isolate(), prototype, "slice",
                            Builtins::kArrayBufferPrototypeSlice, 2, true);
      break;

    case SHARED_ARRAY_BUFFER:
      // Install the "byteLength" getter on the {prototype}.
      SimpleInstallGetter(isolate(), prototype, factory()->byte_length_string(),
                          Builtins::kSharedArrayBufferPrototypeGetByteLength,
                          false,
                          BuiltinFunctionId::kSharedArrayBufferByteLength);

      SimpleInstallFunction(isolate(), prototype, "slice",
                            Builtins::kSharedArrayBufferPrototypeSlice, 2,
                            true);
      break;
  }

  return array_buffer_fun;
}


Handle<JSFunction> Genesis::InstallInternalArray(Handle<JSObject> target,
                                                 const char* name,
                                                 ElementsKind elements_kind) {
  // --- I n t e r n a l   A r r a y ---
  // An array constructor on the builtins object that works like
  // the public Array constructor, except that its prototype
  // doesn't inherit from Object.prototype.
  // To be used only for internal work by builtins. Instances
  // must not be leaked to user code.
  Handle<JSObject> prototype =
      factory()->NewJSObject(isolate()->object_function(), TENURED);
  Handle<JSFunction> array_function =
      InstallFunction(isolate(), target, name, JS_ARRAY_TYPE, JSArray::kSize, 0,
                      prototype, Builtins::kInternalArrayConstructor);

  array_function->shared()->DontAdaptArguments();

  Handle<Map> original_map(array_function->initial_map(), isolate());
  Handle<Map> initial_map = Map::Copy(isolate(), original_map, "InternalArray");
  initial_map->set_elements_kind(elements_kind);
  JSFunction::SetInitialMap(array_function, initial_map, prototype);

  // Make "length" magic on instances.
  Map::EnsureDescriptorSlack(isolate(), initial_map, 1);

  PropertyAttributes attribs = static_cast<PropertyAttributes>(
      DONT_ENUM | DONT_DELETE);

  {  // Add length.
    Descriptor d = Descriptor::AccessorConstant(
        factory()->length_string(), factory()->array_length_accessor(),
        attribs);
    initial_map->AppendDescriptor(&d);
  }

  return array_function;
}

bool Genesis::InstallNatives(GlobalContextType context_type) {
  HandleScope scope(isolate());

  // Set up the utils object as shared container between native scripts.
  Handle<JSObject> utils = factory()->NewJSObject(isolate()->object_function());
  JSObject::NormalizeProperties(utils, CLEAR_INOBJECT_PROPERTIES, 16,
                                "utils container for native scripts");
  native_context()->set_natives_utils_object(*utils);

  // Set up the extras utils object as a shared container between native
  // scripts and extras. (Extras consume things added there by native scripts.)
  Handle<JSObject> extras_utils =
      factory()->NewJSObject(isolate()->object_function());
  native_context()->set_extras_utils_object(*extras_utils);

  InstallInternalArray(extras_utils, "InternalPackedArray", PACKED_ELEMENTS);

  // v8.createPromise(parent)
  Handle<JSFunction> promise_internal_constructor =
      SimpleCreateFunction(isolate(), factory()->empty_string(),
                           Builtins::kPromiseInternalConstructor, 1, true);
  promise_internal_constructor->shared()->set_native(false);
  InstallFunction(isolate(), extras_utils, promise_internal_constructor,
                  factory()->InternalizeUtf8String("createPromise"));

  // v8.rejectPromise(promise, reason)
  Handle<JSFunction> promise_internal_reject =
      SimpleCreateFunction(isolate(), factory()->empty_string(),
                           Builtins::kPromiseInternalReject, 2, true);
  promise_internal_reject->shared()->set_native(false);
  InstallFunction(isolate(), extras_utils, promise_internal_reject,
                  factory()->InternalizeUtf8String("rejectPromise"));

  // v8.resolvePromise(promise, resolution)
  Handle<JSFunction> promise_internal_resolve =
      SimpleCreateFunction(isolate(), factory()->empty_string(),
                           Builtins::kPromiseInternalResolve, 2, true);
  promise_internal_resolve->shared()->set_native(false);
  InstallFunction(isolate(), extras_utils, promise_internal_resolve,
                  factory()->InternalizeUtf8String("resolvePromise"));

  InstallFunction(isolate(), extras_utils, isolate()->is_promise(),
                  factory()->InternalizeUtf8String("isPromise"));

  int builtin_index = Natives::GetDebuggerCount();
  // Only run prologue.js at this point.
  DCHECK_EQ(builtin_index, Natives::GetIndex("prologue"));
  if (!Bootstrapper::CompileBuiltin(isolate(), builtin_index++)) return false;

  {
    // Builtin function for OpaqueReference -- a JSValue-based object,
    // that keeps its field isolated from JavaScript code. It may store
    // objects, that JavaScript code may not access.
    Handle<JSObject> prototype =
        factory()->NewJSObject(isolate()->object_function(), TENURED);
    Handle<JSFunction> opaque_reference_fun =
        CreateFunction(isolate(), factory()->empty_string(), JS_VALUE_TYPE,
                       JSValue::kSize, 0, prototype, Builtins::kIllegal);
    native_context()->set_opaque_reference_function(*opaque_reference_fun);
  }

  // InternalArrays should not use Smi-Only array optimizations. There are too
  // many places in the C++ runtime code (e.g. RegEx) that assume that
  // elements in InternalArrays can be set to non-Smi values without going
  // through a common bottleneck that would make the SMI_ONLY -> FAST_ELEMENT
  // transition easy to trap. Moreover, they rarely are smi-only.
  {
    HandleScope scope(isolate());
    Handle<JSObject> utils =
        Handle<JSObject>::cast(isolate()->natives_utils_object());
    Handle<JSFunction> array_function =
        InstallInternalArray(utils, "InternalArray", HOLEY_ELEMENTS);
    native_context()->set_internal_array_function(*array_function);
  }

  // Run the rest of the native scripts.
  while (builtin_index < Natives::GetBuiltinsCount()) {
    if (!Bootstrapper::CompileBuiltin(isolate(), builtin_index++)) return false;
  }

  if (!CallUtilsFunction(isolate(), "PostNatives")) return false;
  auto fast_template_instantiations_cache = isolate()->factory()->NewFixedArray(
      TemplateInfo::kFastTemplateInstantiationsCacheSize);
  native_context()->set_fast_template_instantiations_cache(
      *fast_template_instantiations_cache);

  auto slow_template_instantiations_cache = SimpleNumberDictionary::New(
      isolate(), ApiNatives::kInitialFunctionCacheSize);
  native_context()->set_slow_template_instantiations_cache(
      *slow_template_instantiations_cache);

  // Store the map for the %ObjectPrototype% after the natives has been compiled
  // and the Object function has been set up.
  {
    Handle<JSFunction> object_function(native_context()->object_function(),
                                       isolate());
    DCHECK(JSObject::cast(object_function->initial_map()->prototype())
               ->HasFastProperties());
    native_context()->set_object_function_prototype_map(
        HeapObject::cast(object_function->initial_map()->prototype())->map());
  }

  // Store the map for the %StringPrototype% after the natives has been compiled
  // and the String function has been set up.
  Handle<JSFunction> string_function(native_context()->string_function(),
                                     isolate());
  JSObject* string_function_prototype =
      JSObject::cast(string_function->initial_map()->prototype());
  DCHECK(string_function_prototype->HasFastProperties());
  native_context()->set_string_function_prototype_map(
      string_function_prototype->map());

  Handle<JSGlobalObject> global_object =
      handle(native_context()->global_object(), isolate());

  // Install Global.decodeURI.
  SimpleInstallFunction(isolate(), global_object, "decodeURI",
                        Builtins::kGlobalDecodeURI, 1, false,
                        BuiltinFunctionId::kGlobalDecodeURI);

  // Install Global.decodeURIComponent.
  SimpleInstallFunction(isolate(), global_object, "decodeURIComponent",
                        Builtins::kGlobalDecodeURIComponent, 1, false,
                        BuiltinFunctionId::kGlobalDecodeURIComponent);

  // Install Global.encodeURI.
  SimpleInstallFunction(isolate(), global_object, "encodeURI",
                        Builtins::kGlobalEncodeURI, 1, false,
                        BuiltinFunctionId::kGlobalEncodeURI);

  // Install Global.encodeURIComponent.
  SimpleInstallFunction(isolate(), global_object, "encodeURIComponent",
                        Builtins::kGlobalEncodeURIComponent, 1, false,
                        BuiltinFunctionId::kGlobalEncodeURIComponent);

  // Install Global.escape.
  SimpleInstallFunction(isolate(), global_object, "escape",
                        Builtins::kGlobalEscape, 1, false,
                        BuiltinFunctionId::kGlobalEscape);

  // Install Global.unescape.
  SimpleInstallFunction(isolate(), global_object, "unescape",
                        Builtins::kGlobalUnescape, 1, false,
                        BuiltinFunctionId::kGlobalUnescape);

  // Install Global.eval.
  {
    Handle<JSFunction> eval = SimpleInstallFunction(
        isolate(), global_object, factory()->eval_string(),
        Builtins::kGlobalEval, 1, false);
    native_context()->set_global_eval_fun(*eval);
  }

  // Install Global.isFinite
  SimpleInstallFunction(isolate(), global_object, "isFinite",
                        Builtins::kGlobalIsFinite, 1, true,
                        BuiltinFunctionId::kGlobalIsFinite);

  // Install Global.isNaN
  SimpleInstallFunction(isolate(), global_object, "isNaN",
                        Builtins::kGlobalIsNaN, 1, true,
                        BuiltinFunctionId::kGlobalIsNaN);

  // Install Array builtin functions.
  {
    Handle<JSFunction> array_constructor(native_context()->array_function(),
                                         isolate());
    Handle<JSArray> proto(JSArray::cast(array_constructor->prototype()),
                          isolate());

    // Verification of important array prototype properties.
    Object* length = proto->length();
    CHECK(length->IsSmi());
    CHECK_EQ(Smi::ToInt(length), 0);
    CHECK(proto->HasSmiOrObjectElements());
    // This is necessary to enable fast checks for absence of elements
    // on Array.prototype and below.
    proto->set_elements(ReadOnlyRoots(heap()).empty_fixed_array());
  }

  // Install InternalArray.prototype.concat
  {
    Handle<JSFunction> array_constructor(
        native_context()->internal_array_function(), isolate());
    Handle<JSObject> proto(JSObject::cast(array_constructor->prototype()),
                           isolate());
    SimpleInstallFunction(isolate(), proto, "concat", Builtins::kArrayConcat, 1,
                          false);
  }

  InstallBuiltinFunctionIds();

  // Create a map for accessor property descriptors (a variant of JSObject
  // that predefines four properties get, set, configurable and enumerable).
  {
    // AccessorPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSAccessorPropertyDescriptor::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 4);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(isolate(), map, 4);

    {  // get
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->get_string(),
                                JSAccessorPropertyDescriptor::kGetIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // set
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->set_string(),
                                JSAccessorPropertyDescriptor::kSetIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->enumerable_string(),
                                JSAccessorPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // configurable
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->configurable_string(),
          JSAccessorPropertyDescriptor::kConfigurableIndex, NONE,
          Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_accessor_property_descriptor_map(*map);
  }

  // Create a map for data property descriptors (a variant of JSObject
  // that predefines four properties value, writable, configurable and
  // enumerable).
  {
    // DataPropertyDescriptor initial map.
    Handle<Map> map =
        factory()->NewMap(JS_OBJECT_TYPE, JSDataPropertyDescriptor::kSize,
                          TERMINAL_FAST_ELEMENTS_KIND, 4);
    // Create the descriptor array for the property descriptor object.
    Map::EnsureDescriptorSlack(isolate(), map, 4);

    {  // value
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->value_string(),
                                JSDataPropertyDescriptor::kValueIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // writable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->writable_string(),
                                JSDataPropertyDescriptor::kWritableIndex, NONE,
                                Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // enumerable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->enumerable_string(),
                                JSDataPropertyDescriptor::kEnumerableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }
    {  // configurable
      Descriptor d =
          Descriptor::DataField(isolate(), factory()->configurable_string(),
                                JSDataPropertyDescriptor::kConfigurableIndex,
                                NONE, Representation::Tagged());
      map->AppendDescriptor(&d);
    }

    Map::SetPrototype(isolate(), map, isolate()->initial_object_prototype());
    map->SetConstructor(native_context()->object_function());

    native_context()->set_data_property_descriptor_map(*map);
  }

  // Create a constructor for RegExp results (a variant of Array that
  // predefines the properties index, input, and groups).
  {
    // JSRegExpResult initial map.

    // Find global.Array.prototype to inherit from.
    Handle<JSFunction> array_constructor(native_context()->array_function(),
                                         isolate());
    Handle<JSObject> array_prototype(
        JSObject::cast(array_constructor->instance_prototype()), isolate());

    // Add initial map.
    Handle<Map> initial_map = factory()->NewMap(
        JS_ARRAY_TYPE, JSRegExpResult::kSize, TERMINAL_FAST_ELEMENTS_KIND,
        JSRegExpResult::kInObjectPropertyCount);
    initial_map->SetConstructor(*array_constructor);

    // Set prototype on map.
    initial_map->set_has_non_instance_prototype(false);
    Map::SetPrototype(isolate(), initial_map, array_prototype);

    // Update map with length accessor from Array and add "index", "input" and
    // "groups".
    Map::EnsureDescriptorSlack(isolate(), initial_map,
                               JSRegExpResult::kInObjectPropertyCount + 1);

    // length descriptor.
    {
      JSFunction* array_function = native_context()->array_function();
      Handle<DescriptorArray> array_descriptors(
          array_function->initial_map()->instance_descriptors(), isolate());
      Handle<String> length = factory()->length_string();
      int old = array_descriptors->SearchWithCache(
          isolate(), *length, array_function->initial_map());
      DCHECK_NE(old, DescriptorArray::kNotFound);
      Descriptor d = Descriptor::AccessorConstant(
          length, handle(array_descriptors->GetStrongValue(old), isolate()),
          array_descriptors->GetDetails(old).attributes());
      initial_map->AppendDescriptor(&d);
    }

    // index descriptor.
    {
      Descriptor d = Descriptor::DataField(isolate(), factory()->index_string(),
                                           JSRegExpResult::kIndexIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(&d);
    }

    // input descriptor.
    {
      Descriptor d = Descriptor::DataField(isolate(), factory()->input_string(),
                                           JSRegExpResult::kInputIndex, NONE,
                                           Representation::Tagged());
      initial_map->AppendDescriptor(&d);
    }

    // groups descriptor.
    {
      Descriptor d = Descriptor::DataField(
          isolate(), factory()->groups_string(), JSRegExpResult::kGroupsIndex,
          NONE, Representation::Tagged());
      initial_map->AppendDescriptor(&d);
    }

    native_context()->set_regexp_result_map(*initial_map);
  }

  // Add @@iterator method to the arguments object maps.
  {
    PropertyAttributes attribs = DONT_ENUM;
    Handle<AccessorInfo> arguments_iterator =
        factory()->arguments_iterator_accessor();
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->sloppy_arguments_map(), isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(&d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->fast_aliased_arguments_map(),
                      isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(&d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->slow_aliased_arguments_map(),
                      isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(&d);
    }
    {
      Descriptor d = Descriptor::AccessorConstant(factory()->iterator_symbol(),
                                                  arguments_iterator, attribs);
      Handle<Map> map(native_context()->strict_arguments_map(), isolate());
      Map::EnsureDescriptorSlack(isolate(), map, 1);
      map->AppendDescriptor(&d);
    }
  }

  return true;
}

bool Genesis::InstallExtraNatives() {
  HandleScope scope(isolate());

  Handle<JSObject> extras_binding =
      factory()->NewJSObject(isolate()->object_function());

  // binding.isTraceCategoryEnabled(category)
  SimpleInstallFunction(isolate(), extras_binding, "isTraceCategoryEnabled",
                        Builtins::kIsTraceCategoryEnabled, 1, true);

  // binding.trace(phase, category, name, id, data)
  SimpleInstallFunction(isolate(), extras_binding, "trace", Builtins::kTrace, 5,
                        true);

  native_context()->set_extras_binding_object(*extras_binding);

  for (int i = ExtraNatives::GetDebuggerCount();
       i < ExtraNatives::GetBuiltinsCount(); i++) {
    if (!Bootstrapper::CompileExtraBuiltin(isolate(), i)) return false;
  }

  return true;
}


bool Genesis::InstallExperimentalExtraNatives() {
  for (int i = ExperimentalExtraNatives::GetDebuggerCount();
       i < ExperimentalExtraNatives::GetBuiltinsCount(); i++) {
    if (!Bootstrapper::CompileExperimentalExtraBuiltin(isolate(), i))
      return false;
  }

  return true;
}


bool Genesis::InstallDebuggerNatives() {
  for (int i = 0; i < Natives::GetDebuggerCount(); ++i) {
    if (!Bootstrapper::CompileBuiltin(isolate(), i)) return false;
  }
  return true;
}

static void InstallBuiltinFunctionId(Isolate* isolate, Handle<JSObject> holder,
                                     const char* function_name,
                                     BuiltinFunctionId id) {
  Handle<Object> function_object =
      JSReceiver::GetProperty(isolate, holder, function_name).ToHandleChecked();
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
  function->shared()->set_builtin_function_id(id);
}

#define INSTALL_BUILTIN_ID(holder_expr, fun_name, name) \
  {#holder_expr, #fun_name, BuiltinFunctionId::k##name},

void Genesis::InstallBuiltinFunctionIds() {
  HandleScope scope(isolate());
  struct BuiltinFunctionIds {
    const char* holder_expr;
    const char* fun_name;
    BuiltinFunctionId id;
  };

  const BuiltinFunctionIds builtins[] = {
      FUNCTIONS_WITH_ID_LIST(INSTALL_BUILTIN_ID)};

  for (const BuiltinFunctionIds& builtin : builtins) {
    Handle<JSObject> holder = ResolveBuiltinIdHolder(
        isolate(), native_context(), builtin.holder_expr);
    InstallBuiltinFunctionId(isolate(), holder, builtin.fun_name, builtin.id);
  }
}

#undef INSTALL_BUILTIN_ID


void Genesis::InitializeNormalizedMapCaches() {
  Handle<NormalizedMapCache> cache = NormalizedMapCache::New(isolate());
  native_context()->set_normalized_map_cache(*cache);
}


bool Bootstrapper::InstallExtensions(Handle<Context> native_context,
                                     v8::ExtensionConfiguration* extensions) {
  // Don't install extensions into the snapshot.
  if (isolate_->serializer_enabled()) return true;
  BootstrapperActive active(this);
  SaveContext saved_context(isolate_);
  isolate_->set_context(*native_context);
  return Genesis::InstallExtensions(isolate_, native_context, extensions) &&
         Genesis::InstallSpecialObjects(isolate_, native_context);
}

bool Genesis::InstallSpecialObjects(Isolate* isolate,
                                    Handle<Context> native_context) {
  HandleScope scope(isolate);

  Handle<JSObject> Error = isolate->error_function();
  Handle<String> name = isolate->factory()->stackTraceLimit_string();
  Handle<Smi> stack_trace_limit(Smi::FromInt(FLAG_stack_trace_limit), isolate);
  JSObject::AddProperty(isolate, Error, name, stack_trace_limit, NONE);

  if (FLAG_expose_wasm) {
    // Install the internal data structures into the isolate and expose on
    // the global object.
    WasmJs::Install(isolate, true);
  } else if (FLAG_validate_asm) {
    // Install the internal data structures only; these are needed for asm.js
    // translated to WASM to work correctly.
    WasmJs::Install(isolate, false);
  }

  return true;
}


static uint32_t Hash(RegisteredExtension* extension) {
  return v8::internal::ComputePointerHash(extension);
}

Genesis::ExtensionStates::ExtensionStates() : map_(8) {}

Genesis::ExtensionTraversalState Genesis::ExtensionStates::get_state(
    RegisteredExtension* extension) {
  base::HashMap::Entry* entry = map_.Lookup(extension, Hash(extension));
  if (entry == nullptr) {
    return UNVISITED;
  }
  return static_cast<ExtensionTraversalState>(
      reinterpret_cast<intptr_t>(entry->value));
}

void Genesis::ExtensionStates::set_state(RegisteredExtension* extension,
                                         ExtensionTraversalState state) {
  map_.LookupOrInsert(extension, Hash(extension))->value =
      reinterpret_cast<void*>(static_cast<intptr_t>(state));
}

bool Genesis::InstallExtensions(Isolate* isolate,
                                Handle<Context> native_context,
                                v8::ExtensionConfiguration* extensions) {
  ExtensionStates extension_states;  // All extensions have state UNVISITED.
  return InstallAutoExtensions(isolate, &extension_states) &&
         (!FLAG_expose_free_buffer ||
          InstallExtension(isolate, "v8/free-buffer", &extension_states)) &&
         (!FLAG_expose_gc ||
          InstallExtension(isolate, "v8/gc", &extension_states)) &&
         (!FLAG_expose_externalize_string ||
          InstallExtension(isolate, "v8/externalize", &extension_states)) &&
         (!FLAG_gc_stats ||
          InstallExtension(isolate, "v8/statistics", &extension_states)) &&
         (!FLAG_expose_trigger_failure ||
          InstallExtension(isolate, "v8/trigger-failure", &extension_states)) &&
         (!FLAG_trace_ignition_dispatches ||
          InstallExtension(isolate, "v8/ignition-statistics",
                           &extension_states)) &&
         InstallRequestedExtensions(isolate, extensions, &extension_states);
}


bool Genesis::InstallAutoExtensions(Isolate* isolate,
                                    ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != nullptr; it = it->next()) {
    if (it->extension()->auto_enable() &&
        !InstallExtension(isolate, it, extension_states)) {
      return false;
    }
  }
  return true;
}


bool Genesis::InstallRequestedExtensions(Isolate* isolate,
                                         v8::ExtensionConfiguration* extensions,
                                         ExtensionStates* extension_states) {
  for (const char** it = extensions->begin(); it != extensions->end(); ++it) {
    if (!InstallExtension(isolate, *it, extension_states)) return false;
  }
  return true;
}


// Installs a named extension.  This methods is unoptimized and does
// not scale well if we want to support a large number of extensions.
bool Genesis::InstallExtension(Isolate* isolate,
                               const char* name,
                               ExtensionStates* extension_states) {
  for (v8::RegisteredExtension* it = v8::RegisteredExtension::first_extension();
       it != nullptr; it = it->next()) {
    if (strcmp(name, it->extension()->name()) == 0) {
      return InstallExtension(isolate, it, extension_states);
    }
  }
  return Utils::ApiCheck(false,
                         "v8::Context::New()",
                         "Cannot find required extension");
}


bool Genesis::InstallExtension(Isolate* isolate,
                               v8::RegisteredExtension* current,
                               ExtensionStates* extension_states) {
  HandleScope scope(isolate);

  if (extension_states->get_state(current) == INSTALLED) return true;
  // The current node has already been visited so there must be a
  // cycle in the dependency graph; fail.
  if (!Utils::ApiCheck(extension_states->get_state(current) != VISITED,
                       "v8::Context::New()",
                       "Circular extension dependency")) {
    return false;
  }
  DCHECK(extension_states->get_state(current) == UNVISITED);
  extension_states->set_state(current, VISITED);
  v8::Extension* extension = current->extension();
  // Install the extension's dependencies
  for (int i = 0; i < extension->dependency_count(); i++) {
    if (!InstallExtension(isolate,
                          extension->dependencies()[i],
                          extension_states)) {
      return false;
    }
  }
  // We do not expect this to throw an exception. Change this if it does.
  bool result = CompileExtension(isolate, extension);
  DCHECK(isolate->has_pending_exception() != result);
  if (!result) {
    // We print out the name of the extension that fail to install.
    // When an error is thrown during bootstrapping we automatically print
    // the line number at which this happened to the console in the isolate
    // error throwing functionality.
    base::OS::PrintError("Error installing extension '%s'.\n",
                         current->extension()->name());
    isolate->clear_pending_exception();
  }
  extension_states->set_state(current, INSTALLED);
  return result;
}


bool Genesis::ConfigureGlobalObjects(
    v8::Local<v8::ObjectTemplate> global_proxy_template) {
  Handle<JSObject> global_proxy(
      JSObject::cast(native_context()->global_proxy()), isolate());
  Handle<JSObject> global_object(
      JSObject::cast(native_context()->global_object()), isolate());

  if (!global_proxy_template.IsEmpty()) {
    // Configure the global proxy object.
    Handle<ObjectTemplateInfo> global_proxy_data =
        v8::Utils::OpenHandle(*global_proxy_template);
    if (!ConfigureApiObject(global_proxy, global_proxy_data)) return false;

    // Configure the global object.
    Handle<FunctionTemplateInfo> proxy_constructor(
        FunctionTemplateInfo::cast(global_proxy_data->constructor()),
        isolate());
    if (!proxy_constructor->prototype_template()->IsUndefined(isolate())) {
      Handle<ObjectTemplateInfo> global_object_data(
          ObjectTemplateInfo::cast(proxy_constructor->prototype_template()),
          isolate());
      if (!ConfigureApiObject(global_object, global_object_data)) return false;
    }
  }

  JSObject::ForceSetPrototype(global_proxy, global_object);

  native_context()->set_array_buffer_map(
      native_context()->array_buffer_fun()->initial_map());

  Handle<JSFunction> js_map_fun(native_context()->js_map_fun(), isolate());
  Handle<JSFunction> js_set_fun(native_context()->js_set_fun(), isolate());
  // Force the Map/Set constructor to fast properties, so that we can use the
  // fast paths for various things like
  //
  //   x instanceof Map
  //   x instanceof Set
  //
  // etc. We should probably come up with a more principled approach once
  // the JavaScript builtins are gone.
  JSObject::MigrateSlowToFast(js_map_fun, 0, "Bootstrapping");
  JSObject::MigrateSlowToFast(js_set_fun, 0, "Bootstrapping");

  native_context()->set_js_map_map(js_map_fun->initial_map());
  native_context()->set_js_set_map(js_set_fun->initial_map());

  return true;
}


bool Genesis::ConfigureApiObject(Handle<JSObject> object,
                                 Handle<ObjectTemplateInfo> object_template) {
  DCHECK(!object_template.is_null());
  DCHECK(FunctionTemplateInfo::cast(object_template->constructor())
             ->IsTemplateFor(object->map()));;

  MaybeHandle<JSObject> maybe_obj =
      ApiNatives::InstantiateObject(object->GetIsolate(), object_template);
  Handle<JSObject> obj;
  if (!maybe_obj.ToHandle(&obj)) {
    DCHECK(isolate()->has_pending_exception());
    isolate()->clear_pending_exception();
    return false;
  }
  TransferObject(obj, object);
  return true;
}


void Genesis::TransferNamedProperties(Handle<JSObject> from,
                                      Handle<JSObject> to) {
  // If JSObject::AddProperty asserts due to already existing property,
  // it is likely due to both global objects sharing property name(s).
  // Merging those two global objects is impossible.
  // The global template must not create properties that already exist
  // in the snapshotted global object.
  if (from->HasFastProperties()) {
    Handle<DescriptorArray> descs =
        Handle<DescriptorArray>(from->map()->instance_descriptors(), isolate());
    for (int i = 0; i < from->map()->NumberOfOwnDescriptors(); i++) {
      PropertyDetails details = descs->GetDetails(i);
      if (details.location() == kField) {
        if (details.kind() == kData) {
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i), isolate());
          FieldIndex index = FieldIndex::ForDescriptor(from->map(), i);
          Handle<Object> value =
              JSObject::FastPropertyAt(from, details.representation(), index);
          JSObject::AddProperty(isolate(), to, key, value,
                                details.attributes());
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          UNREACHABLE();
        }

      } else {
        DCHECK_EQ(kDescriptor, details.location());
        if (details.kind() == kData) {
          DCHECK(!FLAG_track_constant_fields);
          HandleScope inner(isolate());
          Handle<Name> key = Handle<Name>(descs->GetKey(i), isolate());
          Handle<Object> value(descs->GetStrongValue(i), isolate());
          JSObject::AddProperty(isolate(), to, key, value,
                                details.attributes());
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          Handle<Name> key(descs->GetKey(i), isolate());
          LookupIterator it(isolate(), to, key,
                            LookupIterator::OWN_SKIP_INTERCEPTOR);
          CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
          // If the property is already there we skip it
          if (it.IsFound()) continue;
          HandleScope inner(isolate());
          DCHECK(!to->HasFastProperties());
          // Add to dictionary.
          Handle<Object> value(descs->GetStrongValue(i), isolate());
          PropertyDetails d(kAccessor, details.attributes(),
                            PropertyCellType::kMutable);
          JSObject::SetNormalizedProperty(to, key, value, d);
        }
      }
    }
  } else if (from->IsJSGlobalObject()) {
    // Copy all keys and values in enumeration order.
    Handle<GlobalDictionary> properties(
        JSGlobalObject::cast(*from)->global_dictionary(), isolate());
    Handle<FixedArray> indices =
        GlobalDictionary::IterationIndices(isolate(), properties);
    for (int i = 0; i < indices->length(); i++) {
      int index = Smi::ToInt(indices->get(i));
      // If the property is already there we skip it.
      Handle<PropertyCell> cell(properties->CellAt(index), isolate());
      Handle<Name> key(cell->name(), isolate());
      LookupIterator it(isolate(), to, key,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
      if (it.IsFound()) continue;
      // Set the property.
      Handle<Object> value(cell->value(), isolate());
      if (value->IsTheHole(isolate())) continue;
      PropertyDetails details = cell->property_details();
      if (details.kind() != kData) continue;
      JSObject::AddProperty(isolate(), to, key, value, details.attributes());
    }
  } else {
    // Copy all keys and values in enumeration order.
    Handle<NameDictionary> properties =
        Handle<NameDictionary>(from->property_dictionary(), isolate());
    Handle<FixedArray> key_indices =
        NameDictionary::IterationIndices(isolate(), properties);
    ReadOnlyRoots roots(isolate());
    for (int i = 0; i < key_indices->length(); i++) {
      int key_index = Smi::ToInt(key_indices->get(i));
      Object* raw_key = properties->KeyAt(key_index);
      DCHECK(properties->IsKey(roots, raw_key));
      DCHECK(raw_key->IsName());
      // If the property is already there we skip it.
      Handle<Name> key(Name::cast(raw_key), isolate());
      LookupIterator it(isolate(), to, key,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
      if (it.IsFound()) continue;
      // Set the property.
      Handle<Object> value =
          Handle<Object>(properties->ValueAt(key_index), isolate());
      DCHECK(!value->IsCell());
      DCHECK(!value->IsTheHole(isolate()));
      PropertyDetails details = properties->DetailsAt(key_index);
      DCHECK_EQ(kData, details.kind());
      JSObject::AddProperty(isolate(), to, key, value, details.attributes());
    }
  }
}


void Genesis::TransferIndexedProperties(Handle<JSObject> from,
                                        Handle<JSObject> to) {
  // Cloning the elements array is sufficient.
  Handle<FixedArray> from_elements =
      Handle<FixedArray>(FixedArray::cast(from->elements()), isolate());
  Handle<FixedArray> to_elements = factory()->CopyFixedArray(from_elements);
  to->set_elements(*to_elements);
}


void Genesis::TransferObject(Handle<JSObject> from, Handle<JSObject> to) {
  HandleScope outer(isolate());

  DCHECK(!from->IsJSArray());
  DCHECK(!to->IsJSArray());

  TransferNamedProperties(from, to);
  TransferIndexedProperties(from, to);

  // Transfer the prototype (new map is needed).
  Handle<Object> proto(from->map()->prototype(), isolate());
  JSObject::ForceSetPrototype(to, proto);
}


Genesis::Genesis(
    Isolate* isolate, MaybeHandle<JSGlobalProxy> maybe_global_proxy,
    v8::Local<v8::ObjectTemplate> global_proxy_template,
    size_t context_snapshot_index,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
    GlobalContextType context_type)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  result_ = Handle<Context>::null();
  global_proxy_ = Handle<JSGlobalProxy>::null();

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  // The deserializer needs to hook up references to the global proxy.
  // Create an uninitialized global proxy now if we don't have one
  // and initialize it later in CreateNewGlobals.
  Handle<JSGlobalProxy> global_proxy;
  if (!maybe_global_proxy.ToHandle(&global_proxy)) {
    int instance_size = 0;
    if (context_snapshot_index > 0) {
      // The global proxy function to reinitialize this global proxy is in the
      // context that is yet to be deserialized. We need to prepare a global
      // proxy of the correct size.
      Object* size = isolate->heap()->serialized_global_proxy_sizes()->get(
          static_cast<int>(context_snapshot_index) - 1);
      instance_size = Smi::ToInt(size);
    } else {
      instance_size = JSGlobalProxy::SizeWithEmbedderFields(
          global_proxy_template.IsEmpty()
              ? 0
              : global_proxy_template->InternalFieldCount());
    }
    global_proxy =
        isolate->factory()->NewUninitializedJSGlobalProxy(instance_size);
  }

  // We can only de-serialize a context if the isolate was initialized from
  // a snapshot. Otherwise we have to build the context from scratch.
  // Also create a context from scratch to expose natives, if required by flag.
  DCHECK(native_context_.is_null());
  if (isolate->initialized_from_snapshot()) {
    Handle<Context> context;
    if (Snapshot::NewContextFromSnapshot(isolate, global_proxy,
                                         context_snapshot_index,
                                         embedder_fields_deserializer)
            .ToHandle(&context)) {
      native_context_ = Handle<NativeContext>::cast(context);
    }
  }

  if (!native_context().is_null()) {
    AddToWeakNativeContextList(isolate, *native_context());
    isolate->set_context(*native_context());
    isolate->counters()->contexts_created_by_snapshot()->Increment();

    if (context_snapshot_index == 0) {
      Handle<JSGlobalObject> global_object =
          CreateNewGlobals(global_proxy_template, global_proxy);
      HookUpGlobalObject(global_object);

      if (!ConfigureGlobalObjects(global_proxy_template)) return;
    } else {
      // The global proxy needs to be integrated into the native context.
      HookUpGlobalProxy(global_proxy);
    }
    DCHECK(!global_proxy->IsDetachedFrom(native_context()->global_object()));
  } else {
    base::ElapsedTimer timer;
    if (FLAG_profile_deserialization) timer.Start();
    DCHECK_EQ(0u, context_snapshot_index);
    // We get here if there was no context snapshot.
    CreateRoots();
    MathRandom::InitializeContext(isolate, native_context());
    Handle<JSFunction> empty_function = CreateEmptyFunction();
    CreateSloppyModeFunctionMaps(empty_function);
    CreateStrictModeFunctionMaps(empty_function);
    CreateObjectFunction(empty_function);
    CreateIteratorMaps(empty_function);
    CreateAsyncIteratorMaps(empty_function);
    CreateAsyncFunctionMaps(empty_function);
    Handle<JSGlobalObject> global_object =
        CreateNewGlobals(global_proxy_template, global_proxy);
    InitializeGlobal(global_object, empty_function, context_type);
    InitializeNormalizedMapCaches();

    if (!InstallNatives(context_type)) return;
    if (!InstallExtraNatives()) return;
    if (!ConfigureGlobalObjects(global_proxy_template)) return;

    isolate->counters()->contexts_created_from_scratch()->Increment();

    if (FLAG_profile_deserialization) {
      double ms = timer.Elapsed().InMillisecondsF();
      i::PrintF("[Initializing context from scratch took %0.3f ms]\n", ms);
    }
  }

  // Install experimental natives. Do not include them into the
  // snapshot as we should be able to turn them off at runtime. Re-installing
  // them after they have already been deserialized would also fail.
  if (context_type == FULL_CONTEXT) {
    if (!isolate->serializer_enabled()) {
      InitializeExperimentalGlobal();

      if (FLAG_experimental_extras) {
        if (!InstallExperimentalExtraNatives()) return;
      }

      // Store String.prototype's map again in case it has been changed by
      // experimental natives.
      Handle<JSFunction> string_function(native_context()->string_function(),
                                         isolate);
      JSObject* string_function_prototype =
          JSObject::cast(string_function->initial_map()->prototype());
      DCHECK(string_function_prototype->HasFastProperties());
      native_context()->set_string_function_prototype_map(
          string_function_prototype->map());
    }
  } else if (context_type == DEBUG_CONTEXT) {
    DCHECK(!isolate->serializer_enabled());
    InitializeExperimentalGlobal();
    if (!InstallDebuggerNatives()) return;
  }

  if (FLAG_disallow_code_generation_from_strings) {
    native_context()->set_allow_code_gen_from_strings(
        ReadOnlyRoots(isolate).false_value());
  }

  ConfigureUtilsObject(context_type);

  // We created new functions, which may require debug instrumentation.
  if (isolate->debug()->is_active()) {
    isolate->debug()->InstallDebugBreakTrampoline();
  }

  native_context()->ResetErrorsThrown();
  result_ = native_context();
}

Genesis::Genesis(Isolate* isolate,
                 MaybeHandle<JSGlobalProxy> maybe_global_proxy,
                 v8::Local<v8::ObjectTemplate> global_proxy_template)
    : isolate_(isolate), active_(isolate->bootstrapper()) {
  result_ = Handle<Context>::null();
  global_proxy_ = Handle<JSGlobalProxy>::null();

  // Before creating the roots we must save the context and restore it
  // on all function exits.
  SaveContext saved_context(isolate);

  const int proxy_size = JSGlobalProxy::SizeWithEmbedderFields(
      global_proxy_template->InternalFieldCount());

  Handle<JSGlobalProxy> global_proxy;
  if (!maybe_global_proxy.ToHandle(&global_proxy)) {
    global_proxy = factory()->NewUninitializedJSGlobalProxy(proxy_size);
  }

  // Create a remote object as the global object.
  Handle<ObjectTemplateInfo> global_proxy_data =
      Utils::OpenHandle(*global_proxy_template);
  Handle<FunctionTemplateInfo> global_constructor(
      FunctionTemplateInfo::cast(global_proxy_data->constructor()), isolate);

  Handle<ObjectTemplateInfo> global_object_template(
      ObjectTemplateInfo::cast(global_constructor->prototype_template()),
      isolate);
  Handle<JSObject> global_object =
      ApiNatives::InstantiateRemoteObject(
          global_object_template).ToHandleChecked();

  // (Re)initialize the global proxy object.
  DCHECK_EQ(global_proxy_data->embedder_field_count(),
            global_proxy_template->InternalFieldCount());
  Handle<Map> global_proxy_map = isolate->factory()->NewMap(
      JS_GLOBAL_PROXY_TYPE, proxy_size, TERMINAL_FAST_ELEMENTS_KIND);
  global_proxy_map->set_is_access_check_needed(true);
  global_proxy_map->set_has_hidden_prototype(true);
  global_proxy_map->set_may_have_interesting_symbols(true);

  // A remote global proxy has no native context.
  global_proxy->set_native_context(ReadOnlyRoots(heap()).null_value());

  // Configure the hidden prototype chain of the global proxy.
  JSObject::ForceSetPrototype(global_proxy, global_object);
  global_proxy->map()->SetConstructor(*global_constructor);
  // TODO(dcheng): This is a hack. Why does this need to be manually called
  // here? Line 4812 should have taken care of it?
  global_proxy->map()->set_has_hidden_prototype(true);

  global_proxy_ = global_proxy;
}

// Support for thread preemption.

// Reserve space for statics needing saving and restoring.
int Bootstrapper::ArchiveSpacePerThread() {
  return sizeof(NestingCounterType);
}


// Archive statics that are thread-local.
char* Bootstrapper::ArchiveState(char* to) {
  *reinterpret_cast<NestingCounterType*>(to) = nesting_;
  nesting_ = 0;
  return to + sizeof(NestingCounterType);
}


// Restore statics that are thread-local.
char* Bootstrapper::RestoreState(char* from) {
  nesting_ = *reinterpret_cast<NestingCounterType*>(from);
  return from + sizeof(NestingCounterType);
}


// Called when the top-level V8 mutex is destroyed.
void Bootstrapper::FreeThreadResources() {
  DCHECK(!IsActive());
}

}  // namespace internal
}  // namespace v8
