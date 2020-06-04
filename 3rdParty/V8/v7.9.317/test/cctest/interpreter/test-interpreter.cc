// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "src/init/v8.h"

#include "src/api/api-inl.h"
#include "src/base/overflowing-math.h"
#include "src/codegen/compiler.h"
#include "src/execution/execution.h"
#include "src/handles/handles.h"
#include "src/heap/heap-inl.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/interpreter.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "test/cctest/cctest.h"
#include "test/cctest/interpreter/interpreter-tester.h"
#include "test/cctest/test-feedback-vector.h"

namespace v8 {
namespace internal {
namespace interpreter {

static int GetIndex(FeedbackSlot slot) {
  return FeedbackVector::GetIndex(slot);
}

using ToBooleanMode = BytecodeArrayBuilder::ToBooleanMode;

TEST(InterpreterReturn) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Handle<Object> undefined_value = isolate->factory()->undefined_value();

  BytecodeArrayBuilder builder(zone, 1, 0);
  builder.Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}

TEST(InterpreterLoadUndefined) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Handle<Object> undefined_value = isolate->factory()->undefined_value();

  BytecodeArrayBuilder builder(zone, 1, 0);
  builder.LoadUndefined().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(undefined_value));
}

TEST(InterpreterLoadNull) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Handle<Object> null_value = isolate->factory()->null_value();

  BytecodeArrayBuilder builder(zone, 1, 0);
  builder.LoadNull().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(null_value));
}

TEST(InterpreterLoadTheHole) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Handle<Object> the_hole_value = isolate->factory()->the_hole_value();

  BytecodeArrayBuilder builder(zone, 1, 0);
  builder.LoadTheHole().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(the_hole_value));
}

TEST(InterpreterLoadTrue) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Handle<Object> true_value = isolate->factory()->true_value();

  BytecodeArrayBuilder builder(zone, 1, 0);
  builder.LoadTrue().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(true_value));
}

TEST(InterpreterLoadFalse) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Handle<Object> false_value = isolate->factory()->false_value();

  BytecodeArrayBuilder builder(zone, 1, 0);
  builder.LoadFalse().Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val.is_identical_to(false_value));
}

TEST(InterpreterLoadLiteral) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();

  // Small Smis.
  for (int i = -128; i < 128; i++) {
    BytecodeArrayBuilder builder(zone, 1, 0);
    builder.LoadLiteral(Smi::FromInt(i)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(i));
  }

  // Large Smis.
  {
    BytecodeArrayBuilder builder(zone, 1, 0);

    builder.LoadLiteral(Smi::FromInt(0x12345678)).Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(0x12345678));
  }

  // Heap numbers.
  {
    AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                                HashSeed(isolate));

    BytecodeArrayBuilder builder(zone, 1, 0);

    builder.LoadLiteral(-2.1e19).Return();

    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK_EQ(i::HeapNumber::cast(*return_val).value(), -2.1e19);
  }

  // Strings.
  {
    AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                                HashSeed(isolate));

    BytecodeArrayBuilder builder(zone, 1, 0);

    const AstRawString* raw_string = ast_factory.GetOneByteString("String");
    builder.LoadLiteral(raw_string).Return();

    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(i::String::cast(*return_val).Equals(*raw_string->string()));
  }
}

TEST(InterpreterLoadStoreRegisters) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Handle<Object> true_value = isolate->factory()->true_value();
  for (int i = 0; i <= kMaxInt8; i++) {
    BytecodeArrayBuilder builder(zone, 1, i + 1);

    Register reg(i);
    builder.LoadTrue()
        .StoreAccumulatorInRegister(reg)
        .LoadFalse()
        .LoadAccumulatorWithRegister(reg)
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(return_val.is_identical_to(true_value));
  }
}


static const Token::Value kShiftOperators[] = {
    Token::Value::SHL, Token::Value::SAR, Token::Value::SHR};


static const Token::Value kArithmeticOperators[] = {
    Token::Value::BIT_OR, Token::Value::BIT_XOR, Token::Value::BIT_AND,
    Token::Value::SHL,    Token::Value::SAR,     Token::Value::SHR,
    Token::Value::ADD,    Token::Value::SUB,     Token::Value::MUL,
    Token::Value::DIV,    Token::Value::MOD};


static double BinaryOpC(Token::Value op, double lhs, double rhs) {
  switch (op) {
    case Token::Value::ADD:
      return lhs + rhs;
    case Token::Value::SUB:
      return lhs - rhs;
    case Token::Value::MUL:
      return lhs * rhs;
    case Token::Value::DIV:
      return base::Divide(lhs, rhs);
    case Token::Value::MOD:
      return Modulo(lhs, rhs);
    case Token::Value::BIT_OR:
      return (v8::internal::DoubleToInt32(lhs) |
              v8::internal::DoubleToInt32(rhs));
    case Token::Value::BIT_XOR:
      return (v8::internal::DoubleToInt32(lhs) ^
              v8::internal::DoubleToInt32(rhs));
    case Token::Value::BIT_AND:
      return (v8::internal::DoubleToInt32(lhs) &
              v8::internal::DoubleToInt32(rhs));
    case Token::Value::SHL: {
      return base::ShlWithWraparound(DoubleToInt32(lhs), DoubleToInt32(rhs));
    }
    case Token::Value::SAR: {
      int32_t val = v8::internal::DoubleToInt32(lhs);
      uint32_t count = v8::internal::DoubleToUint32(rhs) & 0x1F;
      int32_t result = val >> count;
      return result;
    }
    case Token::Value::SHR: {
      uint32_t val = v8::internal::DoubleToUint32(lhs);
      uint32_t count = v8::internal::DoubleToUint32(rhs) & 0x1F;
      uint32_t result = val >> count;
      return result;
    }
    default:
      UNREACHABLE();
  }
}

TEST(InterpreterShiftOpsSmi) {
  int lhs_inputs[] = {0, -17, -182, 1073741823, -1};
  int rhs_inputs[] = {5, 2, 1, -1, -2, 0, 31, 32, -32, 64, 37};
  for (size_t l = 0; l < arraysize(lhs_inputs); l++) {
    for (size_t r = 0; r < arraysize(rhs_inputs); r++) {
      for (size_t o = 0; o < arraysize(kShiftOperators); o++) {
        HandleAndZoneScope handles;
        Isolate* isolate = handles.main_isolate();
        Zone* zone = handles.main_zone();
        Factory* factory = isolate->factory();
        FeedbackVectorSpec feedback_spec(zone);
        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

        FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        Register reg(0);
        int lhs = lhs_inputs[l];
        int rhs = rhs_inputs[r];
        builder.LoadLiteral(Smi::FromInt(lhs))
            .StoreAccumulatorInRegister(reg)
            .LoadLiteral(Smi::FromInt(rhs))
            .BinaryOperation(kShiftOperators[o], reg, GetIndex(slot))
            .Return();
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        Handle<Object> expected_value =
            factory->NewNumber(BinaryOpC(kShiftOperators[o], lhs, rhs));
        CHECK(return_value->SameValue(*expected_value));
      }
    }
  }
}

TEST(InterpreterBinaryOpsSmi) {
  int lhs_inputs[] = {3266, 1024, 0, -17, -18000};
  int rhs_inputs[] = {3266, 5, 4, 3, 2, 1, -1, -2};
  for (size_t l = 0; l < arraysize(lhs_inputs); l++) {
    for (size_t r = 0; r < arraysize(rhs_inputs); r++) {
      for (size_t o = 0; o < arraysize(kArithmeticOperators); o++) {
        HandleAndZoneScope handles;
        Isolate* isolate = handles.main_isolate();
        Zone* zone = handles.main_zone();
        Factory* factory = isolate->factory();
        FeedbackVectorSpec feedback_spec(zone);
        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

        FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        Register reg(0);
        int lhs = lhs_inputs[l];
        int rhs = rhs_inputs[r];
        builder.LoadLiteral(Smi::FromInt(lhs))
            .StoreAccumulatorInRegister(reg)
            .LoadLiteral(Smi::FromInt(rhs))
            .BinaryOperation(kArithmeticOperators[o], reg, GetIndex(slot))
            .Return();
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        Handle<Object> expected_value =
            factory->NewNumber(BinaryOpC(kArithmeticOperators[o], lhs, rhs));
        CHECK(return_value->SameValue(*expected_value));
      }
    }
  }
}

TEST(InterpreterBinaryOpsHeapNumber) {
  double lhs_inputs[] = {3266.101, 1024.12, 0.01, -17.99, -18000.833, 9.1e17};
  double rhs_inputs[] = {3266.101, 5.999, 4.778, 3.331,  2.643,
                         1.1,      -1.8,  -2.9,  8.3e-27};
  for (size_t l = 0; l < arraysize(lhs_inputs); l++) {
    for (size_t r = 0; r < arraysize(rhs_inputs); r++) {
      for (size_t o = 0; o < arraysize(kArithmeticOperators); o++) {
        HandleAndZoneScope handles;
        Isolate* isolate = handles.main_isolate();
        Zone* zone = handles.main_zone();
        Factory* factory = isolate->factory();
        FeedbackVectorSpec feedback_spec(zone);
        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

        FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        Register reg(0);
        double lhs = lhs_inputs[l];
        double rhs = rhs_inputs[r];
        builder.LoadLiteral(lhs)
            .StoreAccumulatorInRegister(reg)
            .LoadLiteral(rhs)
            .BinaryOperation(kArithmeticOperators[o], reg, GetIndex(slot))
            .Return();
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        Handle<Object> expected_value =
            factory->NewNumber(BinaryOpC(kArithmeticOperators[o], lhs, rhs));
        CHECK(return_value->SameValue(*expected_value));
      }
    }
  }
}

TEST(InterpreterBinaryOpsBigInt) {
  // This test only checks that the recorded type feedback is kBigInt.
  AstBigInt inputs[] = {AstBigInt("1"), AstBigInt("-42"), AstBigInt("0xFFFF")};
  for (size_t l = 0; l < arraysize(inputs); l++) {
    for (size_t r = 0; r < arraysize(inputs); r++) {
      for (size_t o = 0; o < arraysize(kArithmeticOperators); o++) {
        // Skip over unsigned right shift.
        if (kArithmeticOperators[o] == Token::Value::SHR) continue;

        HandleAndZoneScope handles;
        Isolate* isolate = handles.main_isolate();
        Zone* zone = handles.main_zone();
        FeedbackVectorSpec feedback_spec(zone);
        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

        FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        Register reg(0);
        auto lhs = inputs[l];
        auto rhs = inputs[r];
        builder.LoadLiteral(lhs)
            .StoreAccumulatorInRegister(reg)
            .LoadLiteral(rhs)
            .BinaryOperation(kArithmeticOperators[o], reg, GetIndex(slot))
            .Return();
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBigInt());
        if (tester.HasFeedbackMetadata()) {
          MaybeObject feedback = callable.vector().Get(slot);
          CHECK(feedback->IsSmi());
          CHECK_EQ(BinaryOperationFeedback::kBigInt, feedback->ToSmi().value());
        }
      }
    }
  }
}

namespace {

struct LiteralForTest {
  enum Type { kString, kHeapNumber, kSmi, kTrue, kFalse, kUndefined, kNull };

  explicit LiteralForTest(const AstRawString* string)
      : type(kString), string(string) {}
  explicit LiteralForTest(double number) : type(kHeapNumber), number(number) {}
  explicit LiteralForTest(int smi) : type(kSmi), smi(smi) {}
  explicit LiteralForTest(Type type) : type(type) {}

  Type type;
  union {
    const AstRawString* string;
    double number;
    int smi;
  };
};

void LoadLiteralForTest(BytecodeArrayBuilder* builder,
                        const LiteralForTest& value) {
  switch (value.type) {
    case LiteralForTest::kString:
      builder->LoadLiteral(value.string);
      return;
    case LiteralForTest::kHeapNumber:
      builder->LoadLiteral(value.number);
      return;
    case LiteralForTest::kSmi:
      builder->LoadLiteral(Smi::FromInt(value.smi));
      return;
    case LiteralForTest::kTrue:
      builder->LoadTrue();
      return;
    case LiteralForTest::kFalse:
      builder->LoadFalse();
      return;
    case LiteralForTest::kUndefined:
      builder->LoadUndefined();
      return;
    case LiteralForTest::kNull:
      builder->LoadNull();
      return;
  }
  UNREACHABLE();
}

}  // anonymous namespace

TEST(InterpreterStringAdd) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Factory* factory = isolate->factory();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  struct TestCase {
    const AstRawString* lhs;
    LiteralForTest rhs;
    Handle<Object> expected_value;
    int32_t expected_feedback;
  } test_cases[] = {
      {ast_factory.GetOneByteString("a"),
       LiteralForTest(ast_factory.GetOneByteString("b")),
       factory->NewStringFromStaticChars("ab"),
       BinaryOperationFeedback::kString},
      {ast_factory.GetOneByteString("aaaaaa"),
       LiteralForTest(ast_factory.GetOneByteString("b")),
       factory->NewStringFromStaticChars("aaaaaab"),
       BinaryOperationFeedback::kString},
      {ast_factory.GetOneByteString("aaa"),
       LiteralForTest(ast_factory.GetOneByteString("bbbbb")),
       factory->NewStringFromStaticChars("aaabbbbb"),
       BinaryOperationFeedback::kString},
      {ast_factory.GetOneByteString(""),
       LiteralForTest(ast_factory.GetOneByteString("b")),
       factory->NewStringFromStaticChars("b"),
       BinaryOperationFeedback::kString},
      {ast_factory.GetOneByteString("a"),
       LiteralForTest(ast_factory.GetOneByteString("")),
       factory->NewStringFromStaticChars("a"),
       BinaryOperationFeedback::kString},
      {ast_factory.GetOneByteString("1.11"), LiteralForTest(2.5),
       factory->NewStringFromStaticChars("1.112.5"),
       BinaryOperationFeedback::kAny},
      {ast_factory.GetOneByteString("-1.11"), LiteralForTest(2.56),
       factory->NewStringFromStaticChars("-1.112.56"),
       BinaryOperationFeedback::kAny},
      {ast_factory.GetOneByteString(""), LiteralForTest(2.5),
       factory->NewStringFromStaticChars("2.5"), BinaryOperationFeedback::kAny},
  };

  for (size_t i = 0; i < arraysize(test_cases); i++) {
    FeedbackVectorSpec feedback_spec(zone);
    BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);
    FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
    Handle<i::FeedbackMetadata> metadata =
        NewFeedbackMetadata(isolate, &feedback_spec);

    Register reg(0);
    builder.LoadLiteral(test_cases[i].lhs).StoreAccumulatorInRegister(reg);
    LoadLiteralForTest(&builder, test_cases[i].rhs);
    builder.BinaryOperation(Token::Value::ADD, reg, GetIndex(slot)).Return();
    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_cases[i].expected_value));

    if (tester.HasFeedbackMetadata()) {
      MaybeObject feedback = callable.vector().Get(slot);
      CHECK(feedback->IsSmi());
      CHECK_EQ(test_cases[i].expected_feedback, feedback->ToSmi().value());
    }
  }
}

TEST(InterpreterParameter1) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  BytecodeArrayBuilder builder(zone, 1, 0);

  builder.LoadAccumulatorWithRegister(builder.Receiver()).Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<Handle<Object>>();

  // Check for heap objects.
  Handle<Object> true_value = isolate->factory()->true_value();
  Handle<Object> return_val = callable(true_value).ToHandleChecked();
  CHECK(return_val.is_identical_to(true_value));

  // Check for Smis.
  return_val = callable(Handle<Smi>(Smi::FromInt(3), handles.main_isolate()))
                   .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(3));
}

TEST(InterpreterParameter8) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));
  FeedbackVectorSpec feedback_spec(zone);
  BytecodeArrayBuilder builder(zone, 8, 0, &feedback_spec);

  FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot1 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot2 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot3 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot4 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot5 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot6 = feedback_spec.AddBinaryOpICSlot();

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  builder.LoadAccumulatorWithRegister(builder.Receiver())
      .BinaryOperation(Token::Value::ADD, builder.Parameter(0), GetIndex(slot))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(1), GetIndex(slot1))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(2), GetIndex(slot2))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(3), GetIndex(slot3))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(4), GetIndex(slot4))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(5), GetIndex(slot5))
      .BinaryOperation(Token::Value::ADD, builder.Parameter(6), GetIndex(slot6))
      .Return();
  ast_factory.Internalize(isolate);
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array, metadata);
  using H = Handle<Object>;
  auto callable = tester.GetCallable<H, H, H, H, H, H, H, H>();

  Handle<Smi> arg1 = Handle<Smi>(Smi::FromInt(1), handles.main_isolate());
  Handle<Smi> arg2 = Handle<Smi>(Smi::FromInt(2), handles.main_isolate());
  Handle<Smi> arg3 = Handle<Smi>(Smi::FromInt(3), handles.main_isolate());
  Handle<Smi> arg4 = Handle<Smi>(Smi::FromInt(4), handles.main_isolate());
  Handle<Smi> arg5 = Handle<Smi>(Smi::FromInt(5), handles.main_isolate());
  Handle<Smi> arg6 = Handle<Smi>(Smi::FromInt(6), handles.main_isolate());
  Handle<Smi> arg7 = Handle<Smi>(Smi::FromInt(7), handles.main_isolate());
  Handle<Smi> arg8 = Handle<Smi>(Smi::FromInt(8), handles.main_isolate());
  // Check for Smis.
  Handle<Object> return_val =
      callable(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
          .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(36));
}

TEST(InterpreterBinaryOpTypeFeedback) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  struct BinaryOpExpectation {
    Token::Value op;
    LiteralForTest arg1;
    LiteralForTest arg2;
    Handle<Object> result;
    int32_t feedback;
  };

  BinaryOpExpectation const kTestCases[] = {
      // ADD
      {Token::Value::ADD, LiteralForTest(2), LiteralForTest(3),
       Handle<Smi>(Smi::FromInt(5), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::ADD, LiteralForTest(Smi::kMaxValue), LiteralForTest(1),
       isolate->factory()->NewHeapNumber(Smi::kMaxValue + 1.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::ADD, LiteralForTest(3.1415), LiteralForTest(3),
       isolate->factory()->NewHeapNumber(3.1415 + 3),
       BinaryOperationFeedback::kNumber},
      {Token::Value::ADD, LiteralForTest(3.1415), LiteralForTest(1.4142),
       isolate->factory()->NewHeapNumber(3.1415 + 1.4142),
       BinaryOperationFeedback::kNumber},
      {Token::Value::ADD, LiteralForTest(ast_factory.GetOneByteString("foo")),
       LiteralForTest(ast_factory.GetOneByteString("bar")),
       isolate->factory()->NewStringFromAsciiChecked("foobar"),
       BinaryOperationFeedback::kString},
      {Token::Value::ADD, LiteralForTest(2),
       LiteralForTest(ast_factory.GetOneByteString("2")),
       isolate->factory()->NewStringFromAsciiChecked("22"),
       BinaryOperationFeedback::kAny},
      // SUB
      {Token::Value::SUB, LiteralForTest(2), LiteralForTest(3),
       Handle<Smi>(Smi::FromInt(-1), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::SUB, LiteralForTest(Smi::kMinValue), LiteralForTest(1),
       isolate->factory()->NewHeapNumber(Smi::kMinValue - 1.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::SUB, LiteralForTest(3.1415), LiteralForTest(3),
       isolate->factory()->NewHeapNumber(3.1415 - 3),
       BinaryOperationFeedback::kNumber},
      {Token::Value::SUB, LiteralForTest(3.1415), LiteralForTest(1.4142),
       isolate->factory()->NewHeapNumber(3.1415 - 1.4142),
       BinaryOperationFeedback::kNumber},
      {Token::Value::SUB, LiteralForTest(2),
       LiteralForTest(ast_factory.GetOneByteString("1")),
       Handle<Smi>(Smi::FromInt(1), isolate), BinaryOperationFeedback::kAny},
      // MUL
      {Token::Value::MUL, LiteralForTest(2), LiteralForTest(3),
       Handle<Smi>(Smi::FromInt(6), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::MUL, LiteralForTest(Smi::kMinValue), LiteralForTest(2),
       isolate->factory()->NewHeapNumber(Smi::kMinValue * 2.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::MUL, LiteralForTest(3.1415), LiteralForTest(3),
       isolate->factory()->NewHeapNumber(3 * 3.1415),
       BinaryOperationFeedback::kNumber},
      {Token::Value::MUL, LiteralForTest(3.1415), LiteralForTest(1.4142),
       isolate->factory()->NewHeapNumber(3.1415 * 1.4142),
       BinaryOperationFeedback::kNumber},
      {Token::Value::MUL, LiteralForTest(2),
       LiteralForTest(ast_factory.GetOneByteString("1")),
       Handle<Smi>(Smi::FromInt(2), isolate), BinaryOperationFeedback::kAny},
      // DIV
      {Token::Value::DIV, LiteralForTest(6), LiteralForTest(3),
       Handle<Smi>(Smi::FromInt(2), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::DIV, LiteralForTest(3), LiteralForTest(2),
       isolate->factory()->NewHeapNumber(3.0 / 2.0),
       BinaryOperationFeedback::kSignedSmallInputs},
      {Token::Value::DIV, LiteralForTest(3.1415), LiteralForTest(3),
       isolate->factory()->NewHeapNumber(3.1415 / 3),
       BinaryOperationFeedback::kNumber},
      {Token::Value::DIV, LiteralForTest(3.1415),
       LiteralForTest(-std::numeric_limits<double>::infinity()),
       isolate->factory()->NewHeapNumber(-0.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::DIV, LiteralForTest(2),
       LiteralForTest(ast_factory.GetOneByteString("1")),
       Handle<Smi>(Smi::FromInt(2), isolate), BinaryOperationFeedback::kAny},
      // MOD
      {Token::Value::MOD, LiteralForTest(5), LiteralForTest(3),
       Handle<Smi>(Smi::FromInt(2), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::MOD, LiteralForTest(-4), LiteralForTest(2),
       isolate->factory()->NewHeapNumber(-0.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::MOD, LiteralForTest(3.1415), LiteralForTest(3),
       isolate->factory()->NewHeapNumber(fmod(3.1415, 3.0)),
       BinaryOperationFeedback::kNumber},
      {Token::Value::MOD, LiteralForTest(-3.1415), LiteralForTest(-1.4142),
       isolate->factory()->NewHeapNumber(fmod(-3.1415, -1.4142)),
       BinaryOperationFeedback::kNumber},
      {Token::Value::MOD, LiteralForTest(3),
       LiteralForTest(ast_factory.GetOneByteString("-2")),
       Handle<Smi>(Smi::FromInt(1), isolate), BinaryOperationFeedback::kAny}};

  for (const BinaryOpExpectation& test_case : kTestCases) {
    i::FeedbackVectorSpec feedback_spec(zone);
    BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

    i::FeedbackSlot slot0 = feedback_spec.AddBinaryOpICSlot();

    Handle<i::FeedbackMetadata> metadata =
        i::NewFeedbackMetadata(isolate, &feedback_spec);

    Register reg(0);
    LoadLiteralForTest(&builder, test_case.arg1);
    builder.StoreAccumulatorInRegister(reg);
    LoadLiteralForTest(&builder, test_case.arg2);
    builder.BinaryOperation(test_case.op, reg, GetIndex(slot0)).Return();

    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<>();

    Handle<Object> return_val = callable().ToHandleChecked();
    MaybeObject feedback0 = callable.vector().Get(slot0);
    CHECK(feedback0->IsSmi());
    CHECK_EQ(test_case.feedback, feedback0->ToSmi().value());
    CHECK(Object::Equals(isolate, test_case.result, return_val).ToChecked());
  }
}

TEST(InterpreterBinaryOpSmiTypeFeedback) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  struct BinaryOpExpectation {
    Token::Value op;
    LiteralForTest arg1;
    int32_t arg2;
    Handle<Object> result;
    int32_t feedback;
  };

  BinaryOpExpectation const kTestCases[] = {
      // ADD
      {Token::Value::ADD, LiteralForTest(2), 42,
       Handle<Smi>(Smi::FromInt(44), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::ADD, LiteralForTest(2), Smi::kMaxValue,
       isolate->factory()->NewHeapNumber(Smi::kMaxValue + 2.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::ADD, LiteralForTest(3.1415), 2,
       isolate->factory()->NewHeapNumber(3.1415 + 2.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::ADD, LiteralForTest(ast_factory.GetOneByteString("2")), 2,
       isolate->factory()->NewStringFromAsciiChecked("22"),
       BinaryOperationFeedback::kAny},
      // SUB
      {Token::Value::SUB, LiteralForTest(2), 42,
       Handle<Smi>(Smi::FromInt(-40), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::SUB, LiteralForTest(Smi::kMinValue), 1,
       isolate->factory()->NewHeapNumber(Smi::kMinValue - 1.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::SUB, LiteralForTest(3.1415), 2,
       isolate->factory()->NewHeapNumber(3.1415 - 2.0),
       BinaryOperationFeedback::kNumber},
      {Token::Value::SUB, LiteralForTest(ast_factory.GetOneByteString("2")), 2,
       Handle<Smi>(Smi::zero(), isolate), BinaryOperationFeedback::kAny},
      // BIT_OR
      {Token::Value::BIT_OR, LiteralForTest(4), 1,
       Handle<Smi>(Smi::FromInt(5), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::BIT_OR, LiteralForTest(3.1415), 8,
       Handle<Smi>(Smi::FromInt(11), isolate),
       BinaryOperationFeedback::kNumber},
      {Token::Value::BIT_OR, LiteralForTest(ast_factory.GetOneByteString("2")),
       1, Handle<Smi>(Smi::FromInt(3), isolate), BinaryOperationFeedback::kAny},
      // BIT_AND
      {Token::Value::BIT_AND, LiteralForTest(3), 1,
       Handle<Smi>(Smi::FromInt(1), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::BIT_AND, LiteralForTest(3.1415), 2,
       Handle<Smi>(Smi::FromInt(2), isolate), BinaryOperationFeedback::kNumber},
      {Token::Value::BIT_AND, LiteralForTest(ast_factory.GetOneByteString("2")),
       1, Handle<Smi>(Smi::zero(), isolate), BinaryOperationFeedback::kAny},
      // SHL
      {Token::Value::SHL, LiteralForTest(3), 1,
       Handle<Smi>(Smi::FromInt(6), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::SHL, LiteralForTest(3.1415), 2,
       Handle<Smi>(Smi::FromInt(12), isolate),
       BinaryOperationFeedback::kNumber},
      {Token::Value::SHL, LiteralForTest(ast_factory.GetOneByteString("2")), 1,
       Handle<Smi>(Smi::FromInt(4), isolate), BinaryOperationFeedback::kAny},
      // SAR
      {Token::Value::SAR, LiteralForTest(3), 1,
       Handle<Smi>(Smi::FromInt(1), isolate),
       BinaryOperationFeedback::kSignedSmall},
      {Token::Value::SAR, LiteralForTest(3.1415), 2,
       Handle<Smi>(Smi::zero(), isolate), BinaryOperationFeedback::kNumber},
      {Token::Value::SAR, LiteralForTest(ast_factory.GetOneByteString("2")), 1,
       Handle<Smi>(Smi::FromInt(1), isolate), BinaryOperationFeedback::kAny}};

  for (const BinaryOpExpectation& test_case : kTestCases) {
    i::FeedbackVectorSpec feedback_spec(zone);
    BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

    i::FeedbackSlot slot0 = feedback_spec.AddBinaryOpICSlot();

    Handle<i::FeedbackMetadata> metadata =
        i::NewFeedbackMetadata(isolate, &feedback_spec);

    Register reg(0);
    LoadLiteralForTest(&builder, test_case.arg1);
    builder.StoreAccumulatorInRegister(reg)
        .LoadLiteral(Smi::FromInt(test_case.arg2))
        .BinaryOperation(test_case.op, reg, GetIndex(slot0))
        .Return();

    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<>();

    Handle<Object> return_val = callable().ToHandleChecked();
    MaybeObject feedback0 = callable.vector().Get(slot0);
    CHECK(feedback0->IsSmi());
    CHECK_EQ(test_case.feedback, feedback0->ToSmi().value());
    CHECK(Object::Equals(isolate, test_case.result, return_val).ToChecked());
  }
}

TEST(InterpreterUnaryOpFeedback) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();

  Handle<Smi> smi_one = Handle<Smi>(Smi::FromInt(1), isolate);
  Handle<Smi> smi_max = Handle<Smi>(Smi::FromInt(Smi::kMaxValue), isolate);
  Handle<Smi> smi_min = Handle<Smi>(Smi::FromInt(Smi::kMinValue), isolate);
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(2.1);
  Handle<BigInt> bigint =
      BigInt::FromNumber(isolate, smi_max).ToHandleChecked();
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked("42");

  struct TestCase {
    Token::Value op;
    Handle<Smi> smi_feedback_value;
    Handle<Smi> smi_to_number_feedback_value;
    Handle<HeapNumber> number_feedback_value;
    Handle<BigInt> bigint_feedback_value;
    Handle<Object> any_feedback_value;
  };
  TestCase const kTestCases[] = {
      // Testing ADD and BIT_NOT would require generalizing the test setup.
      {Token::Value::SUB, smi_one, smi_min, number, bigint, str},
      {Token::Value::INC, smi_one, smi_max, number, bigint, str},
      {Token::Value::DEC, smi_one, smi_min, number, bigint, str}};
  for (TestCase const& test_case : kTestCases) {
    i::FeedbackVectorSpec feedback_spec(zone);
    BytecodeArrayBuilder builder(zone, 5, 0, &feedback_spec);

    i::FeedbackSlot slot0 = feedback_spec.AddBinaryOpICSlot();
    i::FeedbackSlot slot1 = feedback_spec.AddBinaryOpICSlot();
    i::FeedbackSlot slot2 = feedback_spec.AddBinaryOpICSlot();
    i::FeedbackSlot slot3 = feedback_spec.AddBinaryOpICSlot();
    i::FeedbackSlot slot4 = feedback_spec.AddBinaryOpICSlot();

    Handle<i::FeedbackMetadata> metadata =
        i::NewFeedbackMetadata(isolate, &feedback_spec);

    builder.LoadAccumulatorWithRegister(builder.Receiver())
        .UnaryOperation(test_case.op, GetIndex(slot0))
        .LoadAccumulatorWithRegister(builder.Parameter(0))
        .UnaryOperation(test_case.op, GetIndex(slot1))
        .LoadAccumulatorWithRegister(builder.Parameter(1))
        .UnaryOperation(test_case.op, GetIndex(slot2))
        .LoadAccumulatorWithRegister(builder.Parameter(2))
        .UnaryOperation(test_case.op, GetIndex(slot3))
        .LoadAccumulatorWithRegister(builder.Parameter(3))
        .UnaryOperation(test_case.op, GetIndex(slot4))
        .Return();

    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    using H = Handle<Object>;
    auto callable = tester.GetCallable<H, H, H, H, H>();

    Handle<Object> return_val =
        callable(test_case.smi_feedback_value,
                 test_case.smi_to_number_feedback_value,
                 test_case.number_feedback_value,
                 test_case.bigint_feedback_value, test_case.any_feedback_value)
            .ToHandleChecked();
    USE(return_val);
    MaybeObject feedback0 = callable.vector().Get(slot0);
    CHECK(feedback0->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kSignedSmall, feedback0->ToSmi().value());

    MaybeObject feedback1 = callable.vector().Get(slot1);
    CHECK(feedback1->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kNumber, feedback1->ToSmi().value());

    MaybeObject feedback2 = callable.vector().Get(slot2);
    CHECK(feedback2->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kNumber, feedback2->ToSmi().value());

    MaybeObject feedback3 = callable.vector().Get(slot3);
    CHECK(feedback3->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kBigInt, feedback3->ToSmi().value());

    MaybeObject feedback4 = callable.vector().Get(slot4);
    CHECK(feedback4->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kAny, feedback4->ToSmi().value());
  }
}

TEST(InterpreterBitwiseTypeFeedback) {
  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  const Token::Value kBitwiseBinaryOperators[] = {
      Token::Value::BIT_OR, Token::Value::BIT_XOR, Token::Value::BIT_AND,
      Token::Value::SHL,    Token::Value::SHR,     Token::Value::SAR};

  for (Token::Value op : kBitwiseBinaryOperators) {
    i::FeedbackVectorSpec feedback_spec(zone);
    BytecodeArrayBuilder builder(zone, 4, 0, &feedback_spec);

    i::FeedbackSlot slot0 = feedback_spec.AddBinaryOpICSlot();
    i::FeedbackSlot slot1 = feedback_spec.AddBinaryOpICSlot();
    i::FeedbackSlot slot2 = feedback_spec.AddBinaryOpICSlot();

    Handle<i::FeedbackMetadata> metadata =
        i::NewFeedbackMetadata(isolate, &feedback_spec);

    builder.LoadAccumulatorWithRegister(builder.Receiver())
        .BinaryOperation(op, builder.Parameter(0), GetIndex(slot0))
        .BinaryOperation(op, builder.Parameter(1), GetIndex(slot1))
        .BinaryOperation(op, builder.Parameter(2), GetIndex(slot2))
        .Return();

    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    using H = Handle<Object>;
    auto callable = tester.GetCallable<H, H, H, H>();

    Handle<Smi> arg1 = Handle<Smi>(Smi::FromInt(2), isolate);
    Handle<Smi> arg2 = Handle<Smi>(Smi::FromInt(2), isolate);
    Handle<HeapNumber> arg3 = isolate->factory()->NewHeapNumber(2.2);
    Handle<String> arg4 = isolate->factory()->NewStringFromAsciiChecked("2");

    Handle<Object> return_val =
        callable(arg1, arg2, arg3, arg4).ToHandleChecked();
    USE(return_val);
    MaybeObject feedback0 = callable.vector().Get(slot0);
    CHECK(feedback0->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kSignedSmall, feedback0->ToSmi().value());

    MaybeObject feedback1 = callable.vector().Get(slot1);
    CHECK(feedback1->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kNumber, feedback1->ToSmi().value());

    MaybeObject feedback2 = callable.vector().Get(slot2);
    CHECK(feedback2->IsSmi());
    CHECK_EQ(BinaryOperationFeedback::kAny, feedback2->ToSmi().value());
  }
}

TEST(InterpreterParameter1Assign) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  BytecodeArrayBuilder builder(zone, 1, 0);

  builder.LoadLiteral(Smi::FromInt(5))
      .StoreAccumulatorInRegister(builder.Receiver())
      .LoadAccumulatorWithRegister(builder.Receiver())
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> return_val =
      callable(Handle<Smi>(Smi::FromInt(3), handles.main_isolate()))
          .ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(5));
}

TEST(InterpreterLoadGlobal) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  // Test loading a global.
  std::string source(
      "var global = 321;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return global;\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(321));
}

TEST(InterpreterStoreGlobal) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  // Test storing to a global.
  std::string source(
      "var global = 321;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  global = 999;\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  callable().ToHandleChecked();
  Handle<i::String> name = factory->InternalizeUtf8String("global");
  Handle<i::Object> global_obj =
      Object::GetProperty(isolate, isolate->global_object(), name)
          .ToHandleChecked();
  CHECK_EQ(Smi::cast(*global_obj), Smi::FromInt(999));
}

TEST(InterpreterCallGlobal) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  // Test calling a global function.
  std::string source(
      "function g_add(a, b) { return a + b; }\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return g_add(5, 10);\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(15));
}

TEST(InterpreterLoadUnallocated) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  // Test loading an unallocated global.
  std::string source(
      "unallocated = 123;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  return unallocated;\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));
}

TEST(InterpreterStoreUnallocated) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  // Test storing to an unallocated global.
  std::string source(
      "unallocated = 321;\n"
      "function " + InterpreterTester::function_name() + "() {\n"
      "  unallocated = 999;\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  callable().ToHandleChecked();
  Handle<i::String> name = factory->InternalizeUtf8String("unallocated");
  Handle<i::Object> global_obj =
      Object::GetProperty(isolate, isolate->global_object(), name)
          .ToHandleChecked();
  CHECK_EQ(Smi::cast(*global_obj), Smi::FromInt(999));
}

TEST(InterpreterLoadNamedProperty) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  FeedbackVectorSpec feedback_spec(zone);
  FeedbackSlot slot = feedback_spec.AddLoadICSlot();

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  const AstRawString* name = ast_factory.GetOneByteString("val");

  BytecodeArrayBuilder builder(zone, 1, 0, &feedback_spec);

  builder.LoadNamedProperty(builder.Receiver(), name, GetIndex(slot)).Return();
  ast_factory.Internalize(isolate);
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to monomorphic IC.
  return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to polymorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  return_val = callable(object2).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(456));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ val : 789, val2 : 123 })");
  callable(object3).ToHandleChecked();
  Handle<Object> object4 =
      InterpreterTester::NewObject("({ val : 789, val3 : 123 })");
  callable(object4).ToHandleChecked();
  Handle<Object> object5 =
      InterpreterTester::NewObject("({ val : 789, val4 : 123 })");
  return_val = callable(object5).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(789));
}

TEST(InterpreterLoadKeyedProperty) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  FeedbackVectorSpec feedback_spec(zone);
  FeedbackSlot slot = feedback_spec.AddKeyedLoadICSlot();

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  const AstRawString* key = ast_factory.GetOneByteString("key");

  BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

  builder.LoadLiteral(key)
      .LoadKeyedProperty(builder.Receiver(), GetIndex(slot))
      .Return();
  ast_factory.Internalize(isolate);
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<Object> object = InterpreterTester::NewObject("({ key : 123 })");
  // Test IC miss.
  Handle<Object> return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to monomorphic IC.
  return_val = callable(object).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(123));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ key : 789, val2 : 123 })");
  return_val = callable(object3).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(789));
}

TEST(InterpreterStoreNamedProperty) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  FeedbackVectorSpec feedback_spec(zone);
  FeedbackSlot slot = feedback_spec.AddStoreICSlot(LanguageMode::kStrict);

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  const AstRawString* name = ast_factory.GetOneByteString("val");

  BytecodeArrayBuilder builder(zone, 1, 0, &feedback_spec);

  builder.LoadLiteral(Smi::FromInt(999))
      .StoreNamedProperty(builder.Receiver(), name, GetIndex(slot),
                          LanguageMode::kStrict)
      .Return();
  ast_factory.Internalize(isolate);
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<Handle<Object>>();
  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> result;
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name->string())
            .ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to monomorphic IC.
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name->string())
            .ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to polymorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  callable(object2).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object2, name->string())
            .ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to megamorphic IC.
  Handle<Object> object3 =
      InterpreterTester::NewObject("({ val : 789, val2 : 123 })");
  callable(object3).ToHandleChecked();
  Handle<Object> object4 =
      InterpreterTester::NewObject("({ val : 789, val3 : 123 })");
  callable(object4).ToHandleChecked();
  Handle<Object> object5 =
      InterpreterTester::NewObject("({ val : 789, val4 : 123 })");
  callable(object5).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object5, name->string())
            .ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));
}

TEST(InterpreterStoreKeyedProperty) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  FeedbackVectorSpec feedback_spec(zone);
  FeedbackSlot slot = feedback_spec.AddKeyedStoreICSlot(LanguageMode::kSloppy);

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  const AstRawString* name = ast_factory.GetOneByteString("val");

  BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

  builder.LoadLiteral(name)
      .StoreAccumulatorInRegister(Register(0))
      .LoadLiteral(Smi::FromInt(999))
      .StoreKeyedProperty(builder.Receiver(), Register(0), GetIndex(slot),
                          i::LanguageMode::kSloppy)
      .Return();
  ast_factory.Internalize(isolate);
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<Handle<Object>>();
  Handle<Object> object = InterpreterTester::NewObject("({ val : 123 })");
  // Test IC miss.
  Handle<Object> result;
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name->string())
            .ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to monomorphic IC.
  callable(object).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object, name->string())
            .ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));

  // Test transition to megamorphic IC.
  Handle<Object> object2 =
      InterpreterTester::NewObject("({ val : 456, other : 123 })");
  callable(object2).ToHandleChecked();
  CHECK(Runtime::GetObjectProperty(isolate, object2, name->string())
            .ToHandle(&result));
  CHECK_EQ(Smi::cast(*result), Smi::FromInt(999));
}

TEST(InterpreterCall) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Factory* factory = isolate->factory();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  FeedbackVectorSpec feedback_spec(zone);
  FeedbackSlot slot = feedback_spec.AddLoadICSlot();
  FeedbackSlot call_slot = feedback_spec.AddCallICSlot();

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);
  int slot_index = GetIndex(slot);
  int call_slot_index = -1;
  call_slot_index = GetIndex(call_slot);

  const AstRawString* name = ast_factory.GetOneByteString("func");

  // Check with no args.
  {
    BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);
    Register reg = builder.register_allocator()->NewRegister();
    RegisterList args = builder.register_allocator()->NewRegisterList(1);
    builder.LoadNamedProperty(builder.Receiver(), name, slot_index)
        .StoreAccumulatorInRegister(reg)
        .MoveRegister(builder.Receiver(), args[0]);

    builder.CallProperty(reg, args, call_slot_index);

    builder.Return();
    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { this.func = function() { return 0x265; }})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(0x265));
  }

  // Check that receiver is passed properly.
  {
    BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);
    Register reg = builder.register_allocator()->NewRegister();
    RegisterList args = builder.register_allocator()->NewRegisterList(1);
    builder.LoadNamedProperty(builder.Receiver(), name, slot_index)
        .StoreAccumulatorInRegister(reg)
        .MoveRegister(builder.Receiver(), args[0]);
    builder.CallProperty(reg, args, call_slot_index);
    builder.Return();
    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() {"
        "  this.val = 1234;"
        "  this.func = function() { return this.val; };"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(1234));
  }

  // Check with two parameters (+ receiver).
  {
    BytecodeArrayBuilder builder(zone, 1, 4, &feedback_spec);
    Register reg = builder.register_allocator()->NewRegister();
    RegisterList args = builder.register_allocator()->NewRegisterList(3);

    builder.LoadNamedProperty(builder.Receiver(), name, slot_index)
        .StoreAccumulatorInRegister(reg)
        .LoadAccumulatorWithRegister(builder.Receiver())
        .StoreAccumulatorInRegister(args[0])
        .LoadLiteral(Smi::FromInt(51))
        .StoreAccumulatorInRegister(args[1])
        .LoadLiteral(Smi::FromInt(11))
        .StoreAccumulatorInRegister(args[2]);

    builder.CallProperty(reg, args, call_slot_index);

    builder.Return();

    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { "
        "  this.func = function(a, b) { return a - b; }"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    CHECK(return_val->SameValue(Smi::FromInt(40)));
  }

  // Check with 10 parameters (+ receiver).
  {
    BytecodeArrayBuilder builder(zone, 1, 12, &feedback_spec);
    Register reg = builder.register_allocator()->NewRegister();
    RegisterList args = builder.register_allocator()->NewRegisterList(11);

    builder.LoadNamedProperty(builder.Receiver(), name, slot_index)
        .StoreAccumulatorInRegister(reg)
        .LoadAccumulatorWithRegister(builder.Receiver())
        .StoreAccumulatorInRegister(args[0])
        .LoadLiteral(ast_factory.GetOneByteString("a"))
        .StoreAccumulatorInRegister(args[1])
        .LoadLiteral(ast_factory.GetOneByteString("b"))
        .StoreAccumulatorInRegister(args[2])
        .LoadLiteral(ast_factory.GetOneByteString("c"))
        .StoreAccumulatorInRegister(args[3])
        .LoadLiteral(ast_factory.GetOneByteString("d"))
        .StoreAccumulatorInRegister(args[4])
        .LoadLiteral(ast_factory.GetOneByteString("e"))
        .StoreAccumulatorInRegister(args[5])
        .LoadLiteral(ast_factory.GetOneByteString("f"))
        .StoreAccumulatorInRegister(args[6])
        .LoadLiteral(ast_factory.GetOneByteString("g"))
        .StoreAccumulatorInRegister(args[7])
        .LoadLiteral(ast_factory.GetOneByteString("h"))
        .StoreAccumulatorInRegister(args[8])
        .LoadLiteral(ast_factory.GetOneByteString("i"))
        .StoreAccumulatorInRegister(args[9])
        .LoadLiteral(ast_factory.GetOneByteString("j"))
        .StoreAccumulatorInRegister(args[10]);

    builder.CallProperty(reg, args, call_slot_index);

    builder.Return();

    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<Object> object = InterpreterTester::NewObject(
        "new (function Obj() { "
        "  this.prefix = \"prefix_\";"
        "  this.func = function(a, b, c, d, e, f, g, h, i, j) {"
        "      return this.prefix + a + b + c + d + e + f + g + h + i + j;"
        "  }"
        "})()");
    Handle<Object> return_val = callable(object).ToHandleChecked();
    Handle<i::String> expected =
        factory->NewStringFromAsciiChecked("prefix_abcdefghij");
    CHECK(i::String::cast(*return_val).Equals(*expected));
  }
}

static BytecodeArrayBuilder& SetRegister(BytecodeArrayBuilder* builder,
                                         Register reg, int value,
                                         Register scratch) {
  return builder->StoreAccumulatorInRegister(scratch)
      .LoadLiteral(Smi::FromInt(value))
      .StoreAccumulatorInRegister(reg)
      .LoadAccumulatorWithRegister(scratch);
}

static BytecodeArrayBuilder& IncrementRegister(BytecodeArrayBuilder* builder,
                                               Register reg, int value,
                                               Register scratch,
                                               int slot_index) {
  return builder->StoreAccumulatorInRegister(scratch)
      .LoadLiteral(Smi::FromInt(value))
      .BinaryOperation(Token::Value::ADD, reg, slot_index)
      .StoreAccumulatorInRegister(reg)
      .LoadAccumulatorWithRegister(scratch);
}

TEST(InterpreterJumps) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  FeedbackVectorSpec feedback_spec(zone);
  BytecodeArrayBuilder builder(zone, 1, 2, &feedback_spec);

  FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot1 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot2 = feedback_spec.AddBinaryOpICSlot();

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  Register reg(0), scratch(1);
  BytecodeLoopHeader loop_header;
  BytecodeLabel label[2];

  builder.LoadLiteral(Smi::zero())
      .StoreAccumulatorInRegister(reg)
      .Jump(&label[0]);
  SetRegister(&builder, reg, 1024, scratch).Bind(&loop_header);
  IncrementRegister(&builder, reg, 1, scratch, GetIndex(slot)).Jump(&label[1]);
  SetRegister(&builder, reg, 2048, scratch).Bind(&label[0]);
  IncrementRegister(&builder, reg, 2, scratch, GetIndex(slot1))
      .JumpLoop(&loop_header, 0);
  SetRegister(&builder, reg, 4096, scratch).Bind(&label[1]);
  IncrementRegister(&builder, reg, 4, scratch, GetIndex(slot2))
      .LoadAccumulatorWithRegister(reg)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::ToInt(*return_value), 7);
}

TEST(InterpreterConditionalJumps) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  FeedbackVectorSpec feedback_spec(zone);
  BytecodeArrayBuilder builder(zone, 1, 2, &feedback_spec);

  FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot1 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot2 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot3 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot4 = feedback_spec.AddBinaryOpICSlot();

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  Register reg(0), scratch(1);
  BytecodeLabel label[2];
  BytecodeLabel done, done1;

  builder.LoadLiteral(Smi::zero())
      .StoreAccumulatorInRegister(reg)
      .LoadFalse()
      .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &label[0]);
  IncrementRegister(&builder, reg, 1024, scratch, GetIndex(slot))
      .Bind(&label[0])
      .LoadTrue()
      .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &done);
  IncrementRegister(&builder, reg, 1, scratch, GetIndex(slot1))
      .LoadTrue()
      .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &label[1]);
  IncrementRegister(&builder, reg, 2048, scratch, GetIndex(slot2))
      .Bind(&label[1]);
  IncrementRegister(&builder, reg, 2, scratch, GetIndex(slot3))
      .LoadFalse()
      .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &done1);
  IncrementRegister(&builder, reg, 4, scratch, GetIndex(slot4))
      .LoadAccumulatorWithRegister(reg)
      .Bind(&done)
      .Bind(&done1)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::ToInt(*return_value), 7);
}

TEST(InterpreterConditionalJumps2) {
  // TODO(oth): Add tests for all conditional jumps near and far.
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  FeedbackVectorSpec feedback_spec(zone);
  BytecodeArrayBuilder builder(zone, 1, 2, &feedback_spec);

  FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot1 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot2 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot3 = feedback_spec.AddBinaryOpICSlot();
  FeedbackSlot slot4 = feedback_spec.AddBinaryOpICSlot();

  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  Register reg(0), scratch(1);
  BytecodeLabel label[2];
  BytecodeLabel done, done1;

  builder.LoadLiteral(Smi::zero())
      .StoreAccumulatorInRegister(reg)
      .LoadFalse()
      .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &label[0]);
  IncrementRegister(&builder, reg, 1024, scratch, GetIndex(slot))
      .Bind(&label[0])
      .LoadTrue()
      .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &done);
  IncrementRegister(&builder, reg, 1, scratch, GetIndex(slot1))
      .LoadTrue()
      .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &label[1]);
  IncrementRegister(&builder, reg, 2048, scratch, GetIndex(slot2))
      .Bind(&label[1]);
  IncrementRegister(&builder, reg, 2, scratch, GetIndex(slot3))
      .LoadFalse()
      .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &done1);
  IncrementRegister(&builder, reg, 4, scratch, GetIndex(slot4))
      .LoadAccumulatorWithRegister(reg)
      .Bind(&done)
      .Bind(&done1)
      .Return();

  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Smi::ToInt(*return_value), 7);
}

TEST(InterpreterJumpConstantWith16BitOperand) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));
  FeedbackVectorSpec feedback_spec(zone);
  BytecodeArrayBuilder builder(zone, 1, 257, &feedback_spec);

  FeedbackSlot slot = feedback_spec.AddBinaryOpICSlot();
  Handle<i::FeedbackMetadata> metadata =
      NewFeedbackMetadata(isolate, &feedback_spec);

  Register reg(0), scratch(256);
  BytecodeLabel done, fake;

  builder.LoadLiteral(Smi::zero());
  builder.StoreAccumulatorInRegister(reg);
  // Conditional jump to the fake label, to force both basic blocks to be live.
  builder.JumpIfTrue(ToBooleanMode::kConvertToBoolean, &fake);
  // Consume all 8-bit operands
  for (int i = 1; i <= 256; i++) {
    builder.LoadLiteral(i + 0.5);
    builder.BinaryOperation(Token::Value::ADD, reg, GetIndex(slot));
    builder.StoreAccumulatorInRegister(reg);
  }
  builder.Jump(&done);

  // Emit more than 16-bit immediate operands worth of code to jump over.
  builder.Bind(&fake);
  for (int i = 0; i < 6600; i++) {
    builder.LoadLiteral(Smi::zero());  // 1-byte
    builder.BinaryOperation(Token::Value::ADD, scratch,
                            GetIndex(slot));              // 6-bytes
    builder.StoreAccumulatorInRegister(scratch);          // 4-bytes
    builder.MoveRegister(scratch, reg);                   // 6-bytes
  }
  builder.Bind(&done);
  builder.LoadAccumulatorWithRegister(reg);
  builder.Return();

  ast_factory.Internalize(isolate);
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
  BytecodeArrayIterator iterator(bytecode_array);

  bool found_16bit_constant_jump = false;
  while (!iterator.done()) {
    if (iterator.current_bytecode() == Bytecode::kJumpConstant &&
        iterator.current_operand_scale() == OperandScale::kDouble) {
      found_16bit_constant_jump = true;
      break;
    }
    iterator.Advance();
  }
  CHECK(found_16bit_constant_jump);

  InterpreterTester tester(isolate, bytecode_array, metadata);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Handle<HeapNumber>::cast(return_value)->value(),
           256.0 / 2 * (1.5 + 256.5));
}

TEST(InterpreterJumpWith32BitOperand) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));
  BytecodeArrayBuilder builder(zone, 1, 1);
  Register reg(0);
  BytecodeLabel done;

  builder.LoadLiteral(Smi::zero());
  builder.StoreAccumulatorInRegister(reg);
  // Consume all 16-bit constant pool entries. Make sure to use doubles so that
  // the jump can't re-use an integer.
  for (int i = 1; i <= 65536; i++) {
    builder.LoadLiteral(i + 0.5);
  }
  builder.Jump(&done);
  builder.LoadLiteral(Smi::zero());
  builder.Bind(&done);
  builder.Return();

  ast_factory.Internalize(isolate);
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  BytecodeArrayIterator iterator(bytecode_array);

  bool found_32bit_jump = false;
  while (!iterator.done()) {
    if (iterator.current_bytecode() == Bytecode::kJump &&
        iterator.current_operand_scale() == OperandScale::kQuadruple) {
      found_32bit_jump = true;
      break;
    }
    iterator.Advance();
  }
  CHECK(found_32bit_jump);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();
  Handle<Object> return_value = callable().ToHandleChecked();
  CHECK_EQ(Handle<HeapNumber>::cast(return_value)->value(), 65536.5);
}

static const Token::Value kComparisonTypes[] = {
    Token::Value::EQ,  Token::Value::EQ_STRICT, Token::Value::LT,
    Token::Value::LTE, Token::Value::GT,        Token::Value::GTE};

template <typename T>
bool CompareC(Token::Value op, T lhs, T rhs, bool types_differed = false) {
  switch (op) {
    case Token::Value::EQ:
      return lhs == rhs;
    case Token::Value::NE:
      return lhs != rhs;
    case Token::Value::EQ_STRICT:
      return (lhs == rhs) && !types_differed;
    case Token::Value::NE_STRICT:
      return (lhs != rhs) || types_differed;
    case Token::Value::LT:
      return lhs < rhs;
    case Token::Value::LTE:
      return lhs <= rhs;
    case Token::Value::GT:
      return lhs > rhs;
    case Token::Value::GTE:
      return lhs >= rhs;
    default:
      UNREACHABLE();
  }
}

TEST(InterpreterSmiComparisons) {
  // NB Constants cover 31-bit space.
  int inputs[] = {v8::internal::kMinInt / 2,
                  v8::internal::kMinInt / 4,
                  -108733832,
                  -999,
                  -42,
                  -2,
                  -1,
                  0,
                  +1,
                  +2,
                  42,
                  12345678,
                  v8::internal::kMaxInt / 4,
                  v8::internal::kMaxInt / 2};

  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        HandleAndZoneScope handles;
        Isolate* isolate = handles.main_isolate();
        Zone* zone = handles.main_zone();
        FeedbackVectorSpec feedback_spec(zone);
        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

        FeedbackSlot slot = feedback_spec.AddCompareICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        Register r0(0);
        builder.LoadLiteral(Smi::FromInt(inputs[i]))
            .StoreAccumulatorInRegister(r0)
            .LoadLiteral(Smi::FromInt(inputs[j]))
            .CompareOperation(comparison, r0, GetIndex(slot))
            .Return();

        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBoolean());
        CHECK_EQ(return_value->BooleanValue(isolate),
                 CompareC(comparison, inputs[i], inputs[j]));
        if (tester.HasFeedbackMetadata()) {
          MaybeObject feedback = callable.vector().Get(slot);
          CHECK(feedback->IsSmi());
          CHECK_EQ(CompareOperationFeedback::kSignedSmall,
                   feedback->ToSmi().value());
        }
      }
    }
  }
}

TEST(InterpreterHeapNumberComparisons) {
  double inputs[] = {std::numeric_limits<double>::min(),
                     std::numeric_limits<double>::max(),
                     -0.001,
                     0.01,
                     0.1000001,
                     1e99,
                     -1e-99};
  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        HandleAndZoneScope handles;
        Isolate* isolate = handles.main_isolate();
        Zone* zone = handles.main_zone();
        AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                                    HashSeed(isolate));

        FeedbackVectorSpec feedback_spec(zone);
        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

        FeedbackSlot slot = feedback_spec.AddCompareICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        Register r0(0);
        builder.LoadLiteral(inputs[i])
            .StoreAccumulatorInRegister(r0)
            .LoadLiteral(inputs[j])
            .CompareOperation(comparison, r0, GetIndex(slot))
            .Return();

        ast_factory.Internalize(isolate);
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBoolean());
        CHECK_EQ(return_value->BooleanValue(isolate),
                 CompareC(comparison, inputs[i], inputs[j]));
        if (tester.HasFeedbackMetadata()) {
          MaybeObject feedback = callable.vector().Get(slot);
          CHECK(feedback->IsSmi());
          CHECK_EQ(CompareOperationFeedback::kNumber,
                   feedback->ToSmi().value());
        }
      }
    }
  }
}

TEST(InterpreterBigIntComparisons) {
  // This test only checks that the recorded type feedback is kBigInt.
  AstBigInt inputs[] = {AstBigInt("0"), AstBigInt("-42"),
                        AstBigInt("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF")};
  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        HandleAndZoneScope handles;
        Isolate* isolate = handles.main_isolate();
        Zone* zone = handles.main_zone();
        AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                                    HashSeed(isolate));

        FeedbackVectorSpec feedback_spec(zone);
        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

        FeedbackSlot slot = feedback_spec.AddCompareICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        Register r0(0);
        builder.LoadLiteral(inputs[i])
            .StoreAccumulatorInRegister(r0)
            .LoadLiteral(inputs[j])
            .CompareOperation(comparison, r0, GetIndex(slot))
            .Return();

        ast_factory.Internalize(isolate);
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBoolean());
        if (tester.HasFeedbackMetadata()) {
          MaybeObject feedback = callable.vector().Get(slot);
          CHECK(feedback->IsSmi());
          CHECK_EQ(CompareOperationFeedback::kBigInt,
                   feedback->ToSmi().value());
        }
      }
    }
  }
}

TEST(InterpreterStringComparisons) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();

  std::string inputs[] = {"A", "abc", "z", "", "Foo!", "Foo"};

  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                                    HashSeed(isolate));

        CanonicalHandleScope canonical(isolate);
        const char* lhs = inputs[i].c_str();
        const char* rhs = inputs[j].c_str();

        FeedbackVectorSpec feedback_spec(zone);
        FeedbackSlot slot = feedback_spec.AddCompareICSlot();
        Handle<i::FeedbackMetadata> metadata =
            NewFeedbackMetadata(isolate, &feedback_spec);

        BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);
        Register r0(0);
        builder.LoadLiteral(ast_factory.GetOneByteString(lhs))
            .StoreAccumulatorInRegister(r0)
            .LoadLiteral(ast_factory.GetOneByteString(rhs))
            .CompareOperation(comparison, r0, GetIndex(slot))
            .Return();

        ast_factory.Internalize(isolate);
        Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
        InterpreterTester tester(isolate, bytecode_array, metadata);
        auto callable = tester.GetCallable<>();
        Handle<Object> return_value = callable().ToHandleChecked();
        CHECK(return_value->IsBoolean());
        CHECK_EQ(return_value->BooleanValue(isolate),
                 CompareC(comparison, inputs[i], inputs[j]));
        if (tester.HasFeedbackMetadata()) {
          MaybeObject feedback = callable.vector().Get(slot);
          CHECK(feedback->IsSmi());
          int const expected_feedback =
              Token::IsOrderedRelationalCompareOp(comparison)
                  ? CompareOperationFeedback::kString
                  : CompareOperationFeedback::kInternalizedString;
          CHECK_EQ(expected_feedback, feedback->ToSmi().value());
        }
      }
    }
  }
}

static void LoadStringAndAddSpace(BytecodeArrayBuilder* builder,
                                  AstValueFactory* ast_factory,
                                  const char* cstr,
                                  FeedbackSlot string_add_slot) {
  Register string_reg = builder->register_allocator()->NewRegister();

  (*builder)
      .LoadLiteral(ast_factory->GetOneByteString(cstr))
      .StoreAccumulatorInRegister(string_reg)
      .LoadLiteral(ast_factory->GetOneByteString(" "))
      .BinaryOperation(Token::Value::ADD, string_reg,
                       GetIndex(string_add_slot));
}

TEST(InterpreterMixedComparisons) {
  // This test compares a HeapNumber with a String. The latter is
  // convertible to a HeapNumber so comparison will be between numeric
  // values except for the strict comparisons where no conversion is
  // performed.
  const char* inputs[] = {"-1.77", "-40.333", "0.01", "55.77e50", "2.01"};

  enum WhichSideString { kLhsIsString, kRhsIsString };

  enum StringType { kInternalizedStringConstant, kComputedString };

  for (size_t c = 0; c < arraysize(kComparisonTypes); c++) {
    Token::Value comparison = kComparisonTypes[c];
    for (size_t i = 0; i < arraysize(inputs); i++) {
      for (size_t j = 0; j < arraysize(inputs); j++) {
        // We test the case where either the lhs or the rhs is a string...
        for (WhichSideString which_side : {kLhsIsString, kRhsIsString}) {
          // ... and the case when the string is internalized or computed.
          for (StringType string_type :
               {kInternalizedStringConstant, kComputedString}) {
            const char* lhs_cstr = inputs[i];
            const char* rhs_cstr = inputs[j];
            double lhs = StringToDouble(lhs_cstr, ConversionFlags::NO_FLAGS);
            double rhs = StringToDouble(rhs_cstr, ConversionFlags::NO_FLAGS);
            HandleAndZoneScope handles;
            Isolate* isolate = handles.main_isolate();
            Zone* zone = handles.main_zone();
            AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                                        HashSeed(isolate));
            FeedbackVectorSpec feedback_spec(zone);
            BytecodeArrayBuilder builder(zone, 1, 0, &feedback_spec);

            FeedbackSlot string_add_slot = feedback_spec.AddBinaryOpICSlot();
            FeedbackSlot slot = feedback_spec.AddCompareICSlot();
            Handle<i::FeedbackMetadata> metadata =
                NewFeedbackMetadata(isolate, &feedback_spec);

            // lhs is in a register, rhs is in the accumulator.
            Register lhs_reg = builder.register_allocator()->NewRegister();

            if (which_side == kRhsIsString) {
              // Comparison with HeapNumber on the lhs and String on the rhs.

              builder.LoadLiteral(lhs).StoreAccumulatorInRegister(lhs_reg);

              if (string_type == kInternalizedStringConstant) {
                // rhs string is internalized.
                builder.LoadLiteral(ast_factory.GetOneByteString(rhs_cstr));
              } else {
                CHECK_EQ(string_type, kComputedString);
                // rhs string is not internalized (append a space to the end).
                LoadStringAndAddSpace(&builder, &ast_factory, rhs_cstr,
                                      string_add_slot);
              }
              break;
            } else {
              CHECK_EQ(which_side, kLhsIsString);
              // Comparison with String on the lhs and HeapNumber on the rhs.

              if (string_type == kInternalizedStringConstant) {
                // lhs string is internalized
                builder.LoadLiteral(ast_factory.GetOneByteString(lhs_cstr));
              } else {
                CHECK_EQ(string_type, kComputedString);
                // lhs string is not internalized (append a space to the end).
                LoadStringAndAddSpace(&builder, &ast_factory, lhs_cstr,
                                      string_add_slot);
              }
              builder.StoreAccumulatorInRegister(lhs_reg);

              builder.LoadLiteral(rhs);
            }

            builder.CompareOperation(comparison, lhs_reg, GetIndex(slot))
                .Return();

            ast_factory.Internalize(isolate);
            Handle<BytecodeArray> bytecode_array =
                builder.ToBytecodeArray(isolate);
            InterpreterTester tester(isolate, bytecode_array, metadata);
            auto callable = tester.GetCallable<>();
            Handle<Object> return_value = callable().ToHandleChecked();
            CHECK(return_value->IsBoolean());
            CHECK_EQ(return_value->BooleanValue(isolate),
                     CompareC(comparison, lhs, rhs, true));
            if (tester.HasFeedbackMetadata()) {
              MaybeObject feedback = callable.vector().Get(slot);
              CHECK(feedback->IsSmi());
              // Comparison with a number and string collects kAny feedback.
              CHECK_EQ(CompareOperationFeedback::kAny,
                       feedback->ToSmi().value());
            }
          }
        }
      }
    }
  }
}

TEST(InterpreterStrictNotEqual) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();
  const char* code_snippet =
      "function f(lhs, rhs) {\n"
      "  return lhs !== rhs;\n"
      "}\n"
      "f(0, 0);\n";
  InterpreterTester tester(isolate, code_snippet);
  auto callable = tester.GetCallable<Handle<Object>, Handle<Object>>();

  // Test passing different types.
  const char* inputs[] = {"-1.77", "-40.333", "0.01", "55.77e5", "2.01"};
  for (size_t i = 0; i < arraysize(inputs); i++) {
    for (size_t j = 0; j < arraysize(inputs); j++) {
      double lhs = StringToDouble(inputs[i], ConversionFlags::NO_FLAGS);
      double rhs = StringToDouble(inputs[j], ConversionFlags::NO_FLAGS);
      Handle<Object> lhs_obj = factory->NewNumber(lhs);
      Handle<Object> rhs_obj = factory->NewStringFromAsciiChecked(inputs[j]);

      Handle<Object> return_value =
          callable(lhs_obj, rhs_obj).ToHandleChecked();
      CHECK(return_value->IsBoolean());
      CHECK_EQ(return_value->BooleanValue(isolate),
               CompareC(Token::Value::NE_STRICT, lhs, rhs, true));
    }
  }

  // Test passing string types.
  const char* inputs_str[] = {"A", "abc", "z", "", "Foo!", "Foo"};
  for (size_t i = 0; i < arraysize(inputs_str); i++) {
    for (size_t j = 0; j < arraysize(inputs_str); j++) {
      Handle<Object> lhs_obj =
          factory->NewStringFromAsciiChecked(inputs_str[i]);
      Handle<Object> rhs_obj =
          factory->NewStringFromAsciiChecked(inputs_str[j]);

      Handle<Object> return_value =
          callable(lhs_obj, rhs_obj).ToHandleChecked();
      CHECK(return_value->IsBoolean());
      CHECK_EQ(return_value->BooleanValue(isolate),
               CompareC(Token::Value::NE_STRICT, inputs_str[i], inputs_str[j]));
    }
  }

  // Test passing doubles.
  double inputs_number[] = {std::numeric_limits<double>::min(),
                            std::numeric_limits<double>::max(),
                            -0.001,
                            0.01,
                            0.1000001,
                            1e99,
                            -1e-99};
  for (size_t i = 0; i < arraysize(inputs_number); i++) {
    for (size_t j = 0; j < arraysize(inputs_number); j++) {
      Handle<Object> lhs_obj = factory->NewNumber(inputs_number[i]);
      Handle<Object> rhs_obj = factory->NewNumber(inputs_number[j]);

      Handle<Object> return_value =
          callable(lhs_obj, rhs_obj).ToHandleChecked();
      CHECK(return_value->IsBoolean());
      CHECK_EQ(return_value->BooleanValue(isolate),
               CompareC(Token::Value::NE_STRICT, inputs_number[i],
                        inputs_number[j]));
    }
  }
}

TEST(InterpreterCompareTypeOf) {
  using LiteralFlag = TestTypeOfFlags::LiteralFlag;
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();
  Zone* zone = handles.main_zone();
  std::pair<Handle<Object>, LiteralFlag> inputs[] = {
      {handle(Smi::FromInt(24), isolate), LiteralFlag::kNumber},
      {factory->NewNumber(2.5), LiteralFlag::kNumber},
      {factory->NewStringFromAsciiChecked("foo"), LiteralFlag::kString},
      {factory
           ->NewConsString(factory->NewStringFromAsciiChecked("foo"),
                           factory->NewStringFromAsciiChecked("bar"))
           .ToHandleChecked(),
       LiteralFlag::kString},
      {factory->prototype_string(), LiteralFlag::kString},
      {factory->NewSymbol(), LiteralFlag::kSymbol},
      {factory->true_value(), LiteralFlag::kBoolean},
      {factory->false_value(), LiteralFlag::kBoolean},
      {factory->undefined_value(), LiteralFlag::kUndefined},
      {InterpreterTester::NewObject(
           "(function() { return function() {}; })();"),
       LiteralFlag::kFunction},
      {InterpreterTester::NewObject("new Object();"), LiteralFlag::kObject},
      {factory->null_value(), LiteralFlag::kObject},
  };
  const LiteralFlag kLiterals[] = {
#define LITERAL_FLAG(name, _) LiteralFlag::k##name,
      TYPEOF_LITERAL_LIST(LITERAL_FLAG)
#undef LITERAL_FLAG
  };

  for (size_t l = 0; l < arraysize(kLiterals); l++) {
    LiteralFlag literal_flag = kLiterals[l];
    if (literal_flag == LiteralFlag::kOther) continue;

    BytecodeArrayBuilder builder(zone, 1, 0);
    builder.LoadAccumulatorWithRegister(builder.Receiver())
        .CompareTypeOf(kLiterals[l])
        .Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<Handle<Object>>();

    for (size_t i = 0; i < arraysize(inputs); i++) {
      Handle<Object> return_value = callable(inputs[i].first).ToHandleChecked();
      CHECK(return_value->IsBoolean());
      CHECK_EQ(return_value->BooleanValue(isolate),
               inputs[i].second == literal_flag);
    }
  }
}

TEST(InterpreterInstanceOf) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Factory* factory = isolate->factory();
  Handle<i::String> name = factory->NewStringFromAsciiChecked("cons");
  Handle<i::JSFunction> func = factory->NewFunctionForTest(name);
  Handle<i::JSObject> instance = factory->NewJSObject(func);
  Handle<i::Object> other = factory->NewNumber(3.3333);
  Handle<i::Object> cases[] = {Handle<i::Object>::cast(instance), other};
  for (size_t i = 0; i < arraysize(cases); i++) {
    bool expected_value = (i == 0);
    FeedbackVectorSpec feedback_spec(zone);
    BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

    Register r0(0);
    size_t case_entry = builder.AllocateDeferredConstantPoolEntry();
    builder.SetDeferredConstantPoolEntry(case_entry, cases[i]);
    builder.LoadConstantPoolEntry(case_entry).StoreAccumulatorInRegister(r0);

    FeedbackSlot slot = feedback_spec.AddInstanceOfSlot();
    Handle<i::FeedbackMetadata> metadata =
        NewFeedbackMetadata(isolate, &feedback_spec);

    size_t func_entry = builder.AllocateDeferredConstantPoolEntry();
    builder.SetDeferredConstantPoolEntry(func_entry, func);
    builder.LoadConstantPoolEntry(func_entry)
        .CompareOperation(Token::Value::INSTANCEOF, r0, GetIndex(slot))
        .Return();

    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(isolate), expected_value);
  }
}

TEST(InterpreterTestIn) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  Factory* factory = isolate->factory();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));
  // Allocate an array
  Handle<i::JSArray> array =
      factory->NewJSArray(0, i::ElementsKind::PACKED_SMI_ELEMENTS);
  // Check for these properties on the array object
  const char* properties[] = {"length", "fuzzle", "x", "0"};
  for (size_t i = 0; i < arraysize(properties); i++) {
    bool expected_value = (i == 0);
    FeedbackVectorSpec feedback_spec(zone);
    BytecodeArrayBuilder builder(zone, 1, 1, &feedback_spec);

    Register r0(0);
    builder.LoadLiteral(ast_factory.GetOneByteString(properties[i]))
        .StoreAccumulatorInRegister(r0);

    FeedbackSlot slot = feedback_spec.AddKeyedHasICSlot();
    Handle<i::FeedbackMetadata> metadata =
        NewFeedbackMetadata(isolate, &feedback_spec);

    size_t array_entry = builder.AllocateDeferredConstantPoolEntry();
    builder.SetDeferredConstantPoolEntry(array_entry, array);
    builder.LoadConstantPoolEntry(array_entry)
        .CompareOperation(Token::Value::IN, r0, GetIndex(slot))
        .Return();

    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
    InterpreterTester tester(isolate, bytecode_array, metadata);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(isolate), expected_value);
  }
}

TEST(InterpreterUnaryNot) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  for (size_t i = 1; i < 10; i++) {
    bool expected_value = ((i & 1) == 1);
    BytecodeArrayBuilder builder(zone, 1, 0);

    Register r0(0);
    builder.LoadFalse();
    for (size_t j = 0; j < i; j++) {
      builder.LogicalNot(ToBooleanMode::kAlreadyBoolean);
    }
    builder.Return();
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(isolate), expected_value);
  }
}

TEST(InterpreterUnaryNotNonBoolean) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();
  AstValueFactory ast_factory(zone, isolate->ast_string_constants(),
                              HashSeed(isolate));

  std::pair<LiteralForTest, bool> object_type_tuples[] = {
      std::make_pair(LiteralForTest(LiteralForTest::kUndefined), true),
      std::make_pair(LiteralForTest(LiteralForTest::kNull), true),
      std::make_pair(LiteralForTest(LiteralForTest::kFalse), true),
      std::make_pair(LiteralForTest(LiteralForTest::kTrue), false),
      std::make_pair(LiteralForTest(9.1), false),
      std::make_pair(LiteralForTest(0), true),
      std::make_pair(LiteralForTest(ast_factory.GetOneByteString("hello")),
                     false),
      std::make_pair(LiteralForTest(ast_factory.GetOneByteString("")), true),
  };

  for (size_t i = 0; i < arraysize(object_type_tuples); i++) {
    BytecodeArrayBuilder builder(zone, 1, 0);

    Register r0(0);
    LoadLiteralForTest(&builder, object_type_tuples[i].first);
    builder.LogicalNot(ToBooleanMode::kConvertToBoolean).Return();
    ast_factory.Internalize(isolate);
    Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);
    InterpreterTester tester(isolate, bytecode_array);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->IsBoolean());
    CHECK_EQ(return_value->BooleanValue(isolate), object_type_tuples[i].second);
  }
}

TEST(InterpreterTypeof) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, const char*> typeof_vals[] = {
      std::make_pair("return typeof undefined;", "undefined"),
      std::make_pair("return typeof null;", "object"),
      std::make_pair("return typeof true;", "boolean"),
      std::make_pair("return typeof false;", "boolean"),
      std::make_pair("return typeof 9.1;", "number"),
      std::make_pair("return typeof 7771;", "number"),
      std::make_pair("return typeof 'hello';", "string"),
      std::make_pair("return typeof global_unallocated;", "undefined"),
  };

  for (size_t i = 0; i < arraysize(typeof_vals); i++) {
    std::string source(InterpreterTester::SourceForBody(typeof_vals[i].first));
    InterpreterTester tester(isolate, source.c_str());

    auto callable = tester.GetCallable<>();
    Handle<v8::internal::String> return_value =
        Handle<v8::internal::String>::cast(callable().ToHandleChecked());
    auto actual = return_value->ToCString();
    CHECK_EQ(strcmp(&actual[0], typeof_vals[i].second), 0);
  }
}

TEST(InterpreterCallRuntime) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();

  BytecodeArrayBuilder builder(zone, 1, 2);
  RegisterList args = builder.register_allocator()->NewRegisterList(2);

  builder.LoadLiteral(Smi::FromInt(15))
      .StoreAccumulatorInRegister(args[0])
      .LoadLiteral(Smi::FromInt(40))
      .StoreAccumulatorInRegister(args[1])
      .CallRuntime(Runtime::kAdd, args)
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(55));
}

TEST(InterpreterInvokeIntrinsic) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Zone* zone = handles.main_zone();

  BytecodeArrayBuilder builder(zone, 1, 2);

  builder.LoadLiteral(Smi::FromInt(15))
      .StoreAccumulatorInRegister(Register(0))
      .CallRuntime(Runtime::kInlineIsArray, Register(0))
      .Return();
  Handle<BytecodeArray> bytecode_array = builder.ToBytecodeArray(isolate);

  InterpreterTester tester(isolate, bytecode_array);
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK(return_val->IsBoolean());
  CHECK_EQ(return_val->BooleanValue(isolate), false);
}

TEST(InterpreterFunctionLiteral) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  // Test calling a function literal.
  std::string source(
      "function " + InterpreterTester::function_name() + "(a) {\n"
      "  return (function(x){ return x + 2; })(a);\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<Handle<Object>>();

  Handle<i::Object> return_val = callable(
      Handle<Smi>(Smi::FromInt(3), handles.main_isolate())).ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(5));
}

TEST(InterpreterRegExpLiterals) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("return /abd/.exec('cccabbdd');\n",
                     factory->null_value()),
      std::make_pair("return /ab+d/.exec('cccabbdd')[0];\n",
                     factory->NewStringFromStaticChars("abbd")),
      std::make_pair("return /AbC/i.exec('ssaBC')[0];\n",
                     factory->NewStringFromStaticChars("aBC")),
      std::make_pair("return 'ssaBC'.match(/AbC/i)[0];\n",
                     factory->NewStringFromStaticChars("aBC")),
      std::make_pair("return 'ssaBCtAbC'.match(/(AbC)/gi)[1];\n",
                     factory->NewStringFromStaticChars("AbC")),
  };

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}

TEST(InterpreterArrayLiterals) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("return [][0];\n",
                     factory->undefined_value()),
      std::make_pair("return [1, 3, 2][1];\n",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("return ['a', 'b', 'c'][2];\n",
                     factory->NewStringFromStaticChars("c")),
      std::make_pair("var a = 100; return [a, a + 1, a + 2, a + 3][2];\n",
                     handle(Smi::FromInt(102), isolate)),
      std::make_pair("return [[1, 2, 3], ['a', 'b', 'c']][1][0];\n",
                     factory->NewStringFromStaticChars("a")),
      std::make_pair("var t = 't'; return [[t, t + 'est'], [1 + t]][0][1];\n",
                     factory->NewStringFromStaticChars("test"))
  };

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}

TEST(InterpreterObjectLiterals) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("return { }.name;",
                     factory->undefined_value()),
      std::make_pair("return { name: 'string', val: 9.2 }.name;",
                     factory->NewStringFromStaticChars("string")),
      std::make_pair("var a = 15; return { name: 'string', val: a }.val;",
                     handle(Smi::FromInt(15), isolate)),
      std::make_pair("var a = 5; return { val: a, val: a + 1 }.val;",
                     handle(Smi::FromInt(6), isolate)),
      std::make_pair("return { func: function() { return 'test' } }.func();",
                     factory->NewStringFromStaticChars("test")),
      std::make_pair("return { func(a) { return a + 'st'; } }.func('te');",
                     factory->NewStringFromStaticChars("test")),
      std::make_pair("return { get a() { return 22; } }.a;",
                     handle(Smi::FromInt(22), isolate)),
      std::make_pair("var a = { get b() { return this.x + 't'; },\n"
                     "          set b(val) { this.x = val + 's' } };\n"
                     "a.b = 'te';\n"
                     "return a.b;",
                     factory->NewStringFromStaticChars("test")),
      std::make_pair("var a = 123; return { 1: a }[1];",
                     handle(Smi::FromInt(123), isolate)),
      std::make_pair("return Object.getPrototypeOf({ __proto__: null });",
                     factory->null_value()),
      std::make_pair("var a = 'test'; return { [a]: 1 }.test;",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 'test'; return { b: a, [a]: a + 'ing' }['test']",
                     factory->NewStringFromStaticChars("testing")),
      std::make_pair("var a = 'proto_str';\n"
                     "var b = { [a]: 1, __proto__: { var : a } };\n"
                     "return Object.getPrototypeOf(b).var",
                     factory->NewStringFromStaticChars("proto_str")),
      std::make_pair("var n = 'name';\n"
                     "return { [n]: 'val', get a() { return 987 } }['a'];",
                     handle(Smi::FromInt(987), isolate)),
  };

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}

TEST(InterpreterConstruct) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::string source(
      "function counter() { this.count = 0; }\n"
      "function " +
      InterpreterTester::function_name() +
      "() {\n"
      "  var c = new counter();\n"
      "  return c.count;\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::kZero);
}

TEST(InterpreterConstructWithArgument) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::string source(
      "function counter(arg0) { this.count = 17; this.x = arg0; }\n"
      "function " +
      InterpreterTester::function_name() +
      "() {\n"
      "  var c = new counter(3);\n"
      "  return c.x;\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(3));
}

TEST(InterpreterConstructWithArguments) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::string source(
      "function counter(arg0, arg1) {\n"
      "  this.count = 7; this.x = arg0; this.y = arg1;\n"
      "}\n"
      "function " +
      InterpreterTester::function_name() +
      "() {\n"
      "  var c = new counter(3, 5);\n"
      "  return c.count + c.x + c.y;\n"
      "}");
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<>();

  Handle<Object> return_val = callable().ToHandleChecked();
  CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(15));
}

TEST(InterpreterContextVariables) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::ostringstream unique_vars;
  for (int i = 0; i < 250; i++) {
    unique_vars << "var a" << i << " = 0;";
  }
  std::pair<std::string, Handle<Object>> context_vars[] = {
      std::make_pair("var a; (function() { a = 1; })(); return a;",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 10; (function() { a; })(); return a;",
                     handle(Smi::FromInt(10), isolate)),
      std::make_pair("var a = 20; var b = 30;\n"
                     "return (function() { return a + b; })();",
                     handle(Smi::FromInt(50), isolate)),
      std::make_pair("'use strict'; let a = 1;\n"
                     "{ let b = 2; return (function() { return a + b; })(); }",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("'use strict'; let a = 10;\n"
                     "{ let b = 20; var c = function() { [a, b] };\n"
                     "  return a + b; }",
                     handle(Smi::FromInt(30), isolate)),
      std::make_pair("'use strict';" + unique_vars.str() +
                     "eval(); var b = 100; return b;",
                     handle(Smi::FromInt(100), isolate)),
  };

  for (size_t i = 0; i < arraysize(context_vars); i++) {
    std::string source(
        InterpreterTester::SourceForBody(context_vars[i].first.c_str()));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*context_vars[i].second));
  }
}

TEST(InterpreterContextParameters) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> context_params[] = {
      std::make_pair("return (function() { return arg1; })();",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("(function() { arg1 = 4; })(); return arg1;",
                     handle(Smi::FromInt(4), isolate)),
      std::make_pair("(function() { arg3 = arg2 - arg1; })(); return arg3;",
                     handle(Smi::FromInt(1), isolate)),
  };

  for (size_t i = 0; i < arraysize(context_params); i++) {
    std::string source = "function " + InterpreterTester::function_name() +
                         "(arg1, arg2, arg3) {" + context_params[i].first + "}";
    InterpreterTester tester(isolate, source.c_str());
    auto callable =
        tester.GetCallable<Handle<Object>, Handle<Object>, Handle<Object>>();

    Handle<Object> a1 = handle(Smi::FromInt(1), isolate);
    Handle<Object> a2 = handle(Smi::FromInt(2), isolate);
    Handle<Object> a3 = handle(Smi::FromInt(3), isolate);
    Handle<i::Object> return_value = callable(a1, a2, a3).ToHandleChecked();
    CHECK(return_value->SameValue(*context_params[i].second));
  }
}

TEST(InterpreterOuterContextVariables) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> context_vars[] = {
      std::make_pair("return outerVar * innerArg;",
                     handle(Smi::FromInt(200), isolate)),
      std::make_pair("outerVar = innerArg; return outerVar",
                     handle(Smi::FromInt(20), isolate)),
  };

  std::string header(
      "function Outer() {"
      "  var outerVar = 10;"
      "  function Inner(innerArg) {"
      "    this.innerFunc = function() { ");
  std::string footer(
      "  }}"
      "  this.getInnerFunc = function() { return new Inner(20).innerFunc; }"
      "}"
      "var f = new Outer().getInnerFunc();");

  for (size_t i = 0; i < arraysize(context_vars); i++) {
    std::string source = header + context_vars[i].first + footer;
    InterpreterTester tester(isolate, source.c_str(), "*");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*context_vars[i].second));
  }
}

TEST(InterpreterComma) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("var a; return 0, a;\n", factory->undefined_value()),
      std::make_pair("return 'a', 2.2, 3;\n",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("return 'a', 'b', 'c';\n",
                     factory->NewStringFromStaticChars("c")),
      std::make_pair("return 3.2, 2.3, 4.5;\n", factory->NewNumber(4.5)),
      std::make_pair("var a = 10; return b = a, b = b+1;\n",
                     handle(Smi::FromInt(11), isolate)),
      std::make_pair("var a = 10; return b = a, b = b+1, b + 10;\n",
                     handle(Smi::FromInt(21), isolate))};

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}

TEST(InterpreterLogicalOr) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("var a, b; return a || b;\n", factory->undefined_value()),
      std::make_pair("var a, b = 10; return a || b;\n",
                     handle(Smi::FromInt(10), isolate)),
      std::make_pair("var a = '0', b = 10; return a || b;\n",
                     factory->NewStringFromStaticChars("0")),
      std::make_pair("return 0 || 3.2;\n", factory->NewNumber(3.2)),
      std::make_pair("return 'a' || 0;\n",
                     factory->NewStringFromStaticChars("a")),
      std::make_pair("var a = '0', b = 10; return (a == 0) || b;\n",
                     factory->true_value())};

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}

TEST(InterpreterLogicalAnd) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> literals[] = {
      std::make_pair("var a, b = 10; return a && b;\n",
                     factory->undefined_value()),
      std::make_pair("var a = 0, b = 10; return a && b / a;\n",
                     handle(Smi::kZero, isolate)),
      std::make_pair("var a = '0', b = 10; return a && b;\n",
                     handle(Smi::FromInt(10), isolate)),
      std::make_pair("return 0.0 && 3.2;\n", handle(Smi::kZero, isolate)),
      std::make_pair("return 'a' && 'b';\n",
                     factory->NewStringFromStaticChars("b")),
      std::make_pair("return 'a' && 0 || 'b', 'c';\n",
                     factory->NewStringFromStaticChars("c")),
      std::make_pair("var x = 1, y = 3; return x && 0 + 1 || y;\n",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var x = 1, y = 3; return (x == 1) && (3 == 3) || y;\n",
                     factory->true_value())};

  for (size_t i = 0; i < arraysize(literals); i++) {
    std::string source(InterpreterTester::SourceForBody(literals[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*literals[i].second));
  }
}

TEST(InterpreterTryCatch) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> catches[] = {
      std::make_pair("var a = 1; try { a = 2 } catch(e) { a = 3 }; return a;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a; try { undef.x } catch(e) { a = 2 }; return a;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a; try { throw 1 } catch(e) { a = e + 2 }; return a;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a; try { throw 1 } catch(e) { a = e + 2 };"
                     "       try { throw a } catch(e) { a = e + 3 }; return a;",
                     handle(Smi::FromInt(6), isolate)),
  };

  for (size_t i = 0; i < arraysize(catches); i++) {
    std::string source(InterpreterTester::SourceForBody(catches[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*catches[i].second));
  }
}

TEST(InterpreterTryFinally) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> finallies[] = {
      std::make_pair(
          "var a = 1; try { a = a + 1; } finally { a = a + 2; }; return a;",
          factory->NewStringFromStaticChars("R4")),
      std::make_pair(
          "var a = 1; try { a = 2; return 23; } finally { a = 3 }; return a;",
          factory->NewStringFromStaticChars("R23")),
      std::make_pair(
          "var a = 1; try { a = 2; throw 23; } finally { a = 3 }; return a;",
          factory->NewStringFromStaticChars("E23")),
      std::make_pair(
          "var a = 1; try { a = 2; throw 23; } finally { return a; };",
          factory->NewStringFromStaticChars("R2")),
      std::make_pair(
          "var a = 1; try { a = 2; throw 23; } finally { throw 42; };",
          factory->NewStringFromStaticChars("E42")),
      std::make_pair("var a = 1; for (var i = 10; i < 20; i += 5) {"
                     "  try { a = 2; break; } finally { a = 3; }"
                     "} return a + i;",
                     factory->NewStringFromStaticChars("R13")),
      std::make_pair("var a = 1; for (var i = 10; i < 20; i += 5) {"
                     "  try { a = 2; continue; } finally { a = 3; }"
                     "} return a + i;",
                     factory->NewStringFromStaticChars("R23")),
      std::make_pair("var a = 1; try { a = 2;"
                     "  try { a = 3; throw 23; } finally { a = 4; }"
                     "} catch(e) { a = a + e; } return a;",
                     factory->NewStringFromStaticChars("R27")),
      std::make_pair("var func_name;"
                     "function tcf2(a) {"
                     "  try { throw new Error('boom');} "
                     "  catch(e) {return 153; } "
                     "  finally {func_name = tcf2.name;}"
                     "}"
                     "tcf2();"
                     "return func_name;",
                     factory->NewStringFromStaticChars("Rtcf2")),
  };

  const char* try_wrapper =
      "(function() { try { return 'R' + f() } catch(e) { return 'E' + e }})()";

  for (size_t i = 0; i < arraysize(finallies); i++) {
    std::string source(InterpreterTester::SourceForBody(finallies[i].first));
    InterpreterTester tester(isolate, source.c_str());
    tester.GetCallable<>();
    Handle<Object> wrapped = v8::Utils::OpenHandle(*CompileRun(try_wrapper));
    CHECK(wrapped->SameValue(*finallies[i].second));
  }
}

TEST(InterpreterThrow) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> throws[] = {
      std::make_pair("throw undefined;\n",
                     factory->undefined_value()),
      std::make_pair("throw 1;\n",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("throw 'Error';\n",
                     factory->NewStringFromStaticChars("Error")),
      std::make_pair("var a = true; if (a) { throw 'Error'; }\n",
                     factory->NewStringFromStaticChars("Error")),
      std::make_pair("var a = false; if (a) { throw 'Error'; }\n",
                     factory->undefined_value()),
      std::make_pair("throw 'Error1'; throw 'Error2'\n",
                     factory->NewStringFromStaticChars("Error1")),
  };

  const char* try_wrapper =
      "(function() { try { f(); } catch(e) { return e; }})()";

  for (size_t i = 0; i < arraysize(throws); i++) {
    std::string source(InterpreterTester::SourceForBody(throws[i].first));
    InterpreterTester tester(isolate, source.c_str());
    tester.GetCallable<>();
    Handle<Object> thrown_obj = v8::Utils::OpenHandle(*CompileRun(try_wrapper));
    CHECK(thrown_obj->SameValue(*throws[i].second));
  }
}

TEST(InterpreterCountOperators) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> count_ops[] = {
      std::make_pair("var a = 1; return ++a;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 1; return a++;",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 5; return --a;",
                     handle(Smi::FromInt(4), isolate)),
      std::make_pair("var a = 5; return a--;",
                     handle(Smi::FromInt(5), isolate)),
      std::make_pair("var a = 5.2; return --a;", factory->NewHeapNumber(4.2)),
      std::make_pair("var a = 'string'; return ++a;", factory->nan_value()),
      std::make_pair("var a = 'string'; return a--;", factory->nan_value()),
      std::make_pair("var a = true; return ++a;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = false; return a--;", handle(Smi::kZero, isolate)),
      std::make_pair("var a = { val: 11 }; return ++a.val;",
                     handle(Smi::FromInt(12), isolate)),
      std::make_pair("var a = { val: 11 }; return a.val--;",
                     handle(Smi::FromInt(11), isolate)),
      std::make_pair("var a = { val: 11 }; return ++a.val;",
                     handle(Smi::FromInt(12), isolate)),
      std::make_pair("var name = 'val'; var a = { val: 22 }; return --a[name];",
                     handle(Smi::FromInt(21), isolate)),
      std::make_pair("var name = 'val'; var a = { val: 22 }; return a[name]++;",
                     handle(Smi::FromInt(22), isolate)),
      std::make_pair("var a = 1; (function() { a = 2 })(); return ++a;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1; (function() { a = 2 })(); return a--;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var i = 5; while(i--) {}; return i;",
                     handle(Smi::FromInt(-1), isolate)),
      std::make_pair("var i = 1; if(i--) { return 1; } else { return 2; };",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var i = -2; do {} while(i++) {}; return i;",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var i = -1; for(; i++; ) {}; return i",
                     handle(Smi::FromInt(1), isolate)),
      std::make_pair("var i = 20; switch(i++) {\n"
                     "  case 20: return 1;\n"
                     "  default: return 2;\n"
                     "}",
                     handle(Smi::FromInt(1), isolate)),
  };

  for (size_t i = 0; i < arraysize(count_ops); i++) {
    std::string source(InterpreterTester::SourceForBody(count_ops[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*count_ops[i].second));
  }
}

TEST(InterpreterGlobalCountOperators) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> count_ops[] = {
      std::make_pair("var global = 100;function f(){ return ++global; }",
                     handle(Smi::FromInt(101), isolate)),
      std::make_pair("var global = 100; function f(){ return --global; }",
                     handle(Smi::FromInt(99), isolate)),
      std::make_pair("var global = 100; function f(){ return global++; }",
                     handle(Smi::FromInt(100), isolate)),
      std::make_pair("unallocated = 200; function f(){ return ++unallocated; }",
                     handle(Smi::FromInt(201), isolate)),
      std::make_pair("unallocated = 200; function f(){ return --unallocated; }",
                     handle(Smi::FromInt(199), isolate)),
      std::make_pair("unallocated = 200; function f(){ return unallocated++; }",
                     handle(Smi::FromInt(200), isolate)),
  };

  for (size_t i = 0; i < arraysize(count_ops); i++) {
    InterpreterTester tester(isolate, count_ops[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*count_ops[i].second));
  }
}

TEST(InterpreterCompoundExpressions) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> compound_expr[] = {
      std::make_pair("var a = 1; a += 2; return a;",
                     Handle<Object>(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 10; a /= 2; return a;",
                     Handle<Object>(Smi::FromInt(5), isolate)),
      std::make_pair("var a = 'test'; a += 'ing'; return a;",
                     factory->NewStringFromStaticChars("testing")),
      std::make_pair("var a = { val: 2 }; a.val *= 2; return a.val;",
                     Handle<Object>(Smi::FromInt(4), isolate)),
      std::make_pair("var a = 1; (function f() { a = 2; })(); a += 24;"
                     "return a;",
                     Handle<Object>(Smi::FromInt(26), isolate)),
  };

  for (size_t i = 0; i < arraysize(compound_expr); i++) {
    std::string source(
        InterpreterTester::SourceForBody(compound_expr[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*compound_expr[i].second));
  }
}

TEST(InterpreterGlobalCompoundExpressions) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> compound_expr[2] = {
      std::make_pair("var global = 100;"
                     "function f() { global += 20; return global; }",
                     Handle<Object>(Smi::FromInt(120), isolate)),
      std::make_pair("unallocated = 100;"
                     "function f() { unallocated -= 20; return unallocated; }",
                     Handle<Object>(Smi::FromInt(80), isolate)),
  };

  for (size_t i = 0; i < arraysize(compound_expr); i++) {
    InterpreterTester tester(isolate, compound_expr[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*compound_expr[i].second));
  }
}

TEST(InterpreterCreateArguments) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, int> create_args[] = {
      std::make_pair("function f() { return arguments[0]; }", 0),
      std::make_pair("function f(a) { return arguments[0]; }", 0),
      std::make_pair("function f() { return arguments[2]; }", 2),
      std::make_pair("function f(a) { return arguments[2]; }", 2),
      std::make_pair("function f(a, b, c, d) { return arguments[2]; }", 2),
      std::make_pair("function f(a) {"
                     "'use strict'; return arguments[0]; }",
                     0),
      std::make_pair("function f(a, b, c, d) {"
                     "'use strict'; return arguments[2]; }",
                     2),
      // Check arguments are mapped in sloppy mode and unmapped in strict.
      std::make_pair("function f(a, b, c, d) {"
                     "  c = b; return arguments[2]; }",
                     1),
      std::make_pair("function f(a, b, c, d) {"
                     "  'use strict'; c = b; return arguments[2]; }",
                     2),
      // Check arguments for duplicate parameters in sloppy mode.
      std::make_pair("function f(a, a, b) { return arguments[1]; }", 1),
      // check rest parameters
      std::make_pair("function f(...restArray) { return restArray[0]; }", 0),
      std::make_pair("function f(a, ...restArray) { return restArray[0]; }", 1),
      std::make_pair("function f(a, ...restArray) { return arguments[0]; }", 0),
      std::make_pair("function f(a, ...restArray) { return arguments[1]; }", 1),
      std::make_pair("function f(a, ...restArray) { return restArray[1]; }", 2),
      std::make_pair("function f(a, ...arguments) { return arguments[0]; }", 1),
      std::make_pair("function f(a, b, ...restArray) { return restArray[0]; }",
                     2),
  };

  // Test passing no arguments.
  for (size_t i = 0; i < arraysize(create_args); i++) {
    InterpreterTester tester(isolate, create_args[i].first);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(return_val.is_identical_to(factory->undefined_value()));
  }

  // Test passing one argument.
  for (size_t i = 0; i < arraysize(create_args); i++) {
    InterpreterTester tester(isolate, create_args[i].first);
    auto callable = tester.GetCallable<Handle<Object>>();
    Handle<Object> return_val =
        callable(handle(Smi::FromInt(40), isolate)).ToHandleChecked();
    if (create_args[i].second == 0) {
      CHECK_EQ(Smi::cast(*return_val), Smi::FromInt(40));
    } else {
      CHECK(return_val.is_identical_to(factory->undefined_value()));
    }
  }

  // Test passing three argument.
  for (size_t i = 0; i < arraysize(create_args); i++) {
    Handle<Object> args[3] = {
        handle(Smi::FromInt(40), isolate),
        handle(Smi::FromInt(60), isolate),
        handle(Smi::FromInt(80), isolate),
    };

    InterpreterTester tester(isolate, create_args[i].first);
    auto callable =
        tester.GetCallable<Handle<Object>, Handle<Object>, Handle<Object>>();
    Handle<Object> return_val =
        callable(args[0], args[1], args[2]).ToHandleChecked();
    CHECK(return_val->SameValue(*args[create_args[i].second]));
  }
}

TEST(InterpreterConditional) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> conditional[] = {
      std::make_pair("return true ? 2 : 3;",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("return false ? 2 : 3;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1; return a ? 20 : 30;",
                     handle(Smi::FromInt(20), isolate)),
      std::make_pair("var a = 1; return a ? 20 : 30;",
                     handle(Smi::FromInt(20), isolate)),
      std::make_pair("var a = 'string'; return a ? 20 : 30;",
                     handle(Smi::FromInt(20), isolate)),
      std::make_pair("var a = undefined; return a ? 20 : 30;",
                     handle(Smi::FromInt(30), isolate)),
      std::make_pair("return 1 ? 2 ? 3 : 4 : 5;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("return 0 ? 2 ? 3 : 4 : 5;",
                     handle(Smi::FromInt(5), isolate)),
  };

  for (size_t i = 0; i < arraysize(conditional); i++) {
    std::string source(InterpreterTester::SourceForBody(conditional[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*conditional[i].second));
  }
}

TEST(InterpreterDelete) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  // Tests for delete for local variables that work both in strict
  // and sloppy modes
  std::pair<const char*, Handle<Object>> test_delete[] = {
      std::make_pair(
          "var a = { x:10, y:'abc', z:30.2}; delete a.x; return a.x;\n",
          factory->undefined_value()),
      std::make_pair(
          "var b = { x:10, y:'abc', z:30.2}; delete b.x; return b.y;\n",
          factory->NewStringFromStaticChars("abc")),
      std::make_pair("var c = { x:10, y:'abc', z:30.2}; var d = c; delete d.x; "
                     "return c.x;\n",
                     factory->undefined_value()),
      std::make_pair("var e = { x:10, y:'abc', z:30.2}; var g = e; delete g.x; "
                     "return e.y;\n",
                     factory->NewStringFromStaticChars("abc")),
      std::make_pair("var a = { x:10, y:'abc', z:30.2};\n"
                     "var b = a;"
                     "delete b.x;"
                     "return b.x;\n",
                     factory->undefined_value()),
      std::make_pair("var a = {1:10};\n"
                     "(function f1() {return a;});"
                     "return delete a[1];",
                     factory->ToBoolean(true)),
      std::make_pair("return delete this;", factory->ToBoolean(true)),
      std::make_pair("return delete 'test';", factory->ToBoolean(true))};

  // Test delete in sloppy mode
  for (size_t i = 0; i < arraysize(test_delete); i++) {
    std::string source(InterpreterTester::SourceForBody(test_delete[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_delete[i].second));
  }

  // Test delete in strict mode
  for (size_t i = 0; i < arraysize(test_delete); i++) {
    std::string strict_test =
        "'use strict'; " + std::string(test_delete[i].first);
    std::string source(InterpreterTester::SourceForBody(strict_test.c_str()));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_delete[i].second));
  }
}

TEST(InterpreterDeleteSloppyUnqualifiedIdentifier) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  // These tests generate a syntax error for strict mode. We don't
  // test for it here.
  std::pair<const char*, Handle<Object>> test_delete[] = {
      std::make_pair("var sloppy_a = { x:10, y:'abc'};\n"
                     "var sloppy_b = delete sloppy_a;\n"
                     "if (delete sloppy_a) {\n"
                     "  return undefined;\n"
                     "} else {\n"
                     "  return sloppy_a.x;\n"
                     "}\n",
                     Handle<Object>(Smi::FromInt(10), isolate)),
      // TODO(mythria) When try-catch is implemented change the tests to check
      // if delete actually deletes
      std::make_pair("sloppy_a = { x:10, y:'abc'};\n"
                     "var sloppy_b = delete sloppy_a;\n"
                     // "try{return a.x;} catch(e) {return b;}\n"
                     "return sloppy_b;",
                     factory->ToBoolean(true)),
      std::make_pair("sloppy_a = { x:10, y:'abc'};\n"
                     "var sloppy_b = delete sloppy_c;\n"
                     "return sloppy_b;",
                     factory->ToBoolean(true))};

  for (size_t i = 0; i < arraysize(test_delete); i++) {
    std::string source(InterpreterTester::SourceForBody(test_delete[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_delete[i].second));
  }
}

TEST(InterpreterGlobalDelete) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> test_global_delete[] = {
      std::make_pair("var a = { x:10, y:'abc', z:30.2 };\n"
                     "function f() {\n"
                     "  delete a.x;\n"
                     "  return a.x;\n"
                     "}\n"
                     "f();\n",
                     factory->undefined_value()),
      std::make_pair("var b = {1:10, 2:'abc', 3:30.2 };\n"
                     "function f() {\n"
                     "  delete b[2];\n"
                     "  return b[1];\n"
                     " }\n"
                     "f();\n",
                     Handle<Object>(Smi::FromInt(10), isolate)),
      std::make_pair("var c = { x:10, y:'abc', z:30.2 };\n"
                     "function f() {\n"
                     "   var d = c;\n"
                     "   delete d.y;\n"
                     "   return d.x;\n"
                     "}\n"
                     "f();\n",
                     Handle<Object>(Smi::FromInt(10), isolate)),
      std::make_pair("e = { x:10, y:'abc' };\n"
                     "function f() {\n"
                     "  return delete e;\n"
                     "}\n"
                     "f();\n",
                     factory->ToBoolean(true)),
      std::make_pair("var g = { x:10, y:'abc' };\n"
                     "function f() {\n"
                     "  return delete g;\n"
                     "}\n"
                     "f();\n",
                     factory->ToBoolean(false)),
      std::make_pair("function f() {\n"
                     "  var obj = {h:10, f1() {return delete this;}};\n"
                     "  return obj.f1();\n"
                     "}\n"
                     "f();",
                     factory->ToBoolean(true)),
      std::make_pair("function f() {\n"
                     "  var obj = {h:10,\n"
                     "             f1() {\n"
                     "              'use strict';\n"
                     "              return delete this.h;}};\n"
                     "  return obj.f1();\n"
                     "}\n"
                     "f();",
                     factory->ToBoolean(true))};

  for (size_t i = 0; i < arraysize(test_global_delete); i++) {
    InterpreterTester tester(isolate, test_global_delete[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*test_global_delete[i].second));
  }
}

TEST(InterpreterBasicLoops) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> loops[] = {
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (a) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "};\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 1; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  --a;\n"
                     "} while(a);\n"
                     "return b;\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var b = 1;\n"
                     "for ( var a = 10; a; a--) {\n"
                     "  b *= 2;\n"
                     "}\n"
                     "return b;",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (a > 0) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "};\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 1; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  --a;\n"
                     "} while(a);\n"
                     "return b;\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var b = 1;\n"
                     "for ( var a = 10; a > 0; a--) {\n"
                     "  b *= 2;\n"
                     "}\n"
                     "return b;",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (false) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "}\n"
                     "return b;\n",
                     Handle<Object>(Smi::FromInt(1), isolate)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "while (true) {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "  if (a == 0) break;"
                     "  continue;"
                     "}\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "  if (a == 0) break;"
                     "} while(true);\n"
                     "return b;\n",
                     factory->NewHeapNumber(1024)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "do {\n"
                     "  b = b * 2;\n"
                     "  a = a - 1;\n"
                     "  if (a == 0) break;"
                     "} while(false);\n"
                     "return b;\n",
                     Handle<Object>(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 10; var b = 1;\n"
                     "for ( a = 1, b = 30; false; ) {\n"
                     "  b = b * 2;\n"
                     "}\n"
                     "return b;\n",
                     Handle<Object>(Smi::FromInt(30), isolate))};

  for (size_t i = 0; i < arraysize(loops); i++) {
    std::string source(InterpreterTester::SourceForBody(loops[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*loops[i].second));
  }
}

TEST(InterpreterForIn) {
  std::pair<const char*, int> for_in_samples[] = {
      {"var r = -1;\n"
       "for (var a in null) { r = a; }\n"
       "return r;\n",
       -1},
      {"var r = -1;\n"
       "for (var a in undefined) { r = a; }\n"
       "return r;\n",
       -1},
      {"var r = 0;\n"
       "for (var a in [0,6,7,9]) { r = r + (1 << a); }\n"
       "return r;\n",
       0xF},
      {"var r = 0;\n"
       "for (var a in [0,6,7,9]) { r = r + (1 << a); }\n"
       "var r = 0;\n"
       "for (var a in [0,6,7,9]) { r = r + (1 << a); }\n"
       "return r;\n",
       0xF},
      {"var r = 0;\n"
       "for (var a in 'foobar') { r = r + (1 << a); }\n"
       "return r;\n",
       0x3F},
      {"var r = 0;\n"
       "for (var a in {1:0, 10:1, 100:2, 1000:3}) {\n"
       "  r = r + Number(a);\n"
       " }\n"
       " return r;\n",
       1111},
      {"var r = 0;\n"
       "var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "for (var a in data) {\n"
       "  if (a == 1) delete data[1];\n"
       "  r = r + Number(a);\n"
       " }\n"
       " return r;\n",
       1111},
      {"var r = 0;\n"
       "var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "for (var a in data) {\n"
       "  if (a == 10) delete data[100];\n"
       "  r = r + Number(a);\n"
       " }\n"
       " return r;\n",
       1011},
      {"var r = 0;\n"
       "var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "for (var a in data) {\n"
       "  if (a == 10) data[10000] = 4;\n"
       "  r = r + Number(a);\n"
       " }\n"
       " return r;\n",
       1111},
      {"var r = 0;\n"
       "var input = 'foobar';\n"
       "for (var a in input) {\n"
       "  if (input[a] == 'b') break;\n"
       "  r = r + (1 << a);\n"
       "}\n"
       "return r;\n",
       0x7},
      {"var r = 0;\n"
       "var input = 'foobar';\n"
       "for (var a in input) {\n"
       " if (input[a] == 'b') continue;\n"
       " r = r + (1 << a);\n"
       "}\n"
       "return r;\n",
       0x37},
      {"var r = 0;\n"
       "var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "for (var a in data) {\n"
       "  if (a == 10) {\n"
       "     data[10000] = 4;\n"
       "  }\n"
       "  r = r + Number(a);\n"
       "}\n"
       "return r;\n",
       1111},
      {"var r = [ 3 ];\n"
       "var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "for (r[10] in data) {\n"
       "}\n"
       "return Number(r[10]);\n",
       1000},
      {"var r = [ 3 ];\n"
       "var data = {1:0, 10:1, 100:2, 1000:3};\n"
       "for (r['100'] in data) {\n"
       "}\n"
       "return Number(r['100']);\n",
       1000},
      {"var obj = {}\n"
       "var descObj = new Boolean(false);\n"
       "var accessed = 0;\n"
       "descObj.enumerable = true;\n"
       "Object.defineProperties(obj, { prop:descObj });\n"
       "for (var p in obj) {\n"
       "  if (p === 'prop') { accessed = 1; }\n"
       "}\n"
       "return accessed;",
       1},
      {"var appointment = {};\n"
       "Object.defineProperty(appointment, 'startTime', {\n"
       "    value: 1001,\n"
       "    writable: false,\n"
       "    enumerable: false,\n"
       "    configurable: true\n"
       "});\n"
       "Object.defineProperty(appointment, 'name', {\n"
       "    value: 'NAME',\n"
       "    writable: false,\n"
       "    enumerable: false,\n"
       "    configurable: true\n"
       "});\n"
       "var meeting = Object.create(appointment);\n"
       "Object.defineProperty(meeting, 'conferenceCall', {\n"
       "    value: 'In-person meeting',\n"
       "    writable: false,\n"
       "    enumerable: false,\n"
       "    configurable: true\n"
       "});\n"
       "\n"
       "var teamMeeting = Object.create(meeting);\n"
       "\n"
       "var flags = 0;\n"
       "for (var p in teamMeeting) {\n"
       "    if (p === 'startTime') {\n"
       "        flags |= 1;\n"
       "    }\n"
       "    if (p === 'name') {\n"
       "        flags |= 2;\n"
       "    }\n"
       "    if (p === 'conferenceCall') {\n"
       "        flags |= 4;\n"
       "    }\n"
       "}\n"
       "\n"
       "var hasOwnProperty = !teamMeeting.hasOwnProperty('name') &&\n"
       "    !teamMeeting.hasOwnProperty('startTime') &&\n"
       "    !teamMeeting.hasOwnProperty('conferenceCall');\n"
       "if (!hasOwnProperty) {\n"
       "    flags |= 8;\n"
       "}\n"
       "return flags;\n",
       0},
      {"var data = {x:23, y:34};\n"
       " var result = 0;\n"
       "var o = {};\n"
       "var arr = [o];\n"
       "for (arr[0].p in data)\n"       // This is to test if value is loaded
       "  result += data[arr[0].p];\n"  // back from accumulator before storing
       "return result;\n",              // named properties.
       57},
      {"var data = {x:23, y:34};\n"
       "var result = 0;\n"
       "var o = {};\n"
       "var i = 0;\n"
       "for (o[i++] in data)\n"       // This is to test if value is loaded
       "  result += data[o[i-1]];\n"  // back from accumulator before
       "return result;\n",            // storing keyed properties.
       57}};

  // Two passes are made for this test. On the first, 8-bit register
  // operands are employed, and on the 16-bit register operands are
  // used.
  for (int pass = 0; pass < 2; pass++) {
    HandleAndZoneScope handles;
    Isolate* isolate = handles.main_isolate();
    std::ostringstream wide_os;
    if (pass == 1) {
      for (int i = 0; i < 200; i++) {
        wide_os << "var local" << i << " = 0;\n";
      }
    }

    for (size_t i = 0; i < arraysize(for_in_samples); i++) {
      std::ostringstream body_os;
      body_os << wide_os.str() << for_in_samples[i].first;
      std::string body(body_os.str());
      std::string function = InterpreterTester::SourceForBody(body.c_str());
      InterpreterTester tester(isolate, function.c_str());
      auto callable = tester.GetCallable<>();
      Handle<Object> return_val = callable().ToHandleChecked();
      CHECK_EQ(Handle<Smi>::cast(return_val)->value(),
               for_in_samples[i].second);
    }
  }
}

TEST(InterpreterForOf) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> for_of[] = {
      {"function f() {\n"
       "  var r = 0;\n"
       "  for (var a of [0,6,7,9]) { r += a; }\n"
       "  return r;\n"
       "}",
       handle(Smi::FromInt(22), isolate)},
      {"function f() {\n"
       "  var r = '';\n"
       "  for (var a of 'foobar') { r = a + r; }\n"
       "  return r;\n"
       "}",
       factory->NewStringFromStaticChars("raboof")},
      {"function f() {\n"
       "  var a = [1, 2, 3];\n"
       "  a.name = 4;\n"
       "  var r = 0;\n"
       "  for (var x of a) { r += x; }\n"
       "  return r;\n"
       "}",
       handle(Smi::FromInt(6), isolate)},
      {"function f() {\n"
       "  var r = '';\n"
       "  var data = [1, 2, 3]; \n"
       "  for (a of data) { delete data[0]; r += a; } return r; }",
       factory->NewStringFromStaticChars("123")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var data = [1, 2, 3]; \n"
       "  for (a of data) { delete data[2]; r += a; } return r; }",
       factory->NewStringFromStaticChars("12undefined")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var data = [1, 2, 3]; \n"
       "  for (a of data) { delete data; r += a; } return r; }",
       factory->NewStringFromStaticChars("123")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var input = 'foobar';\n"
       "  for (var a of input) {\n"
       "    if (a == 'b') break;\n"
       "    r += a;\n"
       "  }\n"
       "  return r;\n"
       "}",
       factory->NewStringFromStaticChars("foo")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var input = 'foobar';\n"
       "  for (var a of input) {\n"
       "    if (a == 'b') continue;\n"
       "    r += a;\n"
       "  }\n"
       "  return r;\n"
       "}",
       factory->NewStringFromStaticChars("fooar")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var data = [1, 2, 3, 4]; \n"
       "  for (a of data) { data[2] = 567; r += a; }\n"
       "  return r;\n"
       "}",
       factory->NewStringFromStaticChars("125674")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var data = [1, 2, 3, 4]; \n"
       "  for (a of data) { data[4] = 567; r += a; }\n"
       "  return r;\n"
       "}",
       factory->NewStringFromStaticChars("1234567")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var data = [1, 2, 3, 4]; \n"
       "  for (a of data) { data[5] = 567; r += a; }\n"
       "  return r;\n"
       "}",
       factory->NewStringFromStaticChars("1234undefined567")},
      {"function f() {\n"
       "  var r = '';\n"
       "  var obj = new Object();\n"
       "  obj[Symbol.iterator] = function() { return {\n"
       "    index: 3,\n"
       "    data: ['a', 'b', 'c', 'd'],"
       "    next: function() {"
       "      return {"
       "        done: this.index == -1,\n"
       "        value: this.index < 0 ? undefined : this.data[this.index--]\n"
       "      }\n"
       "    }\n"
       "    }}\n"
       "  for (a of obj) { r += a }\n"
       "  return r;\n"
       "}",
       factory->NewStringFromStaticChars("dcba")},
  };

  for (size_t i = 0; i < arraysize(for_of); i++) {
    InterpreterTester tester(isolate, for_of[i].first);
    auto callable = tester.GetCallable<>();
    Handle<Object> return_val = callable().ToHandleChecked();
    CHECK(return_val->SameValue(*for_of[i].second));
  }
}

TEST(InterpreterSwitch) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> switch_ops[] = {
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 1: return 2;\n"
                     " case 2: return 3;\n"
                     "}\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 2: a = 2; break;\n"
                     " case 1: a = 3; break;\n"
                     "}\n"
                     "return a;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 1: a = 2; // fall-through\n"
                     " case 2: a = 3; break;\n"
                     "}\n"
                     "return a;",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 100;\n"
                     "switch(a) {\n"
                     " case 1: return 100;\n"
                     " case 2: return 200;\n"
                     "}\n"
                     "return undefined;",
                     factory->undefined_value()),
      std::make_pair("var a = 100;\n"
                     "switch(a) {\n"
                     " case 1: return 100;\n"
                     " case 2: return 200;\n"
                     " default: return 300;\n"
                     "}\n"
                     "return undefined;",
                     handle(Smi::FromInt(300), isolate)),
      std::make_pair("var a = 100;\n"
                     "switch(typeof(a)) {\n"
                     " case 'string': return 1;\n"
                     " case 'number': return 2;\n"
                     " default: return 3;\n"
                     "}\n",
                     handle(Smi::FromInt(2), isolate)),
      std::make_pair("var a = 100;\n"
                     "switch(a) {\n"
                     " case a += 20: return 1;\n"
                     " case a -= 10: return 2;\n"
                     " case a -= 10: return 3;\n"
                     " default: return 3;\n"
                     "}\n",
                     handle(Smi::FromInt(3), isolate)),
      std::make_pair("var a = 1;\n"
                     "switch(a) {\n"
                     " case 1: \n"
                     "   switch(a + 1) {\n"
                     "      case 2 : a += 1; break;\n"
                     "      default : a += 2; break;\n"
                     "   }  // fall-through\n"
                     " case 2: a += 3;\n"
                     "}\n"
                     "return a;",
                     handle(Smi::FromInt(5), isolate)),
  };

  for (size_t i = 0; i < arraysize(switch_ops); i++) {
    std::string source(InterpreterTester::SourceForBody(switch_ops[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*switch_ops[i].second));
  }
}

TEST(InterpreterSloppyThis) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> sloppy_this[] = {
      std::make_pair("var global_val = 100;\n"
                     "function f() { return this.global_val; }\n",
                     handle(Smi::FromInt(100), isolate)),
      std::make_pair("var global_val = 110;\n"
                     "function g() { return this.global_val; };"
                     "function f() { return g(); }\n",
                     handle(Smi::FromInt(110), isolate)),
      std::make_pair("var global_val = 110;\n"
                     "function g() { return this.global_val };"
                     "function f() { 'use strict'; return g(); }\n",
                     handle(Smi::FromInt(110), isolate)),
      std::make_pair("function f() { 'use strict'; return this; }\n",
                     factory->undefined_value()),
      std::make_pair("function g() { 'use strict'; return this; };"
                     "function f() { return g(); }\n",
                     factory->undefined_value()),
  };

  for (size_t i = 0; i < arraysize(sloppy_this); i++) {
    InterpreterTester tester(isolate, sloppy_this[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*sloppy_this[i].second));
  }
}

TEST(InterpreterThisFunction) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  InterpreterTester tester(isolate,
                           "var f;\n f = function f() { return f.name; }");
  auto callable = tester.GetCallable<>();

  Handle<i::Object> return_value = callable().ToHandleChecked();
  CHECK(return_value->SameValue(*factory->NewStringFromStaticChars("f")));
}

TEST(InterpreterNewTarget) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  // TODO(rmcilroy): Add tests that we get the original constructor for
  // superclass constructors once we have class support.
  InterpreterTester tester(isolate, "function f() { this.a = new.target; }");
  auto callable = tester.GetCallable<>();
  callable().ToHandleChecked();

  Handle<Object> new_target_name = v8::Utils::OpenHandle(
      *CompileRun("(function() { return (new f()).a.name; })();"));
  CHECK(new_target_name->SameValue(*factory->NewStringFromStaticChars("f")));
}

TEST(InterpreterAssignmentInExpressions) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, int> samples[] = {
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = x + (x = 1) + (x = 2);\n"
       "  return y;\n"
       "}",
       10},
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = x + (x = 1) + (x = 2);\n"
       "  return x;\n"
       "}",
       2},
      {"function f() {\n"
       "  var x = 55;\n"
       "  x = x + (x = 100) + (x = 101);\n"
       "  return x;\n"
       "}",
       256},
      {"function f() {\n"
       "  var x = 7;\n"
       "  return ++x + x + x++;\n"
       "}",
       24},
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = 1 + ++x + x + x++;\n"
       "  return x;\n"
       "}",
       9},
      {"function f() {\n"
       "  var x = 7;\n"
       "  var y = ++x + x + x++;\n"
       "  return x;\n"
       "}",
       9},
      {"function f() {\n"
       "  var x = 7, y = 100, z = 1000;\n"
       "  return x + (x += 3) + y + (y *= 10) + (z *= 7) + z;\n"
       "}",
       15117},
      {"function f() {\n"
       "  var inner = function (x) { return x + (x = 2) + (x = 4) + x; };\n"
       "  return inner(1);\n"
       "}",
       11},
      {"function f() {\n"
       "  var x = 1, y = 2;\n"
       "  x = x + (x = 3) + y + (y = 4), y = y + (y = 5) + y + x;\n"
       "  return x + y;\n"
       "}",
       10 + 24},
      {"function f() {\n"
       "  var x = 0;\n"
       "  var y = x | (x = 1) | (x = 2);\n"
       "  return x;\n"
       "}",
       2},
      {"function f() {\n"
       "  var x = 0;\n"
       "  var y = x || (x = 1);\n"
       "  return x;\n"
       "}",
       1},
      {"function f() {\n"
       "  var x = 1;\n"
       "  var y = x && (x = 2) && (x = 3);\n"
       "  return x;\n"
       "}",
       3},
      {"function f() {\n"
       "  var x = 1;\n"
       "  var y = x || (x = 2);\n"
       "  return x;\n"
       "}",
       1},
      {"function f() {\n"
       "  var x = 1;\n"
       "  x = (x << (x = 3)) | (x = 16);\n"
       "  return x;\n"
       "}",
       24},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  var u = r + s + t + (r = 10) + (s = 20) +"
       "          (t = (r + s)) + r + s + t;\n"
       "  return r + s + t + u;\n"
       "}",
       211},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  return r > (3 * s * (s = 1)) ? (t + (t += 1)) : (r + (r = 4));\n"
       "}",
       11},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  return r > (3 * s * (s = 0)) ? (t + (t += 1)) : (r + (r = 4));\n"
       "}",
       27},
      {"function f() {\n"
       "  var r = 7;\n"
       "  var s = 11;\n"
       "  var t = 13;\n"
       "  return (r + (r = 5)) > s ? r : t;\n"
       "}",
       5},
      {"function f(a) {\n"
       "  return a + (arguments[0] = 10);\n"
       "}",
       50},
      {"function f(a) {\n"
       "  return a + (arguments[0] = 10) + a;\n"
       "}",
       60},
      {"function f(a) {\n"
       "  return a + (arguments[0] = 10) + arguments[0];\n"
       "}",
       60},
  };

  const int arg_value = 40;
  for (size_t i = 0; i < arraysize(samples); i++) {
    InterpreterTester tester(isolate, samples[i].first);
    auto callable = tester.GetCallable<Handle<Object>>();
    Handle<Object> return_val =
        callable(handle(Smi::FromInt(arg_value), handles.main_isolate()))
            .ToHandleChecked();
    CHECK_EQ(Handle<Smi>::cast(return_val)->value(), samples[i].second);
  }
}

TEST(InterpreterToName) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> to_name_tests[] = {
      {"var a = 'val'; var obj = {[a] : 10}; return obj.val;",
       factory->NewNumberFromInt(10)},
      {"var a = 20; var obj = {[a] : 10}; return obj['20'];",
       factory->NewNumberFromInt(10)},
      {"var a = 20; var obj = {[a] : 10}; return obj[20];",
       factory->NewNumberFromInt(10)},
      {"var a = {val:23}; var obj = {[a] : 10}; return obj[a];",
       factory->NewNumberFromInt(10)},
      {"var a = {val:23}; var obj = {[a] : 10};\n"
       "return obj['[object Object]'];",
       factory->NewNumberFromInt(10)},
      {"var a = {toString : function() { return 'x'}};\n"
       "var obj = {[a] : 10};\n"
       "return obj.x;",
       factory->NewNumberFromInt(10)},
      {"var a = {valueOf : function() { return 'x'}};\n"
       "var obj = {[a] : 10};\n"
       "return obj.x;",
       factory->undefined_value()},
      {"var a = {[Symbol.toPrimitive] : function() { return 'x'}};\n"
       "var obj = {[a] : 10};\n"
       "return obj.x;",
       factory->NewNumberFromInt(10)},
  };

  for (size_t i = 0; i < arraysize(to_name_tests); i++) {
    std::string source(
        InterpreterTester::SourceForBody(to_name_tests[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*to_name_tests[i].second));
  }
}

TEST(TemporaryRegisterAllocation) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> reg_tests[] = {
      {"function add(a, b, c) {"
       "   return a + b + c;"
       "}"
       "function f() {"
       "  var a = 10, b = 10;"
       "   return add(a, b++, b);"
       "}",
       factory->NewNumberFromInt(31)},
      {"function add(a, b, c, d) {"
       "  return a + b + c + d;"
       "}"
       "function f() {"
       "  var x = 10, y = 20, z = 30;"
       "  return x + add(x, (y= x++), x, z);"
       "}",
       factory->NewNumberFromInt(71)},
  };

  for (size_t i = 0; i < arraysize(reg_tests); i++) {
    InterpreterTester tester(isolate, reg_tests[i].first);
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*reg_tests[i].second));
  }
}

TEST(InterpreterLookupSlot) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  // TODO(mythria): Add more tests when we have support for eval/with.
  const char* function_prologue = "var f;"
                                  "var x = 1;"
                                  "function f1() {"
                                  "  eval(\"function t() {";
  const char* function_epilogue = "        }; f = t;\");"
                                  "}"
                                  "f1();";


  std::pair<const char*, Handle<Object>> lookup_slot[] = {
      {"return x;", handle(Smi::FromInt(1), isolate)},
      {"return typeof x;", factory->NewStringFromStaticChars("number")},
      {"return typeof dummy;", factory->NewStringFromStaticChars("undefined")},
      {"x = 10; return x;", handle(Smi::FromInt(10), isolate)},
      {"'use strict'; x = 20; return x;", handle(Smi::FromInt(20), isolate)},
  };

  for (size_t i = 0; i < arraysize(lookup_slot); i++) {
    std::string script = std::string(function_prologue) +
                         std::string(lookup_slot[i].first) +
                         std::string(function_epilogue);

    InterpreterTester tester(isolate, script.c_str(), "t");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*lookup_slot[i].second));
  }
}

TEST(InterpreterLookupContextSlot) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* inner_function_prologue = "function inner() {";
  const char* inner_function_epilogue = "};";
  const char* outer_function_epilogue = "return inner();";

  std::tuple<const char*, const char*, Handle<Object>> lookup_slot[] = {
      // Eval in inner context.
      std::make_tuple("var x = 0;", "eval(''); return x;",
                      handle(Smi::kZero, isolate)),
      std::make_tuple("var x = 0;", "eval('var x = 1'); return x;",
                      handle(Smi::FromInt(1), isolate)),
      std::make_tuple("var x = 0;",
                      "'use strict'; eval('var x = 1'); return x;",
                      handle(Smi::kZero, isolate)),
      // Eval in outer context.
      std::make_tuple("var x = 0; eval('');", "return x;",
                      handle(Smi::kZero, isolate)),
      std::make_tuple("var x = 0; eval('var x = 1');", "return x;",
                      handle(Smi::FromInt(1), isolate)),
      std::make_tuple("'use strict'; var x = 0; eval('var x = 1');",
                      "return x;", handle(Smi::kZero, isolate)),
  };

  for (size_t i = 0; i < arraysize(lookup_slot); i++) {
    std::string body = std::string(std::get<0>(lookup_slot[i])) +
                       std::string(inner_function_prologue) +
                       std::string(std::get<1>(lookup_slot[i])) +
                       std::string(inner_function_epilogue) +
                       std::string(outer_function_epilogue);
    std::string script = InterpreterTester::SourceForBody(body.c_str());

    InterpreterTester tester(isolate, script.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*std::get<2>(lookup_slot[i])));
  }
}

TEST(InterpreterLookupGlobalSlot) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* inner_function_prologue = "function inner() {";
  const char* inner_function_epilogue = "};";
  const char* outer_function_epilogue = "return inner();";

  std::tuple<const char*, const char*, Handle<Object>> lookup_slot[] = {
      // Eval in inner context.
      std::make_tuple("x = 0;", "eval(''); return x;",
                      handle(Smi::kZero, isolate)),
      std::make_tuple("x = 0;", "eval('var x = 1'); return x;",
                      handle(Smi::FromInt(1), isolate)),
      std::make_tuple("x = 0;", "'use strict'; eval('var x = 1'); return x;",
                      handle(Smi::kZero, isolate)),
      // Eval in outer context.
      std::make_tuple("x = 0; eval('');", "return x;",
                      handle(Smi::kZero, isolate)),
      std::make_tuple("x = 0; eval('var x = 1');", "return x;",
                      handle(Smi::FromInt(1), isolate)),
      std::make_tuple("'use strict'; x = 0; eval('var x = 1');", "return x;",
                      handle(Smi::kZero, isolate)),
  };

  for (size_t i = 0; i < arraysize(lookup_slot); i++) {
    std::string body = std::string(std::get<0>(lookup_slot[i])) +
                       std::string(inner_function_prologue) +
                       std::string(std::get<1>(lookup_slot[i])) +
                       std::string(inner_function_epilogue) +
                       std::string(outer_function_epilogue);
    std::string script = InterpreterTester::SourceForBody(body.c_str());

    InterpreterTester tester(isolate, script.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*std::get<2>(lookup_slot[i])));
  }
}

TEST(InterpreterCallLookupSlot) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> call_lookup[] = {
      {"g = function(){ return 2 }; eval(''); return g();",
       handle(Smi::FromInt(2), isolate)},
      {"g = function(){ return 2 }; eval('g = function() {return 3}');\n"
       "return g();",
       handle(Smi::FromInt(3), isolate)},
      {"g = { x: function(){ return this.y }, y: 20 };\n"
       "eval('g = { x: g.x, y: 30 }');\n"
       "return g.x();",
       handle(Smi::FromInt(30), isolate)},
  };

  for (size_t i = 0; i < arraysize(call_lookup); i++) {
    std::string source(InterpreterTester::SourceForBody(call_lookup[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*call_lookup[i].second));
  }
}

TEST(InterpreterLookupSlotWide) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  const char* function_prologue =
      "var f;"
      "var x = 1;"
      "function f1() {"
      "  eval(\"function t() {";
  const char* function_epilogue =
      "        }; f = t;\");"
      "}"
      "f1();";
  std::ostringstream str;
  str << "var y = 2.3;";
  for (int i = 1; i < 256; i++) {
    str << "y = " << 2.3 + i << ";";
  }
  std::string init_function_body = str.str();

  std::pair<std::string, Handle<Object>> lookup_slot[] = {
      {init_function_body + "return x;", handle(Smi::FromInt(1), isolate)},
      {init_function_body + "return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {init_function_body + "return x = 10;",
       handle(Smi::FromInt(10), isolate)},
      {"'use strict';" + init_function_body + "x = 20; return x;",
       handle(Smi::FromInt(20), isolate)},
  };

  for (size_t i = 0; i < arraysize(lookup_slot); i++) {
    std::string script = std::string(function_prologue) + lookup_slot[i].first +
                         std::string(function_epilogue);

    InterpreterTester tester(isolate, script.c_str(), "t");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*lookup_slot[i].second));
  }
}

TEST(InterpreterDeleteLookupSlot) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  // TODO(mythria): Add more tests when we have support for eval/with.
  const char* function_prologue = "var f;"
                                  "var x = 1;"
                                  "y = 10;"
                                  "var obj = {val:10};"
                                  "var z = 30;"
                                  "function f1() {"
                                  "  var z = 20;"
                                  "  eval(\"function t() {";
  const char* function_epilogue = "        }; f = t;\");"
                                  "}"
                                  "f1();";


  std::pair<const char*, Handle<Object>> delete_lookup_slot[] = {
      {"return delete x;", factory->false_value()},
      {"return delete y;", factory->true_value()},
      {"return delete z;", factory->false_value()},
      {"return delete obj.val;", factory->true_value()},
      {"'use strict'; return delete obj.val;", factory->true_value()},
  };

  for (size_t i = 0; i < arraysize(delete_lookup_slot); i++) {
    std::string script = std::string(function_prologue) +
                         std::string(delete_lookup_slot[i].first) +
                         std::string(function_epilogue);

    InterpreterTester tester(isolate, script.c_str(), "t");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*delete_lookup_slot[i].second));
  }
}

TEST(JumpWithConstantsAndWideConstants) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();
  const int kStep = 13;
  for (int constants = 11; constants < 256 + 3 * kStep; constants += kStep) {
    std::ostringstream filler_os;
    // Generate a string that consumes constant pool entries and
    // spread out branch distances in script below.
    for (int i = 0; i < constants; i++) {
      filler_os << "var x_ = 'x_" << i << "';\n";
    }
    std::string filler(filler_os.str());
    std::ostringstream script_os;
    script_os << "function " << InterpreterTester::function_name() << "(a) {\n";
    script_os << "  " << filler;
    script_os << "  for (var i = a; i < 2; i++) {\n";
    script_os << "  " << filler;
    script_os << "    if (i == 0) { " << filler << "i = 10; continue; }\n";
    script_os << "    else if (i == a) { " << filler << "i = 12; break; }\n";
    script_os << "    else { " << filler << " }\n";
    script_os << "  }\n";
    script_os << "  return i;\n";
    script_os << "}\n";
    std::string script(script_os.str());
    for (int a = 0; a < 3; a++) {
      InterpreterTester tester(isolate, script.c_str());
      auto callable = tester.GetCallable<Handle<Object>>();
      Handle<Object> argument = factory->NewNumberFromInt(a);
      Handle<Object> return_val = callable(argument).ToHandleChecked();
      static const int results[] = {11, 12, 2};
      CHECK_EQ(Handle<Smi>::cast(return_val)->value(), results[a]);
    }
  }
}

TEST(InterpreterEval) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> eval[] = {
      {"return eval('1;');", handle(Smi::FromInt(1), isolate)},
      {"return eval('100 * 20;');", handle(Smi::FromInt(2000), isolate)},
      {"var x = 10; return eval('x + 20;');",
       handle(Smi::FromInt(30), isolate)},
      {"var x = 10; eval('x = 33;'); return x;",
       handle(Smi::FromInt(33), isolate)},
      {"'use strict'; var x = 20; var z = 0;\n"
       "eval('var x = 33; z = x;'); return x + z;",
       handle(Smi::FromInt(53), isolate)},
      {"eval('var x = 33;'); eval('var y = x + 20'); return x + y;",
       handle(Smi::FromInt(86), isolate)},
      {"var x = 1; eval('for(i = 0; i < 10; i++) x = x + 1;'); return x",
       handle(Smi::FromInt(11), isolate)},
      {"var x = 10; eval('var x = 20;'); return x;",
       handle(Smi::FromInt(20), isolate)},
      {"var x = 1; eval('\"use strict\"; var x = 2;'); return x;",
       handle(Smi::FromInt(1), isolate)},
      {"'use strict'; var x = 1; eval('var x = 2;'); return x;",
       handle(Smi::FromInt(1), isolate)},
      {"var x = 10; eval('x + 20;'); return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {"eval('var y = 10;'); return typeof unallocated;",
       factory->NewStringFromStaticChars("undefined")},
      {"'use strict'; eval('var y = 10;'); return typeof unallocated;",
       factory->NewStringFromStaticChars("undefined")},
      {"eval('var x = 10;'); return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {"var x = {}; eval('var x = 10;'); return typeof x;",
       factory->NewStringFromStaticChars("number")},
      {"'use strict'; var x = {}; eval('var x = 10;'); return typeof x;",
       factory->NewStringFromStaticChars("object")},
  };

  for (size_t i = 0; i < arraysize(eval); i++) {
    std::string source(InterpreterTester::SourceForBody(eval[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();
    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*eval[i].second));
  }
}

TEST(InterpreterEvalParams) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> eval_params[] = {
      {"var x = 10; return eval('x + p1;');",
       handle(Smi::FromInt(30), isolate)},
      {"var x = 10; eval('p1 = x;'); return p1;",
       handle(Smi::FromInt(10), isolate)},
      {"var a = 10;"
       "function inner() { return eval('a + p1;');}"
       "return inner();",
       handle(Smi::FromInt(30), isolate)},
  };

  for (size_t i = 0; i < arraysize(eval_params); i++) {
    std::string source = "function " + InterpreterTester::function_name() +
                         "(p1) {" + eval_params[i].first + "}";
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<Handle<Object>>();

    Handle<i::Object> return_value =
        callable(handle(Smi::FromInt(20), isolate)).ToHandleChecked();
    CHECK(return_value->SameValue(*eval_params[i].second));
  }
}

TEST(InterpreterEvalGlobal) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> eval_global[] = {
      {"function add_global() { eval('function test() { z = 33; }; test()'); };"
       "function f() { add_global(); return z; }; f();",
       handle(Smi::FromInt(33), isolate)},
      {"function add_global() {\n"
       " eval('\"use strict\"; function test() { y = 33; };"
       "      try { test() } catch(e) {}');\n"
       "}\n"
       "function f() { add_global(); return typeof y; } f();",
       factory->NewStringFromStaticChars("undefined")},
  };

  for (size_t i = 0; i < arraysize(eval_global); i++) {
    InterpreterTester tester(isolate, eval_global[i].first, "test");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*eval_global[i].second));
  }
}

TEST(InterpreterEvalVariableDecl) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> eval_global[] = {
      {"function f() { eval('var x = 10; x++;'); return x; }",
       handle(Smi::FromInt(11), isolate)},
      {"function f() { var x = 20; eval('var x = 10; x++;'); return x; }",
       handle(Smi::FromInt(11), isolate)},
      {"function f() {"
       " var x = 20;"
       " eval('\"use strict\"; var x = 10; x++;');"
       " return x; }",
       handle(Smi::FromInt(20), isolate)},
      {"function f() {"
       " var y = 30;"
       " eval('var x = {1:20}; x[2]=y;');"
       " return x[2]; }",
       handle(Smi::FromInt(30), isolate)},
      {"function f() {"
       " eval('var x = {name:\"test\"};');"
       " return x.name; }",
       factory->NewStringFromStaticChars("test")},
      {"function f() {"
       "  eval('var x = [{name:\"test\"}, {type:\"cc\"}];');"
       "  return x[1].type+x[0].name; }",
       factory->NewStringFromStaticChars("cctest")},
      {"function f() {\n"
       " var x = 3;\n"
       " var get_eval_x;\n"
       " eval('\"use strict\"; "
       "      var x = 20; "
       "      get_eval_x = function func() {return x;};');\n"
       " return get_eval_x() + x;\n"
       "}",
       handle(Smi::FromInt(23), isolate)},
      // TODO(mythria): Add tests with const declarations.
  };

  for (size_t i = 0; i < arraysize(eval_global); i++) {
    InterpreterTester tester(isolate, eval_global[i].first, "*");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*eval_global[i].second));
  }
}

TEST(InterpreterEvalFunctionDecl) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> eval_func_decl[] = {
      {"function f() {\n"
       " var x = 3;\n"
       " eval('var x = 20;"
       "       function get_x() {return x;};');\n"
       " return get_x() + x;\n"
       "}",
       handle(Smi::FromInt(40), isolate)},
  };

  for (size_t i = 0; i < arraysize(eval_func_decl); i++) {
    InterpreterTester tester(isolate, eval_func_decl[i].first, "*");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*eval_func_decl[i].second));
  }
}

TEST(InterpreterWideRegisterArithmetic) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  static const size_t kMaxRegisterForTest = 150;
  std::ostringstream os;
  os << "function " << InterpreterTester::function_name() << "(arg) {\n";
  os << "  var retval = -77;\n";
  for (size_t i = 0; i < kMaxRegisterForTest; i++) {
    os << "  var x" << i << " = " << i << ";\n";
  }
  for (size_t i = 0; i < kMaxRegisterForTest / 2; i++) {
    size_t j = kMaxRegisterForTest - i - 1;
    os << "  var tmp = x" << j << ";\n";
    os << "  var x" << j << " = x" << i << ";\n";
    os << "  var x" << i << " = tmp;\n";
  }
  for (size_t i = 0; i < kMaxRegisterForTest / 2; i++) {
    size_t j = kMaxRegisterForTest - i - 1;
    os << "  var tmp = x" << j << ";\n";
    os << "  var x" << j << " = x" << i << ";\n";
    os << "  var x" << i << " = tmp;\n";
  }
  for (size_t i = 0; i < kMaxRegisterForTest; i++) {
    os << "  if (arg == " << i << ") {\n"  //
       << "    retval = x" << i << ";\n"   //
       << "  }\n";                         //
  }
  os << "  return retval;\n";
  os << "}\n";

  std::string source = os.str();
  InterpreterTester tester(isolate, source.c_str());
  auto callable = tester.GetCallable<Handle<Object>>();
  for (size_t i = 0; i < kMaxRegisterForTest; i++) {
    Handle<Object> arg = handle(Smi::FromInt(static_cast<int>(i)), isolate);
    Handle<Object> return_value = callable(arg).ToHandleChecked();
    CHECK(return_value->SameValue(*arg));
  }
}

TEST(InterpreterCallWideRegisters) {
  static const int kPeriod = 25;
  static const int kLength = 512;
  static const int kStartChar = 65;

  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  for (int pass = 0; pass < 3; pass += 1) {
    std::ostringstream os;
    for (int i = 0; i < pass * 97; i += 1) {
      os << "var x" << i << " = " << i << "\n";
    }
    os << "return String.fromCharCode(";
    os << kStartChar;
    for (int i = 1; i < kLength; i += 1) {
      os << "," << kStartChar + (i % kPeriod);
    }
    os << ");";
    std::string source = InterpreterTester::SourceForBody(os.str().c_str());
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable();
    Handle<Object> return_val = callable().ToHandleChecked();
    Handle<String> return_string = Handle<String>::cast(return_val);
    CHECK_EQ(return_string->length(), kLength);
    for (int i = 0; i < kLength; i += 1) {
      CHECK_EQ(return_string->Get(i), 65 + (i % kPeriod));
    }
  }
}

TEST(InterpreterWideParametersPickOne) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  static const int kParameterCount = 130;
  for (int parameter = 0; parameter < 10; parameter++) {
    std::ostringstream os;
    os << "function " << InterpreterTester::function_name() << "(arg) {\n";
    os << "  function selector(i";
    for (int i = 0; i < kParameterCount; i++) {
      os << ","
         << "a" << i;
    }
    os << ") {\n";
    os << "  return a" << parameter << ";\n";
    os << "  };\n";
    os << "  return selector(arg";
    for (int i = 0; i < kParameterCount; i++) {
      os << "," << i;
    }
    os << ");";
    os << "}\n";

    std::string source = os.str();
    InterpreterTester tester(isolate, source.c_str(), "*");
    auto callable = tester.GetCallable<Handle<Object>>();
    Handle<Object> arg = handle(Smi::FromInt(0xAA55), isolate);
    Handle<Object> return_value = callable(arg).ToHandleChecked();
    Handle<Smi> actual = Handle<Smi>::cast(return_value);
    CHECK_EQ(actual->value(), parameter);
  }
}

TEST(InterpreterWideParametersSummation) {
  static int kParameterCount = 200;
  static int kBaseValue = 17000;
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  std::ostringstream os;
  os << "function " << InterpreterTester::function_name() << "(arg) {\n";
  os << "  function summation(i";
  for (int i = 0; i < kParameterCount; i++) {
    os << ","
       << "a" << i;
  }
  os << ") {\n";
  os << "    var sum = " << kBaseValue << ";\n";
  os << "    switch(i) {\n";
  for (int i = 0; i < kParameterCount; i++) {
    int j = kParameterCount - i - 1;
    os << "      case " << j << ": sum += a" << j << ";\n";
  }
  os << "  }\n";
  os << "    return sum;\n";
  os << "  };\n";
  os << "  return summation(arg";
  for (int i = 0; i < kParameterCount; i++) {
    os << "," << i;
  }
  os << ");";
  os << "}\n";

  std::string source = os.str();
  InterpreterTester tester(isolate, source.c_str(), "*");
  auto callable = tester.GetCallable<Handle<Object>>();
  for (int i = 0; i < kParameterCount; i++) {
    Handle<Object> arg = handle(Smi::FromInt(i), isolate);
    Handle<Object> return_value = callable(arg).ToHandleChecked();
    int expected = kBaseValue + i * (i + 1) / 2;
    Handle<Smi> actual = Handle<Smi>::cast(return_value);
    CHECK_EQ(actual->value(), expected);
  }
}

TEST(InterpreterWithStatement) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, Handle<Object>> with_stmt[] = {
      {"with({x:42}) return x;", handle(Smi::FromInt(42), isolate)},
      {"with({}) { var y = 10; return y;}", handle(Smi::FromInt(10), isolate)},
      {"var y = {x:42};"
       " function inner() {"
       "   var x = 20;"
       "   with(y) return x;"
       "}"
       "return inner();",
       handle(Smi::FromInt(42), isolate)},
      {"var y = {x:42};"
       " function inner(o) {"
       "   var x = 20;"
       "   with(o) return x;"
       "}"
       "return inner(y);",
       handle(Smi::FromInt(42), isolate)},
  };

  for (size_t i = 0; i < arraysize(with_stmt); i++) {
    std::string source(InterpreterTester::SourceForBody(with_stmt[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*with_stmt[i].second));
  }
}

TEST(InterpreterClassLiterals) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  std::pair<const char*, Handle<Object>> examples[] = {
      {"class C {\n"
       "  constructor(x) { this.x_ = x; }\n"
       "  method() { return this.x_; }\n"
       "}\n"
       "return new C(99).method();",
       handle(Smi::FromInt(99), isolate)},
      {"class C {\n"
       "  constructor(x) { this.x_ = x; }\n"
       "  static static_method(x) { return x; }\n"
       "}\n"
       "return C.static_method(101);",
       handle(Smi::FromInt(101), isolate)},
      {"class C {\n"
       "  get x() { return 102; }\n"
       "}\n"
       "return new C().x",
       handle(Smi::FromInt(102), isolate)},
      {"class C {\n"
       "  static get x() { return 103; }\n"
       "}\n"
       "return C.x",
       handle(Smi::FromInt(103), isolate)},
      {"class C {\n"
       "  constructor() { this.x_ = 0; }"
       "  set x(value) { this.x_ = value; }\n"
       "  get x() { return this.x_; }\n"
       "}\n"
       "var c = new C();"
       "c.x = 104;"
       "return c.x;",
       handle(Smi::FromInt(104), isolate)},
      {"var x = 0;"
       "class C {\n"
       "  static set x(value) { x = value; }\n"
       "  static get x() { return x; }\n"
       "}\n"
       "C.x = 105;"
       "return C.x;",
       handle(Smi::FromInt(105), isolate)},
      {"var method = 'f';"
       "class C {\n"
       "  [method]() { return 106; }\n"
       "}\n"
       "return new C().f();",
       handle(Smi::FromInt(106), isolate)},
  };

  for (size_t i = 0; i < arraysize(examples); ++i) {
    std::string source(InterpreterTester::SourceForBody(examples[i].first));
    InterpreterTester tester(isolate, source.c_str(), "*");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*examples[i].second));
  }
}

TEST(InterpreterClassAndSuperClass) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  std::pair<const char*, Handle<Object>> examples[] = {
      {"class A {\n"
       "  constructor(x) { this.x_ = x; }\n"
       "  method() { return this.x_; }\n"
       "}\n"
       "class B extends A {\n"
       "   constructor(x, y) { super(x); this.y_ = y; }\n"
       "   method() { return super.method() + 1; }\n"
       "}\n"
       "return new B(998, 0).method();\n",
       handle(Smi::FromInt(999), isolate)},
      {"class A {\n"
       "  constructor() { this.x_ = 2; this.y_ = 3; }\n"
       "}\n"
       "class B extends A {\n"
       "  constructor() { super(); }"
       "  method() { this.x_++; this.y_++; return this.x_ + this.y_; }\n"
       "}\n"
       "return new B().method();\n",
       handle(Smi::FromInt(7), isolate)},
      {"var calls = 0;\n"
       "class B {}\n"
       "B.prototype.x = 42;\n"
       "class C extends B {\n"
       "  constructor() {\n"
       "    super();\n"
       "    calls++;\n"
       "  }\n"
       "}\n"
       "new C;\n"
       "return calls;\n",
       handle(Smi::FromInt(1), isolate)},
      {"class A {\n"
       "  method() { return 1; }\n"
       "  get x() { return 2; }\n"
       "}\n"
       "class B extends A {\n"
       "  method() { return super.x === 2 ? super.method() : -1; }\n"
       "}\n"
       "return new B().method();\n",
       handle(Smi::FromInt(1), isolate)},
      {"var object = { setY(v) { super.y = v; }};\n"
       "object.setY(10);\n"
       "return object.y;\n",
       handle(Smi::FromInt(10), isolate)},
  };

  for (size_t i = 0; i < arraysize(examples); ++i) {
    std::string source(InterpreterTester::SourceForBody(examples[i].first));
    InterpreterTester tester(isolate, source.c_str(), "*");
    auto callable = tester.GetCallable<>();
    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*examples[i].second));
  }
}

TEST(InterpreterConstDeclaration) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> const_decl[] = {
      {"const x = 3; return x;", handle(Smi::FromInt(3), isolate)},
      {"let x = 10; x = x + 20; return x;", handle(Smi::FromInt(30), isolate)},
      {"let x = 10; x = 20; return x;", handle(Smi::FromInt(20), isolate)},
      {"let x; x = 20; return x;", handle(Smi::FromInt(20), isolate)},
      {"let x; return x;", factory->undefined_value()},
      {"var x = 10; { let x = 30; } return x;",
       handle(Smi::FromInt(10), isolate)},
      {"let x = 10; { let x = 20; } return x;",
       handle(Smi::FromInt(10), isolate)},
      {"var x = 10; eval('let x = 20;'); return x;",
       handle(Smi::FromInt(10), isolate)},
      {"var x = 10; eval('const x = 20;'); return x;",
       handle(Smi::FromInt(10), isolate)},
      {"var x = 10; { const x = 20; } return x;",
       handle(Smi::FromInt(10), isolate)},
      {"var x = 10; { const x = 20; return x;} return -1;",
       handle(Smi::FromInt(20), isolate)},
      {"var a = 10;\n"
       "for (var i = 0; i < 10; ++i) {\n"
       " const x = i;\n"  // const declarations are block scoped.
       " a = a + x;\n"
       "}\n"
       "return a;\n",
       handle(Smi::FromInt(55), isolate)},
  };

  // Tests for sloppy mode.
  for (size_t i = 0; i < arraysize(const_decl); i++) {
    std::string source(InterpreterTester::SourceForBody(const_decl[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*const_decl[i].second));
  }

  // Tests for strict mode.
  for (size_t i = 0; i < arraysize(const_decl); i++) {
    std::string strict_body =
        "'use strict'; " + std::string(const_decl[i].first);
    std::string source(InterpreterTester::SourceForBody(strict_body.c_str()));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*const_decl[i].second));
  }
}

TEST(InterpreterConstDeclarationLookupSlots) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> const_decl[] = {
      {"const x = 3; function f1() {return x;}; return x;",
       handle(Smi::FromInt(3), isolate)},
      {"let x = 10; x = x + 20; function f1() {return x;}; return x;",
       handle(Smi::FromInt(30), isolate)},
      {"let x; x = 20; function f1() {return x;}; return x;",
       handle(Smi::FromInt(20), isolate)},
      {"let x; function f1() {return x;}; return x;",
       factory->undefined_value()},
  };

  // Tests for sloppy mode.
  for (size_t i = 0; i < arraysize(const_decl); i++) {
    std::string source(InterpreterTester::SourceForBody(const_decl[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*const_decl[i].second));
  }

  // Tests for strict mode.
  for (size_t i = 0; i < arraysize(const_decl); i++) {
    std::string strict_body =
        "'use strict'; " + std::string(const_decl[i].first);
    std::string source(InterpreterTester::SourceForBody(strict_body.c_str()));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*const_decl[i].second));
  }
}

TEST(InterpreterConstInLookupContextChain) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* prologue =
      "function OuterMost() {\n"
      "  const outerConst = 10;\n"
      "  let outerLet = 20;\n"
      "  function Outer() {\n"
      "    function Inner() {\n"
      "      this.innerFunc = function() { ";
  const char* epilogue =
      "      }\n"
      "    }\n"
      "    this.getInnerFunc ="
      "         function() {return new Inner().innerFunc;}\n"
      "  }\n"
      "  this.getOuterFunc ="
      "     function() {return new Outer().getInnerFunc();}"
      "}\n"
      "var f = new OuterMost().getOuterFunc();\n"
      "f();\n";
  std::pair<const char*, Handle<Object>> const_decl[] = {
      {"return outerConst;", handle(Smi::FromInt(10), isolate)},
      {"return outerLet;", handle(Smi::FromInt(20), isolate)},
      {"outerLet = 30; return outerLet;", handle(Smi::FromInt(30), isolate)},
      {"var outerLet = 40; return outerLet;",
       handle(Smi::FromInt(40), isolate)},
      {"var outerConst = 50; return outerConst;",
       handle(Smi::FromInt(50), isolate)},
      {"try { outerConst = 30 } catch(e) { return -1; }",
       handle(Smi::FromInt(-1), isolate)}};

  for (size_t i = 0; i < arraysize(const_decl); i++) {
    std::string script = std::string(prologue) +
                         std::string(const_decl[i].first) +
                         std::string(epilogue);
    InterpreterTester tester(isolate, script.c_str(), "*");
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*const_decl[i].second));
  }
}

TEST(InterpreterIllegalConstDeclaration) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  std::pair<const char*, const char*> const_decl[] = {
      {"const x = x = 10 + 3; return x;",
       "Uncaught ReferenceError: Cannot access 'x' before initialization"},
      {"const x = 10; x = 20; return x;",
       "Uncaught TypeError: Assignment to constant variable."},
      {"const x = 10; { x = 20; } return x;",
       "Uncaught TypeError: Assignment to constant variable."},
      {"const x = 10; eval('x = 20;'); return x;",
       "Uncaught TypeError: Assignment to constant variable."},
      {"let x = x + 10; return x;",
       "Uncaught ReferenceError: Cannot access 'x' before initialization"},
      {"'use strict'; (function f1() { f1 = 123; })() ",
       "Uncaught TypeError: Assignment to constant variable."},
  };

  // Tests for sloppy mode.
  for (size_t i = 0; i < arraysize(const_decl); i++) {
    std::string source(InterpreterTester::SourceForBody(const_decl[i].first));
    InterpreterTester tester(isolate, source.c_str());
    v8::Local<v8::String> message = tester.CheckThrowsReturnMessage()->Get();
    v8::Local<v8::String> expected_string = v8_str(const_decl[i].second);
    CHECK(
        message->Equals(CcTest::isolate()->GetCurrentContext(), expected_string)
            .FromJust());
  }

  // Tests for strict mode.
  for (size_t i = 0; i < arraysize(const_decl); i++) {
    std::string strict_body =
        "'use strict'; " + std::string(const_decl[i].first);
    std::string source(InterpreterTester::SourceForBody(strict_body.c_str()));
    InterpreterTester tester(isolate, source.c_str());
    v8::Local<v8::String> message = tester.CheckThrowsReturnMessage()->Get();
    v8::Local<v8::String> expected_string = v8_str(const_decl[i].second);
    CHECK(
        message->Equals(CcTest::isolate()->GetCurrentContext(), expected_string)
            .FromJust());
  }
}

TEST(InterpreterGenerators) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Factory* factory = isolate->factory();

  std::pair<const char*, Handle<Object>> tests[] = {
      {"function* f() { }; return f().next().value",
       factory->undefined_value()},
      {"function* f() { yield 42 }; return f().next().value",
       factory->NewNumberFromInt(42)},
      {"function* f() { for (let x of [42]) yield x}; return f().next().value",
       factory->NewNumberFromInt(42)},
  };

  for (size_t i = 0; i < arraysize(tests); i++) {
    std::string source(InterpreterTester::SourceForBody(tests[i].first));
    InterpreterTester tester(isolate, source.c_str());
    auto callable = tester.GetCallable<>();

    Handle<i::Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*tests[i].second));
  }
}

#ifndef V8_TARGET_ARCH_ARM
TEST(InterpreterWithNativeStack) {
  i::FLAG_interpreted_frames_native_stack = true;

  HandleAndZoneScope handles;
  i::Isolate* isolate = handles.main_isolate();

  const char* source_text =
      "function testInterpreterWithNativeStack(a,b) { return a + b };";

  i::Handle<i::Object> o = v8::Utils::OpenHandle(*v8_compile(source_text));
  i::Handle<i::JSFunction> f = i::Handle<i::JSFunction>::cast(o);

  CHECK(f->shared().HasBytecodeArray());
  i::Code code = f->shared().GetCode();
  i::Handle<i::Code> interpreter_entry_trampoline =
      BUILTIN_CODE(isolate, InterpreterEntryTrampoline);

  CHECK(code.IsCode());
  CHECK(code.is_interpreter_trampoline_builtin());
  CHECK_NE(code.address(), interpreter_entry_trampoline->address());
}
#endif  // V8_TARGET_ARCH_ARM

TEST(InterpreterGetBytecodeHandler) {
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();
  Interpreter* interpreter = isolate->interpreter();

  // Test that single-width bytecode handlers deserializer correctly.
  Code wide_handler =
      interpreter->GetBytecodeHandler(Bytecode::kWide, OperandScale::kSingle);

  CHECK_EQ(wide_handler.builtin_index(), Builtins::kWideHandler);

  Code add_handler =
      interpreter->GetBytecodeHandler(Bytecode::kAdd, OperandScale::kSingle);

  CHECK_EQ(add_handler.builtin_index(), Builtins::kAddHandler);

  // Test that double-width bytecode handlers deserializer correctly, including
  // an illegal bytecode handler since there is no Wide.Wide handler.
  Code wide_wide_handler =
      interpreter->GetBytecodeHandler(Bytecode::kWide, OperandScale::kDouble);

  CHECK_EQ(wide_wide_handler.builtin_index(), Builtins::kIllegalHandler);

  Code add_wide_handler =
      interpreter->GetBytecodeHandler(Bytecode::kAdd, OperandScale::kDouble);

  CHECK_EQ(add_wide_handler.builtin_index(), Builtins::kAddWideHandler);
}

TEST(InterpreterCollectSourcePositions) {
  FLAG_enable_lazy_source_positions = true;
  FLAG_stress_lazy_source_positions = false;
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* source =
      "(function () {\n"
      "  return 1;\n"
      "})";

  Handle<JSFunction> function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));

  Handle<SharedFunctionInfo> sfi = handle(function->shared(), isolate);
  Handle<BytecodeArray> bytecode_array =
      handle(sfi->GetBytecodeArray(), isolate);
  CHECK(!bytecode_array->HasSourcePositionTable());

  Compiler::CollectSourcePositions(isolate, sfi);

  ByteArray source_position_table = bytecode_array->SourcePositionTable();
  CHECK(bytecode_array->HasSourcePositionTable());
  CHECK_GT(source_position_table.length(), 0);
}

TEST(InterpreterCollectSourcePositions_StackOverflow) {
  FLAG_enable_lazy_source_positions = true;
  FLAG_stress_lazy_source_positions = false;
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* source =
      "(function () {\n"
      "  return 1;\n"
      "})";

  Handle<JSFunction> function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));

  Handle<SharedFunctionInfo> sfi = handle(function->shared(), isolate);
  Handle<BytecodeArray> bytecode_array =
      handle(sfi->GetBytecodeArray(), isolate);
  CHECK(!bytecode_array->HasSourcePositionTable());

  // Make the stack limit the same as the current position so recompilation
  // overflows.
  uint64_t previous_limit = isolate->stack_guard()->real_climit();
  isolate->stack_guard()->SetStackLimit(GetCurrentStackPosition());
  Compiler::CollectSourcePositions(isolate, sfi);
  // Stack overflowed so source position table can be returned but is empty.
  ByteArray source_position_table = bytecode_array->SourcePositionTable();
  CHECK(!bytecode_array->HasSourcePositionTable());
  CHECK_EQ(source_position_table.length(), 0);

  // Reset the stack limit and try again.
  isolate->stack_guard()->SetStackLimit(previous_limit);
  Compiler::CollectSourcePositions(isolate, sfi);
  source_position_table = bytecode_array->SourcePositionTable();
  CHECK(bytecode_array->HasSourcePositionTable());
  CHECK_GT(source_position_table.length(), 0);
}

TEST(InterpreterCollectSourcePositions_ThrowFrom1stFrame) {
  FLAG_enable_lazy_source_positions = true;
  FLAG_stress_lazy_source_positions = false;
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* source =
      R"javascript(
      (function () {
        throw new Error();
      });
      )javascript";

  Handle<JSFunction> function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));

  Handle<SharedFunctionInfo> sfi = handle(function->shared(), isolate);
  // This is the bytecode for the top-level iife.
  Handle<BytecodeArray> bytecode_array =
      handle(sfi->GetBytecodeArray(), isolate);
  CHECK(!bytecode_array->HasSourcePositionTable());

  {
    v8::TryCatch try_catch(CcTest::isolate());
    MaybeHandle<Object> result = Execution::Call(
        isolate, function, ReadOnlyRoots(isolate).undefined_value_handle(), 0,
        nullptr);
    CHECK(result.is_null());
    CHECK(try_catch.HasCaught());
  }

  // The exception was caught but source positions were not retrieved from it so
  // there should be no source position table.
  CHECK(!bytecode_array->HasSourcePositionTable());
}

TEST(InterpreterCollectSourcePositions_ThrowFrom2ndFrame) {
  FLAG_enable_lazy_source_positions = true;
  FLAG_stress_lazy_source_positions = false;
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* source =
      R"javascript(
      (function () {
        (function () {
          throw new Error();
        })();
      });
      )javascript";

  Handle<JSFunction> function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));

  Handle<SharedFunctionInfo> sfi = handle(function->shared(), isolate);
  // This is the bytecode for the top-level iife.
  Handle<BytecodeArray> bytecode_array =
      handle(sfi->GetBytecodeArray(), isolate);
  CHECK(!bytecode_array->HasSourcePositionTable());

  {
    v8::TryCatch try_catch(CcTest::isolate());
    MaybeHandle<Object> result = Execution::Call(
        isolate, function, ReadOnlyRoots(isolate).undefined_value_handle(), 0,
        nullptr);
    CHECK(result.is_null());
    CHECK(try_catch.HasCaught());
  }

  // The exception was caught but source positions were not retrieved from it so
  // there should be no source position table.
  CHECK(!bytecode_array->HasSourcePositionTable());
}

namespace {

void CheckStringEqual(const char* expected_ptr, const char* actual_ptr) {
  CHECK_NOT_NULL(expected_ptr);
  CHECK_NOT_NULL(actual_ptr);
  std::string expected(expected_ptr);
  std::string actual(actual_ptr);
  CHECK_EQ(expected, actual);
}

void CheckStringEqual(const char* expected_ptr, Handle<Object> actual_handle) {
  v8::String::Utf8Value utf8(
      v8::Isolate::GetCurrent(),
      v8::Utils::ToLocal(Handle<String>::cast(actual_handle)));
  CheckStringEqual(expected_ptr, *utf8);
}

}  // namespace

TEST(InterpreterCollectSourcePositions_GenerateStackTrace) {
  FLAG_enable_lazy_source_positions = true;
  FLAG_stress_lazy_source_positions = false;
  HandleAndZoneScope handles;
  Isolate* isolate = handles.main_isolate();

  const char* source =
      R"javascript(
      (function () {
        try {
          throw new Error();
        } catch (e) {
          return e.stack;
        }
      });
      )javascript";

  Handle<JSFunction> function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));

  Handle<SharedFunctionInfo> sfi = handle(function->shared(), isolate);
  Handle<BytecodeArray> bytecode_array =
      handle(sfi->GetBytecodeArray(), isolate);
  CHECK(!bytecode_array->HasSourcePositionTable());

  {
    Handle<Object> result =
        Execution::Call(isolate, function,
                        ReadOnlyRoots(isolate).undefined_value_handle(), 0,
                        nullptr)
            .ToHandleChecked();
    CheckStringEqual("Error\n    at <anonymous>:4:17", result);
  }

  CHECK(bytecode_array->HasSourcePositionTable());
  ByteArray source_position_table = bytecode_array->SourcePositionTable();
  CHECK_GT(source_position_table.length(), 0);
}

TEST(InterpreterLookupNameOfBytecodeHandler) {
  Interpreter* interpreter = CcTest::i_isolate()->interpreter();
  Code ldaLookupSlot = interpreter->GetBytecodeHandler(Bytecode::kLdaLookupSlot,
                                                       OperandScale::kSingle);
  CheckStringEqual("LdaLookupSlotHandler",
                   interpreter->LookupNameOfBytecodeHandler(ldaLookupSlot));
  Code wideLdaLookupSlot = interpreter->GetBytecodeHandler(
      Bytecode::kLdaLookupSlot, OperandScale::kDouble);
  CheckStringEqual("LdaLookupSlotWideHandler",
                   interpreter->LookupNameOfBytecodeHandler(wideLdaLookupSlot));
  Code extraWideLdaLookupSlot = interpreter->GetBytecodeHandler(
      Bytecode::kLdaLookupSlot, OperandScale::kQuadruple);
  CheckStringEqual(
      "LdaLookupSlotExtraWideHandler",
      interpreter->LookupNameOfBytecodeHandler(extraWideLdaLookupSlot));
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
