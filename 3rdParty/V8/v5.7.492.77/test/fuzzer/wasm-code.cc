// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "include/v8.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/ostreams.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/fuzzer/fuzzer-support.h"

#define WASM_CODE_FUZZER_HASH_SEED 83

using namespace v8::internal::wasm;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Save the flag so that we can change it and restore it later.
  bool generate_test = v8::internal::FLAG_wasm_code_fuzzer_gen_test;
  if (generate_test) {
    v8::internal::OFStream os(stdout);

    os << "// Copyright 2017 the V8 project authors. All rights reserved."
       << std::endl;
    os << "// Use of this source code is governed by a BSD-style license that "
          "can be"
       << std::endl;
    os << "// found in the LICENSE file." << std::endl;
    os << std::endl;
    os << "load(\"test/mjsunit/wasm/wasm-constants.js\");" << std::endl;
    os << "load(\"test/mjsunit/wasm/wasm-module-builder.js\");" << std::endl;
    os << std::endl;
    os << "(function() {" << std::endl;
    os << "  var builder = new WasmModuleBuilder();" << std::endl;
    os << "  builder.addMemory(32, 32, false);" << std::endl;
    os << "  builder.addFunction(\"test\", kSig_i_iii)" << std::endl;
    os << "    .addBodyWithEnd([" << std::endl;
  }
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  v8::internal::Isolate* i_isolate =
      reinterpret_cast<v8::internal::Isolate*>(isolate);

  // Clear any pending exceptions from a prior run.
  if (i_isolate->has_pending_exception()) {
    i_isolate->clear_pending_exception();
  }

  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);

  v8::internal::AccountingAllocator allocator;
  v8::internal::Zone zone(&allocator, ZONE_NAME);

  TestSignatures sigs;

  WasmModuleBuilder builder(&zone);

  v8::internal::wasm::WasmFunctionBuilder* f =
      builder.AddFunction(sigs.i_iii());
  f->EmitCode(data, static_cast<uint32_t>(size));
  uint8_t end_opcode = kExprEnd;
  f->EmitCode(&end_opcode, 1);
  f->ExportAs(v8::internal::CStrVector("main"));

  ZoneBuffer buffer(&zone);
  builder.WriteTo(buffer);

  v8::internal::wasm::testing::SetupIsolateForWasmModule(i_isolate);

  v8::internal::HandleScope scope(i_isolate);

  ErrorThrower interpreter_thrower(i_isolate, "Interpreter");
  std::unique_ptr<const WasmModule> module(testing::DecodeWasmModuleForTesting(
      i_isolate, &interpreter_thrower, buffer.begin(), buffer.end(),
      v8::internal::wasm::ModuleOrigin::kWasmOrigin, true));

  // Clear the flag so that the WebAssembly code is not printed twice.
  v8::internal::FLAG_wasm_code_fuzzer_gen_test = false;
  if (module == nullptr) {
    if (generate_test) {
      v8::internal::OFStream os(stdout);
      os << "            ])" << std::endl;
      os << "            .exportFunc();" << std::endl;
      os << "  assertThrows(function() { builder.instantiate(); });"
         << std::endl;
      os << "})();" << std::endl;
    }
    return 0;
  }
  if (generate_test) {
    v8::internal::OFStream os(stdout);
    os << "            ])" << std::endl;
    os << "            .exportFunc();" << std::endl;
    os << "  var module = builder.instantiate();" << std::endl;
    os << "  module.exports.test(1, 2, 3);" << std::endl;
    os << "})();" << std::endl;
  }

  ModuleWireBytes wire_bytes(buffer.begin(), buffer.end());
  int32_t result_interpreted;
  bool possible_nondeterminism = false;
  {
    WasmVal args[] = {WasmVal(1), WasmVal(2), WasmVal(3)};
    result_interpreted = testing::InterpretWasmModule(
        i_isolate, &interpreter_thrower, module.get(), wire_bytes, 0, args,
        &possible_nondeterminism);
  }

  ErrorThrower compiler_thrower(i_isolate, "Compiler");
  v8::internal::Handle<v8::internal::JSObject> instance =
      testing::InstantiateModuleForTesting(i_isolate, &compiler_thrower,
                                           module.get(), wire_bytes);
  // Restore the flag.
  v8::internal::FLAG_wasm_code_fuzzer_gen_test = generate_test;
  if (!interpreter_thrower.error()) {
    CHECK(!instance.is_null());
  } else {
    return 0;
  }
  int32_t result_compiled;
  {
    v8::internal::Handle<v8::internal::Object> arguments[] = {
        v8::internal::handle(v8::internal::Smi::FromInt(1), i_isolate),
        v8::internal::handle(v8::internal::Smi::FromInt(2), i_isolate),
        v8::internal::handle(v8::internal::Smi::FromInt(3), i_isolate)};
    result_compiled = testing::CallWasmFunctionForTesting(
        i_isolate, instance, &compiler_thrower, "main", arraysize(arguments),
        arguments, v8::internal::wasm::ModuleOrigin::kWasmOrigin);
  }
  if (result_interpreted == bit_cast<int32_t>(0xdeadbeef)) {
    CHECK(i_isolate->has_pending_exception());
    i_isolate->clear_pending_exception();
  } else {
    // The WebAssembly spec allows the sign bit of NaN to be non-deterministic.
    // This sign bit may cause result_interpreted to be different than
    // result_compiled. Therefore we do not check the equality of the results
    // if the execution may have produced a NaN at some point.
    if (!possible_nondeterminism && (result_interpreted != result_compiled)) {
      V8_Fatal(__FILE__, __LINE__, "WasmCodeFuzzerHash=%x",
               v8::internal::StringHasher::HashSequentialString(
                   data, static_cast<int>(size), WASM_CODE_FUZZER_HASH_SEED));
    }
  }
  return 0;
}
