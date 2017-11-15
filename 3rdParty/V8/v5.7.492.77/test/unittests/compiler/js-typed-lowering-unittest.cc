// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-typed-lowering.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/isolate-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::BitEq;
using testing::IsNaN;


namespace v8 {
namespace internal {
namespace compiler {

namespace {

const ExternalArrayType kExternalArrayTypes[] = {
    kExternalUint8Array,   kExternalInt8Array,   kExternalUint16Array,
    kExternalInt16Array,   kExternalUint32Array, kExternalInt32Array,
    kExternalFloat32Array, kExternalFloat64Array};

const size_t kIndices[] = {0, 1, 42, 100, 1024};

Type* const kJSTypes[] = {Type::Undefined(), Type::Null(),   Type::Boolean(),
                          Type::Number(),    Type::String(), Type::Object()};

STATIC_ASSERT(LANGUAGE_END == 2);
const LanguageMode kLanguageModes[] = {SLOPPY, STRICT};

}  // namespace


class JSTypedLoweringTest : public TypedGraphTest {
 public:
  JSTypedLoweringTest()
      : TypedGraphTest(3), javascript_(zone()), deps_(isolate(), zone()) {}
  ~JSTypedLoweringTest() override {}

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSTypedLowering reducer(&graph_reducer, &deps_,
                            JSTypedLowering::kDeoptimizationEnabled, &jsgraph,
                            zone());
    return reducer.Reduce(node);
  }

  Handle<JSArrayBuffer> NewArrayBuffer(void* bytes, size_t byte_length) {
    Handle<JSArrayBuffer> buffer = factory()->NewJSArrayBuffer();
    JSArrayBuffer::Setup(buffer, isolate(), true, bytes, byte_length);
    return buffer;
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
  CompilationDependencies deps_;
};


// -----------------------------------------------------------------------------
// JSToBoolean


TEST_F(JSTypedLoweringTest, JSToBooleanWithBoolean) {
  Node* input = Parameter(Type::Boolean(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithOrderedNumber) {
  Node* input = Parameter(Type::OrderedNumber(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsNumberEqual(input, IsNumberConstant(0.0))));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithNumber) {
  Node* input = Parameter(Type::Number(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberToBoolean(input));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithDetectableReceiverOrNull) {
  Node* input = Parameter(Type::DetectableReceiverOrNull(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsReferenceEqual(input, IsNullConstant())));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithReceiverOrNullOrUndefined) {
  Node* input = Parameter(Type::ReceiverOrNullOrUndefined(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsBooleanNot(IsObjectIsUndetectable(input)));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithAny) {
  Node* input = Parameter(Type::Any(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_FALSE(r.Changed());
}


// -----------------------------------------------------------------------------
// JSToName

TEST_F(JSTypedLoweringTest, JSToNameWithString) {
  Node* const input = Parameter(Type::String(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}

TEST_F(JSTypedLoweringTest, JSToNameWithSymbol) {
  Node* const input = Parameter(Type::Symbol(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}

TEST_F(JSTypedLoweringTest, JSToNameWithAny) {
  Node* const input = Parameter(Type::Any(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_FALSE(r.Changed());
}

// -----------------------------------------------------------------------------
// JSToNumber

TEST_F(JSTypedLoweringTest, JSToNumberWithPlainPrimitive) {
  Node* const input = Parameter(Type::PlainPrimitive(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToNumber(), input, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsPlainPrimitiveToNumber(input));
}


// -----------------------------------------------------------------------------
// JSToObject


TEST_F(JSTypedLoweringTest, JSToObjectWithAny) {
  Node* const input = Parameter(Type::Any(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToObject(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsPhi(MachineRepresentation::kTagged, _, _, _));
}


TEST_F(JSTypedLoweringTest, JSToObjectWithReceiver) {
  Node* const input = Parameter(Type::Receiver(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToObject(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}


// -----------------------------------------------------------------------------
// JSToString


TEST_F(JSTypedLoweringTest, JSToStringWithBoolean) {
  Node* const input = Parameter(Type::Boolean(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToString(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSelect(MachineRepresentation::kTagged, input,
                       IsHeapConstant(factory()->true_string()),
                       IsHeapConstant(factory()->false_string())));
}


// -----------------------------------------------------------------------------
// JSStrictEqual


TEST_F(JSTypedLoweringTest, JSStrictEqualWithTheHole) {
  Node* const the_hole = HeapConstant(factory()->the_hole_value());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(Type*, type, kJSTypes) {
    Node* const lhs = Parameter(type);
    Reduction r = Reduce(
        graph()->NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                         lhs, the_hole, context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsFalseConstant());
  }
}


TEST_F(JSTypedLoweringTest, JSStrictEqualWithUnique) {
  Node* const lhs = Parameter(Type::Unique(), 0);
  Node* const rhs = Parameter(Type::Unique(), 1);
  Node* const context = Parameter(Type::Any(), 2);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(
      graph()->NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                       lhs, rhs, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsReferenceEqual(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSShiftLeft

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndConstant) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* const lhs = Parameter(Type::Signed32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(javascript()->ShiftLeft(hint), lhs,
                                          NumberConstant(rhs), context,
                                          EmptyFrameState(), effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftLeft(lhs, IsNumberConstant(BitEq(rhs))));
  }
}

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndUnsigned32) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ShiftLeft(hint), lhs, rhs, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftLeft(lhs, rhs));
}

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSignedSmallHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftLeft(hint), lhs, rhs,
                                        UndefinedConstant(), EmptyFrameState(),
                                        effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberShiftLeft(NumberOperationHint::kSignedSmall,
                                           lhs, rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32Hint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSigned32;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftLeft(hint), lhs, rhs,
                                        UndefinedConstant(), EmptyFrameState(),
                                        effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberShiftLeft(NumberOperationHint::kSigned32, lhs,
                                           rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSShiftLeftWithNumberOrOddballHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kNumberOrOddball;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftLeft(hint), lhs, rhs,
                                        UndefinedConstant(), EmptyFrameState(),
                                        effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSpeculativeNumberShiftLeft(
                                   NumberOperationHint::kNumberOrOddball, lhs,
                                   rhs, effect, control));
}

// -----------------------------------------------------------------------------
// JSShiftRight


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndConstant) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* const lhs = Parameter(Type::Signed32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRight(hint), lhs,
                                          NumberConstant(rhs), context,
                                          EmptyFrameState(), effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftRight(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndUnsigned32) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ShiftRight(hint), lhs, rhs, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftRight(lhs, rhs));
}

TEST_F(JSTypedLoweringTest, JSShiftRightWithSignedSmallHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRight(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberShiftRight(NumberOperationHint::kSignedSmall,
                                            lhs, rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32Hint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSigned32;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRight(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberShiftRight(NumberOperationHint::kSigned32, lhs,
                                            rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSShiftRightWithNumberOrOddballHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kNumberOrOddball;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRight(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSpeculativeNumberShiftRight(
                                   NumberOperationHint::kNumberOrOddball, lhs,
                                   rhs, effect, control));
}

// -----------------------------------------------------------------------------
// JSShiftRightLogical


TEST_F(JSTypedLoweringTest,
                   JSShiftRightLogicalWithUnsigned32AndConstant) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRightLogical(hint),
                                          lhs, NumberConstant(rhs), context,
                                          EmptyFrameState(), effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftRightLogical(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithUnsigned32AndUnsigned32) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ShiftRightLogical(hint), lhs, rhs,
                              context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftRightLogical(lhs, rhs));
}

TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithSignedSmallHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRightLogical(hint),
                                        lhs, rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSpeculativeNumberShiftRightLogical(
                                   NumberOperationHint::kSignedSmall, lhs, rhs,
                                   effect, control));
}

TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithSigned32Hint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSigned32;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRightLogical(hint),
                                        lhs, rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberShiftRightLogical(
                  NumberOperationHint::kSigned32, lhs, rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithNumberOrOddballHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kNumberOrOddball;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRightLogical(hint),
                                        lhs, rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSpeculativeNumberShiftRightLogical(
                                   NumberOperationHint::kNumberOrOddball, lhs,
                                   rhs, effect, control));
}

// -----------------------------------------------------------------------------
// JSLoadContext


TEST_F(JSTypedLoweringTest, JSLoadContext) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  static bool kBooleans[] = {false, true};
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(bool, immutable, kBooleans) {
      Reduction const r1 = Reduce(graph()->NewNode(
          javascript()->LoadContext(0, index, immutable), context, effect));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsLoadField(AccessBuilder::ForContextSlot(index), context,
                              effect, graph()->start()));

      Reduction const r2 = Reduce(graph()->NewNode(
          javascript()->LoadContext(1, index, immutable), context, effect));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsLoadField(AccessBuilder::ForContextSlot(index),
                              IsLoadField(AccessBuilder::ForContextSlot(
                                              Context::PREVIOUS_INDEX),
                                          context, effect, graph()->start()),
                              _, graph()->start()));
    }
  }
}


// -----------------------------------------------------------------------------
// JSStoreContext


TEST_F(JSTypedLoweringTest, JSStoreContext) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(Type*, type, kJSTypes) {
      Node* const value = Parameter(type);

      Reduction const r1 =
          Reduce(graph()->NewNode(javascript()->StoreContext(0, index), value,
                                  context, effect, control));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsStoreField(AccessBuilder::ForContextSlot(index), context,
                               value, effect, control));

      Reduction const r2 =
          Reduce(graph()->NewNode(javascript()->StoreContext(1, index), value,
                                  context, effect, control));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsStoreField(AccessBuilder::ForContextSlot(index),
                               IsLoadField(AccessBuilder::ForContextSlot(
                                               Context::PREVIOUS_INDEX),
                                           context, effect, graph()->start()),
                               value, _, control));
    }
  }
}


// -----------------------------------------------------------------------------
// JSLoadProperty


TEST_F(JSTypedLoweringTest, JSLoadPropertyFromExternalTypedArray) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  VectorSlotPair feedback;
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    Handle<JSTypedArray> array =
        factory()->NewJSTypedArray(type, buffer, 0, kLength);
    int const element_size = static_cast<int>(array->element_size());

    Node* key = Parameter(
        Type::Range(kMinInt / element_size, kMaxInt / element_size, zone()));
    Node* base = HeapConstant(array);
    Node* context = UndefinedConstant();
    Node* effect = graph()->start();
    Node* control = graph()->start();
    Reduction r =
        Reduce(graph()->NewNode(javascript()->LoadProperty(feedback), base, key,
                                context, EmptyFrameState(), effect, control));

    Matcher<Node*> offset_matcher =
        element_size == 1
            ? key
            : IsNumberShiftLeft(key,
                                IsNumberConstant(WhichPowerOf2(element_size)));

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(
        r.replacement(),
        IsLoadBuffer(BufferAccess(type),
                     IsPointerConstant(bit_cast<intptr_t>(&backing_store[0])),
                     offset_matcher,
                     IsNumberConstant(array->byte_length()->Number()), effect,
                     control));
  }
}


TEST_F(JSTypedLoweringTest, JSLoadPropertyFromExternalTypedArrayWithSafeKey) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  VectorSlotPair feedback;
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    Handle<JSTypedArray> array =
        factory()->NewJSTypedArray(type, buffer, 0, kLength);
    ElementAccess access = AccessBuilder::ForTypedArrayElement(type, true);

    int min = random_number_generator()->NextInt(static_cast<int>(kLength));
    int max = random_number_generator()->NextInt(static_cast<int>(kLength));
    if (min > max) std::swap(min, max);
    Node* key = Parameter(Type::Range(min, max, zone()));
    Node* base = HeapConstant(array);
    Node* context = UndefinedConstant();
    Node* effect = graph()->start();
    Node* control = graph()->start();
    Reduction r =
        Reduce(graph()->NewNode(javascript()->LoadProperty(feedback), base, key,
                                context, EmptyFrameState(), effect, control));

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(
        r.replacement(),
        IsLoadElement(access,
                      IsPointerConstant(bit_cast<intptr_t>(&backing_store[0])),
                      key, effect, control));
  }
}


// -----------------------------------------------------------------------------
// JSStoreProperty


TEST_F(JSTypedLoweringTest, JSStorePropertyToExternalTypedArray) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    TRACED_FOREACH(LanguageMode, language_mode, kLanguageModes) {
      Handle<JSTypedArray> array =
          factory()->NewJSTypedArray(type, buffer, 0, kLength);
      int const element_size = static_cast<int>(array->element_size());

      Node* key = Parameter(
          Type::Range(kMinInt / element_size, kMaxInt / element_size, zone()));
      Node* base = HeapConstant(array);
      Node* value =
          Parameter(AccessBuilder::ForTypedArrayElement(type, true).type);
      Node* context = UndefinedConstant();
      Node* effect = graph()->start();
      Node* control = graph()->start();
      VectorSlotPair feedback;
      const Operator* op = javascript()->StoreProperty(language_mode, feedback);
      Node* node = graph()->NewNode(op, base, key, value, context,
                                    EmptyFrameState(), effect, control);
      Reduction r = Reduce(node);

      Matcher<Node*> offset_matcher =
          element_size == 1
              ? key
              : IsNumberShiftLeft(
                    key, IsNumberConstant(WhichPowerOf2(element_size)));

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(
          r.replacement(),
          IsStoreBuffer(
              BufferAccess(type),
              IsPointerConstant(bit_cast<intptr_t>(&backing_store[0])),
              offset_matcher, IsNumberConstant(array->byte_length()->Number()),
              value, effect, control));
    }
  }
}


TEST_F(JSTypedLoweringTest, JSStorePropertyToExternalTypedArrayWithConversion) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    TRACED_FOREACH(LanguageMode, language_mode, kLanguageModes) {
      Handle<JSTypedArray> array =
          factory()->NewJSTypedArray(type, buffer, 0, kLength);
      int const element_size = static_cast<int>(array->element_size());

      Node* key = Parameter(
          Type::Range(kMinInt / element_size, kMaxInt / element_size, zone()));
      Node* base = HeapConstant(array);
      Node* value = Parameter(Type::PlainPrimitive());
      Node* context = UndefinedConstant();
      Node* effect = graph()->start();
      Node* control = graph()->start();
      // TODO(mstarzinger): Once the effect-control-linearizer provides a frame
      // state we can get rid of this checkpoint again. The reducer won't care.
      Node* checkpoint = graph()->NewNode(common()->Checkpoint(),
                                          EmptyFrameState(), effect, control);
      VectorSlotPair feedback;
      const Operator* op = javascript()->StoreProperty(language_mode, feedback);
      Node* node = graph()->NewNode(op, base, key, value, context,
                                    EmptyFrameState(), checkpoint, control);
      Reduction r = Reduce(node);

      Matcher<Node*> offset_matcher =
          element_size == 1
              ? key
              : IsNumberShiftLeft(
                    key, IsNumberConstant(WhichPowerOf2(element_size)));

      Matcher<Node*> value_matcher = IsPlainPrimitiveToNumber(value);

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(
          r.replacement(),
          IsStoreBuffer(
              BufferAccess(type),
              IsPointerConstant(bit_cast<intptr_t>(&backing_store[0])),
              offset_matcher, IsNumberConstant(array->byte_length()->Number()),
              value_matcher, checkpoint, control));
    }
  }
}


TEST_F(JSTypedLoweringTest, JSStorePropertyToExternalTypedArrayWithSafeKey) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    TRACED_FOREACH(LanguageMode, language_mode, kLanguageModes) {
      Handle<JSTypedArray> array =
          factory()->NewJSTypedArray(type, buffer, 0, kLength);
      ElementAccess access = AccessBuilder::ForTypedArrayElement(type, true);

      int min = random_number_generator()->NextInt(static_cast<int>(kLength));
      int max = random_number_generator()->NextInt(static_cast<int>(kLength));
      if (min > max) std::swap(min, max);
      Node* key = Parameter(Type::Range(min, max, zone()));
      Node* base = HeapConstant(array);
      Node* value = Parameter(access.type);
      Node* context = UndefinedConstant();
      Node* effect = graph()->start();
      Node* control = graph()->start();
      VectorSlotPair feedback;
      const Operator* op = javascript()->StoreProperty(language_mode, feedback);
      Node* node = graph()->NewNode(op, base, key, value, context,
                                    EmptyFrameState(), effect, control);
      Reduction r = Reduce(node);

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(
          r.replacement(),
          IsStoreElement(
              access, IsPointerConstant(bit_cast<intptr_t>(&backing_store[0])),
              key, value, effect, control));
    }
  }
}


// -----------------------------------------------------------------------------
// JSLoadNamed


TEST_F(JSTypedLoweringTest, JSLoadNamedStringLength) {
  VectorSlotPair feedback;
  Handle<Name> name = factory()->length_string();
  Node* const receiver = Parameter(Type::String(), 0);
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r =
      Reduce(graph()->NewNode(javascript()->LoadNamed(name, feedback), receiver,
                              context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsLoadField(AccessBuilder::ForStringLength(),
                                           receiver, effect, control));
}


// -----------------------------------------------------------------------------
// JSAdd


TEST_F(JSTypedLoweringTest, JSAddWithString) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* lhs = Parameter(Type::String(), 0);
  Node* rhs = Parameter(Type::String(), 1);
  Node* context = Parameter(Type::Any(), 2);
  Node* frame_state = EmptyFrameState();
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->Add(hint), lhs, rhs,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsCall(_, IsHeapConstant(
                            CodeFactory::StringAdd(
                                isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED)
                                .code()),
                     lhs, rhs, context, frame_state, effect, control));
}

TEST_F(JSTypedLoweringTest, JSAddSmis) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 0);
  Node* rhs = Parameter(Type::Number(), 1);
  Node* context = Parameter(Type::Any(), 2);
  Node* frame_state = EmptyFrameState();
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->Add(hint), lhs, rhs,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberAdd(NumberOperationHint::kSignedSmall, lhs,
                                     rhs, effect, control));
}

// -----------------------------------------------------------------------------
// JSSubtract

TEST_F(JSTypedLoweringTest, JSSubtractSmis) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 0);
  Node* rhs = Parameter(Type::Number(), 1);
  Node* context = Parameter(Type::Any(), 2);
  Node* frame_state = EmptyFrameState();
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->Subtract(hint), lhs, rhs,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberSubtract(NumberOperationHint::kSignedSmall,
                                          lhs, rhs, effect, control));
}

// -----------------------------------------------------------------------------
// JSBitwiseAnd

TEST_F(JSTypedLoweringTest, JSBitwiseAndWithSignedSmallHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseAnd(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberBitwiseAnd(NumberOperationHint::kSignedSmall,
                                            lhs, rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSBitwiseAndWithSigned32Hint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSigned32;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseAnd(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberBitwiseAnd(NumberOperationHint::kSigned32, lhs,
                                            rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSBitwiseAndWithNumberOrOddballHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kNumberOrOddball;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseAnd(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSpeculativeNumberBitwiseAnd(
                                   NumberOperationHint::kNumberOrOddball, lhs,
                                   rhs, effect, control));
}

// -----------------------------------------------------------------------------
// JSBitwiseOr

TEST_F(JSTypedLoweringTest, JSBitwiseOrWithSignedSmallHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseOr(hint), lhs, rhs,
                                        UndefinedConstant(), EmptyFrameState(),
                                        effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberBitwiseOr(NumberOperationHint::kSignedSmall,
                                           lhs, rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSBitwiseOrWithSigned32Hint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSigned32;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseOr(hint), lhs, rhs,
                                        UndefinedConstant(), EmptyFrameState(),
                                        effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberBitwiseOr(NumberOperationHint::kSigned32, lhs,
                                           rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSBitwiseOrWithNumberOrOddballHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kNumberOrOddball;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseOr(hint), lhs, rhs,
                                        UndefinedConstant(), EmptyFrameState(),
                                        effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSpeculativeNumberBitwiseOr(
                                   NumberOperationHint::kNumberOrOddball, lhs,
                                   rhs, effect, control));
}

// -----------------------------------------------------------------------------
// JSBitwiseXor

TEST_F(JSTypedLoweringTest, JSBitwiseXorWithSignedSmallHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSignedSmall;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseXor(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberBitwiseXor(NumberOperationHint::kSignedSmall,
                                            lhs, rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSBitwiseXorWithSigned32Hint) {
  BinaryOperationHint const hint = BinaryOperationHint::kSigned32;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseXor(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSpeculativeNumberBitwiseXor(NumberOperationHint::kSigned32, lhs,
                                            rhs, effect, control));
}

TEST_F(JSTypedLoweringTest, JSBitwiseXorWithNumberOrOddballHint) {
  BinaryOperationHint const hint = BinaryOperationHint::kNumberOrOddball;
  Node* lhs = Parameter(Type::Number(), 2);
  Node* rhs = Parameter(Type::Number(), 3);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->BitwiseXor(hint), lhs,
                                        rhs, UndefinedConstant(),
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSpeculativeNumberBitwiseXor(
                                   NumberOperationHint::kNumberOrOddball, lhs,
                                   rhs, effect, control));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
