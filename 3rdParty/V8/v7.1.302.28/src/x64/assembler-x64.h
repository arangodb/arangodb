// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

// A lightweight X64 Assembler.

#ifndef V8_X64_ASSEMBLER_X64_H_
#define V8_X64_ASSEMBLER_X64_H_

#include <deque>
#include <forward_list>
#include <vector>

#include "src/assembler.h"
#include "src/x64/constants-x64.h"
#include "src/x64/sse-instr.h"

namespace v8 {
namespace internal {

// Utility functions

#define GENERAL_REGISTERS(V) \
  V(rax)                     \
  V(rcx)                     \
  V(rdx)                     \
  V(rbx)                     \
  V(rsp)                     \
  V(rbp)                     \
  V(rsi)                     \
  V(rdi)                     \
  V(r8)                      \
  V(r9)                      \
  V(r10)                     \
  V(r11)                     \
  V(r12)                     \
  V(r13)                     \
  V(r14)                     \
  V(r15)

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(rax)                                 \
  V(rbx)                                 \
  V(rdx)                                 \
  V(rcx)                                 \
  V(rsi)                                 \
  V(rdi)                                 \
  V(r8)                                  \
  V(r9)                                  \
  V(r11)                                 \
  V(r12)                                 \
  V(r14)                                 \
  V(r15)

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
  bool is_byte_register() const { return reg_code_ <= 3; }
  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const { return reg_code_ >> 3; }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const { return reg_code_ & 0x7; }

 private:
  friend class RegisterBase<Register, kRegAfterLast>;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(Register);
static_assert(sizeof(Register) == sizeof(int),
              "Register can efficiently be passed by value");

#define DECLARE_REGISTER(R) \
  constexpr Register R = Register::from_code<kRegCode_##R>();
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
constexpr Register no_reg = Register::no_reg();

constexpr int kNumRegs = 16;

constexpr RegList kJSCallerSaved =
    Register::ListOf<rax, rcx, rdx,
                     rbx,  // used as a caller-saved register in JavaScript code
                     rdi   // callee function
                     >();

constexpr int kNumJSCallerSaved = 5;

// Number of registers for which space is reserved in safepoints.
constexpr int kNumSafepointRegisters = 16;

#ifdef _WIN64
  // Windows calling convention
constexpr Register arg_reg_1 = rcx;
constexpr Register arg_reg_2 = rdx;
constexpr Register arg_reg_3 = r8;
constexpr Register arg_reg_4 = r9;
#else
  // AMD64 calling convention
constexpr Register arg_reg_1 = rdi;
constexpr Register arg_reg_2 = rsi;
constexpr Register arg_reg_3 = rdx;
constexpr Register arg_reg_4 = rcx;
#endif  // _WIN64


#define DOUBLE_REGISTERS(V) \
  V(xmm0)                   \
  V(xmm1)                   \
  V(xmm2)                   \
  V(xmm3)                   \
  V(xmm4)                   \
  V(xmm5)                   \
  V(xmm6)                   \
  V(xmm7)                   \
  V(xmm8)                   \
  V(xmm9)                   \
  V(xmm10)                  \
  V(xmm11)                  \
  V(xmm12)                  \
  V(xmm13)                  \
  V(xmm14)                  \
  V(xmm15)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

#define ALLOCATABLE_DOUBLE_REGISTERS(V) \
  V(xmm0)                               \
  V(xmm1)                               \
  V(xmm2)                               \
  V(xmm3)                               \
  V(xmm4)                               \
  V(xmm5)                               \
  V(xmm6)                               \
  V(xmm7)                               \
  V(xmm8)                               \
  V(xmm9)                               \
  V(xmm10)                              \
  V(xmm11)                              \
  V(xmm12)                              \
  V(xmm13)                              \
  V(xmm14)

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

class XMMRegister : public RegisterBase<XMMRegister, kDoubleAfterLast> {
 public:
  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const { return reg_code_ >> 3; }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const { return reg_code_ & 0x7; }

 private:
  friend class RegisterBase<XMMRegister, kDoubleAfterLast>;
  explicit constexpr XMMRegister(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(XMMRegister);
static_assert(sizeof(XMMRegister) == sizeof(int),
              "XMMRegister can efficiently be passed by value");

typedef XMMRegister FloatRegister;

typedef XMMRegister DoubleRegister;

typedef XMMRegister Simd128Register;

#define DECLARE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code<kDoubleCode_##R>();
DOUBLE_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

enum Condition {
  // any value < 0 is considered no_condition
  no_condition  = -1,

  overflow      =  0,
  no_overflow   =  1,
  below         =  2,
  above_equal   =  3,
  equal         =  4,
  not_equal     =  5,
  below_equal   =  6,
  above         =  7,
  negative      =  8,
  positive      =  9,
  parity_even   = 10,
  parity_odd    = 11,
  less          = 12,
  greater_equal = 13,
  less_equal    = 14,
  greater       = 15,

  // Fake conditions that are handled by the
  // opcodes using them.
  always        = 16,
  never         = 17,
  // aliases
  carry         = below,
  not_carry     = above_equal,
  zero          = equal,
  not_zero      = not_equal,
  sign          = negative,
  not_sign      = positive,
  last_condition = greater
};


// Returns the equivalent of !cc.
// Negation of the default no_condition (-1) results in a non-default
// no_condition value (-2). As long as tests for no_condition check
// for condition < 0, this will work as expected.
inline Condition NegateCondition(Condition cc) {
  return static_cast<Condition>(cc ^ 1);
}


enum RoundingMode {
  kRoundToNearest = 0x0,
  kRoundDown = 0x1,
  kRoundUp = 0x2,
  kRoundToZero = 0x3
};


// -----------------------------------------------------------------------------
// Machine instruction Immediates

class Immediate {
 public:
  explicit constexpr Immediate(int32_t value) : value_(value) {}
  explicit constexpr Immediate(int32_t value, RelocInfo::Mode rmode)
      : value_(value), rmode_(rmode) {}
  explicit Immediate(Smi* value)
      : value_(static_cast<int32_t>(reinterpret_cast<intptr_t>(value))) {
    DCHECK(SmiValuesAre31Bits());  // Only available for 31-bit SMI.
  }

 private:
  const int32_t value_;
  const RelocInfo::Mode rmode_ = RelocInfo::NONE;

  friend class Assembler;
};
ASSERT_TRIVIALLY_COPYABLE(Immediate);
static_assert(sizeof(Immediate) <= kPointerSize,
              "Immediate must be small enough to pass it by value");

// -----------------------------------------------------------------------------
// Machine instruction Operands

enum ScaleFactor : int8_t {
  times_1 = 0,
  times_2 = 1,
  times_4 = 2,
  times_8 = 3,
  times_int_size = times_4,
  times_pointer_size = (kPointerSize == 8) ? times_8 : times_4
};

class Operand {
 public:
  struct Data {
    byte rex = 0;
    byte buf[9];
    byte len = 1;   // number of bytes of buf_ in use.
    int8_t addend;  // for rip + offset + addend.
  };

  // [base + disp/r]
  Operand(Register base, int32_t disp);

  // [base + index*scale + disp/r]
  Operand(Register base,
          Register index,
          ScaleFactor scale,
          int32_t disp);

  // [index*scale + disp/r]
  Operand(Register index,
          ScaleFactor scale,
          int32_t disp);

  // Offset from existing memory operand.
  // Offset is added to existing displacement as 32-bit signed values and
  // this must not overflow.
  Operand(Operand base, int32_t offset);

  // [rip + disp/r]
  explicit Operand(Label* label, int addend = 0);

  Operand(const Operand&) = default;

  // Checks whether either base or index register is the given register.
  // Does not check the "reg" part of the Operand.
  bool AddressUsesRegister(Register reg) const;

  // Queries related to the size of the generated instruction.
  // Whether the generated instruction will have a REX prefix.
  bool requires_rex() const { return data_.rex != 0; }
  // Size of the ModR/M, SIB and displacement parts of the generated
  // instruction.
  int operand_size() const { return data_.len; }

  const Data& data() const { return data_; }

 private:
  const Data data_;
};
ASSERT_TRIVIALLY_COPYABLE(Operand);
static_assert(sizeof(Operand) <= 2 * kPointerSize,
              "Operand must be small enough to pass it by value");

#define ASSEMBLER_INSTRUCTION_LIST(V) \
  V(add)                              \
  V(and)                              \
  V(cmp)                              \
  V(cmpxchg)                          \
  V(dec)                              \
  V(idiv)                             \
  V(div)                              \
  V(imul)                             \
  V(inc)                              \
  V(lea)                              \
  V(mov)                              \
  V(movzxb)                           \
  V(movzxw)                           \
  V(neg)                              \
  V(not)                              \
  V(or)                               \
  V(repmovs)                          \
  V(sbb)                              \
  V(sub)                              \
  V(test)                             \
  V(xchg)                             \
  V(xor)

// Shift instructions on operands/registers with kPointerSize, kInt32Size and
// kInt64Size.
#define SHIFT_INSTRUCTION_LIST(V) \
  V(rol, 0x0)                     \
  V(ror, 0x1)                     \
  V(rcl, 0x2)                     \
  V(rcr, 0x3)                     \
  V(shl, 0x4)                     \
  V(shr, 0x5)                     \
  V(sar, 0x7)

// Partial Constant Pool
// Different from complete constant pool (like arm does), partial constant pool
// only takes effects for shareable constants in order to reduce code size.
// Partial constant pool does not emit constant pool entries at the end of each
// code object. Instead, it keeps the first shareable constant inlined in the
// instructions and uses rip-relative memory loadings for the same constants in
// subsequent instructions. These rip-relative memory loadings will target at
// the position of the first inlined constant. For example:
//
//  REX.W movq r10,0x7f9f75a32c20   ; 10 bytes
//  …
//  REX.W movq r10,0x7f9f75a32c20   ; 10 bytes
//  …
//
// turns into
//
//  REX.W movq r10,0x7f9f75a32c20   ; 10 bytes
//  …
//  REX.W movq r10,[rip+0xffffff96] ; 7 bytes
//  …

class ConstPool {
 public:
  explicit ConstPool(Assembler* assm) : assm_(assm) {}
  // Returns true when partial constant pool is valid for this entry.
  bool TryRecordEntry(intptr_t data, RelocInfo::Mode mode);
  bool IsEmpty() const { return entries_.empty(); }

  void PatchEntries();
  // Discard any pending pool entries.
  void Clear();

 private:
  // Adds a shared entry to entries_. Returns true if this is not the first time
  // we add this entry, false otherwise.
  bool AddSharedEntry(uint64_t data, int offset);

  // Check if the instruction is a rip-relative move.
  bool IsMoveRipRelative(byte* instr);

  Assembler* assm_;

  // Values, pc offsets of entries.
  typedef std::multimap<uint64_t, int> EntryMap;
  EntryMap entries_;

  // Number of bytes taken up by the displacement of rip-relative addressing.
  static constexpr int kRipRelativeDispSize = 4;  // 32-bit displacement.
  // Distance between the address of the displacement in the rip-relative move
  // instruction and the head address of the instruction.
  static constexpr int kMoveRipRelativeDispOffset =
      3;  // REX Opcode ModRM Displacement
  // Distance between the address of the imm64 in the 'movq reg, imm64'
  // instruction and the head address of the instruction.
  static constexpr int kMoveImm64Offset = 2;  // REX Opcode imm64
  // A mask for rip-relative move instruction.
  static constexpr uint32_t kMoveRipRelativeMask = 0x00C7FFFB;
  // The bits for a rip-relative move instruction after mask.
  static constexpr uint32_t kMoveRipRelativeInstr = 0x00058B48;
};

class V8_EXPORT_PRIVATE Assembler : public AssemblerBase {
 private:
  // We check before assembling an instruction that there is sufficient
  // space to write an instruction and its relocation information.
  // The relocation writer's position must be kGap bytes above the end of
  // the generated instructions. This leaves enough space for the
  // longest possible x64 instruction, 15 bytes, and the longest possible
  // relocation information encoding, RelocInfoWriter::kMaxLength == 16.
  // (There is a 15 byte limit on x64 instruction length that rules out some
  // otherwise valid instructions.)
  // This allows for a single, fast space check per instruction.
  static constexpr int kGap = 32;

 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is nullptr, the assembler allocates and grows its
  // own buffer, and buffer_size determines the initial buffer size. The buffer
  // is owned by the assembler and deallocated upon destruction of the
  // assembler.
  //
  // If the provided buffer is not nullptr, the assembler uses the provided
  // buffer for code generation and assumes its size to be buffer_size. If the
  // buffer is too small, a fatal error occurs. No deallocation of the buffer is
  // done upon destruction of the assembler.
  Assembler(const AssemblerOptions& options, void* buffer, int buffer_size);
  ~Assembler() override = default;

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(Isolate* isolate, CodeDesc* desc);

  // Read/Modify the code target in the relative branch/call instruction at pc.
  // On the x64 architecture, we use relative jumps with a 32-bit displacement
  // to jump to other Code objects in the Code space in the heap.
  // Jumps to C functions are done indirectly through a 64-bit register holding
  // the absolute address of the target.
  // These functions convert between absolute Addresses of Code objects and
  // the relative displacements stored in the code.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  static inline Address target_address_at(Address pc, Address constant_pool);
  static inline void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  static inline Address target_address_from_return_address(Address pc);

  // This sets the branch destination (which is in the instruction on x64).
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Code* code, Address target);

  // Get the size of the special target encoded at 'instruction_payload'.
  inline static int deserialization_special_target_size(
      Address instruction_payload);

  // This sets the internal reference at the pc.
  inline static void deserialization_set_target_internal_reference_at(
      Address pc, Address target,
      RelocInfo::Mode mode = RelocInfo::INTERNAL_REFERENCE);

  inline Handle<Code> code_target_object_handle_at(Address pc);
  inline Address runtime_entry_at(Address pc);

  // Number of bytes taken up by the branch target in the code.
  static constexpr int kSpecialTargetSize = 4;  // 32-bit displacement.
  // Distance between the address of the code target in the call instruction
  // and the return address pushed on the stack.
  static constexpr int kCallTargetAddressOffset = 4;  // 32-bit displacement.
  // The length of call(kScratchRegister).
  static constexpr int kCallScratchRegisterInstructionLength = 3;
  // The length of call(Immediate32).
  static constexpr int kShortCallInstructionLength = 5;
  // The length of movq(kScratchRegister, address).
  static constexpr int kMoveAddressIntoScratchRegisterInstructionLength =
      2 + kPointerSize;
  // The length of movq(kScratchRegister, address) and call(kScratchRegister).
  static constexpr int kCallSequenceLength =
      kMoveAddressIntoScratchRegisterInstructionLength +
      kCallScratchRegisterInstructionLength;

  // One byte opcode for test eax,0xXXXXXXXX.
  static constexpr byte kTestEaxByte = 0xA9;
  // One byte opcode for test al, 0xXX.
  static constexpr byte kTestAlByte = 0xA8;
  // One byte opcode for nop.
  static constexpr byte kNopByte = 0x90;

  // One byte prefix for a short conditional jump.
  static constexpr byte kJccShortPrefix = 0x70;
  static constexpr byte kJncShortOpcode = kJccShortPrefix | not_carry;
  static constexpr byte kJcShortOpcode = kJccShortPrefix | carry;
  static constexpr byte kJnzShortOpcode = kJccShortPrefix | not_zero;
  static constexpr byte kJzShortOpcode = kJccShortPrefix | zero;

  // VEX prefix encodings.
  enum SIMDPrefix { kNone = 0x0, k66 = 0x1, kF3 = 0x2, kF2 = 0x3 };
  enum VectorLength { kL128 = 0x0, kL256 = 0x4, kLIG = kL128, kLZ = kL128 };
  enum VexW { kW0 = 0x0, kW1 = 0x80, kWIG = kW0 };
  enum LeadingOpcode { k0F = 0x1, k0F38 = 0x2, k0F3A = 0x3 };

  // ---------------------------------------------------------------------------
  // Code generation
  //
  // Function names correspond one-to-one to x64 instruction mnemonics.
  // Unless specified otherwise, instructions operate on 64-bit operands.
  //
  // If we need versions of an assembly instruction that operate on different
  // width arguments, we add a single-letter suffix specifying the width.
  // This is done for the following instructions: mov, cmp, inc, dec,
  // add, sub, and test.
  // There are no versions of these instructions without the suffix.
  // - Instructions on 8-bit (byte) operands/registers have a trailing 'b'.
  // - Instructions on 16-bit (word) operands/registers have a trailing 'w'.
  // - Instructions on 32-bit (doubleword) operands/registers use 'l'.
  // - Instructions on 64-bit (quadword) operands/registers use 'q'.
  // - Instructions on operands/registers with pointer size use 'p'.

  STATIC_ASSERT(kPointerSize == kInt64Size || kPointerSize == kInt32Size);

#define DECLARE_INSTRUCTION(instruction)                \
  template<class P1>                                    \
  void instruction##p(P1 p1) {                          \
    emit_##instruction(p1, kPointerSize);               \
  }                                                     \
                                                        \
  template<class P1>                                    \
  void instruction##l(P1 p1) {                          \
    emit_##instruction(p1, kInt32Size);                 \
  }                                                     \
                                                        \
  template<class P1>                                    \
  void instruction##q(P1 p1) {                          \
    emit_##instruction(p1, kInt64Size);                 \
  }                                                     \
                                                        \
  template<class P1, class P2>                          \
  void instruction##p(P1 p1, P2 p2) {                   \
    emit_##instruction(p1, p2, kPointerSize);           \
  }                                                     \
                                                        \
  template<class P1, class P2>                          \
  void instruction##l(P1 p1, P2 p2) {                   \
    emit_##instruction(p1, p2, kInt32Size);             \
  }                                                     \
                                                        \
  template<class P1, class P2>                          \
  void instruction##q(P1 p1, P2 p2) {                   \
    emit_##instruction(p1, p2, kInt64Size);             \
  }                                                     \
                                                        \
  template<class P1, class P2, class P3>                \
  void instruction##p(P1 p1, P2 p2, P3 p3) {            \
    emit_##instruction(p1, p2, p3, kPointerSize);       \
  }                                                     \
                                                        \
  template<class P1, class P2, class P3>                \
  void instruction##l(P1 p1, P2 p2, P3 p3) {            \
    emit_##instruction(p1, p2, p3, kInt32Size);         \
  }                                                     \
                                                        \
  template<class P1, class P2, class P3>                \
  void instruction##q(P1 p1, P2 p2, P3 p3) {            \
    emit_##instruction(p1, p2, p3, kInt64Size);         \
  }
  ASSEMBLER_INSTRUCTION_LIST(DECLARE_INSTRUCTION)
#undef DECLARE_INSTRUCTION

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m, where m must be a power of 2.
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  void Nop(int bytes = 1);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Stack
  void pushfq();
  void popfq();

  void pushq(Immediate value);
  // Push a 32 bit integer, and guarantee that it is actually pushed as a
  // 32 bit value, the normal push will optimize the 8 bit case.
  void pushq_imm32(int32_t imm32);
  void pushq(Register src);
  void pushq(Operand src);

  void popq(Register dst);
  void popq(Operand dst);

  void enter(Immediate size);
  void leave();

  // Moves
  void movb(Register dst, Operand src);
  void movb(Register dst, Immediate imm);
  void movb(Operand dst, Register src);
  void movb(Operand dst, Immediate imm);

  // Move the low 16 bits of a 64-bit register value to a 16-bit
  // memory location.
  void movw(Register dst, Operand src);
  void movw(Operand dst, Register src);
  void movw(Operand dst, Immediate imm);

  // Move the offset of the label location relative to the current
  // position (after the move) to the destination.
  void movl(Operand dst, Label* src);

  // Loads a pointer into a register with a relocation mode.
  void movp(Register dst, Address ptr, RelocInfo::Mode rmode);

  // Load a heap number into a register.
  // The heap number will not be allocated and embedded into the code right
  // away. Instead, we emit the load of a dummy object. Later, when calling
  // Assembler::GetCode, the heap number will be allocated and the code will be
  // patched by replacing the dummy with the actual object. The RelocInfo for
  // the embedded object gets already recorded correctly when emitting the dummy
  // move.
  void movp_heap_number(Register dst, double value);

  void movp_string(Register dst, const StringConstantBase* str);

  // Loads a 64-bit immediate into a register.
  void movq(Register dst, int64_t value,
            RelocInfo::Mode rmode = RelocInfo::NONE);
  void movq(Register dst, uint64_t value,
            RelocInfo::Mode rmode = RelocInfo::NONE);

  void movsxbl(Register dst, Register src);
  void movsxbl(Register dst, Operand src);
  void movsxbq(Register dst, Register src);
  void movsxbq(Register dst, Operand src);
  void movsxwl(Register dst, Register src);
  void movsxwl(Register dst, Operand src);
  void movsxwq(Register dst, Register src);
  void movsxwq(Register dst, Operand src);
  void movsxlq(Register dst, Register src);
  void movsxlq(Register dst, Operand src);

  // Repeated moves.

  void repmovsb();
  void repmovsw();
  void repmovsp() { emit_repmovs(kPointerSize); }
  void repmovsl() { emit_repmovs(kInt32Size); }
  void repmovsq() { emit_repmovs(kInt64Size); }

  // Instruction to load from an immediate 64-bit pointer into RAX.
  void load_rax(Address value, RelocInfo::Mode rmode);
  void load_rax(ExternalReference ext);

  // Conditional moves.
  void cmovq(Condition cc, Register dst, Register src);
  void cmovq(Condition cc, Register dst, Operand src);
  void cmovl(Condition cc, Register dst, Register src);
  void cmovl(Condition cc, Register dst, Operand src);

  void cmpb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x7, dst, src);
  }

  void cmpb_al(Immediate src);

  void cmpb(Register dst, Register src) {
    arithmetic_op_8(0x3A, dst, src);
  }

  void cmpb(Register dst, Operand src) { arithmetic_op_8(0x3A, dst, src); }

  void cmpb(Operand dst, Register src) { arithmetic_op_8(0x38, src, dst); }

  void cmpb(Operand dst, Immediate src) {
    immediate_arithmetic_op_8(0x7, dst, src);
  }

  void cmpw(Operand dst, Immediate src) {
    immediate_arithmetic_op_16(0x7, dst, src);
  }

  void cmpw(Register dst, Immediate src) {
    immediate_arithmetic_op_16(0x7, dst, src);
  }

  void cmpw(Register dst, Operand src) { arithmetic_op_16(0x3B, dst, src); }

  void cmpw(Register dst, Register src) {
    arithmetic_op_16(0x3B, dst, src);
  }

  void cmpw(Operand dst, Register src) { arithmetic_op_16(0x39, src, dst); }

  void testb(Register reg, Operand op) { testb(op, reg); }

  void testw(Register reg, Operand op) { testw(op, reg); }

  void andb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x4, dst, src);
  }

  void decb(Register dst);
  void decb(Operand dst);

  // Lock prefix.
  void lock();

  void xchgb(Register reg, Operand op);
  void xchgw(Register reg, Operand op);

  void cmpxchgb(Operand dst, Register src);
  void cmpxchgw(Operand dst, Register src);

  // Sign-extends rax into rdx:rax.
  void cqo();
  // Sign-extends eax into edx:eax.
  void cdq();

  // Multiply eax by src, put the result in edx:eax.
  void mull(Register src);
  void mull(Operand src);
  // Multiply rax by src, put the result in rdx:rax.
  void mulq(Register src);

#define DECLARE_SHIFT_INSTRUCTION(instruction, subcode)                       \
  void instruction##p(Register dst, Immediate imm8) {                         \
    shift(dst, imm8, subcode, kPointerSize);                                  \
  }                                                                           \
                                                                              \
  void instruction##l(Register dst, Immediate imm8) {                         \
    shift(dst, imm8, subcode, kInt32Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##q(Register dst, Immediate imm8) {                         \
    shift(dst, imm8, subcode, kInt64Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##p(Operand dst, Immediate imm8) {                          \
    shift(dst, imm8, subcode, kPointerSize);                                  \
  }                                                                           \
                                                                              \
  void instruction##l(Operand dst, Immediate imm8) {                          \
    shift(dst, imm8, subcode, kInt32Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##q(Operand dst, Immediate imm8) {                          \
    shift(dst, imm8, subcode, kInt64Size);                                    \
  }                                                                           \
                                                                              \
  void instruction##p_cl(Register dst) { shift(dst, subcode, kPointerSize); } \
                                                                              \
  void instruction##l_cl(Register dst) { shift(dst, subcode, kInt32Size); }   \
                                                                              \
  void instruction##q_cl(Register dst) { shift(dst, subcode, kInt64Size); }   \
                                                                              \
  void instruction##p_cl(Operand dst) { shift(dst, subcode, kPointerSize); }  \
                                                                              \
  void instruction##l_cl(Operand dst) { shift(dst, subcode, kInt32Size); }    \
                                                                              \
  void instruction##q_cl(Operand dst) { shift(dst, subcode, kInt64Size); }
  SHIFT_INSTRUCTION_LIST(DECLARE_SHIFT_INSTRUCTION)
#undef DECLARE_SHIFT_INSTRUCTION

  // Shifts dst:src left by cl bits, affecting only dst.
  void shld(Register dst, Register src);

  // Shifts src:dst right by cl bits, affecting only dst.
  void shrd(Register dst, Register src);

  void store_rax(Address dst, RelocInfo::Mode mode);
  void store_rax(ExternalReference ref);

  void subb(Register dst, Immediate src) {
    immediate_arithmetic_op_8(0x5, dst, src);
  }

  void sub_sp_32(uint32_t imm);

  void testb(Register dst, Register src);
  void testb(Register reg, Immediate mask);
  void testb(Operand op, Immediate mask);
  void testb(Operand op, Register reg);

  void testw(Register dst, Register src);
  void testw(Register reg, Immediate mask);
  void testw(Operand op, Immediate mask);
  void testw(Operand op, Register reg);

  // Bit operations.
  void bswapl(Register dst);
  void bswapq(Register dst);
  void btq(Operand dst, Register src);
  void btsq(Operand dst, Register src);
  void btsq(Register dst, Immediate imm8);
  void btrq(Register dst, Immediate imm8);
  void bsrq(Register dst, Register src);
  void bsrq(Register dst, Operand src);
  void bsrl(Register dst, Register src);
  void bsrl(Register dst, Operand src);
  void bsfq(Register dst, Register src);
  void bsfq(Register dst, Operand src);
  void bsfl(Register dst, Register src);
  void bsfl(Register dst, Operand src);

  // Miscellaneous
  void clc();
  void cld();
  void cpuid();
  void hlt();
  void int3();
  void nop();
  void ret(int imm16);
  void ud2();
  void setcc(Condition cc, Register reg);

  void pshufw(XMMRegister dst, XMMRegister src, uint8_t shuffle);
  void pshufw(XMMRegister dst, Operand src, uint8_t shuffle);
  void pblendw(XMMRegister dst, Operand src, uint8_t mask);
  void pblendw(XMMRegister dst, XMMRegister src, uint8_t mask);
  void palignr(XMMRegister dst, Operand src, uint8_t mask);
  void palignr(XMMRegister dst, XMMRegister src, uint8_t mask);

  // Label operations & relative jumps (PPUM Appendix D)
  //
  // Takes a branch opcode (cc) and a label (L) and generates
  // either a backward branch or a forward branch and links it
  // to the label fixup chain. Usage:
  //
  // Label L;    // unbound label
  // j(cc, &L);  // forward branch to unbound label
  // bind(&L);   // bind label to the current pc
  // j(cc, &L);  // backward branch to bound label
  // bind(&L);   // illegal: a label may be bound only once
  //
  // Note: The same Label can be used for forward and backward branches
  // but it may be bound only once.

  void bind(Label* L);  // binds an unbound label L to the current code position

  // Calls
  // Call near relative 32-bit displacement, relative to next instruction.
  void call(Label* L);
  void call(Address entry, RelocInfo::Mode rmode);
  void near_call(Address entry, RelocInfo::Mode rmode);
  void near_jmp(Address entry, RelocInfo::Mode rmode);
  void call(CodeStub* stub);
  void call(Handle<Code> target,
            RelocInfo::Mode rmode = RelocInfo::CODE_TARGET);

  // Calls directly to the given address using a relative offset.
  // Should only ever be used in Code objects for calls within the
  // same Code object. Should not be used when generating new code (use labels),
  // but only when patching existing code.
  void call(Address target);

  // Call near absolute indirect, address in register
  void call(Register adr);

  // Jumps
  // Jump short or near relative.
  // Use a 32-bit signed displacement.
  // Unconditional jump to L
  void jmp(Label* L, Label::Distance distance = Label::kFar);
  void jmp(Handle<Code> target, RelocInfo::Mode rmode);

  // Jump near absolute indirect (r64)
  void jmp(Register adr);
  void jmp(Operand src);

  // Conditional jumps
  void j(Condition cc,
         Label* L,
         Label::Distance distance = Label::kFar);
  void j(Condition cc, Address entry, RelocInfo::Mode rmode);
  void j(Condition cc, Handle<Code> target, RelocInfo::Mode rmode);

  // Floating-point operations
  void fld(int i);

  void fld1();
  void fldz();
  void fldpi();
  void fldln2();

  void fld_s(Operand adr);
  void fld_d(Operand adr);

  void fstp_s(Operand adr);
  void fstp_d(Operand adr);
  void fstp(int index);

  void fild_s(Operand adr);
  void fild_d(Operand adr);

  void fist_s(Operand adr);

  void fistp_s(Operand adr);
  void fistp_d(Operand adr);

  void fisttp_s(Operand adr);
  void fisttp_d(Operand adr);

  void fabs();
  void fchs();

  void fadd(int i);
  void fsub(int i);
  void fmul(int i);
  void fdiv(int i);

  void fisub_s(Operand adr);

  void faddp(int i = 1);
  void fsubp(int i = 1);
  void fsubrp(int i = 1);
  void fmulp(int i = 1);
  void fdivp(int i = 1);
  void fprem();
  void fprem1();

  void fxch(int i = 1);
  void fincstp();
  void ffree(int i = 0);

  void ftst();
  void fucomp(int i);
  void fucompp();
  void fucomi(int i);
  void fucomip();

  void fcompp();
  void fnstsw_ax();
  void fwait();
  void fnclex();

  void fsin();
  void fcos();
  void fptan();
  void fyl2x();
  void f2xm1();
  void fscale();
  void fninit();

  void frndint();

  void sahf();

  // SSE instructions
  void addss(XMMRegister dst, XMMRegister src);
  void addss(XMMRegister dst, Operand src);
  void subss(XMMRegister dst, XMMRegister src);
  void subss(XMMRegister dst, Operand src);
  void mulss(XMMRegister dst, XMMRegister src);
  void mulss(XMMRegister dst, Operand src);
  void divss(XMMRegister dst, XMMRegister src);
  void divss(XMMRegister dst, Operand src);

  void maxss(XMMRegister dst, XMMRegister src);
  void maxss(XMMRegister dst, Operand src);
  void minss(XMMRegister dst, XMMRegister src);
  void minss(XMMRegister dst, Operand src);

  void sqrtss(XMMRegister dst, XMMRegister src);
  void sqrtss(XMMRegister dst, Operand src);

  void ucomiss(XMMRegister dst, XMMRegister src);
  void ucomiss(XMMRegister dst, Operand src);
  void movaps(XMMRegister dst, XMMRegister src);

  // Don't use this unless it's important to keep the
  // top half of the destination register unchanged.
  // Use movaps when moving float values and movd for integer
  // values in xmm registers.
  void movss(XMMRegister dst, XMMRegister src);

  void movss(XMMRegister dst, Operand src);
  void movss(Operand dst, XMMRegister src);
  void shufps(XMMRegister dst, XMMRegister src, byte imm8);

  void cvttss2si(Register dst, Operand src);
  void cvttss2si(Register dst, XMMRegister src);
  void cvtlsi2ss(XMMRegister dst, Operand src);
  void cvtlsi2ss(XMMRegister dst, Register src);

  void andps(XMMRegister dst, XMMRegister src);
  void andps(XMMRegister dst, Operand src);
  void orps(XMMRegister dst, XMMRegister src);
  void orps(XMMRegister dst, Operand src);
  void xorps(XMMRegister dst, XMMRegister src);
  void xorps(XMMRegister dst, Operand src);

  void addps(XMMRegister dst, XMMRegister src);
  void addps(XMMRegister dst, Operand src);
  void subps(XMMRegister dst, XMMRegister src);
  void subps(XMMRegister dst, Operand src);
  void mulps(XMMRegister dst, XMMRegister src);
  void mulps(XMMRegister dst, Operand src);
  void divps(XMMRegister dst, XMMRegister src);
  void divps(XMMRegister dst, Operand src);

  void movmskps(Register dst, XMMRegister src);

  void vinstr(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2,
              SIMDPrefix pp, LeadingOpcode m, VexW w);
  void vinstr(byte op, XMMRegister dst, XMMRegister src1, Operand src2,
              SIMDPrefix pp, LeadingOpcode m, VexW w);

  // SSE2 instructions
  void sse2_instr(XMMRegister dst, XMMRegister src, byte prefix, byte escape,
                  byte opcode);
  void sse2_instr(XMMRegister dst, Operand src, byte prefix, byte escape,
                  byte opcode);
#define DECLARE_SSE2_INSTRUCTION(instruction, prefix, escape, opcode) \
  void instruction(XMMRegister dst, XMMRegister src) {                \
    sse2_instr(dst, src, 0x##prefix, 0x##escape, 0x##opcode);         \
  }                                                                   \
  void instruction(XMMRegister dst, Operand src) {                    \
    sse2_instr(dst, src, 0x##prefix, 0x##escape, 0x##opcode);         \
  }

  SSE2_INSTRUCTION_LIST(DECLARE_SSE2_INSTRUCTION)
#undef DECLARE_SSE2_INSTRUCTION

#define DECLARE_SSE2_AVX_INSTRUCTION(instruction, prefix, escape, opcode)    \
  void v##instruction(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    vinstr(0x##opcode, dst, src1, src2, k##prefix, k##escape, kW0);          \
  }                                                                          \
  void v##instruction(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    vinstr(0x##opcode, dst, src1, src2, k##prefix, k##escape, kW0);          \
  }

  SSE2_INSTRUCTION_LIST(DECLARE_SSE2_AVX_INSTRUCTION)
#undef DECLARE_SSE2_AVX_INSTRUCTION

  // SSE3
  void lddqu(XMMRegister dst, Operand src);

  // SSSE3
  void ssse3_instr(XMMRegister dst, XMMRegister src, byte prefix, byte escape1,
                   byte escape2, byte opcode);
  void ssse3_instr(XMMRegister dst, Operand src, byte prefix, byte escape1,
                   byte escape2, byte opcode);

#define DECLARE_SSSE3_INSTRUCTION(instruction, prefix, escape1, escape2,     \
                                  opcode)                                    \
  void instruction(XMMRegister dst, XMMRegister src) {                       \
    ssse3_instr(dst, src, 0x##prefix, 0x##escape1, 0x##escape2, 0x##opcode); \
  }                                                                          \
  void instruction(XMMRegister dst, Operand src) {                           \
    ssse3_instr(dst, src, 0x##prefix, 0x##escape1, 0x##escape2, 0x##opcode); \
  }

  SSSE3_INSTRUCTION_LIST(DECLARE_SSSE3_INSTRUCTION)
#undef DECLARE_SSSE3_INSTRUCTION

  // SSE4
  void sse4_instr(XMMRegister dst, XMMRegister src, byte prefix, byte escape1,
                  byte escape2, byte opcode);
  void sse4_instr(XMMRegister dst, Operand src, byte prefix, byte escape1,
                  byte escape2, byte opcode);
#define DECLARE_SSE4_INSTRUCTION(instruction, prefix, escape1, escape2,     \
                                 opcode)                                    \
  void instruction(XMMRegister dst, XMMRegister src) {                      \
    sse4_instr(dst, src, 0x##prefix, 0x##escape1, 0x##escape2, 0x##opcode); \
  }                                                                         \
  void instruction(XMMRegister dst, Operand src) {                          \
    sse4_instr(dst, src, 0x##prefix, 0x##escape1, 0x##escape2, 0x##opcode); \
  }

  SSE4_INSTRUCTION_LIST(DECLARE_SSE4_INSTRUCTION)
#undef DECLARE_SSE4_INSTRUCTION

#define DECLARE_SSE34_AVX_INSTRUCTION(instruction, prefix, escape1, escape2,  \
                                      opcode)                                 \
  void v##instruction(XMMRegister dst, XMMRegister src1, XMMRegister src2) {  \
    vinstr(0x##opcode, dst, src1, src2, k##prefix, k##escape1##escape2, kW0); \
  }                                                                           \
  void v##instruction(XMMRegister dst, XMMRegister src1, Operand src2) {      \
    vinstr(0x##opcode, dst, src1, src2, k##prefix, k##escape1##escape2, kW0); \
  }

  SSSE3_INSTRUCTION_LIST(DECLARE_SSE34_AVX_INSTRUCTION)
  SSE4_INSTRUCTION_LIST(DECLARE_SSE34_AVX_INSTRUCTION)
#undef DECLARE_SSE34_AVX_INSTRUCTION

  void movd(XMMRegister dst, Register src);
  void movd(XMMRegister dst, Operand src);
  void movd(Register dst, XMMRegister src);
  void movq(XMMRegister dst, Register src);
  void movq(Register dst, XMMRegister src);
  void movq(XMMRegister dst, XMMRegister src);

  // Don't use this unless it's important to keep the
  // top half of the destination register unchanged.
  // Use movapd when moving double values and movq for integer
  // values in xmm registers.
  void movsd(XMMRegister dst, XMMRegister src);

  void movsd(Operand dst, XMMRegister src);
  void movsd(XMMRegister dst, Operand src);

  void movdqa(Operand dst, XMMRegister src);
  void movdqa(XMMRegister dst, Operand src);

  void movdqu(Operand dst, XMMRegister src);
  void movdqu(XMMRegister dst, Operand src);

  void movapd(XMMRegister dst, XMMRegister src);
  void movupd(XMMRegister dst, Operand src);
  void movupd(Operand dst, XMMRegister src);

  void psllq(XMMRegister reg, byte imm8);
  void psrlq(XMMRegister reg, byte imm8);
  void psllw(XMMRegister reg, byte imm8);
  void pslld(XMMRegister reg, byte imm8);
  void psrlw(XMMRegister reg, byte imm8);
  void psrld(XMMRegister reg, byte imm8);
  void psraw(XMMRegister reg, byte imm8);
  void psrad(XMMRegister reg, byte imm8);

  void cvttsd2si(Register dst, Operand src);
  void cvttsd2si(Register dst, XMMRegister src);
  void cvttss2siq(Register dst, XMMRegister src);
  void cvttss2siq(Register dst, Operand src);
  void cvttsd2siq(Register dst, XMMRegister src);
  void cvttsd2siq(Register dst, Operand src);
  void cvttps2dq(XMMRegister dst, Operand src);
  void cvttps2dq(XMMRegister dst, XMMRegister src);

  void cvtlsi2sd(XMMRegister dst, Operand src);
  void cvtlsi2sd(XMMRegister dst, Register src);

  void cvtqsi2ss(XMMRegister dst, Operand src);
  void cvtqsi2ss(XMMRegister dst, Register src);

  void cvtqsi2sd(XMMRegister dst, Operand src);
  void cvtqsi2sd(XMMRegister dst, Register src);


  void cvtss2sd(XMMRegister dst, XMMRegister src);
  void cvtss2sd(XMMRegister dst, Operand src);
  void cvtsd2ss(XMMRegister dst, XMMRegister src);
  void cvtsd2ss(XMMRegister dst, Operand src);

  void cvtsd2si(Register dst, XMMRegister src);
  void cvtsd2siq(Register dst, XMMRegister src);

  void addsd(XMMRegister dst, XMMRegister src);
  void addsd(XMMRegister dst, Operand src);
  void subsd(XMMRegister dst, XMMRegister src);
  void subsd(XMMRegister dst, Operand src);
  void mulsd(XMMRegister dst, XMMRegister src);
  void mulsd(XMMRegister dst, Operand src);
  void divsd(XMMRegister dst, XMMRegister src);
  void divsd(XMMRegister dst, Operand src);

  void maxsd(XMMRegister dst, XMMRegister src);
  void maxsd(XMMRegister dst, Operand src);
  void minsd(XMMRegister dst, XMMRegister src);
  void minsd(XMMRegister dst, Operand src);

  void andpd(XMMRegister dst, XMMRegister src);
  void andpd(XMMRegister dst, Operand src);
  void orpd(XMMRegister dst, XMMRegister src);
  void orpd(XMMRegister dst, Operand src);
  void xorpd(XMMRegister dst, XMMRegister src);
  void xorpd(XMMRegister dst, Operand src);
  void sqrtsd(XMMRegister dst, XMMRegister src);
  void sqrtsd(XMMRegister dst, Operand src);

  void haddps(XMMRegister dst, XMMRegister src);
  void haddps(XMMRegister dst, Operand src);

  void ucomisd(XMMRegister dst, XMMRegister src);
  void ucomisd(XMMRegister dst, Operand src);
  void cmpltsd(XMMRegister dst, XMMRegister src);

  void movmskpd(Register dst, XMMRegister src);

  // SSE 4.1 instruction
  void insertps(XMMRegister dst, XMMRegister src, byte imm8);
  void extractps(Register dst, XMMRegister src, byte imm8);
  void pextrb(Register dst, XMMRegister src, int8_t imm8);
  void pextrb(Operand dst, XMMRegister src, int8_t imm8);
  void pextrw(Register dst, XMMRegister src, int8_t imm8);
  void pextrw(Operand dst, XMMRegister src, int8_t imm8);
  void pextrd(Register dst, XMMRegister src, int8_t imm8);
  void pextrd(Operand dst, XMMRegister src, int8_t imm8);
  void pinsrb(XMMRegister dst, Register src, int8_t imm8);
  void pinsrb(XMMRegister dst, Operand src, int8_t imm8);
  void pinsrw(XMMRegister dst, Register src, int8_t imm8);
  void pinsrw(XMMRegister dst, Operand src, int8_t imm8);
  void pinsrd(XMMRegister dst, Register src, int8_t imm8);
  void pinsrd(XMMRegister dst, Operand src, int8_t imm8);

  void roundss(XMMRegister dst, XMMRegister src, RoundingMode mode);
  void roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode);

  void cmpps(XMMRegister dst, XMMRegister src, int8_t cmp);
  void cmpps(XMMRegister dst, Operand src, int8_t cmp);
  void cmppd(XMMRegister dst, XMMRegister src, int8_t cmp);
  void cmppd(XMMRegister dst, Operand src, int8_t cmp);

#define SSE_CMP_P(instr, imm8)                                                \
  void instr##ps(XMMRegister dst, XMMRegister src) { cmpps(dst, src, imm8); } \
  void instr##ps(XMMRegister dst, Operand src) { cmpps(dst, src, imm8); }     \
  void instr##pd(XMMRegister dst, XMMRegister src) { cmppd(dst, src, imm8); } \
  void instr##pd(XMMRegister dst, Operand src) { cmppd(dst, src, imm8); }

  SSE_CMP_P(cmpeq, 0x0);
  SSE_CMP_P(cmplt, 0x1);
  SSE_CMP_P(cmple, 0x2);
  SSE_CMP_P(cmpneq, 0x4);
  SSE_CMP_P(cmpnlt, 0x5);
  SSE_CMP_P(cmpnle, 0x6);

#undef SSE_CMP_P

  void minps(XMMRegister dst, XMMRegister src);
  void minps(XMMRegister dst, Operand src);
  void maxps(XMMRegister dst, XMMRegister src);
  void maxps(XMMRegister dst, Operand src);
  void rcpps(XMMRegister dst, XMMRegister src);
  void rcpps(XMMRegister dst, Operand src);
  void rsqrtps(XMMRegister dst, XMMRegister src);
  void rsqrtps(XMMRegister dst, Operand src);
  void sqrtps(XMMRegister dst, XMMRegister src);
  void sqrtps(XMMRegister dst, Operand src);
  void movups(XMMRegister dst, XMMRegister src);
  void movups(XMMRegister dst, Operand src);
  void movups(Operand dst, XMMRegister src);
  void psrldq(XMMRegister dst, uint8_t shift);
  void pshufd(XMMRegister dst, XMMRegister src, uint8_t shuffle);
  void pshufd(XMMRegister dst, Operand src, uint8_t shuffle);
  void pshufhw(XMMRegister dst, XMMRegister src, uint8_t shuffle);
  void pshuflw(XMMRegister dst, XMMRegister src, uint8_t shuffle);
  void cvtdq2ps(XMMRegister dst, XMMRegister src);
  void cvtdq2ps(XMMRegister dst, Operand src);

  // AVX instruction
  void vfmadd132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x99, dst, src1, src2);
  }
  void vfmadd213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xa9, dst, src1, src2);
  }
  void vfmadd231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xb9, dst, src1, src2);
  }
  void vfmadd132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x99, dst, src1, src2);
  }
  void vfmadd213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xa9, dst, src1, src2);
  }
  void vfmadd231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xb9, dst, src1, src2);
  }
  void vfmsub132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x9b, dst, src1, src2);
  }
  void vfmsub213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xab, dst, src1, src2);
  }
  void vfmsub231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xbb, dst, src1, src2);
  }
  void vfmsub132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x9b, dst, src1, src2);
  }
  void vfmsub213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xab, dst, src1, src2);
  }
  void vfmsub231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xbb, dst, src1, src2);
  }
  void vfnmadd132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x9d, dst, src1, src2);
  }
  void vfnmadd213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xad, dst, src1, src2);
  }
  void vfnmadd231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xbd, dst, src1, src2);
  }
  void vfnmadd132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x9d, dst, src1, src2);
  }
  void vfnmadd213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xad, dst, src1, src2);
  }
  void vfnmadd231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xbd, dst, src1, src2);
  }
  void vfnmsub132sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0x9f, dst, src1, src2);
  }
  void vfnmsub213sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xaf, dst, src1, src2);
  }
  void vfnmsub231sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmasd(0xbf, dst, src1, src2);
  }
  void vfnmsub132sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0x9f, dst, src1, src2);
  }
  void vfnmsub213sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xaf, dst, src1, src2);
  }
  void vfnmsub231sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmasd(0xbf, dst, src1, src2);
  }
  void vfmasd(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vfmasd(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vfmadd132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x99, dst, src1, src2);
  }
  void vfmadd213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xa9, dst, src1, src2);
  }
  void vfmadd231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xb9, dst, src1, src2);
  }
  void vfmadd132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x99, dst, src1, src2);
  }
  void vfmadd213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xa9, dst, src1, src2);
  }
  void vfmadd231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xb9, dst, src1, src2);
  }
  void vfmsub132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x9b, dst, src1, src2);
  }
  void vfmsub213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xab, dst, src1, src2);
  }
  void vfmsub231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xbb, dst, src1, src2);
  }
  void vfmsub132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x9b, dst, src1, src2);
  }
  void vfmsub213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xab, dst, src1, src2);
  }
  void vfmsub231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xbb, dst, src1, src2);
  }
  void vfnmadd132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x9d, dst, src1, src2);
  }
  void vfnmadd213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xad, dst, src1, src2);
  }
  void vfnmadd231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xbd, dst, src1, src2);
  }
  void vfnmadd132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x9d, dst, src1, src2);
  }
  void vfnmadd213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xad, dst, src1, src2);
  }
  void vfnmadd231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xbd, dst, src1, src2);
  }
  void vfnmsub132ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0x9f, dst, src1, src2);
  }
  void vfnmsub213ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xaf, dst, src1, src2);
  }
  void vfnmsub231ss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vfmass(0xbf, dst, src1, src2);
  }
  void vfnmsub132ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0x9f, dst, src1, src2);
  }
  void vfnmsub213ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xaf, dst, src1, src2);
  }
  void vfnmsub231ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vfmass(0xbf, dst, src1, src2);
  }
  void vfmass(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vfmass(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vmovd(XMMRegister dst, Register src);
  void vmovd(XMMRegister dst, Operand src);
  void vmovd(Register dst, XMMRegister src);
  void vmovq(XMMRegister dst, Register src);
  void vmovq(XMMRegister dst, Operand src);
  void vmovq(Register dst, XMMRegister src);

  void vmovsd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vsd(0x10, dst, src1, src2);
  }
  void vmovsd(XMMRegister dst, Operand src) { vsd(0x10, dst, xmm0, src); }
  void vmovsd(Operand dst, XMMRegister src) { vsd(0x11, src, xmm0, dst); }

#define AVX_SP_3(instr, opcode) \
  AVX_S_3(instr, opcode)        \
  AVX_P_3(instr, opcode)

#define AVX_S_3(instr, opcode)  \
  AVX_3(instr##ss, opcode, vss) \
  AVX_3(instr##sd, opcode, vsd)

#define AVX_P_3(instr, opcode)  \
  AVX_3(instr##ps, opcode, vps) \
  AVX_3(instr##pd, opcode, vpd)

#define AVX_3(instr, opcode, impl)                                  \
  void instr(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    impl(opcode, dst, src1, src2);                                  \
  }                                                                 \
  void instr(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    impl(opcode, dst, src1, src2);                                  \
  }

  AVX_SP_3(vsqrt, 0x51);
  AVX_SP_3(vadd, 0x58);
  AVX_SP_3(vsub, 0x5c);
  AVX_SP_3(vmul, 0x59);
  AVX_SP_3(vdiv, 0x5e);
  AVX_SP_3(vmin, 0x5d);
  AVX_SP_3(vmax, 0x5f);
  AVX_P_3(vand, 0x54);
  AVX_P_3(vor, 0x56);
  AVX_P_3(vxor, 0x57);
  AVX_3(vcvtsd2ss, 0x5a, vsd);
  AVX_3(vhaddps, 0x7c, vsd);

#undef AVX_3
#undef AVX_S_3
#undef AVX_P_3
#undef AVX_SP_3

  void vpsrlq(XMMRegister dst, XMMRegister src, byte imm8) {
    vpd(0x73, xmm2, dst, src);
    emit(imm8);
  }
  void vpsllq(XMMRegister dst, XMMRegister src, byte imm8) {
    vpd(0x73, xmm6, dst, src);
    emit(imm8);
  }
  void vcvtss2sd(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vinstr(0x5a, dst, src1, src2, kF3, k0F, kWIG);
  }
  void vcvtss2sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vinstr(0x5a, dst, src1, src2, kF3, k0F, kWIG);
  }
  void vcvtlsi2sd(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = XMMRegister::from_code(src2.code());
    vinstr(0x2a, dst, src1, isrc2, kF2, k0F, kW0);
  }
  void vcvtlsi2sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vinstr(0x2a, dst, src1, src2, kF2, k0F, kW0);
  }
  void vcvtlsi2ss(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = XMMRegister::from_code(src2.code());
    vinstr(0x2a, dst, src1, isrc2, kF3, k0F, kW0);
  }
  void vcvtlsi2ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vinstr(0x2a, dst, src1, src2, kF3, k0F, kW0);
  }
  void vcvtqsi2ss(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = XMMRegister::from_code(src2.code());
    vinstr(0x2a, dst, src1, isrc2, kF3, k0F, kW1);
  }
  void vcvtqsi2ss(XMMRegister dst, XMMRegister src1, Operand src2) {
    vinstr(0x2a, dst, src1, src2, kF3, k0F, kW1);
  }
  void vcvtqsi2sd(XMMRegister dst, XMMRegister src1, Register src2) {
    XMMRegister isrc2 = XMMRegister::from_code(src2.code());
    vinstr(0x2a, dst, src1, isrc2, kF2, k0F, kW1);
  }
  void vcvtqsi2sd(XMMRegister dst, XMMRegister src1, Operand src2) {
    vinstr(0x2a, dst, src1, src2, kF2, k0F, kW1);
  }
  void vcvttss2si(Register dst, XMMRegister src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF3, k0F, kW0);
  }
  void vcvttss2si(Register dst, Operand src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF3, k0F, kW0);
  }
  void vcvttsd2si(Register dst, XMMRegister src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF2, k0F, kW0);
  }
  void vcvttsd2si(Register dst, Operand src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF2, k0F, kW0);
  }
  void vcvttss2siq(Register dst, XMMRegister src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF3, k0F, kW1);
  }
  void vcvttss2siq(Register dst, Operand src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF3, k0F, kW1);
  }
  void vcvttsd2siq(Register dst, XMMRegister src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF2, k0F, kW1);
  }
  void vcvttsd2siq(Register dst, Operand src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2c, idst, xmm0, src, kF2, k0F, kW1);
  }
  void vcvtsd2si(Register dst, XMMRegister src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x2d, idst, xmm0, src, kF2, k0F, kW0);
  }
  void vucomisd(XMMRegister dst, XMMRegister src) {
    vinstr(0x2e, dst, xmm0, src, k66, k0F, kWIG);
  }
  void vucomisd(XMMRegister dst, Operand src) {
    vinstr(0x2e, dst, xmm0, src, k66, k0F, kWIG);
  }
  void vroundss(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                RoundingMode mode) {
    vinstr(0x0a, dst, src1, src2, k66, k0F3A, kWIG);
    emit(static_cast<byte>(mode) | 0x8);  // Mask precision exception.
  }
  void vroundsd(XMMRegister dst, XMMRegister src1, XMMRegister src2,
                RoundingMode mode) {
    vinstr(0x0b, dst, src1, src2, k66, k0F3A, kWIG);
    emit(static_cast<byte>(mode) | 0x8);  // Mask precision exception.
  }

  void vsd(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vinstr(op, dst, src1, src2, kF2, k0F, kWIG);
  }
  void vsd(byte op, XMMRegister dst, XMMRegister src1, Operand src2) {
    vinstr(op, dst, src1, src2, kF2, k0F, kWIG);
  }

  void vmovss(XMMRegister dst, XMMRegister src1, XMMRegister src2) {
    vss(0x10, dst, src1, src2);
  }
  void vmovss(XMMRegister dst, Operand src) { vss(0x10, dst, xmm0, src); }
  void vmovss(Operand dst, XMMRegister src) { vss(0x11, src, xmm0, dst); }
  void vucomiss(XMMRegister dst, XMMRegister src);
  void vucomiss(XMMRegister dst, Operand src);
  void vss(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vss(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  void vmovaps(XMMRegister dst, XMMRegister src) { vps(0x28, dst, xmm0, src); }
  void vmovups(XMMRegister dst, XMMRegister src) { vps(0x10, dst, xmm0, src); }
  void vmovups(XMMRegister dst, Operand src) { vps(0x10, dst, xmm0, src); }
  void vmovups(Operand dst, XMMRegister src) { vps(0x11, src, xmm0, dst); }
  void vmovapd(XMMRegister dst, XMMRegister src) { vpd(0x28, dst, xmm0, src); }
  void vmovupd(XMMRegister dst, Operand src) { vpd(0x10, dst, xmm0, src); }
  void vmovupd(Operand dst, XMMRegister src) { vpd(0x11, src, xmm0, dst); }
  void vmovmskps(Register dst, XMMRegister src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vps(0x50, idst, xmm0, src);
  }
  void vmovmskpd(Register dst, XMMRegister src) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vpd(0x50, idst, xmm0, src);
  }
  void vcmpps(XMMRegister dst, XMMRegister src1, XMMRegister src2, int8_t cmp) {
    vps(0xC2, dst, src1, src2);
    emit(cmp);
  }
  void vcmpps(XMMRegister dst, XMMRegister src1, Operand src2, int8_t cmp) {
    vps(0xC2, dst, src1, src2);
    emit(cmp);
  }
  void vcmppd(XMMRegister dst, XMMRegister src1, XMMRegister src2, int8_t cmp) {
    vpd(0xC2, dst, src1, src2);
    emit(cmp);
  }
  void vcmppd(XMMRegister dst, XMMRegister src1, Operand src2, int8_t cmp) {
    vpd(0xC2, dst, src1, src2);
    emit(cmp);
  }

#define AVX_CMP_P(instr, imm8)                                          \
  void instr##ps(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    vcmpps(dst, src1, src2, imm8);                                      \
  }                                                                     \
  void instr##ps(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    vcmpps(dst, src1, src2, imm8);                                      \
  }                                                                     \
  void instr##pd(XMMRegister dst, XMMRegister src1, XMMRegister src2) { \
    vcmppd(dst, src1, src2, imm8);                                      \
  }                                                                     \
  void instr##pd(XMMRegister dst, XMMRegister src1, Operand src2) {     \
    vcmppd(dst, src1, src2, imm8);                                      \
  }

  AVX_CMP_P(vcmpeq, 0x0);
  AVX_CMP_P(vcmplt, 0x1);
  AVX_CMP_P(vcmple, 0x2);
  AVX_CMP_P(vcmpneq, 0x4);
  AVX_CMP_P(vcmpnlt, 0x5);
  AVX_CMP_P(vcmpnle, 0x6);

#undef AVX_CMP_P

  void vlddqu(XMMRegister dst, Operand src) {
    vinstr(0xF0, dst, xmm0, src, kF2, k0F, kWIG);
  }
  void vpsllw(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x71, xmm6, dst, src, k66, k0F, kWIG);
    emit(imm8);
  }
  void vpsrlw(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x71, xmm2, dst, src, k66, k0F, kWIG);
    emit(imm8);
  }
  void vpsraw(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x71, xmm4, dst, src, k66, k0F, kWIG);
    emit(imm8);
  }
  void vpslld(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x72, xmm6, dst, src, k66, k0F, kWIG);
    emit(imm8);
  }
  void vpsrld(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x72, xmm2, dst, src, k66, k0F, kWIG);
    emit(imm8);
  }
  void vpsrad(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x72, xmm4, dst, src, k66, k0F, kWIG);
    emit(imm8);
  }
  void vpextrb(Register dst, XMMRegister src, uint8_t imm8) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x14, src, xmm0, idst, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpextrb(Operand dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x14, src, xmm0, dst, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpextrw(Register dst, XMMRegister src, uint8_t imm8) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0xc5, idst, xmm0, src, k66, k0F, kW0);
    emit(imm8);
  }
  void vpextrw(Operand dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x15, src, xmm0, dst, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpextrd(Register dst, XMMRegister src, uint8_t imm8) {
    XMMRegister idst = XMMRegister::from_code(dst.code());
    vinstr(0x16, src, xmm0, idst, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpextrd(Operand dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x16, src, xmm0, dst, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpinsrb(XMMRegister dst, XMMRegister src1, Register src2, uint8_t imm8) {
    XMMRegister isrc = XMMRegister::from_code(src2.code());
    vinstr(0x20, dst, src1, isrc, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpinsrb(XMMRegister dst, XMMRegister src1, Operand src2, uint8_t imm8) {
    vinstr(0x20, dst, src1, src2, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpinsrw(XMMRegister dst, XMMRegister src1, Register src2, uint8_t imm8) {
    XMMRegister isrc = XMMRegister::from_code(src2.code());
    vinstr(0xc4, dst, src1, isrc, k66, k0F, kW0);
    emit(imm8);
  }
  void vpinsrw(XMMRegister dst, XMMRegister src1, Operand src2, uint8_t imm8) {
    vinstr(0xc4, dst, src1, src2, k66, k0F, kW0);
    emit(imm8);
  }
  void vpinsrd(XMMRegister dst, XMMRegister src1, Register src2, uint8_t imm8) {
    XMMRegister isrc = XMMRegister::from_code(src2.code());
    vinstr(0x22, dst, src1, isrc, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpinsrd(XMMRegister dst, XMMRegister src1, Operand src2, uint8_t imm8) {
    vinstr(0x22, dst, src1, src2, k66, k0F3A, kW0);
    emit(imm8);
  }
  void vpshufd(XMMRegister dst, XMMRegister src, uint8_t imm8) {
    vinstr(0x70, dst, xmm0, src, k66, k0F, kWIG);
    emit(imm8);
  }

  void vps(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vps(byte op, XMMRegister dst, XMMRegister src1, Operand src2);
  void vpd(byte op, XMMRegister dst, XMMRegister src1, XMMRegister src2);
  void vpd(byte op, XMMRegister dst, XMMRegister src1, Operand src2);

  // BMI instruction
  void andnq(Register dst, Register src1, Register src2) {
    bmi1q(0xf2, dst, src1, src2);
  }
  void andnq(Register dst, Register src1, Operand src2) {
    bmi1q(0xf2, dst, src1, src2);
  }
  void andnl(Register dst, Register src1, Register src2) {
    bmi1l(0xf2, dst, src1, src2);
  }
  void andnl(Register dst, Register src1, Operand src2) {
    bmi1l(0xf2, dst, src1, src2);
  }
  void bextrq(Register dst, Register src1, Register src2) {
    bmi1q(0xf7, dst, src2, src1);
  }
  void bextrq(Register dst, Operand src1, Register src2) {
    bmi1q(0xf7, dst, src2, src1);
  }
  void bextrl(Register dst, Register src1, Register src2) {
    bmi1l(0xf7, dst, src2, src1);
  }
  void bextrl(Register dst, Operand src1, Register src2) {
    bmi1l(0xf7, dst, src2, src1);
  }
  void blsiq(Register dst, Register src) { bmi1q(0xf3, rbx, dst, src); }
  void blsiq(Register dst, Operand src) { bmi1q(0xf3, rbx, dst, src); }
  void blsil(Register dst, Register src) { bmi1l(0xf3, rbx, dst, src); }
  void blsil(Register dst, Operand src) { bmi1l(0xf3, rbx, dst, src); }
  void blsmskq(Register dst, Register src) { bmi1q(0xf3, rdx, dst, src); }
  void blsmskq(Register dst, Operand src) { bmi1q(0xf3, rdx, dst, src); }
  void blsmskl(Register dst, Register src) { bmi1l(0xf3, rdx, dst, src); }
  void blsmskl(Register dst, Operand src) { bmi1l(0xf3, rdx, dst, src); }
  void blsrq(Register dst, Register src) { bmi1q(0xf3, rcx, dst, src); }
  void blsrq(Register dst, Operand src) { bmi1q(0xf3, rcx, dst, src); }
  void blsrl(Register dst, Register src) { bmi1l(0xf3, rcx, dst, src); }
  void blsrl(Register dst, Operand src) { bmi1l(0xf3, rcx, dst, src); }
  void tzcntq(Register dst, Register src);
  void tzcntq(Register dst, Operand src);
  void tzcntl(Register dst, Register src);
  void tzcntl(Register dst, Operand src);

  void lzcntq(Register dst, Register src);
  void lzcntq(Register dst, Operand src);
  void lzcntl(Register dst, Register src);
  void lzcntl(Register dst, Operand src);

  void popcntq(Register dst, Register src);
  void popcntq(Register dst, Operand src);
  void popcntl(Register dst, Register src);
  void popcntl(Register dst, Operand src);

  void bzhiq(Register dst, Register src1, Register src2) {
    bmi2q(kNone, 0xf5, dst, src2, src1);
  }
  void bzhiq(Register dst, Operand src1, Register src2) {
    bmi2q(kNone, 0xf5, dst, src2, src1);
  }
  void bzhil(Register dst, Register src1, Register src2) {
    bmi2l(kNone, 0xf5, dst, src2, src1);
  }
  void bzhil(Register dst, Operand src1, Register src2) {
    bmi2l(kNone, 0xf5, dst, src2, src1);
  }
  void mulxq(Register dst1, Register dst2, Register src) {
    bmi2q(kF2, 0xf6, dst1, dst2, src);
  }
  void mulxq(Register dst1, Register dst2, Operand src) {
    bmi2q(kF2, 0xf6, dst1, dst2, src);
  }
  void mulxl(Register dst1, Register dst2, Register src) {
    bmi2l(kF2, 0xf6, dst1, dst2, src);
  }
  void mulxl(Register dst1, Register dst2, Operand src) {
    bmi2l(kF2, 0xf6, dst1, dst2, src);
  }
  void pdepq(Register dst, Register src1, Register src2) {
    bmi2q(kF2, 0xf5, dst, src1, src2);
  }
  void pdepq(Register dst, Register src1, Operand src2) {
    bmi2q(kF2, 0xf5, dst, src1, src2);
  }
  void pdepl(Register dst, Register src1, Register src2) {
    bmi2l(kF2, 0xf5, dst, src1, src2);
  }
  void pdepl(Register dst, Register src1, Operand src2) {
    bmi2l(kF2, 0xf5, dst, src1, src2);
  }
  void pextq(Register dst, Register src1, Register src2) {
    bmi2q(kF3, 0xf5, dst, src1, src2);
  }
  void pextq(Register dst, Register src1, Operand src2) {
    bmi2q(kF3, 0xf5, dst, src1, src2);
  }
  void pextl(Register dst, Register src1, Register src2) {
    bmi2l(kF3, 0xf5, dst, src1, src2);
  }
  void pextl(Register dst, Register src1, Operand src2) {
    bmi2l(kF3, 0xf5, dst, src1, src2);
  }
  void sarxq(Register dst, Register src1, Register src2) {
    bmi2q(kF3, 0xf7, dst, src2, src1);
  }
  void sarxq(Register dst, Operand src1, Register src2) {
    bmi2q(kF3, 0xf7, dst, src2, src1);
  }
  void sarxl(Register dst, Register src1, Register src2) {
    bmi2l(kF3, 0xf7, dst, src2, src1);
  }
  void sarxl(Register dst, Operand src1, Register src2) {
    bmi2l(kF3, 0xf7, dst, src2, src1);
  }
  void shlxq(Register dst, Register src1, Register src2) {
    bmi2q(k66, 0xf7, dst, src2, src1);
  }
  void shlxq(Register dst, Operand src1, Register src2) {
    bmi2q(k66, 0xf7, dst, src2, src1);
  }
  void shlxl(Register dst, Register src1, Register src2) {
    bmi2l(k66, 0xf7, dst, src2, src1);
  }
  void shlxl(Register dst, Operand src1, Register src2) {
    bmi2l(k66, 0xf7, dst, src2, src1);
  }
  void shrxq(Register dst, Register src1, Register src2) {
    bmi2q(kF2, 0xf7, dst, src2, src1);
  }
  void shrxq(Register dst, Operand src1, Register src2) {
    bmi2q(kF2, 0xf7, dst, src2, src1);
  }
  void shrxl(Register dst, Register src1, Register src2) {
    bmi2l(kF2, 0xf7, dst, src2, src1);
  }
  void shrxl(Register dst, Operand src1, Register src2) {
    bmi2l(kF2, 0xf7, dst, src2, src1);
  }
  void rorxq(Register dst, Register src, byte imm8);
  void rorxq(Register dst, Operand src, byte imm8);
  void rorxl(Register dst, Register src, byte imm8);
  void rorxl(Register dst, Operand src, byte imm8);

  void lfence();
  void pause();

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, SourcePosition position,
                         int id);

  void PatchConstantPoolAccessInstruction(int pc_offset, int offset,
                                          ConstantPoolEntry::Access access,
                                          ConstantPoolEntry::Type type) {
    // No embedded constant pool support.
    UNREACHABLE();
  }

  // Writes a single word of data in the code stream.
  // Used for inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);
  void dp(uintptr_t data) { dq(data); }
  void dq(Label* label);

  // Patch entries for partial constant pool.
  void PatchConstPool();

  // Check if use partial constant pool for this rmode.
  static bool UseConstPoolFor(RelocInfo::Mode rmode);

  // Check if there is less than kGap bytes available in the buffer.
  // If this is the case, we need to grow the buffer before emitting
  // an instruction or relocation information.
  inline bool buffer_overflow() const {
    return pc_ >= reloc_info_writer.pos() - kGap;
  }

  // Get the number of bytes available in the buffer.
  inline int available_space() const {
    return static_cast<int>(reloc_info_writer.pos() - pc_);
  }

  static bool IsNop(Address addr);

  // Avoid overflows for displacements etc.
  static constexpr int kMaximalBufferSize = 512 * MB;

  byte byte_at(int pos)  { return buffer_[pos]; }
  void set_byte_at(int pos, byte value) { buffer_[pos] = value; }

 protected:
  // Call near indirect
  void call(Operand operand);

 private:
  byte* addr_at(int pos)  { return buffer_ + pos; }
  uint32_t long_at(int pos)  {
    return *reinterpret_cast<uint32_t*>(addr_at(pos));
  }
  void long_at_put(int pos, uint32_t x)  {
    *reinterpret_cast<uint32_t*>(addr_at(pos)) = x;
  }

  // code emission
  void GrowBuffer();

  void emit(byte x) { *pc_++ = x; }
  inline void emitl(uint32_t x);
  inline void emitp(Address x, RelocInfo::Mode rmode);
  inline void emitq(uint64_t x);
  inline void emitw(uint16_t x);
  inline void emit_runtime_entry(Address entry, RelocInfo::Mode rmode);
  inline void emit(Immediate x);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of both register codes.
  // High bit of reg goes to REX.R, high bit of rm_reg goes to REX.B.
  // REX.W is set.
  inline void emit_rex_64(XMMRegister reg, Register rm_reg);
  inline void emit_rex_64(Register reg, XMMRegister rm_reg);
  inline void emit_rex_64(Register reg, Register rm_reg);
  inline void emit_rex_64(XMMRegister reg, XMMRegister rm_reg);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of the destination, index, and base register codes.
  // The high bit of reg is used for REX.R, the high bit of op's base
  // register is used for REX.B, and the high bit of op's index register
  // is used for REX.X.  REX.W is set.
  inline void emit_rex_64(Register reg, Operand op);
  inline void emit_rex_64(XMMRegister reg, Operand op);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of the register code.
  // The high bit of register is used for REX.B.
  // REX.W is set and REX.R and REX.X are clear.
  inline void emit_rex_64(Register rm_reg);

  // Emits a REX prefix that encodes a 64-bit operand size and
  // the top bit of the index and base register codes.
  // The high bit of op's base register is used for REX.B, and the high
  // bit of op's index register is used for REX.X.
  // REX.W is set and REX.R clear.
  inline void emit_rex_64(Operand op);

  // Emit a REX prefix that only sets REX.W to choose a 64-bit operand size.
  void emit_rex_64() { emit(0x48); }

  // High bit of reg goes to REX.R, high bit of rm_reg goes to REX.B.
  // REX.W is clear.
  inline void emit_rex_32(Register reg, Register rm_reg);

  // The high bit of reg is used for REX.R, the high bit of op's base
  // register is used for REX.B, and the high bit of op's index register
  // is used for REX.X.  REX.W is cleared.
  inline void emit_rex_32(Register reg, Operand op);

  // High bit of rm_reg goes to REX.B.
  // REX.W, REX.R and REX.X are clear.
  inline void emit_rex_32(Register rm_reg);

  // High bit of base goes to REX.B and high bit of index to REX.X.
  // REX.W and REX.R are clear.
  inline void emit_rex_32(Operand op);

  // High bit of reg goes to REX.R, high bit of rm_reg goes to REX.B.
  // REX.W is cleared.  If no REX bits are set, no byte is emitted.
  inline void emit_optional_rex_32(Register reg, Register rm_reg);

  // The high bit of reg is used for REX.R, the high bit of op's base
  // register is used for REX.B, and the high bit of op's index register
  // is used for REX.X.  REX.W is cleared.  If no REX bits are set, nothing
  // is emitted.
  inline void emit_optional_rex_32(Register reg, Operand op);

  // As for emit_optional_rex_32(Register, Register), except that
  // the registers are XMM registers.
  inline void emit_optional_rex_32(XMMRegister reg, XMMRegister base);

  // As for emit_optional_rex_32(Register, Register), except that
  // one of the registers is an XMM registers.
  inline void emit_optional_rex_32(XMMRegister reg, Register base);

  // As for emit_optional_rex_32(Register, Register), except that
  // one of the registers is an XMM registers.
  inline void emit_optional_rex_32(Register reg, XMMRegister base);

  // As for emit_optional_rex_32(Register, Operand), except that
  // the register is an XMM register.
  inline void emit_optional_rex_32(XMMRegister reg, Operand op);

  // Optionally do as emit_rex_32(Register) if the register number has
  // the high bit set.
  inline void emit_optional_rex_32(Register rm_reg);
  inline void emit_optional_rex_32(XMMRegister rm_reg);

  // Optionally do as emit_rex_32(Operand) if the operand register
  // numbers have a high bit set.
  inline void emit_optional_rex_32(Operand op);

  void emit_rex(int size) {
    if (size == kInt64Size) {
      emit_rex_64();
    } else {
      DCHECK_EQ(size, kInt32Size);
    }
  }

  template<class P1>
  void emit_rex(P1 p1, int size) {
    if (size == kInt64Size) {
      emit_rex_64(p1);
    } else {
      DCHECK_EQ(size, kInt32Size);
      emit_optional_rex_32(p1);
    }
  }

  template<class P1, class P2>
  void emit_rex(P1 p1, P2 p2, int size) {
    if (size == kInt64Size) {
      emit_rex_64(p1, p2);
    } else {
      DCHECK_EQ(size, kInt32Size);
      emit_optional_rex_32(p1, p2);
    }
  }

  // Emit vex prefix
  void emit_vex2_byte0() { emit(0xc5); }
  inline void emit_vex2_byte1(XMMRegister reg, XMMRegister v, VectorLength l,
                              SIMDPrefix pp);
  void emit_vex3_byte0() { emit(0xc4); }
  inline void emit_vex3_byte1(XMMRegister reg, XMMRegister rm, LeadingOpcode m);
  inline void emit_vex3_byte1(XMMRegister reg, Operand rm, LeadingOpcode m);
  inline void emit_vex3_byte2(VexW w, XMMRegister v, VectorLength l,
                              SIMDPrefix pp);
  inline void emit_vex_prefix(XMMRegister reg, XMMRegister v, XMMRegister rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);
  inline void emit_vex_prefix(Register reg, Register v, Register rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);
  inline void emit_vex_prefix(XMMRegister reg, XMMRegister v, Operand rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);
  inline void emit_vex_prefix(Register reg, Register v, Operand rm,
                              VectorLength l, SIMDPrefix pp, LeadingOpcode m,
                              VexW w);

  // Emit the ModR/M byte, and optionally the SIB byte and
  // 1- or 4-byte offset for a memory operand.  Also encodes
  // the second operand of the operation, a register or operation
  // subcode, into the reg field of the ModR/M byte.
  void emit_operand(Register reg, Operand adr) {
    emit_operand(reg.low_bits(), adr);
  }

  // Emit the ModR/M byte, and optionally the SIB byte and
  // 1- or 4-byte offset for a memory operand.  Also used to encode
  // a three-bit opcode extension into the ModR/M byte.
  void emit_operand(int rm, Operand adr);

  // Emit a ModR/M byte with registers coded in the reg and rm_reg fields.
  void emit_modrm(Register reg, Register rm_reg) {
    emit(0xC0 | reg.low_bits() << 3 | rm_reg.low_bits());
  }

  // Emit a ModR/M byte with an operation subcode in the reg field and
  // a register in the rm_reg field.
  void emit_modrm(int code, Register rm_reg) {
    DCHECK(is_uint3(code));
    emit(0xC0 | code << 3 | rm_reg.low_bits());
  }

  // Emit the code-object-relative offset of the label's position
  inline void emit_code_relative_offset(Label* label);

  // The first argument is the reg field, the second argument is the r/m field.
  void emit_sse_operand(XMMRegister dst, XMMRegister src);
  void emit_sse_operand(XMMRegister reg, Operand adr);
  void emit_sse_operand(Register reg, Operand adr);
  void emit_sse_operand(XMMRegister dst, Register src);
  void emit_sse_operand(Register dst, XMMRegister src);
  void emit_sse_operand(XMMRegister dst);

  // Emit machine code for one of the operations ADD, ADC, SUB, SBC,
  // AND, OR, XOR, or CMP.  The encodings of these operations are all
  // similar, differing just in the opcode or in the reg field of the
  // ModR/M byte.
  void arithmetic_op_8(byte opcode, Register reg, Register rm_reg);
  void arithmetic_op_8(byte opcode, Register reg, Operand rm_reg);
  void arithmetic_op_16(byte opcode, Register reg, Register rm_reg);
  void arithmetic_op_16(byte opcode, Register reg, Operand rm_reg);
  // Operate on operands/registers with pointer size, 32-bit or 64-bit size.
  void arithmetic_op(byte opcode, Register reg, Register rm_reg, int size);
  void arithmetic_op(byte opcode, Register reg, Operand rm_reg, int size);
  // Operate on a byte in memory or register.
  void immediate_arithmetic_op_8(byte subcode,
                                 Register dst,
                                 Immediate src);
  void immediate_arithmetic_op_8(byte subcode, Operand dst, Immediate src);
  // Operate on a word in memory or register.
  void immediate_arithmetic_op_16(byte subcode,
                                  Register dst,
                                  Immediate src);
  void immediate_arithmetic_op_16(byte subcode, Operand dst, Immediate src);
  // Operate on operands/registers with pointer size, 32-bit or 64-bit size.
  void immediate_arithmetic_op(byte subcode,
                               Register dst,
                               Immediate src,
                               int size);
  void immediate_arithmetic_op(byte subcode, Operand dst, Immediate src,
                               int size);

  // Emit machine code for a shift operation.
  void shift(Operand dst, Immediate shift_amount, int subcode, int size);
  void shift(Register dst, Immediate shift_amount, int subcode, int size);
  // Shift dst by cl % 64 bits.
  void shift(Register dst, int subcode, int size);
  void shift(Operand dst, int subcode, int size);

  void emit_farith(int b1, int b2, int i);

  // labels
  // void print(Label* L);
  void bind_to(Label* L, int pos);

  // record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);

  // Arithmetics
  void emit_add(Register dst, Register src, int size) {
    arithmetic_op(0x03, dst, src, size);
  }

  void emit_add(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x0, dst, src, size);
  }

  void emit_add(Register dst, Operand src, int size) {
    arithmetic_op(0x03, dst, src, size);
  }

  void emit_add(Operand dst, Register src, int size) {
    arithmetic_op(0x1, src, dst, size);
  }

  void emit_add(Operand dst, Immediate src, int size) {
    immediate_arithmetic_op(0x0, dst, src, size);
  }

  void emit_and(Register dst, Register src, int size) {
    arithmetic_op(0x23, dst, src, size);
  }

  void emit_and(Register dst, Operand src, int size) {
    arithmetic_op(0x23, dst, src, size);
  }

  void emit_and(Operand dst, Register src, int size) {
    arithmetic_op(0x21, src, dst, size);
  }

  void emit_and(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x4, dst, src, size);
  }

  void emit_and(Operand dst, Immediate src, int size) {
    immediate_arithmetic_op(0x4, dst, src, size);
  }

  void emit_cmp(Register dst, Register src, int size) {
    arithmetic_op(0x3B, dst, src, size);
  }

  void emit_cmp(Register dst, Operand src, int size) {
    arithmetic_op(0x3B, dst, src, size);
  }

  void emit_cmp(Operand dst, Register src, int size) {
    arithmetic_op(0x39, src, dst, size);
  }

  void emit_cmp(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x7, dst, src, size);
  }

  void emit_cmp(Operand dst, Immediate src, int size) {
    immediate_arithmetic_op(0x7, dst, src, size);
  }

  // Compare {al,ax,eax,rax} with src.  If equal, set ZF and write dst into
  // src. Otherwise clear ZF and write src into {al,ax,eax,rax}.  This
  // operation is only atomic if prefixed by the lock instruction.
  void emit_cmpxchg(Operand dst, Register src, int size);

  void emit_dec(Register dst, int size);
  void emit_dec(Operand dst, int size);

  // Divide rdx:rax by src.  Quotient in rax, remainder in rdx when size is 64.
  // Divide edx:eax by lower 32 bits of src.  Quotient in eax, remainder in edx
  // when size is 32.
  void emit_idiv(Register src, int size);
  void emit_div(Register src, int size);

  // Signed multiply instructions.
  // rdx:rax = rax * src when size is 64 or edx:eax = eax * src when size is 32.
  void emit_imul(Register src, int size);
  void emit_imul(Operand src, int size);
  void emit_imul(Register dst, Register src, int size);
  void emit_imul(Register dst, Operand src, int size);
  void emit_imul(Register dst, Register src, Immediate imm, int size);
  void emit_imul(Register dst, Operand src, Immediate imm, int size);

  void emit_inc(Register dst, int size);
  void emit_inc(Operand dst, int size);

  void emit_lea(Register dst, Operand src, int size);

  void emit_mov(Register dst, Operand src, int size);
  void emit_mov(Register dst, Register src, int size);
  void emit_mov(Operand dst, Register src, int size);
  void emit_mov(Register dst, Immediate value, int size);
  void emit_mov(Operand dst, Immediate value, int size);

  void emit_movzxb(Register dst, Operand src, int size);
  void emit_movzxb(Register dst, Register src, int size);
  void emit_movzxw(Register dst, Operand src, int size);
  void emit_movzxw(Register dst, Register src, int size);

  void emit_neg(Register dst, int size);
  void emit_neg(Operand dst, int size);

  void emit_not(Register dst, int size);
  void emit_not(Operand dst, int size);

  void emit_or(Register dst, Register src, int size) {
    arithmetic_op(0x0B, dst, src, size);
  }

  void emit_or(Register dst, Operand src, int size) {
    arithmetic_op(0x0B, dst, src, size);
  }

  void emit_or(Operand dst, Register src, int size) {
    arithmetic_op(0x9, src, dst, size);
  }

  void emit_or(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x1, dst, src, size);
  }

  void emit_or(Operand dst, Immediate src, int size) {
    immediate_arithmetic_op(0x1, dst, src, size);
  }

  void emit_repmovs(int size);

  void emit_sbb(Register dst, Register src, int size) {
    arithmetic_op(0x1b, dst, src, size);
  }

  void emit_sub(Register dst, Register src, int size) {
    arithmetic_op(0x2B, dst, src, size);
  }

  void emit_sub(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x5, dst, src, size);
  }

  void emit_sub(Register dst, Operand src, int size) {
    arithmetic_op(0x2B, dst, src, size);
  }

  void emit_sub(Operand dst, Register src, int size) {
    arithmetic_op(0x29, src, dst, size);
  }

  void emit_sub(Operand dst, Immediate src, int size) {
    immediate_arithmetic_op(0x5, dst, src, size);
  }

  void emit_test(Register dst, Register src, int size);
  void emit_test(Register reg, Immediate mask, int size);
  void emit_test(Operand op, Register reg, int size);
  void emit_test(Operand op, Immediate mask, int size);
  void emit_test(Register reg, Operand op, int size) {
    return emit_test(op, reg, size);
  }

  void emit_xchg(Register dst, Register src, int size);
  void emit_xchg(Register dst, Operand src, int size);

  void emit_xor(Register dst, Register src, int size) {
    if (size == kInt64Size && dst.code() == src.code()) {
    // 32 bit operations zero the top 32 bits of 64 bit registers. Therefore
    // there is no need to make this a 64 bit operation.
      arithmetic_op(0x33, dst, src, kInt32Size);
    } else {
      arithmetic_op(0x33, dst, src, size);
    }
  }

  void emit_xor(Register dst, Operand src, int size) {
    arithmetic_op(0x33, dst, src, size);
  }

  void emit_xor(Register dst, Immediate src, int size) {
    immediate_arithmetic_op(0x6, dst, src, size);
  }

  void emit_xor(Operand dst, Immediate src, int size) {
    immediate_arithmetic_op(0x6, dst, src, size);
  }

  void emit_xor(Operand dst, Register src, int size) {
    arithmetic_op(0x31, src, dst, size);
  }

  // Most BMI instructions are similar.
  void bmi1q(byte op, Register reg, Register vreg, Register rm);
  void bmi1q(byte op, Register reg, Register vreg, Operand rm);
  void bmi1l(byte op, Register reg, Register vreg, Register rm);
  void bmi1l(byte op, Register reg, Register vreg, Operand rm);
  void bmi2q(SIMDPrefix pp, byte op, Register reg, Register vreg, Register rm);
  void bmi2q(SIMDPrefix pp, byte op, Register reg, Register vreg, Operand rm);
  void bmi2l(SIMDPrefix pp, byte op, Register reg, Register vreg, Register rm);
  void bmi2l(SIMDPrefix pp, byte op, Register reg, Register vreg, Operand rm);

  // record the position of jmp/jcc instruction
  void record_farjmp_position(Label* L, int pos);

  bool is_optimizable_farjmp(int idx);

  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);

  friend class EnsureSpace;
  friend class RegExpMacroAssemblerX64;

  // code generation
  RelocInfoWriter reloc_info_writer;

  // Internal reference positions, required for (potential) patching in
  // GrowBuffer(); contains only those internal references whose labels
  // are already bound.
  std::deque<int> internal_reference_positions_;

  // Variables for this instance of assembler
  int farjmp_num_ = 0;
  std::deque<int> farjmp_positions_;
  std::map<Label*, std::vector<int>> label_farjmp_maps_;

  ConstPool constpool_;

  friend class ConstPool;
};


// Helper class that ensures that there is enough space for generating
// instructions and relocation information.  The constructor makes
// sure that there is enough space and (in debug mode) the destructor
// checks that we did not generate too much.
class EnsureSpace {
 public:
  explicit EnsureSpace(Assembler* assembler) : assembler_(assembler) {
    if (assembler_->buffer_overflow()) assembler_->GrowBuffer();
#ifdef DEBUG
    space_before_ = assembler_->available_space();
#endif
  }

#ifdef DEBUG
  ~EnsureSpace() {
    int bytes_generated = space_before_ - assembler_->available_space();
    DCHECK(bytes_generated < assembler_->kGap);
  }
#endif

 private:
  Assembler* assembler_;
#ifdef DEBUG
  int space_before_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_X64_ASSEMBLER_X64_H_
