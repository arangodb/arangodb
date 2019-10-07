// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/v8.h"

#include "src/ast/scopes.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-jump-table.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/objects-inl.h"
#include "test/unittests/interpreter/bytecode-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayBuilderTest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayBuilderTest() = default;
  ~BytecodeArrayBuilderTest() override = default;
};

using ToBooleanMode = BytecodeArrayBuilder::ToBooleanMode;

TEST_F(BytecodeArrayBuilderTest, AllBytecodesGenerated) {
  FeedbackVectorSpec feedback_spec(zone());
  BytecodeArrayBuilder builder(zone(), 1, 131, &feedback_spec);
  Factory* factory = isolate()->factory();
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());
  DeclarationScope scope(zone(), &ast_factory);

  CHECK_EQ(builder.locals_count(), 131);
  CHECK_EQ(builder.fixed_register_count(), 131);

  Register reg(0);
  Register other(reg.index() + 1);
  Register wide(128);
  RegisterList empty;
  RegisterList single = BytecodeUtils::NewRegisterList(0, 1);
  RegisterList pair = BytecodeUtils::NewRegisterList(0, 2);
  RegisterList triple = BytecodeUtils::NewRegisterList(0, 3);
  RegisterList reg_list = BytecodeUtils::NewRegisterList(0, 10);

  // Emit argument creation operations.
  builder.CreateArguments(CreateArgumentsType::kMappedArguments)
      .CreateArguments(CreateArgumentsType::kUnmappedArguments)
      .CreateArguments(CreateArgumentsType::kRestParameter);

  // Emit constant loads.
  builder.LoadLiteral(Smi::kZero)
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(8))
      .CompareOperation(Token::Value::EQ, reg,
                        1)  // Prevent peephole optimization
                            // LdaSmi, Star -> LdrSmi.
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(Smi::FromInt(10000000))
      .StoreAccumulatorInRegister(reg)
      .LoadLiteral(ast_factory.GetOneByteString("A constant"))
      .StoreAccumulatorInRegister(reg)
      .LoadUndefined()
      .StoreAccumulatorInRegister(reg)
      .LoadNull()
      .StoreAccumulatorInRegister(reg)
      .LoadTheHole()
      .StoreAccumulatorInRegister(reg)
      .LoadTrue()
      .StoreAccumulatorInRegister(reg)
      .LoadFalse()
      .StoreAccumulatorInRegister(wide);

  // Emit Ldar and Star taking care to foil the register optimizer.
  builder.StackCheck(0)
      .LoadAccumulatorWithRegister(other)
      .BinaryOperation(Token::ADD, reg, 1)
      .StoreAccumulatorInRegister(reg)
      .LoadNull();

  // Emit register-register transfer.
  builder.MoveRegister(reg, other);
  builder.MoveRegister(reg, wide);

  FeedbackSlot load_global_slot =
      feedback_spec.AddLoadGlobalICSlot(NOT_INSIDE_TYPEOF);
  FeedbackSlot load_global_typeof_slot =
      feedback_spec.AddLoadGlobalICSlot(INSIDE_TYPEOF);
  FeedbackSlot sloppy_store_global_slot =
      feedback_spec.AddStoreGlobalICSlot(LanguageMode::kSloppy);
  FeedbackSlot load_slot = feedback_spec.AddLoadICSlot();
  FeedbackSlot keyed_load_slot = feedback_spec.AddKeyedLoadICSlot();
  FeedbackSlot sloppy_store_slot =
      feedback_spec.AddStoreICSlot(LanguageMode::kSloppy);
  FeedbackSlot strict_store_slot =
      feedback_spec.AddStoreICSlot(LanguageMode::kStrict);
  FeedbackSlot sloppy_keyed_store_slot =
      feedback_spec.AddKeyedStoreICSlot(LanguageMode::kSloppy);
  FeedbackSlot strict_keyed_store_slot =
      feedback_spec.AddKeyedStoreICSlot(LanguageMode::kStrict);
  FeedbackSlot store_own_slot = feedback_spec.AddStoreOwnICSlot();
  FeedbackSlot store_array_element_slot =
      feedback_spec.AddStoreInArrayLiteralICSlot();

  // Emit global load / store operations.
  const AstRawString* name = ast_factory.GetOneByteString("var_name");
  builder
      .LoadGlobal(name, load_global_slot.ToInt(), TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadGlobal(name, load_global_typeof_slot.ToInt(),
                  TypeofMode::INSIDE_TYPEOF)
      .StoreGlobal(name, sloppy_store_global_slot.ToInt());

  // Emit context operations.
  builder.PushContext(reg)
      .PopContext(reg)
      .LoadContextSlot(reg, 1, 0, BytecodeArrayBuilder::kMutableSlot)
      .StoreContextSlot(reg, 1, 0)
      .LoadContextSlot(reg, 2, 0, BytecodeArrayBuilder::kImmutableSlot)
      .StoreContextSlot(reg, 3, 0);

  // Emit context operations which operate on the local context.
  builder
      .LoadContextSlot(Register::current_context(), 1, 0,
                       BytecodeArrayBuilder::kMutableSlot)
      .StoreContextSlot(Register::current_context(), 1, 0)
      .LoadContextSlot(Register::current_context(), 2, 0,
                       BytecodeArrayBuilder::kImmutableSlot)
      .StoreContextSlot(Register::current_context(), 3, 0);

  // Emit load / store property operations.
  builder.LoadNamedProperty(reg, name, load_slot.ToInt())
      .LoadNamedPropertyNoFeedback(reg, name)
      .LoadKeyedProperty(reg, keyed_load_slot.ToInt())
      .StoreNamedProperty(reg, name, sloppy_store_slot.ToInt(),
                          LanguageMode::kSloppy)
      .StoreNamedPropertyNoFeedback(reg, name, LanguageMode::kStrict)
      .StoreNamedPropertyNoFeedback(reg, name, LanguageMode::kSloppy)
      .StoreKeyedProperty(reg, reg, sloppy_keyed_store_slot.ToInt(),
                          LanguageMode::kSloppy)
      .StoreNamedProperty(reg, name, strict_store_slot.ToInt(),
                          LanguageMode::kStrict)
      .StoreKeyedProperty(reg, reg, strict_keyed_store_slot.ToInt(),
                          LanguageMode::kStrict)
      .StoreNamedOwnProperty(reg, name, store_own_slot.ToInt())
      .StoreInArrayLiteral(reg, reg, store_array_element_slot.ToInt());

  // Emit load / store lookup slots.
  builder.LoadLookupSlot(name, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadLookupSlot(name, TypeofMode::INSIDE_TYPEOF)
      .StoreLookupSlot(name, LanguageMode::kSloppy, LookupHoistingMode::kNormal)
      .StoreLookupSlot(name, LanguageMode::kSloppy,
                       LookupHoistingMode::kLegacySloppy)
      .StoreLookupSlot(name, LanguageMode::kStrict,
                       LookupHoistingMode::kNormal);

  // Emit load / store lookup slots with context fast paths.
  builder.LoadLookupContextSlot(name, TypeofMode::NOT_INSIDE_TYPEOF, 1, 0)
      .LoadLookupContextSlot(name, TypeofMode::INSIDE_TYPEOF, 1, 0);

  // Emit load / store lookup slots with global fast paths.
  builder.LoadLookupGlobalSlot(name, TypeofMode::NOT_INSIDE_TYPEOF, 1, 0)
      .LoadLookupGlobalSlot(name, TypeofMode::INSIDE_TYPEOF, 1, 0);

  // Emit closure operations.
  builder.CreateClosure(0, 1, NOT_TENURED);

  // Emit create context operation.
  builder.CreateBlockContext(&scope);
  builder.CreateCatchContext(reg, &scope);
  builder.CreateFunctionContext(&scope, 1);
  builder.CreateEvalContext(&scope, 1);
  builder.CreateWithContext(reg, &scope);

  // Emit literal creation operations.
  builder.CreateRegExpLiteral(ast_factory.GetOneByteString("a"), 0, 0);
  builder.CreateArrayLiteral(0, 0, 0);
  builder.CreateObjectLiteral(0, 0, 0, reg);

  // Emit tagged template operations.
  builder.GetTemplateObject(0, 0);

  // Call operations.
  builder.CallAnyReceiver(reg, reg_list, 1)
      .CallProperty(reg, reg_list, 1)
      .CallProperty(reg, single, 1)
      .CallProperty(reg, pair, 1)
      .CallProperty(reg, triple, 1)
      .CallUndefinedReceiver(reg, reg_list, 1)
      .CallUndefinedReceiver(reg, empty, 1)
      .CallUndefinedReceiver(reg, single, 1)
      .CallUndefinedReceiver(reg, pair, 1)
      .CallRuntime(Runtime::kIsArray, reg)
      .CallRuntimeForPair(Runtime::kLoadLookupSlotForCall, reg_list, pair)
      .CallJSRuntime(Context::OBJECT_CREATE, reg_list)
      .CallWithSpread(reg, reg_list, 1)
      .CallNoFeedback(reg, reg_list);

  // Emit binary operator invocations.
  builder.BinaryOperation(Token::Value::ADD, reg, 1)
      .BinaryOperation(Token::Value::SUB, reg, 2)
      .BinaryOperation(Token::Value::MUL, reg, 3)
      .BinaryOperation(Token::Value::DIV, reg, 4)
      .BinaryOperation(Token::Value::MOD, reg, 5)
      .BinaryOperation(Token::Value::EXP, reg, 6);

  // Emit bitwise operator invocations
  builder.BinaryOperation(Token::Value::BIT_OR, reg, 6)
      .BinaryOperation(Token::Value::BIT_XOR, reg, 7)
      .BinaryOperation(Token::Value::BIT_AND, reg, 8);

  // Emit shift operator invocations
  builder.BinaryOperation(Token::Value::SHL, reg, 9)
      .BinaryOperation(Token::Value::SAR, reg, 10)
      .BinaryOperation(Token::Value::SHR, reg, 11);

  // Emit Smi binary operations.
  builder.BinaryOperationSmiLiteral(Token::Value::ADD, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::SUB, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::MUL, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::DIV, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::MOD, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::EXP, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::BIT_OR, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::BIT_XOR, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::BIT_AND, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::SHL, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::SAR, Smi::FromInt(42), 2)
      .BinaryOperationSmiLiteral(Token::Value::SHR, Smi::FromInt(42), 2);

  // Emit unary and count operator invocations.
  builder.UnaryOperation(Token::Value::INC, 1)
      .UnaryOperation(Token::Value::DEC, 1)
      .UnaryOperation(Token::Value::ADD, 1)
      .UnaryOperation(Token::Value::SUB, 1)
      .UnaryOperation(Token::Value::BIT_NOT, 1);

  // Emit unary operator invocations.
  builder.LogicalNot(ToBooleanMode::kConvertToBoolean)
      .LogicalNot(ToBooleanMode::kAlreadyBoolean)
      .TypeOf();

  // Emit delete
  builder.Delete(reg, LanguageMode::kSloppy).Delete(reg, LanguageMode::kStrict);

  // Emit construct.
  builder.Construct(reg, reg_list, 1).ConstructWithSpread(reg, reg_list, 1);

  // Emit test operator invocations.
  builder.CompareOperation(Token::Value::EQ, reg, 1)
      .CompareOperation(Token::Value::EQ_STRICT, reg, 2)
      .CompareOperation(Token::Value::LT, reg, 3)
      .CompareOperation(Token::Value::GT, reg, 4)
      .CompareOperation(Token::Value::LTE, reg, 5)
      .CompareOperation(Token::Value::GTE, reg, 6)
      .CompareTypeOf(TestTypeOfFlags::LiteralFlag::kNumber)
      .CompareOperation(Token::Value::INSTANCEOF, reg, 7)
      .CompareOperation(Token::Value::IN, reg)
      .CompareReference(reg)
      .CompareUndetectable()
      .CompareUndefined()
      .CompareNull();

  // Emit conversion operator invocations.
  builder.ToNumber(1).ToNumeric(1).ToObject(reg).ToName(reg).ToString();

  // Emit GetSuperConstructor.
  builder.GetSuperConstructor(reg);

  // Hole checks.
  builder.ThrowReferenceErrorIfHole(name)
      .ThrowSuperAlreadyCalledIfNotHole()
      .ThrowSuperNotCalledIfHole();

  // Short jumps with Imm8 operands
  {
    BytecodeLabel start, after_jump1, after_jump2, after_jump3, after_jump4,
        after_jump5, after_jump6, after_jump7, after_jump8, after_jump9,
        after_jump10;
    builder.Bind(&start)
        .Jump(&after_jump1)
        .Bind(&after_jump1)
        .JumpIfNull(&after_jump2)
        .Bind(&after_jump2)
        .JumpIfNotNull(&after_jump3)
        .Bind(&after_jump3)
        .JumpIfUndefined(&after_jump4)
        .Bind(&after_jump4)
        .JumpIfNotUndefined(&after_jump5)
        .Bind(&after_jump5)
        .JumpIfJSReceiver(&after_jump6)
        .Bind(&after_jump6)
        .JumpIfTrue(ToBooleanMode::kConvertToBoolean, &after_jump7)
        .Bind(&after_jump7)
        .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &after_jump8)
        .Bind(&after_jump8)
        .JumpIfFalse(ToBooleanMode::kConvertToBoolean, &after_jump9)
        .Bind(&after_jump9)
        .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &after_jump10)
        .Bind(&after_jump10)
        .JumpLoop(&start, 0);
  }

  // Longer jumps with constant operands
  BytecodeLabel end[10];
  {
    BytecodeLabel after_jump;
    builder.Jump(&end[0])
        .Bind(&after_jump)
        .JumpIfTrue(ToBooleanMode::kConvertToBoolean, &end[1])
        .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &end[2])
        .JumpIfFalse(ToBooleanMode::kConvertToBoolean, &end[3])
        .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &end[4])
        .JumpIfNull(&end[5])
        .JumpIfNotNull(&end[6])
        .JumpIfUndefined(&end[7])
        .JumpIfNotUndefined(&end[8])
        .LoadLiteral(ast_factory.prototype_string())
        .JumpIfJSReceiver(&end[9]);
  }

  // Emit Smi table switch bytecode.
  BytecodeJumpTable* jump_table = builder.AllocateJumpTable(1, 0);
  builder.SwitchOnSmiNoFeedback(jump_table).Bind(jump_table, 0);

  // Emit set pending message bytecode.
  builder.SetPendingMessage();

  // Emit stack check bytecode.
  builder.StackCheck(0);

  // Emit throw and re-throw in it's own basic block so that the rest of the
  // code isn't omitted due to being dead.
  BytecodeLabel after_throw;
  builder.Throw().Bind(&after_throw);
  BytecodeLabel after_rethrow;
  builder.ReThrow().Bind(&after_rethrow);

  builder.ForInEnumerate(reg)
      .ForInPrepare(triple, 1)
      .ForInContinue(reg, reg)
      .ForInNext(reg, reg, pair, 1)
      .ForInStep(reg);

  // Wide constant pool loads
  for (int i = 0; i < 256; i++) {
    // Emit junk in constant pool to force wide constant pool index.
    builder.LoadLiteral(2.5321 + i);
  }
  builder.LoadLiteral(Smi::FromInt(20000000));
  const AstRawString* wide_name = ast_factory.GetOneByteString("var_wide_name");

  builder.StoreDataPropertyInLiteral(reg, reg,
                                     DataPropertyInLiteralFlag::kNoFlags, 0);

  // Emit wide context operations.
  builder.LoadContextSlot(reg, 1024, 0, BytecodeArrayBuilder::kMutableSlot)
      .StoreContextSlot(reg, 1024, 0);

  // Emit wide load / store lookup slots.
  builder.LoadLookupSlot(wide_name, TypeofMode::NOT_INSIDE_TYPEOF)
      .LoadLookupSlot(wide_name, TypeofMode::INSIDE_TYPEOF)
      .StoreLookupSlot(wide_name, LanguageMode::kSloppy,
                       LookupHoistingMode::kNormal)
      .StoreLookupSlot(wide_name, LanguageMode::kSloppy,
                       LookupHoistingMode::kLegacySloppy)
      .StoreLookupSlot(wide_name, LanguageMode::kStrict,
                       LookupHoistingMode::kNormal);

  // CreateClosureWide
  builder.CreateClosure(1000, 321, NOT_TENURED);

  // Emit wide variant of literal creation operations.
  builder
      .CreateRegExpLiteral(ast_factory.GetOneByteString("wide_literal"), 0, 0)
      .CreateArrayLiteral(0, 0, 0)
      .CreateEmptyArrayLiteral(0)
      .CreateArrayFromIterable()
      .CreateObjectLiteral(0, 0, 0, reg)
      .CreateEmptyObjectLiteral()
      .CloneObject(reg, 0, 0);

  // Emit load and store operations for module variables.
  builder.LoadModuleVariable(-1, 42)
      .LoadModuleVariable(0, 42)
      .LoadModuleVariable(1, 42)
      .StoreModuleVariable(-1, 42)
      .StoreModuleVariable(0, 42)
      .StoreModuleVariable(1, 42);

  // Emit generator operations.
  {
    // We have to skip over suspend because it returns and marks the remaining
    // bytecode dead.
    BytecodeLabel after_suspend;
    builder.JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &after_suspend)
        .SuspendGenerator(reg, reg_list, 0)
        .Bind(&after_suspend)
        .ResumeGenerator(reg, reg_list);
  }
  BytecodeJumpTable* gen_jump_table = builder.AllocateJumpTable(1, 0);
  builder.SwitchOnGeneratorState(reg, gen_jump_table).Bind(gen_jump_table, 0);

  // Intrinsics handled by the interpreter.
  builder.CallRuntime(Runtime::kInlineIsArray, reg_list);

  // Emit debugger bytecode.
  builder.Debugger();

  // Emit abort bytecode.
  {
    BytecodeLabel after;
    builder.Abort(AbortReason::kOperandIsASmi).Bind(&after);
  }

  // Insert dummy ops to force longer jumps.
  for (int i = 0; i < 256; i++) {
    builder.Debugger();
  }

  // Emit block counter increments.
  builder.IncBlockCounter(0);

  // Bind labels for long jumps at the very end.
  for (size_t i = 0; i < arraysize(end); i++) {
    builder.Bind(&end[i]);
  }

  // Return must be the last instruction.
  builder.Return();

  // Generate BytecodeArray.
  scope.SetScriptScopeInfo(factory->NewScopeInfo(1));
  ast_factory.Internalize(isolate());
  Handle<BytecodeArray> the_array = builder.ToBytecodeArray(isolate());
  CHECK_EQ(the_array->frame_size(),
           builder.total_register_count() * kPointerSize);

  // Build scorecard of bytecodes encountered in the BytecodeArray.
  std::vector<int> scorecard(Bytecodes::ToByte(Bytecode::kLast) + 1);

  Bytecode final_bytecode = Bytecode::kLdaZero;
  int i = 0;
  while (i < the_array->length()) {
    uint8_t code = the_array->get(i);
    scorecard[code] += 1;
    final_bytecode = Bytecodes::FromByte(code);
    OperandScale operand_scale = OperandScale::kSingle;
    int prefix_offset = 0;
    if (Bytecodes::IsPrefixScalingBytecode(final_bytecode)) {
      operand_scale = Bytecodes::PrefixBytecodeToOperandScale(final_bytecode);
      prefix_offset = 1;
      code = the_array->get(i + 1);
      scorecard[code] += 1;
      final_bytecode = Bytecodes::FromByte(code);
    }
    i += prefix_offset + Bytecodes::Size(final_bytecode, operand_scale);
  }

  // Insert entry for illegal bytecode as this is never willingly emitted.
  scorecard[Bytecodes::ToByte(Bytecode::kIllegal)] = 1;

  // Bytecode for CollectTypeProfile is only emitted when
  // Type Information for DevTools is turned on.
  scorecard[Bytecodes::ToByte(Bytecode::kCollectTypeProfile)] = 1;

  // Check return occurs at the end and only once in the BytecodeArray.
  CHECK_EQ(final_bytecode, Bytecode::kReturn);
  CHECK_EQ(scorecard[Bytecodes::ToByte(final_bytecode)], 1);

#define CHECK_BYTECODE_PRESENT(Name, ...)                                \
  /* Check Bytecode is marked in scorecard, unless it's a debug break */ \
  if (!Bytecodes::IsDebugBreak(Bytecode::k##Name)) {                     \
    EXPECT_GE(scorecard[Bytecodes::ToByte(Bytecode::k##Name)], 1);       \
  }
  BYTECODE_LIST(CHECK_BYTECODE_PRESENT)
#undef CHECK_BYTECODE_PRESENT
}


TEST_F(BytecodeArrayBuilderTest, FrameSizesLookGood) {
  for (int locals = 0; locals < 5; locals++) {
    for (int temps = 0; temps < 3; temps++) {
      BytecodeArrayBuilder builder(zone(), 1, locals);
      BytecodeRegisterAllocator* allocator(builder.register_allocator());
      for (int i = 0; i < locals; i++) {
        builder.LoadLiteral(Smi::kZero);
        builder.StoreAccumulatorInRegister(Register(i));
      }
      for (int i = 0; i < temps; i++) {
        Register temp = allocator->NewRegister();
        builder.LoadLiteral(Smi::kZero);
        builder.StoreAccumulatorInRegister(temp);
        // Ensure temporaries are used so not optimized away by the
        // register optimizer.
        builder.ToName(temp);
      }
      builder.Return();

      Handle<BytecodeArray> the_array = builder.ToBytecodeArray(isolate());
      int total_registers = locals + temps;
      CHECK_EQ(the_array->frame_size(), total_registers * kPointerSize);
    }
  }
}


TEST_F(BytecodeArrayBuilderTest, RegisterValues) {
  int index = 1;

  Register the_register(index);
  CHECK_EQ(the_register.index(), index);

  int actual_operand = the_register.ToOperand();
  int actual_index = Register::FromOperand(actual_operand).index();
  CHECK_EQ(actual_index, index);
}


TEST_F(BytecodeArrayBuilderTest, Parameters) {
  BytecodeArrayBuilder builder(zone(), 10, 0);

  Register receiver(builder.Receiver());
  Register param8(builder.Parameter(8));
  CHECK_EQ(param8.index() - receiver.index(), 9);
}


TEST_F(BytecodeArrayBuilderTest, Constants) {
  BytecodeArrayBuilder builder(zone(), 1, 0);
  AstValueFactory ast_factory(zone(), isolate()->ast_string_constants(),
                              isolate()->heap()->HashSeed());

  double heap_num_1 = 3.14;
  double heap_num_2 = 5.2;
  double nan = std::numeric_limits<double>::quiet_NaN();
  const AstRawString* string = ast_factory.GetOneByteString("foo");
  const AstRawString* string_copy = ast_factory.GetOneByteString("foo");

  builder.LoadLiteral(heap_num_1)
      .LoadLiteral(heap_num_2)
      .LoadLiteral(string)
      .LoadLiteral(heap_num_1)
      .LoadLiteral(heap_num_1)
      .LoadLiteral(nan)
      .LoadLiteral(string_copy)
      .LoadLiteral(heap_num_2)
      .LoadLiteral(nan)
      .Return();

  ast_factory.Internalize(isolate());
  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  // Should only have one entry for each identical constant.
  EXPECT_EQ(4, array->constant_pool()->length());
}

TEST_F(BytecodeArrayBuilderTest, ForwardJumps) {
  static const int kFarJumpDistance = 256 + 20;

  BytecodeArrayBuilder builder(zone(), 1, 1);

  Register reg(0);
  BytecodeLabel far0, far1, far2, far3, far4;
  BytecodeLabel near0, near1, near2, near3, near4;
  BytecodeLabel after_jump0, after_jump1;

  builder.Jump(&near0)
      .Bind(&after_jump0)
      .CompareOperation(Token::Value::EQ, reg, 1)
      .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &near1)
      .CompareOperation(Token::Value::EQ, reg, 2)
      .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &near2)
      .BinaryOperation(Token::Value::ADD, reg, 1)
      .JumpIfTrue(ToBooleanMode::kConvertToBoolean, &near3)
      .BinaryOperation(Token::Value::ADD, reg, 2)
      .JumpIfFalse(ToBooleanMode::kConvertToBoolean, &near4)
      .Bind(&near0)
      .Bind(&near1)
      .Bind(&near2)
      .Bind(&near3)
      .Bind(&near4)
      .Jump(&far0)
      .Bind(&after_jump1)
      .CompareOperation(Token::Value::EQ, reg, 3)
      .JumpIfTrue(ToBooleanMode::kAlreadyBoolean, &far1)
      .CompareOperation(Token::Value::EQ, reg, 4)
      .JumpIfFalse(ToBooleanMode::kAlreadyBoolean, &far2)
      .BinaryOperation(Token::Value::ADD, reg, 3)
      .JumpIfTrue(ToBooleanMode::kConvertToBoolean, &far3)
      .BinaryOperation(Token::Value::ADD, reg, 4)
      .JumpIfFalse(ToBooleanMode::kConvertToBoolean, &far4);
  for (int i = 0; i < kFarJumpDistance - 22; i++) {
    builder.Debugger();
  }
  builder.Bind(&far0).Bind(&far1).Bind(&far2).Bind(&far3).Bind(&far4);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  DCHECK_EQ(array->length(), 44 + kFarJumpDistance - 22 + 1);

  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 22);
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 17);
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalse);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 12);
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrue);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 7);
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanFalse);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 2);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpConstant);
  CHECK_EQ(iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance));
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfTrueConstant);
  CHECK_EQ(iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 5));
  iterator.Advance();

  // Ignore compare operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfFalseConstant);
  CHECK_EQ(iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 10));
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpIfToBooleanTrueConstant);
  CHECK_EQ(iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 15));
  iterator.Advance();

  // Ignore add operation.
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(),
           Bytecode::kJumpIfToBooleanFalseConstant);
  CHECK_EQ(iterator.GetConstantForIndexOperand(0),
           Smi::FromInt(kFarJumpDistance - 20));
  iterator.Advance();
}


TEST_F(BytecodeArrayBuilderTest, BackwardJumps) {
  BytecodeArrayBuilder builder(zone(), 1, 1);

  Register reg(0);

  BytecodeLabel label0;
  builder.Bind(&label0).JumpLoop(&label0, 0);
  for (int i = 0; i < 42; i++) {
    BytecodeLabel after_jump;
    builder.JumpLoop(&label0, 0).Bind(&after_jump);
  }

  // Add padding to force wide backwards jumps.
  for (int i = 0; i < 256; i++) {
    builder.Debugger();
  }

  builder.JumpLoop(&label0, 0);
  BytecodeLabel end;
  builder.Bind(&end);
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 0);
  iterator.Advance();
  for (unsigned i = 0; i < 42; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);
    CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
    // offset of 3 (because kJumpLoop takes two immediate operands)
    CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), i * 3 + 3);
    iterator.Advance();
  }
  // Check padding to force wide backwards jumps.
  for (int i = 0; i < 256; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
    iterator.Advance();
  }
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kDouble);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 386);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}

TEST_F(BytecodeArrayBuilderTest, SmallSwitch) {
  BytecodeArrayBuilder builder(zone(), 1, 1);

  // Small jump table that fits into the single-size constant pool
  int small_jump_table_size = 5;
  int small_jump_table_base = -2;
  BytecodeJumpTable* small_jump_table =
      builder.AllocateJumpTable(small_jump_table_size, small_jump_table_base);

  builder.LoadLiteral(Smi::FromInt(7)).SwitchOnSmiNoFeedback(small_jump_table);
  for (int i = 0; i < small_jump_table_size; i++) {
    builder.Bind(small_jump_table, small_jump_table_base + i).Debugger();
  }
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  BytecodeArrayIterator iterator(array);

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kSwitchOnSmiNoFeedback);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kSingle);
  {
    int i = 0;
    int switch_end =
        iterator.current_offset() + iterator.current_bytecode_size();

    for (const auto& entry : iterator.GetJumpTableTargetOffsets()) {
      CHECK_EQ(entry.case_value, small_jump_table_base + i);
      CHECK_EQ(entry.target_offset, switch_end + i);

      i++;
    }
    CHECK_EQ(i, small_jump_table_size);
  }
  iterator.Advance();

  for (int i = 0; i < small_jump_table_size; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
    iterator.Advance();
  }

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}

TEST_F(BytecodeArrayBuilderTest, WideSwitch) {
  BytecodeArrayBuilder builder(zone(), 1, 1);

  // Large jump table that requires a wide Switch bytecode.
  int large_jump_table_size = 256;
  int large_jump_table_base = -10;
  BytecodeJumpTable* large_jump_table =
      builder.AllocateJumpTable(large_jump_table_size, large_jump_table_base);

  builder.LoadLiteral(Smi::FromInt(7)).SwitchOnSmiNoFeedback(large_jump_table);
  for (int i = 0; i < large_jump_table_size; i++) {
    builder.Bind(large_jump_table, large_jump_table_base + i).Debugger();
  }
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  BytecodeArrayIterator iterator(array);

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kLdaSmi);
  iterator.Advance();

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kSwitchOnSmiNoFeedback);
  CHECK_EQ(iterator.current_operand_scale(), OperandScale::kDouble);
  {
    int i = 0;
    int switch_end =
        iterator.current_offset() + iterator.current_bytecode_size();

    for (const auto& entry : iterator.GetJumpTableTargetOffsets()) {
      CHECK_EQ(entry.case_value, large_jump_table_base + i);
      CHECK_EQ(entry.target_offset, switch_end + i);

      i++;
    }
    CHECK_EQ(i, large_jump_table_size);
  }
  iterator.Advance();

  for (int i = 0; i < large_jump_table_size; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kDebugger);
    iterator.Advance();
  }

  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}

TEST_F(BytecodeArrayBuilderTest, LabelReuse) {
  BytecodeArrayBuilder builder(zone(), 1, 0);

  // Labels can only have 1 forward reference, but
  // can be referred to mulitple times once bound.
  BytecodeLabel label, after_jump0, after_jump1;

  builder.Jump(&label)
      .Bind(&label)
      .JumpLoop(&label, 0)
      .Bind(&after_jump0)
      .JumpLoop(&label, 0)
      .Bind(&after_jump1)
      .Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  BytecodeArrayIterator iterator(array);
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 2);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 0);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);
  CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 3);
  iterator.Advance();
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}


TEST_F(BytecodeArrayBuilderTest, LabelAddressReuse) {
  static const int kRepeats = 3;

  BytecodeArrayBuilder builder(zone(), 1, 0);
  for (int i = 0; i < kRepeats; i++) {
    BytecodeLabel label, after_jump0, after_jump1;
    builder.Jump(&label)
        .Bind(&label)
        .JumpLoop(&label, 0)
        .Bind(&after_jump0)
        .JumpLoop(&label, 0)
        .Bind(&after_jump1);
  }
  builder.Return();

  Handle<BytecodeArray> array = builder.ToBytecodeArray(isolate());
  BytecodeArrayIterator iterator(array);
  for (int i = 0; i < kRepeats; i++) {
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJump);
    CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 2);
    iterator.Advance();
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);
    CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 0);
    iterator.Advance();
    CHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);
    CHECK_EQ(iterator.GetUnsignedImmediateOperand(0), 3);
    iterator.Advance();
  }
  CHECK_EQ(iterator.current_bytecode(), Bytecode::kReturn);
  iterator.Advance();
  CHECK(iterator.done());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
