// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_WRITER_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_WRITER_H_

#include "src/base/compiler-specific.h"
#include "src/globals.h"
#include "src/interpreter/bytecode-pipeline.h"
#include "src/source-position-table.h"

namespace v8 {
namespace internal {

class SourcePositionTableBuilder;

namespace interpreter {

class BytecodeLabel;
class ConstantArrayBuilder;

// Class for emitting bytecode as the final stage of the bytecode
// generation pipeline.
class V8_EXPORT_PRIVATE BytecodeArrayWriter final
    : public NON_EXPORTED_BASE(BytecodePipelineStage) {
 public:
  BytecodeArrayWriter(
      Zone* zone, ConstantArrayBuilder* constant_array_builder,
      SourcePositionTableBuilder::RecordingMode source_position_mode);
  virtual ~BytecodeArrayWriter();

  // BytecodePipelineStage interface.
  void Write(BytecodeNode* node) override;
  void WriteJump(BytecodeNode* node, BytecodeLabel* label) override;
  void BindLabel(BytecodeLabel* label) override;
  void BindLabel(const BytecodeLabel& target, BytecodeLabel* label) override;
  Handle<BytecodeArray> ToBytecodeArray(
      Isolate* isolate, int register_count, int parameter_count,
      Handle<FixedArray> handler_table) override;

 private:
  // Maximum sized packed bytecode is comprised of a prefix bytecode,
  // plus the actual bytecode, plus the maximum number of operands times
  // the maximum operand size.
  static const size_t kMaxSizeOfPackedBytecode =
      2 * sizeof(Bytecode) +
      Bytecodes::kMaxOperands * static_cast<size_t>(OperandSize::kLast);

  // Constants that act as placeholders for jump operands to be
  // patched. These have operand sizes that match the sizes of
  // reserved constant pool entries.
  const uint32_t k8BitJumpPlaceholder = 0x7f;
  const uint32_t k16BitJumpPlaceholder =
      k8BitJumpPlaceholder | (k8BitJumpPlaceholder << 8);
  const uint32_t k32BitJumpPlaceholder =
      k16BitJumpPlaceholder | (k16BitJumpPlaceholder << 16);

  void PatchJump(size_t jump_target, size_t jump_location);
  void PatchJumpWith8BitOperand(size_t jump_location, int delta);
  void PatchJumpWith16BitOperand(size_t jump_location, int delta);
  void PatchJumpWith32BitOperand(size_t jump_location, int delta);

  void EmitBytecode(const BytecodeNode* const node);
  void EmitJump(BytecodeNode* node, BytecodeLabel* label);
  void UpdateSourcePositionTable(const BytecodeNode* const node);

  ZoneVector<uint8_t>* bytecodes() { return &bytecodes_; }
  SourcePositionTableBuilder* source_position_table_builder() {
    return &source_position_table_builder_;
  }
  ConstantArrayBuilder* constant_array_builder() {
    return constant_array_builder_;
  }

  ZoneVector<uint8_t> bytecodes_;
  int unbound_jumps_;
  SourcePositionTableBuilder source_position_table_builder_;
  ConstantArrayBuilder* constant_array_builder_;

  friend class BytecodeArrayWriterUnittest;
  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayWriter);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_WRITER_H_
