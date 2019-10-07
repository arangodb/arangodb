// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/api-inl.h"
#include "src/assembler-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

#define ADD_CODE(vec, ...)                                              \
  do {                                                                  \
    byte __buf[] = {__VA_ARGS__};                                       \
    for (size_t i = 0; i < sizeof(__buf); i++) vec.push_back(__buf[i]); \
  } while (false)

namespace {
// A helper for generating predictable but unique argument values that
// are easy to debug (e.g. with misaligned stacks).
class PredictableInputValues {
 public:
  int base_;
  explicit PredictableInputValues(int base) : base_(base) {}
  double arg_d(int which) { return base_ * which + ((which & 1) * 0.5); }
  float arg_f(int which) { return base_ * which + ((which & 1) * 0.25); }
  int32_t arg_i(int which) { return base_ * which + ((which & 1) * kMinInt); }
  int64_t arg_l(int which) {
    return base_ * which + ((which & 1) * (0x04030201LL << 32));
  }
};

ManuallyImportedJSFunction CreateJSSelector(FunctionSig* sig, int which) {
  const int kMaxParams = 11;
  static const char* formals[kMaxParams] = {"",
                                            "a",
                                            "a,b",
                                            "a,b,c",
                                            "a,b,c,d",
                                            "a,b,c,d,e",
                                            "a,b,c,d,e,f",
                                            "a,b,c,d,e,f,g",
                                            "a,b,c,d,e,f,g,h",
                                            "a,b,c,d,e,f,g,h,i",
                                            "a,b,c,d,e,f,g,h,i,j"};
  CHECK_LT(which, static_cast<int>(sig->parameter_count()));
  CHECK_LT(static_cast<int>(sig->parameter_count()), kMaxParams);

  i::EmbeddedVector<char, 256> source;
  char param = 'a' + which;
  SNPrintF(source, "(function(%s) { return %c; })",
           formals[sig->parameter_count()], param);

  Handle<JSFunction> js_function =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(
          *v8::Local<v8::Function>::Cast(CompileRun(source.start()))));
  ManuallyImportedJSFunction import = {sig, js_function};

  return import;
}

void EXPECT_CALL(double expected, Handle<JSFunction> jsfunc,
                 Handle<Object>* buffer, int count) {
  Isolate* isolate = jsfunc->GetIsolate();
  Handle<Object> global(isolate->context()->global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, jsfunc, global, count, buffer);

  CHECK(!retval.is_null());
  Handle<Object> result = retval.ToHandleChecked();
  if (result->IsSmi()) {
    CHECK_EQ(expected, Smi::ToInt(*result));
  } else {
    CHECK(result->IsHeapNumber());
    CHECK_FLOAT_EQ(expected, HeapNumber::cast(*result)->value());
  }
}

void EXPECT_CALL(double expected, Handle<JSFunction> jsfunc, double a,
                 double b) {
  Isolate* isolate = jsfunc->GetIsolate();
  Handle<Object> buffer[] = {isolate->factory()->NewNumber(a),
                             isolate->factory()->NewNumber(b)};
  EXPECT_CALL(expected, jsfunc, buffer, 2);
}
}  // namespace

WASM_EXEC_TEST(Run_Int32Sub_jswrapped) {
  WasmRunner<int, int, int> r(execution_tier);
  BUILD(r, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = r.builder().WrapCode(r.function()->func_index);

  EXPECT_CALL(33, jsfunc, 44, 11);
  EXPECT_CALL(-8723487, jsfunc, -8000000, 723487);
}

WASM_EXEC_TEST(Run_Float32Div_jswrapped) {
  WasmRunner<float, float, float> r(execution_tier);
  BUILD(r, WASM_F32_DIV(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = r.builder().WrapCode(r.function()->func_index);

  EXPECT_CALL(92, jsfunc, 46, 0.5);
  EXPECT_CALL(64, jsfunc, -16, -0.25);
}

WASM_EXEC_TEST(Run_Float64Add_jswrapped) {
  WasmRunner<double, double, double> r(execution_tier);
  BUILD(r, WASM_F64_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = r.builder().WrapCode(r.function()->func_index);

  EXPECT_CALL(3, jsfunc, 2, 1);
  EXPECT_CALL(-5.5, jsfunc, -5.25, -0.25);
}

WASM_EXEC_TEST(Run_I32Popcount_jswrapped) {
  WasmRunner<int, int> r(execution_tier);
  BUILD(r, WASM_I32_POPCNT(WASM_GET_LOCAL(0)));
  Handle<JSFunction> jsfunc = r.builder().WrapCode(r.function()->func_index);

  EXPECT_CALL(2, jsfunc, 9, 0);
  EXPECT_CALL(3, jsfunc, 11, 0);
  EXPECT_CALL(6, jsfunc, 0x3F, 0);
}

WASM_EXEC_TEST(Run_CallJS_Add_jswrapped) {
  TestSignatures sigs;
  HandleScope scope(CcTest::InitIsolateOnce());
  const char* source = "(function(a) { return a + 99; })";
  Handle<JSFunction> js_function =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(
          *v8::Local<v8::Function>::Cast(CompileRun(source))));
  ManuallyImportedJSFunction import = {sigs.i_i(), js_function};
  WasmRunner<int, int> r(execution_tier, &import);
  uint32_t js_index = 0;

  WasmFunctionCompiler& t = r.NewFunction(sigs.i_i());
  BUILD(t, WASM_CALL_FUNCTION(js_index, WASM_GET_LOCAL(0)));

  Handle<JSFunction> jsfunc = r.builder().WrapCode(t.function_index());

  EXPECT_CALL(101, jsfunc, 2, -8);
  EXPECT_CALL(199, jsfunc, 100, -1);
  EXPECT_CALL(-666666801, jsfunc, -666666900, -1);
}

void RunJSSelectTest(ExecutionTier tier, int which) {
  const int kMaxParams = 8;
  PredictableInputValues inputs(0x100);
  ValueType type = kWasmF64;
  ValueType types[kMaxParams + 1] = {type, type, type, type, type,
                                     type, type, type, type};
  for (int num_params = which + 1; num_params < kMaxParams; num_params++) {
    HandleScope scope(CcTest::InitIsolateOnce());
    FunctionSig sig(1, num_params, types);

    ManuallyImportedJSFunction import = CreateJSSelector(&sig, which);
    WasmRunner<void> r(tier, &import);
    uint32_t js_index = 0;

    WasmFunctionCompiler& t = r.NewFunction(&sig);

    {
      std::vector<byte> code;

      for (int i = 0; i < num_params; i++) {
        ADD_CODE(code, WASM_F64(inputs.arg_d(i)));
      }

      ADD_CODE(code, kExprCallFunction, static_cast<byte>(js_index));

      size_t end = code.size();
      code.push_back(0);
      t.Build(&code[0], &code[end]);
    }

    Handle<JSFunction> jsfunc = r.builder().WrapCode(t.function_index());
    double expected = inputs.arg_d(which);
    EXPECT_CALL(expected, jsfunc, 0.0, 0.0);
  }
}

WASM_EXEC_TEST(Run_JSSelect_0) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 0);
}

WASM_EXEC_TEST(Run_JSSelect_1) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 1);
}

WASM_EXEC_TEST(Run_JSSelect_2) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 2);
}

WASM_EXEC_TEST(Run_JSSelect_3) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 3);
}

WASM_EXEC_TEST(Run_JSSelect_4) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 4);
}

WASM_EXEC_TEST(Run_JSSelect_5) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 5);
}

WASM_EXEC_TEST(Run_JSSelect_6) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 6);
}

WASM_EXEC_TEST(Run_JSSelect_7) {
  CcTest::InitializeVM();
  RunJSSelectTest(execution_tier, 7);
}

void RunWASMSelectTest(ExecutionTier tier, int which) {
  PredictableInputValues inputs(0x200);
  Isolate* isolate = CcTest::InitIsolateOnce();
  const int kMaxParams = 8;
  for (int num_params = which + 1; num_params < kMaxParams; num_params++) {
    ValueType type = kWasmF64;
    ValueType types[kMaxParams + 1] = {type, type, type, type, type,
                                       type, type, type, type};
    FunctionSig sig(1, num_params, types);

    WasmRunner<void> r(tier);
    WasmFunctionCompiler& t = r.NewFunction(&sig);
    BUILD(t, WASM_GET_LOCAL(which));
    Handle<JSFunction> jsfunc = r.builder().WrapCode(t.function_index());

    Handle<Object> args[] = {
        isolate->factory()->NewNumber(inputs.arg_d(0)),
        isolate->factory()->NewNumber(inputs.arg_d(1)),
        isolate->factory()->NewNumber(inputs.arg_d(2)),
        isolate->factory()->NewNumber(inputs.arg_d(3)),
        isolate->factory()->NewNumber(inputs.arg_d(4)),
        isolate->factory()->NewNumber(inputs.arg_d(5)),
        isolate->factory()->NewNumber(inputs.arg_d(6)),
        isolate->factory()->NewNumber(inputs.arg_d(7)),
    };

    double expected = inputs.arg_d(which);
    EXPECT_CALL(expected, jsfunc, args, kMaxParams);
  }
}

WASM_EXEC_TEST(Run_WASMSelect_0) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 0);
}

WASM_EXEC_TEST(Run_WASMSelect_1) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 1);
}

WASM_EXEC_TEST(Run_WASMSelect_2) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 2);
}

WASM_EXEC_TEST(Run_WASMSelect_3) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 3);
}

WASM_EXEC_TEST(Run_WASMSelect_4) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 4);
}

WASM_EXEC_TEST(Run_WASMSelect_5) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 5);
}

WASM_EXEC_TEST(Run_WASMSelect_6) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 6);
}

WASM_EXEC_TEST(Run_WASMSelect_7) {
  CcTest::InitializeVM();
  RunWASMSelectTest(execution_tier, 7);
}

void RunWASMSelectAlignTest(ExecutionTier tier, int num_args, int num_params) {
  PredictableInputValues inputs(0x300);
  Isolate* isolate = CcTest::InitIsolateOnce();
  const int kMaxParams = 10;
  DCHECK_LE(num_args, kMaxParams);
  ValueType type = kWasmF64;
  ValueType types[kMaxParams + 1] = {type, type, type, type, type, type,
                                     type, type, type, type, type};
  FunctionSig sig(1, num_params, types);

  for (int which = 0; which < num_params; which++) {
    WasmRunner<void> r(tier);
    WasmFunctionCompiler& t = r.NewFunction(&sig);
    BUILD(t, WASM_GET_LOCAL(which));
    Handle<JSFunction> jsfunc = r.builder().WrapCode(t.function_index());

    Handle<Object> args[] = {isolate->factory()->NewNumber(inputs.arg_d(0)),
                             isolate->factory()->NewNumber(inputs.arg_d(1)),
                             isolate->factory()->NewNumber(inputs.arg_d(2)),
                             isolate->factory()->NewNumber(inputs.arg_d(3)),
                             isolate->factory()->NewNumber(inputs.arg_d(4)),
                             isolate->factory()->NewNumber(inputs.arg_d(5)),
                             isolate->factory()->NewNumber(inputs.arg_d(6)),
                             isolate->factory()->NewNumber(inputs.arg_d(7)),
                             isolate->factory()->NewNumber(inputs.arg_d(8)),
                             isolate->factory()->NewNumber(inputs.arg_d(9))};

    double nan = std::numeric_limits<double>::quiet_NaN();
    double expected = which < num_args ? inputs.arg_d(which) : nan;
    EXPECT_CALL(expected, jsfunc, args, num_args);
  }
}

WASM_EXEC_TEST(Run_WASMSelectAlign_0) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 0, 1);
  RunWASMSelectAlignTest(execution_tier, 0, 2);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_1) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 1, 2);
  RunWASMSelectAlignTest(execution_tier, 1, 3);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_2) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 2, 3);
  RunWASMSelectAlignTest(execution_tier, 2, 4);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_3) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 3, 3);
  RunWASMSelectAlignTest(execution_tier, 3, 4);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_4) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 4, 3);
  RunWASMSelectAlignTest(execution_tier, 4, 4);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_7) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 7, 5);
  RunWASMSelectAlignTest(execution_tier, 7, 6);
  RunWASMSelectAlignTest(execution_tier, 7, 7);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_8) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 8, 5);
  RunWASMSelectAlignTest(execution_tier, 8, 6);
  RunWASMSelectAlignTest(execution_tier, 8, 7);
  RunWASMSelectAlignTest(execution_tier, 8, 8);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_9) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 9, 6);
  RunWASMSelectAlignTest(execution_tier, 9, 7);
  RunWASMSelectAlignTest(execution_tier, 9, 8);
  RunWASMSelectAlignTest(execution_tier, 9, 9);
}

WASM_EXEC_TEST(Run_WASMSelectAlign_10) {
  CcTest::InitializeVM();
  RunWASMSelectAlignTest(execution_tier, 10, 7);
  RunWASMSelectAlignTest(execution_tier, 10, 8);
  RunWASMSelectAlignTest(execution_tier, 10, 9);
  RunWASMSelectAlignTest(execution_tier, 10, 10);
}

void RunJSSelectAlignTest(ExecutionTier tier, int num_args, int num_params) {
  PredictableInputValues inputs(0x400);
  Isolate* isolate = CcTest::InitIsolateOnce();
  Factory* factory = isolate->factory();
  const int kMaxParams = 10;
  CHECK_LE(num_args, kMaxParams);
  CHECK_LE(num_params, kMaxParams);
  ValueType type = kWasmF64;
  ValueType types[kMaxParams + 1] = {type, type, type, type, type, type,
                                     type, type, type, type, type};
  FunctionSig sig(1, num_params, types);
  i::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  // Build the calling code.
  std::vector<byte> code;

  for (int i = 0; i < num_params; i++) {
    ADD_CODE(code, WASM_GET_LOCAL(i));
  }

  uint8_t imported_js_index = 0;
  ADD_CODE(code, kExprCallFunction, imported_js_index);

  size_t end = code.size();
  code.push_back(0);

  // Call different select JS functions.
  for (int which = 0; which < num_params; which++) {
    HandleScope scope(isolate);
    ManuallyImportedJSFunction import = CreateJSSelector(&sig, which);
    WasmRunner<void> r(tier, &import);
    WasmFunctionCompiler& t = r.NewFunction(&sig);
    t.Build(&code[0], &code[end]);

    Handle<JSFunction> jsfunc = r.builder().WrapCode(t.function_index());

    Handle<Object> args[] = {
        factory->NewNumber(inputs.arg_d(0)),
        factory->NewNumber(inputs.arg_d(1)),
        factory->NewNumber(inputs.arg_d(2)),
        factory->NewNumber(inputs.arg_d(3)),
        factory->NewNumber(inputs.arg_d(4)),
        factory->NewNumber(inputs.arg_d(5)),
        factory->NewNumber(inputs.arg_d(6)),
        factory->NewNumber(inputs.arg_d(7)),
        factory->NewNumber(inputs.arg_d(8)),
        factory->NewNumber(inputs.arg_d(9)),
    };

    double nan = std::numeric_limits<double>::quiet_NaN();
    double expected = which < num_args ? inputs.arg_d(which) : nan;
    EXPECT_CALL(expected, jsfunc, args, num_args);
  }
}

WASM_EXEC_TEST(Run_JSSelectAlign_0) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 0, 1);
  RunJSSelectAlignTest(execution_tier, 0, 2);
}

WASM_EXEC_TEST(Run_JSSelectAlign_1) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 1, 2);
  RunJSSelectAlignTest(execution_tier, 1, 3);
}

WASM_EXEC_TEST(Run_JSSelectAlign_2) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 2, 3);
  RunJSSelectAlignTest(execution_tier, 2, 4);
}

WASM_EXEC_TEST(Run_JSSelectAlign_3) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 3, 3);
  RunJSSelectAlignTest(execution_tier, 3, 4);
}

WASM_EXEC_TEST(Run_JSSelectAlign_4) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 4, 3);
  RunJSSelectAlignTest(execution_tier, 4, 4);
}

WASM_EXEC_TEST(Run_JSSelectAlign_7) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 7, 3);
  RunJSSelectAlignTest(execution_tier, 7, 4);
  RunJSSelectAlignTest(execution_tier, 7, 4);
  RunJSSelectAlignTest(execution_tier, 7, 4);
}

WASM_EXEC_TEST(Run_JSSelectAlign_8) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 8, 5);
  RunJSSelectAlignTest(execution_tier, 8, 6);
  RunJSSelectAlignTest(execution_tier, 8, 7);
  RunJSSelectAlignTest(execution_tier, 8, 8);
}

WASM_EXEC_TEST(Run_JSSelectAlign_9) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 9, 6);
  RunJSSelectAlignTest(execution_tier, 9, 7);
  RunJSSelectAlignTest(execution_tier, 9, 8);
  RunJSSelectAlignTest(execution_tier, 9, 9);
}

WASM_EXEC_TEST(Run_JSSelectAlign_10) {
  CcTest::InitializeVM();
  RunJSSelectAlignTest(execution_tier, 10, 7);
  RunJSSelectAlignTest(execution_tier, 10, 8);
  RunJSSelectAlignTest(execution_tier, 10, 9);
  RunJSSelectAlignTest(execution_tier, 10, 10);
}

#undef ADD_CODE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
