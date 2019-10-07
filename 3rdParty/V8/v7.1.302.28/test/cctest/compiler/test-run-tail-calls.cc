// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/base/utils/random-number-generator.h"
#include "src/code-stub-assembler.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace test_run_tail_calls {

#define __ assembler.

namespace {

// Function that takes a number of pointer-sized integer arguments, calculates a
// weighted sum of them and returns it.
Handle<Code> BuildCallee(Isolate* isolate, CallDescriptor* call_descriptor) {
  CodeAssemblerTester tester(isolate, call_descriptor, "callee");
  CodeStubAssembler assembler(tester.state());
  int param_count = static_cast<int>(call_descriptor->StackParameterCount());
  Node* sum = __ IntPtrConstant(0);
  for (int i = 0; i < param_count; ++i) {
    Node* product = __ IntPtrMul(__ Parameter(i), __ IntPtrConstant(i + 1));
    sum = __ IntPtrAdd(sum, product);
  }
  __ Return(sum);
  return tester.GenerateCodeCloseAndEscape();
}

// Function that tail-calls another function with a number of pointer-sized
// integer arguments.
Handle<Code> BuildCaller(Isolate* isolate, CallDescriptor* call_descriptor,
                         CallDescriptor* callee_descriptor) {
  CodeAssemblerTester tester(isolate, call_descriptor, "caller");
  CodeStubAssembler assembler(tester.state());
  std::vector<Node*> params;
  // The first parameter is always the callee.
  params.push_back(__ HeapConstant(BuildCallee(isolate, callee_descriptor)));
  int param_count = static_cast<int>(callee_descriptor->StackParameterCount());
  for (int i = 0; i < param_count; ++i) {
    params.push_back(__ IntPtrConstant(i));
  }
  DCHECK_EQ(param_count + 1, params.size());
  tester.raw_assembler_for_testing()->TailCallN(callee_descriptor,
                                                param_count + 1, params.data());
  return tester.GenerateCodeCloseAndEscape();
}

// Setup function, which calls "caller".
Handle<Code> BuildSetupFunction(Isolate* isolate,
                                CallDescriptor* caller_descriptor,
                                CallDescriptor* callee_descriptor) {
  CodeAssemblerTester tester(isolate, 0);
  CodeStubAssembler assembler(tester.state());
  std::vector<Node*> params;
  // The first parameter is always the callee.
  params.push_back(__ HeapConstant(
      BuildCaller(isolate, caller_descriptor, callee_descriptor)));
  // Set up arguments for "Caller".
  int param_count = static_cast<int>(caller_descriptor->StackParameterCount());
  for (int i = 0; i < param_count; ++i) {
    // Use values that are different from the ones we will pass to this
    // function's callee later.
    params.push_back(__ IntPtrConstant(i + 42));
  }
  DCHECK_EQ(param_count + 1, params.size());
  Node* raw_result = tester.raw_assembler_for_testing()->CallN(
      caller_descriptor, param_count + 1, params.data());
  __ Return(__ SmiTag(raw_result));
  return tester.GenerateCodeCloseAndEscape();
}

CallDescriptor* CreateDescriptorForStackArguments(Zone* zone,
                                                  int stack_param_count) {
  LocationSignature::Builder locations(zone, 1,
                                       static_cast<size_t>(stack_param_count));

  locations.AddReturn(LinkageLocation::ForRegister(kReturnRegister0.code(),
                                                   MachineType::IntPtr()));

  for (int i = 0; i < stack_param_count; ++i) {
    locations.AddParam(LinkageLocation::ForCallerFrameSlot(
        i - stack_param_count, MachineType::IntPtr()));
  }

  return new (zone)
      CallDescriptor(CallDescriptor::kCallCodeObject,  // kind
                     MachineType::AnyTagged(),         // target MachineType
                     LinkageLocation::ForAnyRegister(
                         MachineType::AnyTagged()),  // target location
                     locations.Build(),              // location_sig
                     stack_param_count,              // stack_parameter_count
                     Operator::kNoProperties,        // properties
                     kNoCalleeSaved,                 // callee-saved registers
                     kNoCalleeSaved,                 // callee-saved fp
                     CallDescriptor::kNoFlags);      // flags
}

// Test a tail call from a caller with n parameters to a callee with m
// parameters. All parameters are pointer-sized.
void TestHelper(int n, int m) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  CanonicalHandleScope canonical(isolate);
  Zone* zone = scope.main_zone();
  CallDescriptor* caller_descriptor =
      CreateDescriptorForStackArguments(zone, n);
  CallDescriptor* callee_descriptor =
      CreateDescriptorForStackArguments(zone, m);
  Handle<Code> setup =
      BuildSetupFunction(isolate, caller_descriptor, callee_descriptor);
  FunctionTester ft(setup, 0);
  Handle<Object> result = ft.Call().ToHandleChecked();
  int expected = 0;
  for (int i = 0; i < m; ++i) expected += (i + 1) * i;
  CHECK_EQ(expected, Handle<Smi>::cast(result)->value());
}

}  // namespace

#undef __

TEST(CallerOddCalleeEven) {
  TestHelper(1, 0);
  TestHelper(1, 2);
  TestHelper(3, 2);
  TestHelper(3, 4);
}

TEST(CallerOddCalleeOdd) {
  TestHelper(1, 1);
  TestHelper(1, 3);
  TestHelper(3, 1);
  TestHelper(3, 3);
}

TEST(CallerEvenCalleeEven) {
  TestHelper(0, 0);
  TestHelper(0, 2);
  TestHelper(2, 0);
  TestHelper(2, 2);
}

TEST(CallerEvenCalleeOdd) {
  TestHelper(0, 1);
  TestHelper(0, 3);
  TestHelper(2, 1);
  TestHelper(2, 3);
}

TEST(FuzzStackParamCount) {
  const int kNumTests = 20;
  const int kMaxSlots = 30;
  base::RandomNumberGenerator* const rng = CcTest::random_number_generator();
  for (int i = 0; i < kNumTests; ++i) {
    int n = rng->NextInt(kMaxSlots);
    int m = rng->NextInt(kMaxSlots);
    TestHelper(n, m);
  }
}

}  // namespace test_run_tail_calls
}  // namespace compiler
}  // namespace internal
}  // namespace v8
