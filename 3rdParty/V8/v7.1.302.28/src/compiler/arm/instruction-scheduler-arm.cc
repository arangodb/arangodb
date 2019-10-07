// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

bool InstructionScheduler::SchedulerSupported() { return true; }


int InstructionScheduler::GetTargetInstructionFlags(
    const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kArmAdd:
    case kArmAnd:
    case kArmBic:
    case kArmClz:
    case kArmCmp:
    case kArmCmn:
    case kArmTst:
    case kArmTeq:
    case kArmOrr:
    case kArmEor:
    case kArmSub:
    case kArmRsb:
    case kArmMul:
    case kArmMla:
    case kArmMls:
    case kArmSmmul:
    case kArmSmull:
    case kArmSmmla:
    case kArmUmull:
    case kArmSdiv:
    case kArmUdiv:
    case kArmMov:
    case kArmMvn:
    case kArmBfc:
    case kArmUbfx:
    case kArmSbfx:
    case kArmSxtb:
    case kArmSxth:
    case kArmSxtab:
    case kArmSxtah:
    case kArmUxtb:
    case kArmUxth:
    case kArmUxtab:
    case kArmUxtah:
    case kArmRbit:
    case kArmRev:
    case kArmAddPair:
    case kArmSubPair:
    case kArmMulPair:
    case kArmLslPair:
    case kArmLsrPair:
    case kArmAsrPair:
    case kArmVcmpF32:
    case kArmVaddF32:
    case kArmVsubF32:
    case kArmVmulF32:
    case kArmVmlaF32:
    case kArmVmlsF32:
    case kArmVdivF32:
    case kArmVabsF32:
    case kArmVnegF32:
    case kArmVsqrtF32:
    case kArmVcmpF64:
    case kArmVaddF64:
    case kArmVsubF64:
    case kArmVmulF64:
    case kArmVmlaF64:
    case kArmVmlsF64:
    case kArmVdivF64:
    case kArmVmodF64:
    case kArmVabsF64:
    case kArmVnegF64:
    case kArmVsqrtF64:
    case kArmVrintmF32:
    case kArmVrintmF64:
    case kArmVrintpF32:
    case kArmVrintpF64:
    case kArmVrintzF32:
    case kArmVrintzF64:
    case kArmVrintaF64:
    case kArmVrintnF32:
    case kArmVrintnF64:
    case kArmVcvtF32F64:
    case kArmVcvtF64F32:
    case kArmVcvtF32S32:
    case kArmVcvtF32U32:
    case kArmVcvtF64S32:
    case kArmVcvtF64U32:
    case kArmVcvtS32F32:
    case kArmVcvtU32F32:
    case kArmVcvtS32F64:
    case kArmVcvtU32F64:
    case kArmVmovU32F32:
    case kArmVmovF32U32:
    case kArmVmovLowU32F64:
    case kArmVmovLowF64U32:
    case kArmVmovHighU32F64:
    case kArmVmovHighF64U32:
    case kArmVmovF64U32U32:
    case kArmVmovU32U32F64:
    case kArmFloat32Max:
    case kArmFloat64Max:
    case kArmFloat32Min:
    case kArmFloat64Min:
    case kArmFloat64SilenceNaN:
    case kArmF32x4Splat:
    case kArmF32x4ExtractLane:
    case kArmF32x4ReplaceLane:
    case kArmF32x4SConvertI32x4:
    case kArmF32x4UConvertI32x4:
    case kArmF32x4Abs:
    case kArmF32x4Neg:
    case kArmF32x4RecipApprox:
    case kArmF32x4RecipSqrtApprox:
    case kArmF32x4Add:
    case kArmF32x4AddHoriz:
    case kArmF32x4Sub:
    case kArmF32x4Mul:
    case kArmF32x4Min:
    case kArmF32x4Max:
    case kArmF32x4Eq:
    case kArmF32x4Ne:
    case kArmF32x4Lt:
    case kArmF32x4Le:
    case kArmI32x4Splat:
    case kArmI32x4ExtractLane:
    case kArmI32x4ReplaceLane:
    case kArmI32x4SConvertF32x4:
    case kArmI32x4SConvertI16x8Low:
    case kArmI32x4SConvertI16x8High:
    case kArmI32x4Neg:
    case kArmI32x4Shl:
    case kArmI32x4ShrS:
    case kArmI32x4Add:
    case kArmI32x4AddHoriz:
    case kArmI32x4Sub:
    case kArmI32x4Mul:
    case kArmI32x4MinS:
    case kArmI32x4MaxS:
    case kArmI32x4Eq:
    case kArmI32x4Ne:
    case kArmI32x4GtS:
    case kArmI32x4GeS:
    case kArmI32x4UConvertF32x4:
    case kArmI32x4UConvertI16x8Low:
    case kArmI32x4UConvertI16x8High:
    case kArmI32x4ShrU:
    case kArmI32x4MinU:
    case kArmI32x4MaxU:
    case kArmI32x4GtU:
    case kArmI32x4GeU:
    case kArmI16x8Splat:
    case kArmI16x8ExtractLane:
    case kArmI16x8ReplaceLane:
    case kArmI16x8SConvertI8x16Low:
    case kArmI16x8SConvertI8x16High:
    case kArmI16x8Neg:
    case kArmI16x8Shl:
    case kArmI16x8ShrS:
    case kArmI16x8SConvertI32x4:
    case kArmI16x8Add:
    case kArmI16x8AddSaturateS:
    case kArmI16x8AddHoriz:
    case kArmI16x8Sub:
    case kArmI16x8SubSaturateS:
    case kArmI16x8Mul:
    case kArmI16x8MinS:
    case kArmI16x8MaxS:
    case kArmI16x8Eq:
    case kArmI16x8Ne:
    case kArmI16x8GtS:
    case kArmI16x8GeS:
    case kArmI16x8UConvertI8x16Low:
    case kArmI16x8UConvertI8x16High:
    case kArmI16x8ShrU:
    case kArmI16x8UConvertI32x4:
    case kArmI16x8AddSaturateU:
    case kArmI16x8SubSaturateU:
    case kArmI16x8MinU:
    case kArmI16x8MaxU:
    case kArmI16x8GtU:
    case kArmI16x8GeU:
    case kArmI8x16Splat:
    case kArmI8x16ExtractLane:
    case kArmI8x16ReplaceLane:
    case kArmI8x16Neg:
    case kArmI8x16Shl:
    case kArmI8x16ShrS:
    case kArmI8x16SConvertI16x8:
    case kArmI8x16Add:
    case kArmI8x16AddSaturateS:
    case kArmI8x16Sub:
    case kArmI8x16SubSaturateS:
    case kArmI8x16Mul:
    case kArmI8x16MinS:
    case kArmI8x16MaxS:
    case kArmI8x16Eq:
    case kArmI8x16Ne:
    case kArmI8x16GtS:
    case kArmI8x16GeS:
    case kArmI8x16UConvertI16x8:
    case kArmI8x16AddSaturateU:
    case kArmI8x16SubSaturateU:
    case kArmI8x16ShrU:
    case kArmI8x16MinU:
    case kArmI8x16MaxU:
    case kArmI8x16GtU:
    case kArmI8x16GeU:
    case kArmS128Zero:
    case kArmS128Dup:
    case kArmS128And:
    case kArmS128Or:
    case kArmS128Xor:
    case kArmS128Not:
    case kArmS128Select:
    case kArmS32x4ZipLeft:
    case kArmS32x4ZipRight:
    case kArmS32x4UnzipLeft:
    case kArmS32x4UnzipRight:
    case kArmS32x4TransposeLeft:
    case kArmS32x4TransposeRight:
    case kArmS32x4Shuffle:
    case kArmS16x8ZipLeft:
    case kArmS16x8ZipRight:
    case kArmS16x8UnzipLeft:
    case kArmS16x8UnzipRight:
    case kArmS16x8TransposeLeft:
    case kArmS16x8TransposeRight:
    case kArmS8x16ZipLeft:
    case kArmS8x16ZipRight:
    case kArmS8x16UnzipLeft:
    case kArmS8x16UnzipRight:
    case kArmS8x16TransposeLeft:
    case kArmS8x16TransposeRight:
    case kArmS8x16Concat:
    case kArmS8x16Shuffle:
    case kArmS32x2Reverse:
    case kArmS16x4Reverse:
    case kArmS16x2Reverse:
    case kArmS8x8Reverse:
    case kArmS8x4Reverse:
    case kArmS8x2Reverse:
    case kArmS1x4AnyTrue:
    case kArmS1x4AllTrue:
    case kArmS1x8AnyTrue:
    case kArmS1x8AllTrue:
    case kArmS1x16AnyTrue:
    case kArmS1x16AllTrue:
      return kNoOpcodeFlags;

    case kArmVldrF32:
    case kArmVldrF64:
    case kArmVld1F64:
    case kArmVld1S128:
    case kArmLdrb:
    case kArmLdrsb:
    case kArmLdrh:
    case kArmLdrsh:
    case kArmLdr:
    case kArmPeek:
    case kArmWord32AtomicPairLoad:
      return kIsLoadOperation;

    case kArmVstrF32:
    case kArmVstrF64:
    case kArmVst1F64:
    case kArmVst1S128:
    case kArmStrb:
    case kArmStrh:
    case kArmStr:
    case kArmPush:
    case kArmPoke:
    case kArmDsbIsb:
    case kArmWord32AtomicPairStore:
    case kArmWord32AtomicPairAdd:
    case kArmWord32AtomicPairSub:
    case kArmWord32AtomicPairAnd:
    case kArmWord32AtomicPairOr:
    case kArmWord32AtomicPairXor:
    case kArmWord32AtomicPairExchange:
    case kArmWord32AtomicPairCompareExchange:
      return kHasSideEffect;

#define CASE(Name) case k##Name:
    COMMON_ARCH_OPCODE_LIST(CASE)
#undef CASE
      // Already covered in architecture independent code.
      UNREACHABLE();
  }

  UNREACHABLE();
}


int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  // TODO(all): Add instruction cost modeling.
  return 1;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
