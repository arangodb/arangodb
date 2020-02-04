// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_bulk_memory {

namespace {
void CheckMemoryEquals(TestingModuleBuilder* builder, size_t index,
                       const std::vector<byte>& expected) {
  const byte* mem_start = builder->raw_mem_start<byte>();
  const byte* mem_end = builder->raw_mem_end<byte>();
  size_t mem_size = mem_end - mem_start;
  CHECK_LE(index, mem_size);
  CHECK_LE(index + expected.size(), mem_size);
  for (size_t i = 0; i < expected.size(); ++i) {
    CHECK_EQ(expected[i], mem_start[index + i]);
  }
}

void CheckMemoryEqualsZero(TestingModuleBuilder* builder, size_t index,
                           size_t length) {
  const byte* mem_start = builder->raw_mem_start<byte>();
  const byte* mem_end = builder->raw_mem_end<byte>();
  size_t mem_size = mem_end - mem_start;
  CHECK_LE(index, mem_size);
  CHECK_LE(index + length, mem_size);
  for (size_t i = 0; i < length; ++i) {
    CHECK_EQ(0, mem_start[index + i]);
  }
}

void CheckMemoryEqualsFollowedByZeroes(TestingModuleBuilder* builder,
                                       const std::vector<byte>& expected) {
  CheckMemoryEquals(builder, 0, expected);
  CheckMemoryEqualsZero(builder, expected.size(),
                        builder->mem_size() - expected.size());
}
}  // namespace

WASM_EXEC_TEST(MemoryInit) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  r.builder().AddPassiveDataSegment(ArrayVector(data));
  BUILD(r,
        WASM_MEMORY_INIT(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                         WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  // All zeroes.
  CheckMemoryEqualsZero(&r.builder(), 0, kWasmPageSize);

  // Copy all bytes from data segment 0, to memory at [10, 20).
  CHECK_EQ(0, r.Call(10, 0, 10));
  CheckMemoryEqualsFollowedByZeroes(
      &r.builder(),
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

  // Copy bytes in range [5, 10) from data segment 0, to memory at [0, 5).
  CHECK_EQ(0, r.Call(0, 5, 5));
  CheckMemoryEqualsFollowedByZeroes(
      &r.builder(),
      {5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

  // Copy 0 bytes does nothing.
  CHECK_EQ(0, r.Call(10, 1, 0));
  CheckMemoryEqualsFollowedByZeroes(
      &r.builder(),
      {5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

  // Copy 0 at end of memory region or data segment is OK.
  CHECK_EQ(0, r.Call(kWasmPageSize, 0, 0));
  CHECK_EQ(0, r.Call(0, sizeof(data), 0));
}

WASM_EXEC_TEST(MemoryInitOutOfBoundsData) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  r.builder().AddPassiveDataSegment(ArrayVector(data));
  BUILD(r,
        WASM_MEMORY_INIT(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                         WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  const uint32_t last_5_bytes = kWasmPageSize - 5;

  // Write all values up to the out-of-bounds write.
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize - 5, 0, 6));
  CheckMemoryEquals(&r.builder(), last_5_bytes, {0, 1, 2, 3, 4});

  // Write all values up to the out-of-bounds read.
  r.builder().BlankMemory();
  CHECK_EQ(0xDEADBEEF, r.Call(0, 5, 6));
  CheckMemoryEqualsFollowedByZeroes(&r.builder(), {5, 6, 7, 8, 9});
}

WASM_EXEC_TEST(MemoryInitOutOfBounds) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[kWasmPageSize] = {};
  r.builder().AddPassiveDataSegment(ArrayVector(data));
  BUILD(r,
        WASM_MEMORY_INIT(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                         WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  // OK, copy the full data segment to memory.
  r.Call(0, 0, kWasmPageSize);

  // Source range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1000, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, kWasmPageSize, 1));

  // Destination range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(1000, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize, 0, 1));

  // Copy 0 out-of-bounds succeeds.
  CHECK_EQ(0, r.Call(kWasmPageSize + 1, 0, 0));
  CHECK_EQ(0, r.Call(0, kWasmPageSize + 1, 0));

  // Make sure bounds aren't checked with 32-bit wrapping.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 1, 0xFFFFFFFF));
}

WASM_EXEC_TEST(MemoryCopy) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  byte* mem = r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte initial[] = {0, 11, 22, 33, 44, 55, 66, 77};
  memcpy(mem, initial, sizeof(initial));

  // Copy from [1, 8] to [10, 16].
  CHECK_EQ(0, r.Call(10, 1, 8));
  CheckMemoryEqualsFollowedByZeroes(
      &r.builder(),
      {0, 11, 22, 33, 44, 55, 66, 77, 0, 0, 11, 22, 33, 44, 55, 66, 77});

  // Copy 0 bytes does nothing.
  CHECK_EQ(0, r.Call(10, 2, 0));
  CheckMemoryEqualsFollowedByZeroes(
      &r.builder(),
      {0, 11, 22, 33, 44, 55, 66, 77, 0, 0, 11, 22, 33, 44, 55, 66, 77});

  // Copy 0 at end of memory region is OK.
  CHECK_EQ(0, r.Call(kWasmPageSize, 0, 0));
  CHECK_EQ(0, r.Call(0, kWasmPageSize, 0));
}

WASM_EXEC_TEST(MemoryCopyOverlapping) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  byte* mem = r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte initial[] = {10, 20, 30};
  memcpy(mem, initial, sizeof(initial));

  // Copy from [0, 3] -> [2, 5]. The copy must not overwrite 30 before copying
  // it (i.e. cannot copy forward in this case).
  CHECK_EQ(0, r.Call(2, 0, 3));
  CheckMemoryEqualsFollowedByZeroes(&r.builder(), {10, 20, 10, 20, 30});

  // Copy from [2, 5] -> [0, 3]. The copy must not write the first 10 (i.e.
  // cannot copy backward in this case).
  CHECK_EQ(0, r.Call(0, 2, 3));
  CheckMemoryEqualsFollowedByZeroes(&r.builder(), {10, 20, 30, 20, 30});
}

WASM_EXEC_TEST(MemoryCopyOutOfBoundsData) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  byte* mem = r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte data[] = {11, 22, 33, 44, 55, 66, 77, 88};
  memcpy(mem, data, sizeof(data));

  const uint32_t last_5_bytes = kWasmPageSize - 5;

  // Copy with source < destination. Copy would happen backwards,
  // but the first byte to copy is out-of-bounds, so no data should be written.
  CHECK_EQ(0xDEADBEEF, r.Call(last_5_bytes, 0, 6));
  CheckMemoryEquals(&r.builder(), last_5_bytes, {0, 0, 0, 0, 0});

  // Copy overlapping with destination < source. Copy will happen forwards, up
  // to the out-of-bounds access.
  r.builder().BlankMemory();
  memcpy(mem + last_5_bytes, data, 5);
  CHECK_EQ(0xDEADBEEF, r.Call(0, last_5_bytes, kWasmPageSize));
  CheckMemoryEquals(&r.builder(), 0, {11, 22, 33, 44, 55});

  // Copy overlapping with source < destination. Copy would happen backwards,
  // but the first byte to copy is out-of-bounds, so no data should be written.
  r.builder().BlankMemory();
  memcpy(mem, data, 5);
  CHECK_EQ(0xDEADBEEF, r.Call(last_5_bytes, 0, kWasmPageSize));
  CheckMemoryEquals(&r.builder(), last_5_bytes, {0, 0, 0, 0, 0});
}

WASM_EXEC_TEST(MemoryCopyOutOfBounds) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_COPY(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  // Copy full range is OK.
  CHECK_EQ(0, r.Call(0, 0, kWasmPageSize));

  // Source range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, 1000, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(0, kWasmPageSize, 1));

  // Destination range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(1000, 0, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize, 0, 1));

  // Copy 0 out-of-bounds always succeeds.
  CHECK_EQ(0, r.Call(kWasmPageSize + 1, 0, 0));
  CHECK_EQ(0, r.Call(0, kWasmPageSize + 1, 0));

  // Make sure bounds aren't checked with 32-bit wrapping.
  CHECK_EQ(0xDEADBEEF, r.Call(1, 1, 0xFFFFFFFF));
}

WASM_EXEC_TEST(MemoryFill) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);
  CHECK_EQ(0, r.Call(1, 33, 5));
  CheckMemoryEqualsFollowedByZeroes(&r.builder(), {0, 33, 33, 33, 33, 33});

  CHECK_EQ(0, r.Call(4, 66, 4));
  CheckMemoryEqualsFollowedByZeroes(&r.builder(),
                                    {0, 33, 33, 33, 66, 66, 66, 66});

  // Fill 0 bytes does nothing.
  CHECK_EQ(0, r.Call(4, 66, 0));
  CheckMemoryEqualsFollowedByZeroes(&r.builder(),
                                    {0, 33, 33, 33, 66, 66, 66, 66});

  // Fill 0 at end of memory region is OK.
  CHECK_EQ(0, r.Call(kWasmPageSize, 66, 0));
}

WASM_EXEC_TEST(MemoryFillValueWrapsToByte) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);
  CHECK_EQ(0, r.Call(0, 1000, 3));
  const byte expected = 1000 & 255;
  CheckMemoryEqualsFollowedByZeroes(&r.builder(),
                                    {expected, expected, expected});
}

WASM_EXEC_TEST(MemoryFillOutOfBoundsData) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);
  const byte v = 123;
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize - 5, v, 999));
  CheckMemoryEquals(&r.builder(), kWasmPageSize - 6, {0, v, v, v, v, v});
}

WASM_EXEC_TEST(MemoryFillOutOfBounds) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_MEMORY_FILL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
      kExprI32Const, 0);

  const byte v = 123;

  // Destination range must not be out of bounds.
  CHECK_EQ(0xDEADBEEF, r.Call(1, v, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(1000, v, kWasmPageSize));
  CHECK_EQ(0xDEADBEEF, r.Call(kWasmPageSize, v, 1));

  // Fill 0 out-of-bounds succeeds.
  CHECK_EQ(0, r.Call(kWasmPageSize + 1, v, 0));

  // Make sure bounds aren't checked with 32-bit wrapping.
  CHECK_EQ(0xDEADBEEF, r.Call(1, v, 0xFFFFFFFF));
}

WASM_EXEC_TEST(DataDropTwice) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0};
  r.builder().AddPassiveDataSegment(ArrayVector(data));
  BUILD(r, WASM_DATA_DROP(0), kExprI32Const, 0);

  CHECK_EQ(0, r.Call());
  CHECK_EQ(0xDEADBEEF, r.Call());
}

WASM_EXEC_TEST(DataDropThenMemoryInit) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddMemory(kWasmPageSize);
  const byte data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  r.builder().AddPassiveDataSegment(ArrayVector(data));
  BUILD(r, WASM_DATA_DROP(0),
        WASM_MEMORY_INIT(0, WASM_I32V_1(0), WASM_I32V_1(1), WASM_I32V_1(2)),
        kExprI32Const, 0);

  CHECK_EQ(0xDEADBEEF, r.Call());
}

void TestTableCopyInbounds(ExecutionTier execution_tier, int table_dst,
                           int table_src) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  const uint32_t kTableSize = 5;
  // Add 10 function tables, even though we only test one table.
  for (int i = 0; i < 10; ++i) {
    r.builder().AddIndirectFunctionTable(nullptr, kTableSize);
  }
  BUILD(r,
        WASM_TABLE_COPY(table_dst, table_src, WASM_GET_LOCAL(0),
                        WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  for (uint32_t i = 0; i <= kTableSize; ++i) {
    r.CheckCallViaJS(0, 0, 0, i);  // nop
    r.CheckCallViaJS(0, 0, i, kTableSize - i);
    r.CheckCallViaJS(0, i, 0, kTableSize - i);
  }
}

WASM_EXEC_TEST(TableCopyInboundsFrom0To0) {
  TestTableCopyInbounds(execution_tier, 0, 0);
}

WASM_EXEC_TEST(TableCopyInboundsFrom3To0) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyInbounds(execution_tier, 3, 0);
}

WASM_EXEC_TEST(TableCopyInboundsFrom5To9) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyInbounds(execution_tier, 5, 9);
}

WASM_EXEC_TEST(TableCopyInboundsFrom6To6) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyInbounds(execution_tier, 6, 6);
}

namespace {
template <typename... Args>
void CheckTable(Isolate* isolate, Handle<WasmTableObject> table, Args... args) {
  uint32_t args_length = static_cast<uint32_t>(sizeof...(args));
  CHECK_EQ(table->current_length(), args_length);
  Handle<Object> handles[] = {args...};
  for (uint32_t i = 0; i < args_length; ++i) {
    CHECK(WasmTableObject::Get(isolate, table, i).is_identical_to(handles[i]));
  }
}

template <typename WasmRunner, typename... Args>
void CheckTableCall(Isolate* isolate, Handle<WasmTableObject> table,
                    WasmRunner* r, uint32_t function_index, Args... args) {
  uint32_t args_length = static_cast<uint32_t>(sizeof...(args));
  CHECK_EQ(table->current_length(), args_length);
  double expected[] = {args...};
  for (uint32_t i = 0; i < args_length; ++i) {
    Handle<Object> buffer[] = {isolate->factory()->NewNumber(i)};
    r->CheckCallApplyViaJS(expected[i], function_index, buffer, 1);
  }
}
}  // namespace

void TestTableInitElems(ExecutionTier execution_tier, int table_index) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  const uint32_t kTableSize = 5;
  std::vector<uint32_t> function_indexes;
  const uint32_t sig_index = r.builder().AddSignature(sigs.i_v());

  for (uint32_t i = 0; i < kTableSize; ++i) {
    WasmFunctionCompiler& fn = r.NewFunction(sigs.i_v(), "f");
    BUILD(fn, WASM_I32V_1(i));
    fn.SetSigIndex(sig_index);
    function_indexes.push_back(fn.function_index());
  }

  // Passive element segment has [f0, f1, f2, f3, f4, null].
  function_indexes.push_back(WasmElemSegment::kNullIndex);

  // Add 10 function tables, even though we only test one table.
  for (int i = 0; i < 10; ++i) {
    r.builder().AddIndirectFunctionTable(nullptr, kTableSize);
  }
  r.builder().AddPassiveElementSegment(function_indexes);

  WasmFunctionCompiler& call = r.NewFunction(sigs.i_i(), "call");
  BUILD(call,
        WASM_CALL_INDIRECT_TABLE0(table_index, sig_index, WASM_GET_LOCAL(0)));
  const uint32_t call_index = call.function_index();

  BUILD(r,
        WASM_TABLE_INIT(table_index, 0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                        WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  auto table =
      handle(WasmTableObject::cast(
                 r.builder().instance_object()->tables().get(table_index)),
             isolate);
  const double null = 0xDEADBEEF;

  CheckTableCall(isolate, table, &r, call_index, null, null, null, null, null);

  // 0 count is ok in bounds, and at end of regions.
  r.CheckCallViaJS(0, 0, 0, 0);
  r.CheckCallViaJS(0, kTableSize, 0, 0);
  r.CheckCallViaJS(0, 0, kTableSize, 0);

  // Test actual writes.
  r.CheckCallViaJS(0, 0, 0, 1);
  CheckTableCall(isolate, table, &r, call_index, 0, null, null, null, null);
  r.CheckCallViaJS(0, 0, 0, 2);
  CheckTableCall(isolate, table, &r, call_index, 0, 1, null, null, null);
  r.CheckCallViaJS(0, 0, 0, 3);
  CheckTableCall(isolate, table, &r, call_index, 0, 1, 2, null, null);
  r.CheckCallViaJS(0, 3, 0, 2);
  CheckTableCall(isolate, table, &r, call_index, 0, 1, 2, 0, 1);
  r.CheckCallViaJS(0, 3, 1, 2);
  CheckTableCall(isolate, table, &r, call_index, 0, 1, 2, 1, 2);
  r.CheckCallViaJS(0, 3, 2, 2);
  CheckTableCall(isolate, table, &r, call_index, 0, 1, 2, 2, 3);
  r.CheckCallViaJS(0, 3, 3, 2);
  CheckTableCall(isolate, table, &r, call_index, 0, 1, 2, 3, 4);
}

WASM_EXEC_TEST(TableInitElems0) { TestTableInitElems(execution_tier, 0); }
WASM_EXEC_TEST(TableInitElems7) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableInitElems(execution_tier, 7);
}
WASM_EXEC_TEST(TableInitElems9) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableInitElems(execution_tier, 9);
}

void TestTableInitOob(ExecutionTier execution_tier, int table_index) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  const uint32_t kTableSize = 5;
  std::vector<uint32_t> function_indexes;
  const uint32_t sig_index = r.builder().AddSignature(sigs.i_v());

  for (uint32_t i = 0; i < kTableSize; ++i) {
    WasmFunctionCompiler& fn = r.NewFunction(sigs.i_v(), "f");
    BUILD(fn, WASM_I32V_1(i));
    fn.SetSigIndex(sig_index);
    function_indexes.push_back(fn.function_index());
  }

  for (int i = 0; i < 10; ++i) {
    r.builder().AddIndirectFunctionTable(nullptr, kTableSize);
  }
  r.builder().AddPassiveElementSegment(function_indexes);

  WasmFunctionCompiler& call = r.NewFunction(sigs.i_i(), "call");
  BUILD(call,
        WASM_CALL_INDIRECT_TABLE0(table_index, sig_index, WASM_GET_LOCAL(0)));
  const uint32_t call_index = call.function_index();

  BUILD(r,
        WASM_TABLE_INIT(table_index, 0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                        WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  auto table =
      handle(WasmTableObject::cast(
                 r.builder().instance_object()->tables().get(table_index)),
             isolate);
  const double null = 0xDEADBEEF;

  CheckTableCall(isolate, table, &r, call_index, null, null, null, null, null);

  // Write all values up to the out-of-bounds write.
  r.CheckCallViaJS(0xDEADBEEF, 3, 0, 3);
  CheckTableCall(isolate, table, &r, call_index, null, null, null, 0, 1);

  // Write all values up to the out-of-bounds read.
  r.CheckCallViaJS(0xDEADBEEF, 0, 3, 3);
  CheckTableCall(isolate, table, &r, call_index, 3, 4, null, 0, 1);

  // 0-count is never oob.
  r.CheckCallViaJS(0, kTableSize + 1, 0, 0);
  r.CheckCallViaJS(0, 0, kTableSize + 1, 0);

  r.CheckCallViaJS(0xDEADBEEF, 0, 0, 6);
  r.CheckCallViaJS(0xDEADBEEF, 0, 1, 5);
  r.CheckCallViaJS(0xDEADBEEF, 0, 2, 4);
  r.CheckCallViaJS(0xDEADBEEF, 0, 3, 3);
  r.CheckCallViaJS(0xDEADBEEF, 0, 4, 2);
  r.CheckCallViaJS(0xDEADBEEF, 0, 5, 1);

  r.CheckCallViaJS(0xDEADBEEF, 0, 0, 6);
  r.CheckCallViaJS(0xDEADBEEF, 1, 0, 5);
  r.CheckCallViaJS(0xDEADBEEF, 2, 0, 4);
  r.CheckCallViaJS(0xDEADBEEF, 3, 0, 3);
  r.CheckCallViaJS(0xDEADBEEF, 4, 0, 2);
  r.CheckCallViaJS(0xDEADBEEF, 5, 0, 1);

  r.CheckCallViaJS(0xDEADBEEF, 10, 0, 1);
  r.CheckCallViaJS(0xDEADBEEF, 0, 10, 1);
}

WASM_EXEC_TEST(TableInitOob0) { TestTableInitOob(execution_tier, 0); }
WASM_EXEC_TEST(TableInitOob7) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableInitOob(execution_tier, 7);
}
WASM_EXEC_TEST(TableInitOob9) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableInitOob(execution_tier, 9);
}

void TestTableCopyElems(ExecutionTier execution_tier, int table_dst,
                        int table_src) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  const uint32_t kTableSize = 5;
  uint16_t function_indexes[kTableSize];
  const uint32_t sig_index = r.builder().AddSignature(sigs.i_v());

  for (uint32_t i = 0; i < kTableSize; ++i) {
    WasmFunctionCompiler& fn = r.NewFunction(sigs.i_v(), "f");
    BUILD(fn, WASM_I32V_1(i));
    fn.SetSigIndex(sig_index);
    function_indexes[i] = fn.function_index();
  }

  for (int i = 0; i < 10; ++i) {
    r.builder().AddIndirectFunctionTable(function_indexes, kTableSize);
  }

  BUILD(r,
        WASM_TABLE_COPY(table_dst, table_src, WASM_GET_LOCAL(0),
                        WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  r.builder().FreezeSignatureMapAndInitializeWrapperCache();

  auto table =
      handle(WasmTableObject::cast(
                 r.builder().instance_object()->tables().get(table_dst)),
             isolate);
  r.CheckCallViaJS(0, 0, 0, kTableSize);
  auto f0 = WasmTableObject::Get(isolate, table, 0);
  auto f1 = WasmTableObject::Get(isolate, table, 1);
  auto f2 = WasmTableObject::Get(isolate, table, 2);
  auto f3 = WasmTableObject::Get(isolate, table, 3);
  auto f4 = WasmTableObject::Get(isolate, table, 4);

  if (table_dst == table_src) {
    CheckTable(isolate, table, f0, f1, f2, f3, f4);
    r.CheckCallViaJS(0, 0, 1, 1);
    CheckTable(isolate, table, f1, f1, f2, f3, f4);
    r.CheckCallViaJS(0, 0, 1, 2);
    CheckTable(isolate, table, f1, f2, f2, f3, f4);
    r.CheckCallViaJS(0, 3, 0, 2);
    CheckTable(isolate, table, f1, f2, f2, f1, f2);
    r.CheckCallViaJS(0, 1, 0, 2);
    CheckTable(isolate, table, f1, f1, f2, f1, f2);
  } else {
    CheckTable(isolate, table, f0, f1, f2, f3, f4);
    r.CheckCallViaJS(0, 0, 1, 1);
    CheckTable(isolate, table, f1, f1, f2, f3, f4);
    r.CheckCallViaJS(0, 0, 1, 2);
    CheckTable(isolate, table, f1, f2, f2, f3, f4);
    r.CheckCallViaJS(0, 3, 0, 2);
    CheckTable(isolate, table, f1, f2, f2, f0, f1);
    r.CheckCallViaJS(0, 1, 0, 2);
    CheckTable(isolate, table, f1, f0, f1, f0, f1);
  }
}

WASM_EXEC_TEST(TableCopyElemsFrom0To0) {
  TestTableCopyElems(execution_tier, 0, 0);
}

WASM_EXEC_TEST(TableCopyElemsFrom3To0) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyElems(execution_tier, 3, 0);
}

WASM_EXEC_TEST(TableCopyElemsFrom5To9) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyElems(execution_tier, 5, 9);
}

WASM_EXEC_TEST(TableCopyElemsFrom6To6) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyElems(execution_tier, 6, 6);
}

void TestTableCopyCalls(ExecutionTier execution_tier, int table_dst,
                        int table_src) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  const uint32_t kTableSize = 5;
  uint16_t function_indexes[kTableSize];
  const uint32_t sig_index = r.builder().AddSignature(sigs.i_v());

  for (uint32_t i = 0; i < kTableSize; ++i) {
    WasmFunctionCompiler& fn = r.NewFunction(sigs.i_v(), "f");
    BUILD(fn, WASM_I32V_1(i));
    fn.SetSigIndex(sig_index);
    function_indexes[i] = fn.function_index();
  }

  for (int i = 0; i < 10; ++i) {
    r.builder().AddIndirectFunctionTable(function_indexes, kTableSize);
  }

  WasmFunctionCompiler& call = r.NewFunction(sigs.i_i(), "call");
  BUILD(call,
        WASM_CALL_INDIRECT_TABLE0(table_dst, sig_index, WASM_GET_LOCAL(0)));
  const uint32_t call_index = call.function_index();

  BUILD(r,
        WASM_TABLE_COPY(table_dst, table_src, WASM_GET_LOCAL(0),
                        WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  auto table =
      handle(WasmTableObject::cast(
                 r.builder().instance_object()->tables().get(table_dst)),
             isolate);

  if (table_dst == table_src) {
    CheckTableCall(isolate, table, &r, call_index, 0, 1, 2, 3, 4);
    r.CheckCallViaJS(0, 0, 1, 1);
    CheckTableCall(isolate, table, &r, call_index, 1, 1, 2, 3, 4);
    r.CheckCallViaJS(0, 0, 1, 2);
    CheckTableCall(isolate, table, &r, call_index, 1, 2, 2, 3, 4);
    r.CheckCallViaJS(0, 3, 0, 2);
    CheckTableCall(isolate, table, &r, call_index, 1, 2, 2, 1, 2);
  } else {
    CheckTableCall(isolate, table, &r, call_index, 0, 1, 2, 3, 4);
    r.CheckCallViaJS(0, 0, 1, 1);
    CheckTableCall(isolate, table, &r, call_index, 1, 1, 2, 3, 4);
    r.CheckCallViaJS(0, 0, 1, 2);
    CheckTableCall(isolate, table, &r, call_index, 1, 2, 2, 3, 4);
    r.CheckCallViaJS(0, 3, 0, 2);
    CheckTableCall(isolate, table, &r, call_index, 1, 2, 2, 0, 1);
  }
}

WASM_EXEC_TEST(TableCopyCallsFrom0To0) {
  TestTableCopyCalls(execution_tier, 0, 0);
}

WASM_EXEC_TEST(TableCopyCallsFrom3To0) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyCalls(execution_tier, 3, 0);
}

WASM_EXEC_TEST(TableCopyCallsFrom5To9) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyCalls(execution_tier, 5, 9);
}

WASM_EXEC_TEST(TableCopyCallsFrom6To6) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyCalls(execution_tier, 6, 6);
}

void TestTableCopyOobWrites(ExecutionTier execution_tier, int table_dst,
                            int table_src) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  const uint32_t kTableSize = 5;
  uint16_t function_indexes[kTableSize];
  const uint32_t sig_index = r.builder().AddSignature(sigs.i_v());

  for (uint32_t i = 0; i < kTableSize; ++i) {
    WasmFunctionCompiler& fn = r.NewFunction(sigs.i_v(), "f");
    BUILD(fn, WASM_I32V_1(i));
    fn.SetSigIndex(sig_index);
    function_indexes[i] = fn.function_index();
  }

  for (int i = 0; i < 10; ++i) {
    r.builder().AddIndirectFunctionTable(function_indexes, kTableSize);
  }

  BUILD(r,
        WASM_TABLE_COPY(table_dst, table_src, WASM_GET_LOCAL(0),
                        WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  r.builder().FreezeSignatureMapAndInitializeWrapperCache();

  auto table =
      handle(WasmTableObject::cast(
                 r.builder().instance_object()->tables().get(table_dst)),
             isolate);
  // Fill the dst table with values from the src table, to make checks easier.
  r.CheckCallViaJS(0, 0, 0, kTableSize);
  auto f0 = WasmTableObject::Get(isolate, table, 0);
  auto f1 = WasmTableObject::Get(isolate, table, 1);
  auto f2 = WasmTableObject::Get(isolate, table, 2);
  auto f3 = WasmTableObject::Get(isolate, table, 3);
  auto f4 = WasmTableObject::Get(isolate, table, 4);

  CheckTable(isolate, table, f0, f1, f2, f3, f4);

  // Non-overlapping, src < dst. Because of src < dst, we copy backwards.
  // Therefore the first access already traps, and the table is not changed.
  r.CheckCallViaJS(0xDEADBEEF, 3, 0, 3);
  CheckTable(isolate, table, f0, f1, f2, f3, f4);

  // Non-overlapping, dst < src.
  r.CheckCallViaJS(0xDEADBEEF, 0, 4, 2);
  CheckTable(isolate, table, f4, f1, f2, f3, f4);

  // Overlapping, src < dst. This is required to copy backward, but the first
  // access will be out-of-bounds, so nothing changes.
  r.CheckCallViaJS(0xDEADBEEF, 3, 0, 99);
  CheckTable(isolate, table, f4, f1, f2, f3, f4);

  // Overlapping, dst < src.
  r.CheckCallViaJS(0xDEADBEEF, 0, 1, 99);
  CheckTable(isolate, table, f1, f2, f3, f4, f4);
}

WASM_EXEC_TEST(TableCopyOobWritesFrom0To0) {
  TestTableCopyOobWrites(execution_tier, 0, 0);
}

WASM_EXEC_TEST(TableCopyOobWritesFrom3To0) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyOobWrites(execution_tier, 3, 0);
}

WASM_EXEC_TEST(TableCopyOobWritesFrom5To9) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyOobWrites(execution_tier, 5, 9);
}

WASM_EXEC_TEST(TableCopyOobWritesFrom6To6) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyOobWrites(execution_tier, 6, 6);
}

void TestTableCopyOob1(ExecutionTier execution_tier, int table_dst,
                       int table_src) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(execution_tier);
  const uint32_t kTableSize = 5;

  for (int i = 0; i < 10; ++i) {
    r.builder().AddIndirectFunctionTable(nullptr, kTableSize);
  }

  BUILD(r,
        WASM_TABLE_COPY(table_dst, table_src, WASM_GET_LOCAL(0),
                        WASM_GET_LOCAL(1), WASM_GET_LOCAL(2)),
        kExprI32Const, 0);

  r.CheckCallViaJS(0, 0, 0, 1);           // nop
  r.CheckCallViaJS(0, 0, 0, kTableSize);  // nop
  r.CheckCallViaJS(0xDEADBEEF, 0, 0, kTableSize + 1);
  r.CheckCallViaJS(0xDEADBEEF, 1, 0, kTableSize);
  r.CheckCallViaJS(0xDEADBEEF, 0, 1, kTableSize);

  {
    const uint32_t big = 1000000;
    r.CheckCallViaJS(0, big, 0, 0);
    r.CheckCallViaJS(0, 0, big, 0);
  }

  for (uint32_t big = 4294967295; big > 1000; big >>= 1) {
    r.CheckCallViaJS(0xDEADBEEF, big, 0, 1);
    r.CheckCallViaJS(0xDEADBEEF, 0, big, 1);
    r.CheckCallViaJS(0xDEADBEEF, 0, 0, big);
  }

  for (uint32_t big = -1000; big != 0; big <<= 1) {
    r.CheckCallViaJS(0xDEADBEEF, big, 0, 1);
    r.CheckCallViaJS(0xDEADBEEF, 0, big, 1);
    r.CheckCallViaJS(0xDEADBEEF, 0, 0, big);
  }
}

WASM_EXEC_TEST(TableCopyOob1From0To0) {
  TestTableCopyOob1(execution_tier, 0, 0);
}

WASM_EXEC_TEST(TableCopyOob1From3To0) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyOob1(execution_tier, 3, 0);
}

WASM_EXEC_TEST(TableCopyOob1From5To9) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyOob1(execution_tier, 5, 9);
}

WASM_EXEC_TEST(TableCopyOob1From6To6) {
  EXPERIMENTAL_FLAG_SCOPE(anyref);
  TestTableCopyOob1(execution_tier, 6, 6);
}

WASM_EXEC_TEST(ElemDropTwice) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddIndirectFunctionTable(nullptr, 1);
  r.builder().AddPassiveElementSegment({});
  BUILD(r, WASM_ELEM_DROP(0), kExprI32Const, 0);

  r.CheckCallViaJS(0);
  r.CheckCallViaJS(0xDEADBEEF);
}

WASM_EXEC_TEST(ElemDropThenTableInit) {
  EXPERIMENTAL_FLAG_SCOPE(bulk_memory);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddIndirectFunctionTable(nullptr, 1);
  r.builder().AddPassiveElementSegment({});
  BUILD(r, WASM_ELEM_DROP(0),
        WASM_TABLE_INIT(0, 0, WASM_I32V_1(0), WASM_I32V_1(0), WASM_I32V_1(0)),
        kExprI32Const, 0);

  r.CheckCallViaJS(0xDEADBEEF);
}

}  // namespace test_run_wasm_bulk_memory
}  // namespace wasm
}  // namespace internal
}  // namespace v8
