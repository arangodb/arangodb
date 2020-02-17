// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/wasm/wasm-module-builder.h"
#include "test/cctest/cctest.h"
#include "test/cctest/manually-externalized-buffer.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_grow_memory {

using testing::CompileAndInstantiateForTesting;
using v8::internal::testing::ManuallyExternalizedBuffer;

namespace {
void ExportAsMain(WasmFunctionBuilder* f) {
  f->builder()->AddExport(CStrVector("main"), f);
}
#define EMIT_CODE_WITH_END(f, code)  \
  do {                               \
    f->EmitCode(code, sizeof(code)); \
    f->Emit(kExprEnd);               \
  } while (false)

void Cleanup(Isolate* isolate = CcTest::InitIsolateOnce()) {
  // By sending a low memory notifications, we will try hard to collect all
  // garbage and will therefore also invoke all weak callbacks of actually
  // unreachable persistent handles.
  reinterpret_cast<v8::Isolate*>(isolate)->LowMemoryNotification();
}
}  // namespace

TEST(GrowMemDetaches) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    Handle<WasmMemoryObject> memory_object =
        WasmMemoryObject::New(isolate, 16, 100, SharedFlag::kNotShared)
            .ToHandleChecked();
    Handle<JSArrayBuffer> buffer(memory_object->array_buffer(), isolate);
    int32_t result = WasmMemoryObject::Grow(isolate, memory_object, 0);
    CHECK_EQ(16, result);
    CHECK_NE(*buffer, memory_object->array_buffer());
    CHECK(buffer->was_detached());
  }
  Cleanup();
}

TEST(Externalized_GrowMemMemSize) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    Handle<WasmMemoryObject> memory_object =
        WasmMemoryObject::New(isolate, 16, 100, SharedFlag::kNotShared)
            .ToHandleChecked();
    ManuallyExternalizedBuffer external(
        handle(memory_object->array_buffer(), isolate));
    int32_t result = WasmMemoryObject::Grow(isolate, memory_object, 0);
    CHECK_EQ(16, result);
    CHECK_NE(*external.buffer_, memory_object->array_buffer());
    CHECK(external.buffer_->was_detached());
  }
  Cleanup();
}

TEST(Run_WasmModule_Buffer_Externalized_GrowMem) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(6)), WASM_DROP,
                   WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);

    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "Test");
    const Handle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(
            isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()))
            .ToHandleChecked();
    Handle<WasmMemoryObject> memory_object(instance->memory_object(), isolate);

    // Fake the Embedder flow by externalizing the array buffer.
    ManuallyExternalizedBuffer external1(
        handle(memory_object->array_buffer(), isolate));

    // Grow using the API.
    uint32_t result = WasmMemoryObject::Grow(isolate, memory_object, 4);
    CHECK_EQ(16, result);
    CHECK(external1.buffer_->was_detached());  // growing always detaches
    CHECK_EQ(0, external1.buffer_->byte_length());

    CHECK_NE(*external1.buffer_, memory_object->array_buffer());

    // Fake the Embedder flow by externalizing the array buffer.
    ManuallyExternalizedBuffer external2(
        handle(memory_object->array_buffer(), isolate));

    // Grow using an internal WASM bytecode.
    result = testing::RunWasmModuleForTesting(isolate, instance, 0, nullptr);
    CHECK_EQ(26, result);
    CHECK(external2.buffer_->was_detached());  // growing always detaches
    CHECK_EQ(0, external2.buffer_->byte_length());
    CHECK_NE(*external2.buffer_, memory_object->array_buffer());
  }
  Cleanup();
}

}  // namespace test_grow_memory
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef EMIT_CODE_WITH_END
