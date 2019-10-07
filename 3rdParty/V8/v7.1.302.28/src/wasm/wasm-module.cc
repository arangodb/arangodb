// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>

#include "src/api-inl.h"
#include "src/assembler-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/interface-types.h"
#include "src/frames-inl.h"
#include "src/objects.h"
#include "src/objects/js-array-inl.h"
#include "src/property-descriptor.h"
#include "src/simulator.h"
#include "src/snapshot/snapshot.h"
#include "src/v8.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

WireBytesRef WasmModule::LookupFunctionName(const ModuleWireBytes& wire_bytes,
                                            uint32_t function_index) const {
  if (!function_names) {
    function_names.reset(new std::unordered_map<uint32_t, WireBytesRef>());
    DecodeFunctionNames(wire_bytes.start(), wire_bytes.end(),
                        function_names.get());
  }
  auto it = function_names->find(function_index);
  if (it == function_names->end()) return WireBytesRef();
  return it->second;
}

void WasmModule::AddFunctionNameForTesting(int function_index,
                                           WireBytesRef name) {
  if (!function_names) {
    function_names.reset(new std::unordered_map<uint32_t, WireBytesRef>());
  }
  function_names->insert(std::make_pair(function_index, name));
}

// Get a string stored in the module bytes representing a name.
WasmName ModuleWireBytes::GetNameOrNull(WireBytesRef ref) const {
  if (!ref.is_set()) return {nullptr, 0};  // no name.
  CHECK(BoundsCheck(ref.offset(), ref.length()));
  return WasmName::cast(
      module_bytes_.SubVector(ref.offset(), ref.end_offset()));
}

// Get a string stored in the module bytes representing a function name.
WasmName ModuleWireBytes::GetNameOrNull(const WasmFunction* function,
                                        const WasmModule* module) const {
  return GetNameOrNull(module->LookupFunctionName(*this, function->func_index));
}

std::ostream& operator<<(std::ostream& os, const WasmFunctionName& name) {
  os << "#" << name.function_->func_index;
  if (!name.name_.is_empty()) {
    if (name.name_.start()) {
      os << ":";
      os.write(name.name_.start(), name.name_.length());
    }
  } else {
    os << "?";
  }
  return os;
}

WasmModule::WasmModule(std::unique_ptr<Zone> owned)
    : signature_zone(std::move(owned)) {}

bool IsWasmCodegenAllowed(Isolate* isolate, Handle<Context> context) {
  // TODO(wasm): Once wasm has its own CSP policy, we should introduce a
  // separate callback that includes information about the module about to be
  // compiled. For the time being, pass an empty string as placeholder for the
  // sources.
  if (auto wasm_codegen_callback = isolate->allow_wasm_code_gen_callback()) {
    return wasm_codegen_callback(
        v8::Utils::ToLocal(context),
        v8::Utils::ToLocal(isolate->factory()->empty_string()));
  }
  auto codegen_callback = isolate->allow_code_gen_callback();
  return codegen_callback == nullptr ||
         codegen_callback(
             v8::Utils::ToLocal(context),
             v8::Utils::ToLocal(isolate->factory()->empty_string()));
}

Handle<JSArray> GetImports(Isolate* isolate,
                           Handle<WasmModuleObject> module_object) {
  Factory* factory = isolate->factory();

  Handle<String> module_string = factory->InternalizeUtf8String("module");
  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");
  Handle<String> exception_string = factory->InternalizeUtf8String("exception");

  // Create the result array.
  const WasmModule* module = module_object->module();
  int num_imports = static_cast<int>(module->import_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_imports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_imports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_imports; ++index) {
    const WasmImport& import = module->import_table[index];

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    Handle<String> import_kind;
    switch (import.kind) {
      case kExternalFunction:
        import_kind = function_string;
        break;
      case kExternalTable:
        import_kind = table_string;
        break;
      case kExternalMemory:
        import_kind = memory_string;
        break;
      case kExternalGlobal:
        import_kind = global_string;
        break;
      case kExternalException:
        import_kind = exception_string;
        break;
      default:
        UNREACHABLE();
    }

    MaybeHandle<String> import_module =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, import.module_name);

    MaybeHandle<String> import_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, import.field_name);

    JSObject::AddProperty(isolate, entry, module_string,
                          import_module.ToHandleChecked(), NONE);
    JSObject::AddProperty(isolate, entry, name_string,
                          import_name.ToHandleChecked(), NONE);
    JSObject::AddProperty(isolate, entry, kind_string, import_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> GetExports(Isolate* isolate,
                           Handle<WasmModuleObject> module_object) {
  Factory* factory = isolate->factory();

  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");
  Handle<String> exception_string = factory->InternalizeUtf8String("exception");

  // Create the result array.
  const WasmModule* module = module_object->module();
  int num_exports = static_cast<int>(module->export_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_exports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_exports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_exports; ++index) {
    const WasmExport& exp = module->export_table[index];

    Handle<String> export_kind;
    switch (exp.kind) {
      case kExternalFunction:
        export_kind = function_string;
        break;
      case kExternalTable:
        export_kind = table_string;
        break;
      case kExternalMemory:
        export_kind = memory_string;
        break;
      case kExternalGlobal:
        export_kind = global_string;
        break;
      case kExternalException:
        export_kind = exception_string;
        break;
      default:
        UNREACHABLE();
    }

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    MaybeHandle<String> export_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, exp.name);

    JSObject::AddProperty(isolate, entry, name_string,
                          export_name.ToHandleChecked(), NONE);
    JSObject::AddProperty(isolate, entry, kind_string, export_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> GetCustomSections(Isolate* isolate,
                                  Handle<WasmModuleObject> module_object,
                                  Handle<String> name, ErrorThrower* thrower) {
  Factory* factory = isolate->factory();

  Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  std::vector<CustomSectionOffset> custom_sections =
      DecodeCustomSections(wire_bytes.start(), wire_bytes.end());

  std::vector<Handle<Object>> matching_sections;

  // Gather matching sections.
  for (auto& section : custom_sections) {
    MaybeHandle<String> section_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, section.name);

    if (!name->Equals(*section_name.ToHandleChecked())) continue;

    // Make a copy of the payload data in the section.
    size_t size = section.payload.length();
    void* memory =
        size == 0 ? nullptr : isolate->array_buffer_allocator()->Allocate(size);

    if (size && !memory) {
      thrower->RangeError("out of memory allocating custom section data");
      return Handle<JSArray>();
    }
    Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
    constexpr bool is_external = false;
    JSArrayBuffer::Setup(buffer, isolate, is_external, memory, size);
    memcpy(memory, wire_bytes.start() + section.payload.offset(),
           section.payload.length());

    matching_sections.push_back(buffer);
  }

  int num_custom_sections = static_cast<int>(matching_sections.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_custom_sections);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_custom_sections));

  for (int i = 0; i < num_custom_sections; i++) {
    storage->set(i, *matching_sections[i]);
  }

  return array_object;
}

Handle<FixedArray> DecodeLocalNames(Isolate* isolate,
                                    Handle<WasmModuleObject> module_object) {
  Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  LocalNames decoded_locals;
  DecodeLocalNames(wire_bytes.start(), wire_bytes.end(), &decoded_locals);
  Handle<FixedArray> locals_names =
      isolate->factory()->NewFixedArray(decoded_locals.max_function_index + 1);
  for (LocalNamesPerFunction& func : decoded_locals.names) {
    Handle<FixedArray> func_locals_names =
        isolate->factory()->NewFixedArray(func.max_local_index + 1);
    locals_names->set(func.function_index, *func_locals_names);
    for (LocalName& name : func.names) {
      Handle<String> name_str =
          WasmModuleObject::ExtractUtf8StringFromModuleBytes(
              isolate, module_object, name.name)
              .ToHandleChecked();
      func_locals_names->set(name.local_index, *name_str);
    }
  }
  return locals_names;
}

namespace {
template <typename T>
inline size_t VectorSize(const std::vector<T>& vector) {
  return sizeof(T) * vector.size();
}
}  // namespace

size_t EstimateWasmModuleSize(const WasmModule* module) {
  size_t estimate =
      sizeof(WasmModule) + VectorSize(module->signatures) +
      VectorSize(module->signature_ids) + VectorSize(module->functions) +
      VectorSize(module->data_segments) + VectorSize(module->tables) +
      VectorSize(module->import_table) + VectorSize(module->export_table) +
      VectorSize(module->exceptions) + VectorSize(module->table_inits);
  // TODO(wasm): include names table and wire bytes in size estimate
  return estimate;
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
