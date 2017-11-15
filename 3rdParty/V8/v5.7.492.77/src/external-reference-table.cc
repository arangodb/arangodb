// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/external-reference-table.h"

#include "src/accessors.h"
#include "src/assembler.h"
#include "src/builtins/builtins.h"
#include "src/counters.h"
#include "src/deoptimizer.h"
#include "src/ic/stub-cache.h"

#if defined(DEBUG) && defined(V8_OS_LINUX) && !defined(V8_OS_ANDROID)
#define SYMBOLIZE_FUNCTION
#include <execinfo.h>
#endif  // DEBUG && V8_OS_LINUX && !V8_OS_ANDROID

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Object* Builtin_##Name(int argc, Object** args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)
#undef FORWARD_DECLARE

ExternalReferenceTable* ExternalReferenceTable::instance(Isolate* isolate) {
  ExternalReferenceTable* external_reference_table =
      isolate->external_reference_table();
  if (external_reference_table == NULL) {
    external_reference_table = new ExternalReferenceTable(isolate);
    isolate->set_external_reference_table(external_reference_table);
  }
  return external_reference_table;
}

ExternalReferenceTable::ExternalReferenceTable(Isolate* isolate) {
  // nullptr is preserved through serialization/deserialization.
  Add(nullptr, "nullptr");
  AddReferences(isolate);
  AddBuiltins(isolate);
  AddRuntimeFunctions(isolate);
  AddIsolateAddresses(isolate);
  AddAccessors(isolate);
  AddStubCache(isolate);
  AddDeoptEntries(isolate);
  AddApiReferences(isolate);
}

#ifdef DEBUG
void ExternalReferenceTable::ResetCount() {
  for (ExternalReferenceEntry& entry : refs_) entry.count = 0;
}

void ExternalReferenceTable::PrintCount() {
  for (int i = 0; i < refs_.length(); i++) {
    v8::base::OS::Print("index=%5d count=%5d  %-60s\n", i, refs_[i].count,
                        refs_[i].name);
  }
}
#endif  // DEBUG

// static
const char* ExternalReferenceTable::ResolveSymbol(void* address) {
#ifdef SYMBOLIZE_FUNCTION
  return backtrace_symbols(&address, 1)[0];
#else
  return "<unresolved>";
#endif  // SYMBOLIZE_FUNCTION
}

void ExternalReferenceTable::AddReferences(Isolate* isolate) {
  // Miscellaneous
  Add(ExternalReference::roots_array_start(isolate).address(),
      "Heap::roots_array_start()");
  Add(ExternalReference::address_of_stack_limit(isolate).address(),
      "StackGuard::address_of_jslimit()");
  Add(ExternalReference::address_of_real_stack_limit(isolate).address(),
      "StackGuard::address_of_real_jslimit()");
  Add(ExternalReference::new_space_allocation_limit_address(isolate).address(),
      "Heap::NewSpaceAllocationLimitAddress()");
  Add(ExternalReference::new_space_allocation_top_address(isolate).address(),
      "Heap::NewSpaceAllocationTopAddress()");
  Add(ExternalReference::mod_two_doubles_operation(isolate).address(),
      "mod_two_doubles");
  Add(ExternalReference::handle_scope_next_address(isolate).address(),
      "HandleScope::next");
  Add(ExternalReference::handle_scope_limit_address(isolate).address(),
      "HandleScope::limit");
  Add(ExternalReference::handle_scope_level_address(isolate).address(),
      "HandleScope::level");
  Add(ExternalReference::new_deoptimizer_function(isolate).address(),
      "Deoptimizer::New()");
  Add(ExternalReference::compute_output_frames_function(isolate).address(),
      "Deoptimizer::ComputeOutputFrames()");
  Add(ExternalReference::address_of_min_int().address(),
      "LDoubleConstant::min_int");
  Add(ExternalReference::address_of_one_half().address(),
      "LDoubleConstant::one_half");
  Add(ExternalReference::isolate_address(isolate).address(), "isolate");
  Add(ExternalReference::interpreter_dispatch_table_address(isolate).address(),
      "Interpreter::dispatch_table_address");
  Add(ExternalReference::address_of_negative_infinity().address(),
      "LDoubleConstant::negative_infinity");
  Add(ExternalReference::power_double_double_function(isolate).address(),
      "power_double_double_function");
  Add(ExternalReference::ieee754_acos_function(isolate).address(),
      "base::ieee754::acos");
  Add(ExternalReference::ieee754_acosh_function(isolate).address(),
      "base::ieee754::acosh");
  Add(ExternalReference::ieee754_asin_function(isolate).address(),
      "base::ieee754::asin");
  Add(ExternalReference::ieee754_asinh_function(isolate).address(),
      "base::ieee754::asinh");
  Add(ExternalReference::ieee754_atan_function(isolate).address(),
      "base::ieee754::atan");
  Add(ExternalReference::ieee754_atanh_function(isolate).address(),
      "base::ieee754::atanh");
  Add(ExternalReference::ieee754_atan2_function(isolate).address(),
      "base::ieee754::atan2");
  Add(ExternalReference::ieee754_cbrt_function(isolate).address(),
      "base::ieee754::cbrt");
  Add(ExternalReference::ieee754_cos_function(isolate).address(),
      "base::ieee754::cos");
  Add(ExternalReference::ieee754_cosh_function(isolate).address(),
      "base::ieee754::cosh");
  Add(ExternalReference::ieee754_exp_function(isolate).address(),
      "base::ieee754::exp");
  Add(ExternalReference::ieee754_expm1_function(isolate).address(),
      "base::ieee754::expm1");
  Add(ExternalReference::ieee754_log_function(isolate).address(),
      "base::ieee754::log");
  Add(ExternalReference::ieee754_log1p_function(isolate).address(),
      "base::ieee754::log1p");
  Add(ExternalReference::ieee754_log10_function(isolate).address(),
      "base::ieee754::log10");
  Add(ExternalReference::ieee754_log2_function(isolate).address(),
      "base::ieee754::log2");
  Add(ExternalReference::ieee754_sin_function(isolate).address(),
      "base::ieee754::sin");
  Add(ExternalReference::ieee754_sinh_function(isolate).address(),
      "base::ieee754::sinh");
  Add(ExternalReference::ieee754_tan_function(isolate).address(),
      "base::ieee754::tan");
  Add(ExternalReference::ieee754_tanh_function(isolate).address(),
      "base::ieee754::tanh");
  Add(ExternalReference::store_buffer_top(isolate).address(),
      "store_buffer_top");
  Add(ExternalReference::address_of_the_hole_nan().address(), "the_hole_nan");
  Add(ExternalReference::get_date_field_function(isolate).address(),
      "JSDate::GetField");
  Add(ExternalReference::date_cache_stamp(isolate).address(),
      "date_cache_stamp");
  Add(ExternalReference::address_of_pending_message_obj(isolate).address(),
      "address_of_pending_message_obj");
  Add(ExternalReference::get_make_code_young_function(isolate).address(),
      "Code::MakeCodeYoung");
  Add(ExternalReference::cpu_features().address(), "cpu_features");
  Add(ExternalReference::old_space_allocation_top_address(isolate).address(),
      "Heap::OldSpaceAllocationTopAddress");
  Add(ExternalReference::old_space_allocation_limit_address(isolate).address(),
      "Heap::OldSpaceAllocationLimitAddress");
  Add(ExternalReference::allocation_sites_list_address(isolate).address(),
      "Heap::allocation_sites_list_address()");
  Add(ExternalReference::address_of_uint32_bias().address(), "uint32_bias");
  Add(ExternalReference::get_mark_code_as_executed_function(isolate).address(),
      "Code::MarkCodeAsExecuted");
  Add(ExternalReference::is_profiling_address(isolate).address(),
      "Isolate::is_profiling");
  Add(ExternalReference::scheduled_exception_address(isolate).address(),
      "Isolate::scheduled_exception");
  Add(ExternalReference::invoke_function_callback(isolate).address(),
      "InvokeFunctionCallback");
  Add(ExternalReference::invoke_accessor_getter_callback(isolate).address(),
      "InvokeAccessorGetterCallback");
  Add(ExternalReference::wasm_f32_trunc(isolate).address(),
      "wasm::f32_trunc_wrapper");
  Add(ExternalReference::wasm_f32_floor(isolate).address(),
      "wasm::f32_floor_wrapper");
  Add(ExternalReference::wasm_f32_ceil(isolate).address(),
      "wasm::f32_ceil_wrapper");
  Add(ExternalReference::wasm_f32_nearest_int(isolate).address(),
      "wasm::f32_nearest_int_wrapper");
  Add(ExternalReference::wasm_f64_trunc(isolate).address(),
      "wasm::f64_trunc_wrapper");
  Add(ExternalReference::wasm_f64_floor(isolate).address(),
      "wasm::f64_floor_wrapper");
  Add(ExternalReference::wasm_f64_ceil(isolate).address(),
      "wasm::f64_ceil_wrapper");
  Add(ExternalReference::wasm_f64_nearest_int(isolate).address(),
      "wasm::f64_nearest_int_wrapper");
  Add(ExternalReference::wasm_int64_to_float32(isolate).address(),
      "wasm::int64_to_float32_wrapper");
  Add(ExternalReference::wasm_uint64_to_float32(isolate).address(),
      "wasm::uint64_to_float32_wrapper");
  Add(ExternalReference::wasm_int64_to_float64(isolate).address(),
      "wasm::int64_to_float64_wrapper");
  Add(ExternalReference::wasm_uint64_to_float64(isolate).address(),
      "wasm::uint64_to_float64_wrapper");
  Add(ExternalReference::wasm_float32_to_int64(isolate).address(),
      "wasm::float32_to_int64_wrapper");
  Add(ExternalReference::wasm_float32_to_uint64(isolate).address(),
      "wasm::float32_to_uint64_wrapper");
  Add(ExternalReference::wasm_float64_to_int64(isolate).address(),
      "wasm::float64_to_int64_wrapper");
  Add(ExternalReference::wasm_float64_to_uint64(isolate).address(),
      "wasm::float64_to_uint64_wrapper");
  Add(ExternalReference::wasm_float64_pow(isolate).address(),
      "wasm::float64_pow");
  Add(ExternalReference::wasm_int64_div(isolate).address(), "wasm::int64_div");
  Add(ExternalReference::wasm_int64_mod(isolate).address(), "wasm::int64_mod");
  Add(ExternalReference::wasm_uint64_div(isolate).address(),
      "wasm::uint64_div");
  Add(ExternalReference::wasm_uint64_mod(isolate).address(),
      "wasm::uint64_mod");
  Add(ExternalReference::wasm_word32_ctz(isolate).address(),
      "wasm::word32_ctz");
  Add(ExternalReference::wasm_word64_ctz(isolate).address(),
      "wasm::word64_ctz");
  Add(ExternalReference::wasm_word32_popcnt(isolate).address(),
      "wasm::word32_popcnt");
  Add(ExternalReference::wasm_word64_popcnt(isolate).address(),
      "wasm::word64_popcnt");
  Add(ExternalReference::f64_acos_wrapper_function(isolate).address(),
      "f64_acos_wrapper");
  Add(ExternalReference::f64_asin_wrapper_function(isolate).address(),
      "f64_asin_wrapper");
  Add(ExternalReference::f64_mod_wrapper_function(isolate).address(),
      "f64_mod_wrapper");
  Add(ExternalReference::wasm_call_trap_callback_for_testing(isolate).address(),
      "wasm::call_trap_callback_for_testing");
  Add(ExternalReference::libc_memchr_function(isolate).address(),
      "libc_memchr");
  Add(ExternalReference::log_enter_external_function(isolate).address(),
      "Logger::EnterExternal");
  Add(ExternalReference::log_leave_external_function(isolate).address(),
      "Logger::LeaveExternal");
  Add(ExternalReference::address_of_minus_one_half().address(),
      "double_constants.minus_one_half");
  Add(ExternalReference::stress_deopt_count(isolate).address(),
      "Isolate::stress_deopt_count_address()");
  Add(ExternalReference::runtime_function_table_address(isolate).address(),
      "Runtime::runtime_function_table_address()");
  Add(ExternalReference::is_tail_call_elimination_enabled_address(isolate)
          .address(),
      "Isolate::is_tail_call_elimination_enabled_address()");
  Add(ExternalReference::address_of_float_abs_constant().address(),
      "float_absolute_constant");
  Add(ExternalReference::address_of_float_neg_constant().address(),
      "float_negate_constant");
  Add(ExternalReference::address_of_double_abs_constant().address(),
      "double_absolute_constant");
  Add(ExternalReference::address_of_double_neg_constant().address(),
      "double_negate_constant");
  Add(ExternalReference::promise_hook_address(isolate).address(),
      "Isolate::promise_hook_address()");

  // Debug addresses
  Add(ExternalReference::debug_after_break_target_address(isolate).address(),
      "Debug::after_break_target_address()");
  Add(ExternalReference::debug_is_active_address(isolate).address(),
      "Debug::is_active_address()");
  Add(ExternalReference::debug_hook_on_function_call_address(isolate).address(),
      "Debug::hook_on_function_call_address()");
  Add(ExternalReference::debug_last_step_action_address(isolate).address(),
      "Debug::step_in_enabled_address()");
  Add(ExternalReference::debug_suspended_generator_address(isolate).address(),
      "Debug::step_suspended_generator_address()");

#ifndef V8_INTERPRETED_REGEXP
  Add(ExternalReference::re_case_insensitive_compare_uc16(isolate).address(),
      "NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16()");
  Add(ExternalReference::re_check_stack_guard_state(isolate).address(),
      "RegExpMacroAssembler*::CheckStackGuardState()");
  Add(ExternalReference::re_grow_stack(isolate).address(),
      "NativeRegExpMacroAssembler::GrowStack()");
  Add(ExternalReference::re_word_character_map().address(),
      "NativeRegExpMacroAssembler::word_character_map");
  Add(ExternalReference::address_of_regexp_stack_limit(isolate).address(),
      "RegExpStack::limit_address()");
  Add(ExternalReference::address_of_regexp_stack_memory_address(isolate)
          .address(),
      "RegExpStack::memory_address()");
  Add(ExternalReference::address_of_regexp_stack_memory_size(isolate).address(),
      "RegExpStack::memory_size()");
  Add(ExternalReference::address_of_static_offsets_vector(isolate).address(),
      "OffsetsVector::static_offsets_vector");
#endif  // V8_INTERPRETED_REGEXP

  // Runtime entries
  Add(ExternalReference::delete_handle_scope_extensions(isolate).address(),
      "HandleScope::DeleteExtensions");
  Add(ExternalReference::incremental_marking_record_write_function(isolate)
          .address(),
      "IncrementalMarking::RecordWrite");
  Add(ExternalReference::incremental_marking_record_write_code_entry_function(
          isolate)
          .address(),
      "IncrementalMarking::RecordWriteOfCodeEntryFromCode");
  Add(ExternalReference::store_buffer_overflow_function(isolate).address(),
      "StoreBuffer::StoreBufferOverflow");
}

void ExternalReferenceTable::AddBuiltins(Isolate* isolate) {
  struct CBuiltinEntry {
    Address address;
    const char* name;
  };
  static const CBuiltinEntry c_builtins[] = {
#define DEF_ENTRY(Name, ...) {FUNCTION_ADDR(&Builtin_##Name), "Builtin_" #Name},
      BUILTIN_LIST_C(DEF_ENTRY)
#undef DEF_ENTRY
  };
  for (unsigned i = 0; i < arraysize(c_builtins); ++i) {
    Add(ExternalReference(c_builtins[i].address, isolate).address(),
        c_builtins[i].name);
  }

  struct BuiltinEntry {
    Builtins::Name id;
    const char* name;
  };
  static const BuiltinEntry builtins[] = {
#define DEF_ENTRY(Name, ...) {Builtins::k##Name, "Builtin_" #Name},
      BUILTIN_LIST_C(DEF_ENTRY) BUILTIN_LIST_A(DEF_ENTRY)
#undef DEF_ENTRY
  };
  for (unsigned i = 0; i < arraysize(builtins); ++i) {
    Add(isolate->builtins()->builtin_address(builtins[i].id), builtins[i].name);
  }
}

void ExternalReferenceTable::AddRuntimeFunctions(Isolate* isolate) {
  struct RuntimeEntry {
    Runtime::FunctionId id;
    const char* name;
  };

  static const RuntimeEntry runtime_functions[] = {
#define RUNTIME_ENTRY(name, i1, i2) {Runtime::k##name, "Runtime::" #name},
      FOR_EACH_INTRINSIC(RUNTIME_ENTRY)
#undef RUNTIME_ENTRY
  };

  for (unsigned i = 0; i < arraysize(runtime_functions); ++i) {
    ExternalReference ref(runtime_functions[i].id, isolate);
    Add(ref.address(), runtime_functions[i].name);
  }
}

void ExternalReferenceTable::AddIsolateAddresses(Isolate* isolate) {
  // Top addresses
  static const char* address_names[] = {
#define BUILD_NAME_LITERAL(Name, name) "Isolate::" #name "_address",
      FOR_EACH_ISOLATE_ADDRESS_NAME(BUILD_NAME_LITERAL) NULL
#undef BUILD_NAME_LITERAL
  };

  for (int i = 0; i < Isolate::kIsolateAddressCount; ++i) {
    Add(isolate->get_address_from_id(static_cast<Isolate::AddressId>(i)),
        address_names[i]);
  }
}

void ExternalReferenceTable::AddAccessors(Isolate* isolate) {
  // Accessors
  struct AccessorRefTable {
    Address address;
    const char* name;
  };

  static const AccessorRefTable getters[] = {
#define ACCESSOR_INFO_DECLARATION(name) \
  {FUNCTION_ADDR(&Accessors::name##Getter), "Accessors::" #name "Getter"},
      ACCESSOR_INFO_LIST(ACCESSOR_INFO_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION
  };
  static const AccessorRefTable setters[] = {
#define ACCESSOR_SETTER_DECLARATION(name) \
  { FUNCTION_ADDR(&Accessors::name), "Accessors::" #name},
      ACCESSOR_SETTER_LIST(ACCESSOR_SETTER_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION
  };

  for (unsigned i = 0; i < arraysize(getters); ++i) {
    Add(getters[i].address, getters[i].name);
  }

  for (unsigned i = 0; i < arraysize(setters); ++i) {
    Add(setters[i].address, setters[i].name);
  }
}

void ExternalReferenceTable::AddStubCache(Isolate* isolate) {
  StubCache* load_stub_cache = isolate->load_stub_cache();

  // Stub cache tables
  Add(load_stub_cache->key_reference(StubCache::kPrimary).address(),
      "Load StubCache::primary_->key");
  Add(load_stub_cache->value_reference(StubCache::kPrimary).address(),
      "Load StubCache::primary_->value");
  Add(load_stub_cache->map_reference(StubCache::kPrimary).address(),
      "Load StubCache::primary_->map");
  Add(load_stub_cache->key_reference(StubCache::kSecondary).address(),
      "Load StubCache::secondary_->key");
  Add(load_stub_cache->value_reference(StubCache::kSecondary).address(),
      "Load StubCache::secondary_->value");
  Add(load_stub_cache->map_reference(StubCache::kSecondary).address(),
      "Load StubCache::secondary_->map");

  StubCache* store_stub_cache = isolate->store_stub_cache();

  // Stub cache tables
  Add(store_stub_cache->key_reference(StubCache::kPrimary).address(),
      "Store StubCache::primary_->key");
  Add(store_stub_cache->value_reference(StubCache::kPrimary).address(),
      "Store StubCache::primary_->value");
  Add(store_stub_cache->map_reference(StubCache::kPrimary).address(),
      "Store StubCache::primary_->map");
  Add(store_stub_cache->key_reference(StubCache::kSecondary).address(),
      "Store StubCache::secondary_->key");
  Add(store_stub_cache->value_reference(StubCache::kSecondary).address(),
      "Store StubCache::secondary_->value");
  Add(store_stub_cache->map_reference(StubCache::kSecondary).address(),
      "Store StubCache::secondary_->map");
}

void ExternalReferenceTable::AddDeoptEntries(Isolate* isolate) {
  // Add a small set of deopt entry addresses to encoder without generating
  // the
  // deopt table code, which isn't possible at deserialization time.
  HandleScope scope(isolate);
  for (int entry = 0; entry < kDeoptTableSerializeEntryCount; ++entry) {
    Address address = Deoptimizer::GetDeoptimizationEntry(
        isolate, entry, Deoptimizer::LAZY,
        Deoptimizer::CALCULATE_ENTRY_ADDRESS);
    Add(address, "lazy_deopt");
  }
}

void ExternalReferenceTable::AddApiReferences(Isolate* isolate) {
  // Add external references provided by the embedder (a null-terminated
  // array).
  intptr_t* api_external_references = isolate->api_external_references();
  if (api_external_references != nullptr) {
    while (*api_external_references != 0) {
      Address address = reinterpret_cast<Address>(*api_external_references);
      Add(address, ResolveSymbol(address));
      api_external_references++;
    }
  }
}

}  // namespace internal
}  // namespace v8
