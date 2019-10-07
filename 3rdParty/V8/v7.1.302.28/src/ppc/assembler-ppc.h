// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the
// distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2014 the V8 project authors. All rights reserved.

// A light-weight PPC Assembler
// Generates user mode instructions for the PPC architecture up

#ifndef V8_PPC_ASSEMBLER_PPC_H_
#define V8_PPC_ASSEMBLER_PPC_H_

#include <stdio.h>
#include <vector>

#include "src/assembler.h"
#include "src/double.h"
#include "src/ppc/constants-ppc.h"

#if V8_HOST_ARCH_PPC && \
    (V8_OS_AIX || (V8_TARGET_ARCH_PPC64 && V8_TARGET_BIG_ENDIAN))
#define ABI_USES_FUNCTION_DESCRIPTORS 1
#else
#define ABI_USES_FUNCTION_DESCRIPTORS 0
#endif

#if !V8_HOST_ARCH_PPC || V8_OS_AIX || V8_TARGET_ARCH_PPC64
#define ABI_PASSES_HANDLES_IN_REGS 1
#else
#define ABI_PASSES_HANDLES_IN_REGS 0
#endif

#if !V8_HOST_ARCH_PPC || !V8_TARGET_ARCH_PPC64 || V8_TARGET_LITTLE_ENDIAN
#define ABI_RETURNS_OBJECT_PAIRS_IN_REGS 1
#else
#define ABI_RETURNS_OBJECT_PAIRS_IN_REGS 0
#endif

#if !V8_HOST_ARCH_PPC || (V8_TARGET_ARCH_PPC64 && V8_TARGET_LITTLE_ENDIAN)
#define ABI_CALL_VIA_IP 1
#else
#define ABI_CALL_VIA_IP 0
#endif

#if !V8_HOST_ARCH_PPC || V8_OS_AIX || V8_TARGET_ARCH_PPC64
#define ABI_TOC_REGISTER 2
#else
#define ABI_TOC_REGISTER 13
#endif

#define INSTR_AND_DATA_CACHE_COHERENCY LWSYNC

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(r0)  V(sp)  V(r2)  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)  \
  V(r8)  V(r9)  V(r10) V(r11) V(ip) V(r13) V(r14) V(r15)  \
  V(r16) V(r17) V(r18) V(r19) V(r20) V(r21) V(r22) V(r23) \
  V(r24) V(r25) V(r26) V(r27) V(r28) V(r29) V(r30) V(fp)

#if V8_EMBEDDED_CONSTANT_POOL
#define ALLOCATABLE_GENERAL_REGISTERS(V)                  \
  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)                       \
  V(r8)  V(r9)  V(r10) V(r14) V(r15)                      \
  V(r16) V(r17) V(r18) V(r19) V(r20) V(r21) V(r22) V(r23) \
  V(r24) V(r25) V(r26) V(r27) V(r30)
#else
#define ALLOCATABLE_GENERAL_REGISTERS(V)                  \
  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)                       \
  V(r8)  V(r9)  V(r10) V(r14) V(r15)                      \
  V(r16) V(r17) V(r18) V(r19) V(r20) V(r21) V(r22) V(r23) \
  V(r24) V(r25) V(r26) V(r27) V(r28) V(r30)
#endif

#define LOW_DOUBLE_REGISTERS(V)                           \
  V(d0)  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)  \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d13) V(d14) V(d15)

#define NON_LOW_DOUBLE_REGISTERS(V)                       \
  V(d16) V(d17) V(d18) V(d19) V(d20) V(d21) V(d22) V(d23) \
  V(d24) V(d25) V(d26) V(d27) V(d28) V(d29) V(d30) V(d31)

#define DOUBLE_REGISTERS(V) \
  LOW_DOUBLE_REGISTERS(V) NON_LOW_DOUBLE_REGISTERS(V)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                   \
  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)         \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d15)               \
  V(d16) V(d17) V(d18) V(d19) V(d20) V(d21) V(d22) V(d23) \
  V(d24) V(d25) V(d26) V(d27) V(d28) V(d29) V(d30) V(d31)

#define C_REGISTERS(V)                                            \
  V(cr0)  V(cr1)  V(cr2)  V(cr3)  V(cr4)  V(cr5)  V(cr6)  V(cr7)  \
  V(cr8)  V(cr9)  V(cr10) V(cr11) V(cr12) V(cr15)
// clang-format on

// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding
const int kNumRegs = 32;

// Caller-saved/arguments registers
const RegList kJSCallerSaved = 1 << 3 |   // r3  a1
                               1 << 4 |   // r4  a2
                               1 << 5 |   // r5  a3
                               1 << 6 |   // r6  a4
                               1 << 7 |   // r7  a5
                               1 << 8 |   // r8  a6
                               1 << 9 |   // r9  a7
                               1 << 10 |  // r10 a8
                               1 << 11;

const int kNumJSCallerSaved = 9;

// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0
int JSCallerSavedCode(int n);

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved = 1 << 14 |  // r14
                             1 << 15 |  // r15
                             1 << 16 |  // r16
                             1 << 17 |  // r17
                             1 << 18 |  // r18
                             1 << 19 |  // r19
                             1 << 20 |  // r20
                             1 << 21 |  // r21
                             1 << 22 |  // r22
                             1 << 23 |  // r23
                             1 << 24 |  // r24
                             1 << 25 |  // r25
                             1 << 26 |  // r26
                             1 << 27 |  // r27
                             1 << 28 |  // r28
                             1 << 29 |  // r29
                             1 << 30 |  // r20
                             1 << 31;   // r31

const int kNumCalleeSaved = 18;

const RegList kCallerSavedDoubles = 1 << 0 |   // d0
                                    1 << 1 |   // d1
                                    1 << 2 |   // d2
                                    1 << 3 |   // d3
                                    1 << 4 |   // d4
                                    1 << 5 |   // d5
                                    1 << 6 |   // d6
                                    1 << 7 |   // d7
                                    1 << 8 |   // d8
                                    1 << 9 |   // d9
                                    1 << 10 |  // d10
                                    1 << 11 |  // d11
                                    1 << 12 |  // d12
                                    1 << 13;   // d13

const int kNumCallerSavedDoubles = 14;

const RegList kCalleeSavedDoubles = 1 << 14 |  // d14
                                    1 << 15 |  // d15
                                    1 << 16 |  // d16
                                    1 << 17 |  // d17
                                    1 << 18 |  // d18
                                    1 << 19 |  // d19
                                    1 << 20 |  // d20
                                    1 << 21 |  // d21
                                    1 << 22 |  // d22
                                    1 << 23 |  // d23
                                    1 << 24 |  // d24
                                    1 << 25 |  // d25
                                    1 << 26 |  // d26
                                    1 << 27 |  // d27
                                    1 << 28 |  // d28
                                    1 << 29 |  // d29
                                    1 << 30 |  // d30
                                    1 << 31;   // d31

const int kNumCalleeSavedDoubles = 18;

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
const int kNumSafepointRegisters = 32;

// The following constants describe the stack frame linkage area as
// defined by the ABI.  Note that kNumRequiredStackFrameSlots must
// satisfy alignment requirements (rounding up if required).
#if V8_TARGET_ARCH_PPC64 && V8_TARGET_LITTLE_ENDIAN  // ppc64le linux
// [0] back chain
// [1] condition register save area
// [2] link register save area
// [3] TOC save area
// [4] Parameter1 save area
// ...
// [11] Parameter8 save area
// [12] Parameter9 slot (if necessary)
// ...
const int kNumRequiredStackFrameSlots = 12;
const int kStackFrameLRSlot = 2;
const int kStackFrameExtraParamSlot = 12;
#else  // AIX
// [0] back chain
// [1] condition register save area
// [2] link register save area
// [3] reserved for compiler
// [4] reserved by binder
// [5] TOC save area
// [6] Parameter1 save area
// ...
// [13] Parameter8 save area
// [14] Parameter9 slot (if necessary)
// ...
const int kNumRequiredStackFrameSlots = 14;
const int kStackFrameLRSlot = 2;
const int kStackFrameExtraParamSlot = 14;
#endif

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
const int kNumSafepointSavedRegisters = kNumJSCallerSaved + kNumCalleeSaved;

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
#if V8_TARGET_LITTLE_ENDIAN
  static constexpr int kMantissaOffset = 0;
  static constexpr int kExponentOffset = 4;
#else
  static constexpr int kMantissaOffset = 4;
  static constexpr int kExponentOffset = 0;
#endif

 private:
  friend class RegisterBase;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(Register);
static_assert(sizeof(Register) == sizeof(int),
              "Register can efficiently be passed by value");

#define DEFINE_REGISTER(R) \
  constexpr Register R = Register::from_code<kRegCode_##R>();
GENERAL_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr Register no_reg = Register::no_reg();

// Aliases
constexpr Register kConstantPoolRegister = r28;  // Constant pool.
constexpr Register kRootRegister = r29;          // Roots array pointer.
constexpr Register cp = r30;                     // JavaScript context pointer.

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

// Double word FP register.
class DoubleRegister : public RegisterBase<DoubleRegister, kDoubleAfterLast> {
 public:
  // A few double registers are reserved: one as a scratch register and one to
  // hold 0.0, that does not fit in the immediate field of vmov instructions.
  // d14: 0.0
  // d15: scratch register.
  static constexpr int kSizeInBytes = 8;
  inline static int NumRegisters();

 private:
  friend class RegisterBase;
  explicit constexpr DoubleRegister(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(DoubleRegister);
static_assert(sizeof(DoubleRegister) == sizeof(int),
              "DoubleRegister can efficiently be passed by value");

typedef DoubleRegister FloatRegister;

// TODO(ppc) Define SIMD registers.
typedef DoubleRegister Simd128Register;

#define DEFINE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code<kDoubleCode_##R>();
DOUBLE_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

constexpr DoubleRegister kFirstCalleeSavedDoubleReg = d14;
constexpr DoubleRegister kLastCalleeSavedDoubleReg = d31;
constexpr DoubleRegister kDoubleRegZero = d14;
constexpr DoubleRegister kScratchDoubleReg = d13;

Register ToRegister(int num);

enum CRegisterCode {
#define REGISTER_CODE(R) kCCode_##R,
  C_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kCAfterLast
};

// Coprocessor register
class CRegister : public RegisterBase<CRegister, kCAfterLast> {
  friend class RegisterBase;
  explicit constexpr CRegister(int code) : RegisterBase(code) {}
};

constexpr CRegister no_creg = CRegister::no_reg();
#define DECLARE_C_REGISTER(R) \
  constexpr CRegister R = CRegister::from_code<kCCode_##R>();
C_REGISTERS(DECLARE_C_REGISTER)
#undef DECLARE_C_REGISTER

// -----------------------------------------------------------------------------
// Machine instruction Operands

// Class Operand represents a shifter operand in data processing instructions
class Operand {
 public:
  // immediate
  V8_INLINE explicit Operand(intptr_t immediate,
                             RelocInfo::Mode rmode = RelocInfo::NONE)
      : rmode_(rmode) {
    value_.immediate = immediate;
  }
  V8_INLINE static Operand Zero() { return Operand(static_cast<intptr_t>(0)); }
  V8_INLINE explicit Operand(const ExternalReference& f)
      : rmode_(RelocInfo::EXTERNAL_REFERENCE) {
    value_.immediate = static_cast<intptr_t>(f.address());
  }
  explicit Operand(Handle<HeapObject> handle);
  V8_INLINE explicit Operand(Smi* value) : rmode_(RelocInfo::NONE) {
    value_.immediate = reinterpret_cast<intptr_t>(value);
  }
  // rm
  V8_INLINE explicit Operand(Register rm);

  static Operand EmbeddedNumber(double number);  // Smi or HeapNumber.
  static Operand EmbeddedStringConstant(const StringConstantBase* str);
  static Operand EmbeddedCode(CodeStub* stub);

  // Return true if this is a register operand.
  V8_INLINE bool is_reg() const { return rm_.is_valid(); }

  bool must_output_reloc_info(const Assembler* assembler) const;

  inline intptr_t immediate() const {
    DCHECK(IsImmediate());
    DCHECK(!IsHeapObjectRequest());
    return value_.immediate;
  }
  bool IsImmediate() const { return !rm_.is_valid(); }

  HeapObjectRequest heap_object_request() const {
    DCHECK(IsHeapObjectRequest());
    return value_.heap_object_request;
  }

  Register rm() const { return rm_; }

  bool IsHeapObjectRequest() const {
    DCHECK_IMPLIES(is_heap_object_request_, IsImmediate());
    DCHECK_IMPLIES(is_heap_object_request_,
                   rmode_ == RelocInfo::EMBEDDED_OBJECT ||
                       rmode_ == RelocInfo::CODE_TARGET);
    return is_heap_object_request_;
  }

 private:
  Register rm_ = no_reg;
  union Value {
    Value() {}
    HeapObjectRequest heap_object_request;  // if is_heap_object_request_
    intptr_t immediate;                     // otherwise
  } value_;                                 // valid if rm_ == no_reg
  bool is_heap_object_request_ = false;

  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};


// Class MemOperand represents a memory operand in load and store instructions
// On PowerPC we have base register + 16bit signed value
// Alternatively we can have a 16bit signed value immediate
class MemOperand {
 public:
  explicit MemOperand(Register rn, int32_t offset = 0);

  explicit MemOperand(Register ra, Register rb);

  int32_t offset() const {
    return offset_;
  }

  // PowerPC - base register
  Register ra() const {
    return ra_;
  }

  Register rb() const {
    return rb_;
  }

 private:
  Register ra_;     // base
  int32_t offset_;  // offset
  Register rb_;     // index

  friend class Assembler;
};


class DeferredRelocInfo {
 public:
  DeferredRelocInfo() {}
  DeferredRelocInfo(int position, RelocInfo::Mode rmode, intptr_t data)
      : position_(position), rmode_(rmode), data_(data) {}

  int position() const { return position_; }
  RelocInfo::Mode rmode() const { return rmode_; }
  intptr_t data() const { return data_; }

 private:
  int position_;
  RelocInfo::Mode rmode_;
  intptr_t data_;
};


class Assembler : public AssemblerBase {
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
  virtual ~Assembler() {}

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(Isolate* isolate, CodeDesc* desc);

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

  // Links a label at the current pc_offset().  If already bound, returns the
  // bound position.  If already linked, returns the position of the prior link.
  // Otherwise, returns the current pc_offset().
  int link(Label* L);

  // Determines if Label is bound and near enough so that a single
  // branch instruction can be used to reach it.
  bool is_near(Label* L, Condition cond);

  // Returns the branch offset to the given label from the current code position
  // Links the label to the current position if it is still unbound
  int branch_offset(Label* L) {
    if (L->is_unused() && !trampoline_emitted_) {
      TrackBranch();
    }
    return link(L) - pc_offset();
  }

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);

  V8_INLINE static bool IsConstantPoolLoadStart(
      Address pc, ConstantPoolEntry::Access* access = nullptr);
  V8_INLINE static bool IsConstantPoolLoadEnd(
      Address pc, ConstantPoolEntry::Access* access = nullptr);
  V8_INLINE static int GetConstantPoolOffset(Address pc,
                                             ConstantPoolEntry::Access access,
                                             ConstantPoolEntry::Type type);
  V8_INLINE void PatchConstantPoolAccessInstruction(
      int pc_offset, int offset, ConstantPoolEntry::Access access,
      ConstantPoolEntry::Type type);

  // Return the address in the constant pool of the code target address used by
  // the branch/call instruction at pc, or the object in a mov.
  V8_INLINE static Address target_constant_pool_address_at(
      Address pc, Address constant_pool, ConstantPoolEntry::Access access,
      ConstantPoolEntry::Type type);

  // Read/Modify the code target address in the branch/call instruction at pc.
  // The isolate argument is unused (and may be nullptr) when skipping flushing.
  V8_INLINE static Address target_address_at(Address pc, Address constant_pool);
  V8_INLINE static void set_target_address_at(
      Address pc, Address constant_pool, Address target,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // Given the address of the beginning of a call, return the address
  // in the instruction stream that the call will return to.
  V8_INLINE static Address return_address_from_call_start(Address pc);

  // This sets the branch destination.
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

  // Here we are patching the address in the LUI/ORI instruction pair.
  // These values are used in the serialization process and must be zero for
  // PPC platform, as Code, Embedded Object or External-reference pointers
  // are split across two consecutive instructions and don't exist separately
  // in the code, so the serializer should not step forwards in memory after
  // a target is resolved and written.
  static constexpr int kSpecialTargetSize = 0;

// Number of instructions to load an address via a mov sequence.
#if V8_TARGET_ARCH_PPC64
  static constexpr int kMovInstructionsConstantPool = 1;
  static constexpr int kMovInstructionsNoConstantPool = 5;
#if defined(V8_PPC_TAGGING_OPT)
  static constexpr int kTaggedLoadInstructions = 1;
#else
  static constexpr int kTaggedLoadInstructions = 2;
#endif
#else
  static constexpr int kMovInstructionsConstantPool = 1;
  static constexpr int kMovInstructionsNoConstantPool = 2;
  static constexpr int kTaggedLoadInstructions = 1;
#endif
  static constexpr int kMovInstructions = FLAG_enable_embedded_constant_pool
                                              ? kMovInstructionsConstantPool
                                              : kMovInstructionsNoConstantPool;

  // Distance between the instruction referring to the address of the call
  // target and the return address.

  // Call sequence is a FIXED_SEQUENCE:
  // mov     r8, @ call address
  // mtlr    r8
  // blrl
  //                      @ return address
  static constexpr int kCallTargetAddressOffset =
      (kMovInstructions + 2) * kInstrSize;

  static inline int encode_crbit(const CRegister& cr, enum CRBit crbit) {
    return ((cr.code() * CRWIDTH) + crbit);
  }

#define DECLARE_PPC_X_INSTRUCTIONS_A_FORM(name, instr_name, instr_value)    \
  inline void name(const Register rt, const Register ra,                    \
                   const Register rb, const RCBit rc = LeaveRC) {           \
    x_form(instr_name, rt, ra, rb, rc);                                     \
  }

#define DECLARE_PPC_X_INSTRUCTIONS_B_FORM(name, instr_name, instr_value)    \
  inline void name(const Register ra, const Register rs,                    \
                   const Register rb, const RCBit rc = LeaveRC) {           \
    x_form(instr_name, rs, ra, rb, rc);                                     \
  }

#define DECLARE_PPC_X_INSTRUCTIONS_C_FORM(name, instr_name, instr_value)    \
  inline void name(const Register dst, const Register src,                  \
                   const RCBit rc = LeaveRC) {                              \
    x_form(instr_name, src, dst, r0, rc);                                   \
  }

#define DECLARE_PPC_X_INSTRUCTIONS_D_FORM(name, instr_name, instr_value) \
  template <class R>                                                     \
  inline void name(const R rt, const Register ra, const Register rb,     \
                   const RCBit rc = LeaveRC) {                           \
    x_form(instr_name, rt.code(), ra.code(), rb.code(), rc);             \
  }                                                                      \
  template <class R>                                                     \
  inline void name(const R dst, const MemOperand& src) {                 \
    name(dst, src.ra(), src.rb());                                       \
  }

#define DECLARE_PPC_X_INSTRUCTIONS_E_FORM(name, instr_name, instr_value)    \
  inline void name(const Register dst, const Register src,                  \
                   const int sh, const RCBit rc = LeaveRC) {                \
    x_form(instr_name, src.code(), dst.code(), sh, rc);                     \
  }

#define DECLARE_PPC_X_INSTRUCTIONS_F_FORM(name, instr_name, instr_value)    \
  inline void name(const Register src1, const Register src2,                \
                   const CRegister cr = cr7, const RCBit rc = LeaveRC) {    \
    x_form(instr_name, cr, src1, src2, rc);                                 \
  }                                                                         \
  inline void name##w(const Register src1, const Register src2,             \
                      const CRegister cr = cr7, const RCBit rc = LeaveRC) { \
    x_form(instr_name, cr.code() * B2, src1.code(), src2.code(), LeaveRC);  \
  }

#define DECLARE_PPC_X_INSTRUCTIONS_EH_S_FORM(name, instr_name, instr_value) \
  inline void name(const Register dst, const MemOperand& src) {             \
    x_form(instr_name, src.ra(), dst, src.rb(), SetEH);                     \
  }
#define DECLARE_PPC_X_INSTRUCTIONS_EH_L_FORM(name, instr_name, instr_value) \
  inline void name(const Register dst, const MemOperand& src) {             \
    DCHECK(src.ra_ != r0);                                                  \
    x_form(instr_name, src.ra(), dst, src.rb(), SetEH);                     \
  }

  inline void x_form(Instr instr, int f1, int f2, int f3, int rc) {
    emit(instr | f1 * B21 | f2 * B16 | f3 * B11 | rc);
  }
  inline void x_form(Instr instr, Register rs, Register ra, Register rb,
                     RCBit rc) {
    emit(instr | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 | rc);
  }
  inline void x_form(Instr instr, Register ra, Register rs, Register rb,
                     EHBit eh = SetEH) {
    emit(instr | rs.code() * B21 | ra.code() * B16 | rb.code() * B11 | eh);
  }
  inline void x_form(Instr instr, CRegister cr, Register s1, Register s2,
                     RCBit rc) {
#if V8_TARGET_ARCH_PPC64
    int L = 1;
#else
    int L = 0;
#endif
    emit(instr | cr.code() * B23 | L * B21 | s1.code() * B16 |
         s2.code() * B11 | rc);
  }

  PPC_X_OPCODE_A_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_A_FORM)
  PPC_X_OPCODE_B_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_B_FORM)
  PPC_X_OPCODE_C_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_C_FORM)
  PPC_X_OPCODE_D_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_D_FORM)
  PPC_X_OPCODE_E_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_E_FORM)
  PPC_X_OPCODE_F_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_F_FORM)
  PPC_X_OPCODE_EH_S_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_EH_S_FORM)
  PPC_X_OPCODE_EH_L_FORM_LIST(DECLARE_PPC_X_INSTRUCTIONS_EH_L_FORM)

  inline void notx(Register dst, Register src, RCBit rc = LeaveRC) {
    nor(dst, src, src, rc);
  }
  inline void lwax(Register rt, const MemOperand& src) {
#if V8_TARGET_ARCH_PPC64
    Register ra = src.ra();
    Register rb = src.rb();
    DCHECK(ra != r0);
    x_form(LWAX, rt, ra, rb, LeaveRC);
#else
    lwzx(rt, src);
#endif
  }
  inline void extsw(Register rs, Register ra, RCBit rc = LeaveRC) {
#if V8_TARGET_ARCH_PPC64
    emit(EXT2 | EXTSW | ra.code() * B21 | rs.code() * B16 | rc);
#else
    // nop on 32-bit
    DCHECK(rs == ra && rc == LeaveRC);
#endif
  }

#undef DECLARE_PPC_X_INSTRUCTIONS_A_FORM
#undef DECLARE_PPC_X_INSTRUCTIONS_B_FORM
#undef DECLARE_PPC_X_INSTRUCTIONS_C_FORM
#undef DECLARE_PPC_X_INSTRUCTIONS_D_FORM
#undef DECLARE_PPC_X_INSTRUCTIONS_E_FORM
#undef DECLARE_PPC_X_INSTRUCTIONS_F_FORM
#undef DECLARE_PPC_X_INSTRUCTIONS_EH_S_FORM
#undef DECLARE_PPC_X_INSTRUCTIONS_EH_L_FORM

#define DECLARE_PPC_XX3_INSTRUCTIONS(name, instr_name, instr_value)  \
  inline void name(const DoubleRegister rt, const DoubleRegister ra, \
                   const DoubleRegister rb) {                        \
    xx3_form(instr_name, rt, ra, rb);                                \
  }

  inline void xx3_form(Instr instr, DoubleRegister t, DoubleRegister a,
                       DoubleRegister b) {
    int AX = ((a.code() & 0x20) >> 5) & 0x1;
    int BX = ((b.code() & 0x20) >> 5) & 0x1;
    int TX = ((t.code() & 0x20) >> 5) & 0x1;

    emit(instr | (t.code() & 0x1F) * B21 | (a.code() & 0x1F) * B16 |
         (b.code() & 0x1F) * B11 | AX * B2 | BX * B1 | TX);
  }

  PPC_XX3_OPCODE_LIST(DECLARE_PPC_XX3_INSTRUCTIONS)
#undef DECLARE_PPC_XX3_INSTRUCTIONS

  // ---------------------------------------------------------------------------
  // Code generation

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Insert the smallest number of zero bytes possible to align the pc offset
  // to a mulitple of m. m must be a power of 2 (>= 2).
  void DataAlign(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  // Branch instructions
  void bclr(BOfield bo, int condition_bit, LKBit lk);
  void blr();
  void bc(int branch_offset, BOfield bo, int condition_bit, LKBit lk = LeaveLK);
  void b(int branch_offset, LKBit lk);

  void bcctr(BOfield bo, int condition_bit, LKBit lk);
  void bctr();
  void bctrl();

  // Convenience branch instructions using labels
  void b(Label* L, LKBit lk = LeaveLK) { b(branch_offset(L), lk); }

  inline CRegister cmpi_optimization(CRegister cr) {
    // Check whether the branch is preceded by an optimizable cmpi against 0.
    // The cmpi can be deleted if it is also preceded by an instruction that
    // sets the register used by the compare and supports a dot form.
    unsigned int sradi_mask = kOpcodeMask | kExt2OpcodeVariant2Mask;
    unsigned int srawi_mask = kOpcodeMask | kExt2OpcodeMask;
    int pos = pc_offset();
    int cmpi_pos = pc_offset() - kInstrSize;

    if (cmpi_pos > 0 && optimizable_cmpi_pos_ == cmpi_pos &&
        cmpi_cr_.code() == cr.code() && last_bound_pos_ != pos) {
      int xpos = cmpi_pos - kInstrSize;
      int xinstr = instr_at(xpos);
      int cmpi_ra = (instr_at(cmpi_pos) & 0x1f0000) >> 16;
      // ra is at the same bit position for the three cases below.
      int ra = (xinstr & 0x1f0000) >> 16;
      if (cmpi_ra == ra) {
        if ((xinstr & sradi_mask) == (EXT2 | SRADIX)) {
          cr = cr0;
          instr_at_put(xpos, xinstr | SetRC);
          pc_ -= kInstrSize;
        } else if ((xinstr & srawi_mask) == (EXT2 | SRAWIX)) {
          cr = cr0;
          instr_at_put(xpos, xinstr | SetRC);
          pc_ -= kInstrSize;
        } else if ((xinstr & kOpcodeMask) == ANDIx) {
          cr = cr0;
          pc_ -= kInstrSize;
          // nothing to do here since andi. records.
        }
        // didn't match one of the above, must keep cmpwi.
      }
    }
    return cr;
  }

  void bc_short(Condition cond, Label* L, CRegister cr = cr7,
                LKBit lk = LeaveLK) {
    DCHECK(cond != al);
    DCHECK(cr.code() >= 0 && cr.code() <= 7);

    cr = cmpi_optimization(cr);

    int b_offset = branch_offset(L);

    switch (cond) {
      case eq:
        bc(b_offset, BT, encode_crbit(cr, CR_EQ), lk);
        break;
      case ne:
        bc(b_offset, BF, encode_crbit(cr, CR_EQ), lk);
        break;
      case gt:
        bc(b_offset, BT, encode_crbit(cr, CR_GT), lk);
        break;
      case le:
        bc(b_offset, BF, encode_crbit(cr, CR_GT), lk);
        break;
      case lt:
        bc(b_offset, BT, encode_crbit(cr, CR_LT), lk);
        break;
      case ge:
        bc(b_offset, BF, encode_crbit(cr, CR_LT), lk);
        break;
      case unordered:
        bc(b_offset, BT, encode_crbit(cr, CR_FU), lk);
        break;
      case ordered:
        bc(b_offset, BF, encode_crbit(cr, CR_FU), lk);
        break;
      case overflow:
        bc(b_offset, BT, encode_crbit(cr, CR_SO), lk);
        break;
      case nooverflow:
        bc(b_offset, BF, encode_crbit(cr, CR_SO), lk);
        break;
      default:
        UNIMPLEMENTED();
    }
  }

  void bclr(Condition cond, CRegister cr = cr7, LKBit lk = LeaveLK) {
    DCHECK(cond != al);
    DCHECK(cr.code() >= 0 && cr.code() <= 7);

    cr = cmpi_optimization(cr);

    switch (cond) {
      case eq:
        bclr(BT, encode_crbit(cr, CR_EQ), lk);
        break;
      case ne:
        bclr(BF, encode_crbit(cr, CR_EQ), lk);
        break;
      case gt:
        bclr(BT, encode_crbit(cr, CR_GT), lk);
        break;
      case le:
        bclr(BF, encode_crbit(cr, CR_GT), lk);
        break;
      case lt:
        bclr(BT, encode_crbit(cr, CR_LT), lk);
        break;
      case ge:
        bclr(BF, encode_crbit(cr, CR_LT), lk);
        break;
      case unordered:
        bclr(BT, encode_crbit(cr, CR_FU), lk);
        break;
      case ordered:
        bclr(BF, encode_crbit(cr, CR_FU), lk);
        break;
      case overflow:
        bclr(BT, encode_crbit(cr, CR_SO), lk);
        break;
      case nooverflow:
        bclr(BF, encode_crbit(cr, CR_SO), lk);
        break;
      default:
        UNIMPLEMENTED();
    }
  }

  void isel(Register rt, Register ra, Register rb, int cb);
  void isel(Condition cond, Register rt, Register ra, Register rb,
            CRegister cr = cr7) {
    DCHECK(cond != al);
    DCHECK(cr.code() >= 0 && cr.code() <= 7);

    cr = cmpi_optimization(cr);

    switch (cond) {
      case eq:
        isel(rt, ra, rb, encode_crbit(cr, CR_EQ));
        break;
      case ne:
        isel(rt, rb, ra, encode_crbit(cr, CR_EQ));
        break;
      case gt:
        isel(rt, ra, rb, encode_crbit(cr, CR_GT));
        break;
      case le:
        isel(rt, rb, ra, encode_crbit(cr, CR_GT));
        break;
      case lt:
        isel(rt, ra, rb, encode_crbit(cr, CR_LT));
        break;
      case ge:
        isel(rt, rb, ra, encode_crbit(cr, CR_LT));
        break;
      case unordered:
        isel(rt, ra, rb, encode_crbit(cr, CR_FU));
        break;
      case ordered:
        isel(rt, rb, ra, encode_crbit(cr, CR_FU));
        break;
      case overflow:
        isel(rt, ra, rb, encode_crbit(cr, CR_SO));
        break;
      case nooverflow:
        isel(rt, rb, ra, encode_crbit(cr, CR_SO));
        break;
      default:
        UNIMPLEMENTED();
    }
  }

  void b(Condition cond, Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    if (cond == al) {
      b(L, lk);
      return;
    }

    if ((L->is_bound() && is_near(L, cond)) || !is_trampoline_emitted()) {
      bc_short(cond, L, cr, lk);
      return;
    }

    Label skip;
    Condition neg_cond = NegateCondition(cond);
    bc_short(neg_cond, &skip, cr);
    b(L, lk);
    bind(&skip);
  }

  void bne(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(ne, L, cr, lk);
  }
  void beq(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(eq, L, cr, lk);
  }
  void blt(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(lt, L, cr, lk);
  }
  void bge(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(ge, L, cr, lk);
  }
  void ble(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(le, L, cr, lk);
  }
  void bgt(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(gt, L, cr, lk);
  }
  void bunordered(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(unordered, L, cr, lk);
  }
  void bordered(Label* L, CRegister cr = cr7, LKBit lk = LeaveLK) {
    b(ordered, L, cr, lk);
  }
  void boverflow(Label* L, CRegister cr = cr0, LKBit lk = LeaveLK) {
    b(overflow, L, cr, lk);
  }
  void bnooverflow(Label* L, CRegister cr = cr0, LKBit lk = LeaveLK) {
    b(nooverflow, L, cr, lk);
  }

  // Decrement CTR; branch if CTR != 0
  void bdnz(Label* L, LKBit lk = LeaveLK) {
    bc(branch_offset(L), DCBNZ, 0, lk);
  }

  // Data-processing instructions

  void sub(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
           RCBit r = LeaveRC);

  void subc(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
            RCBit r = LeaveRC);
  void sube(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
            RCBit r = LeaveRC);

  void subfic(Register dst, Register src, const Operand& imm);

  void add(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
           RCBit r = LeaveRC);

  void addc(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
            RCBit r = LeaveRC);
  void adde(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
            RCBit r = LeaveRC);
  void addze(Register dst, Register src1, OEBit o = LeaveOE, RCBit r = LeaveRC);

  void mullw(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);

  void mulhw(Register dst, Register src1, Register src2, RCBit r = LeaveRC);
  void mulhwu(Register dst, Register src1, Register src2, RCBit r = LeaveRC);

  void divw(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
            RCBit r = LeaveRC);
  void divwu(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);

  void addi(Register dst, Register src, const Operand& imm);
  void addis(Register dst, Register src, const Operand& imm);
  void addic(Register dst, Register src, const Operand& imm);

  void andi(Register ra, Register rs, const Operand& imm);
  void andis(Register ra, Register rs, const Operand& imm);
  void ori(Register dst, Register src, const Operand& imm);
  void oris(Register dst, Register src, const Operand& imm);
  void xori(Register dst, Register src, const Operand& imm);
  void xoris(Register ra, Register rs, const Operand& imm);
  void cmpi(Register src1, const Operand& src2, CRegister cr = cr7);
  void cmpli(Register src1, const Operand& src2, CRegister cr = cr7);
  void cmpwi(Register src1, const Operand& src2, CRegister cr = cr7);
  void cmplwi(Register src1, const Operand& src2, CRegister cr = cr7);
  void li(Register dst, const Operand& src);
  void lis(Register dst, const Operand& imm);
  void mr(Register dst, Register src);

  void lbz(Register dst, const MemOperand& src);
  void lhz(Register dst, const MemOperand& src);
  void lha(Register dst, const MemOperand& src);
  void lwz(Register dst, const MemOperand& src);
  void lwzu(Register dst, const MemOperand& src);
  void lwa(Register dst, const MemOperand& src);
  void stb(Register dst, const MemOperand& src);
  void sth(Register dst, const MemOperand& src);
  void stw(Register dst, const MemOperand& src);
  void stwu(Register dst, const MemOperand& src);
  void neg(Register rt, Register ra, OEBit o = LeaveOE, RCBit c = LeaveRC);

#if V8_TARGET_ARCH_PPC64
  void ld(Register rd, const MemOperand& src);
  void ldu(Register rd, const MemOperand& src);
  void std(Register rs, const MemOperand& src);
  void stdu(Register rs, const MemOperand& src);
  void rldic(Register dst, Register src, int sh, int mb, RCBit r = LeaveRC);
  void rldicl(Register dst, Register src, int sh, int mb, RCBit r = LeaveRC);
  void rldcl(Register ra, Register rs, Register rb, int mb, RCBit r = LeaveRC);
  void rldicr(Register dst, Register src, int sh, int me, RCBit r = LeaveRC);
  void rldimi(Register dst, Register src, int sh, int mb, RCBit r = LeaveRC);
  void sldi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void srdi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void clrrdi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void clrldi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void sradi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void rotld(Register ra, Register rs, Register rb, RCBit r = LeaveRC);
  void rotldi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void rotrdi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void mulld(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);
  void divd(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
            RCBit r = LeaveRC);
  void divdu(Register dst, Register src1, Register src2, OEBit o = LeaveOE,
             RCBit r = LeaveRC);
#endif

  void rlwinm(Register ra, Register rs, int sh, int mb, int me,
              RCBit rc = LeaveRC);
  void rlwimi(Register ra, Register rs, int sh, int mb, int me,
              RCBit rc = LeaveRC);
  void rlwnm(Register ra, Register rs, Register rb, int mb, int me,
             RCBit rc = LeaveRC);
  void slwi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void srwi(Register dst, Register src, const Operand& val, RCBit rc = LeaveRC);
  void clrrwi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void clrlwi(Register dst, Register src, const Operand& val,
              RCBit rc = LeaveRC);
  void rotlw(Register ra, Register rs, Register rb, RCBit r = LeaveRC);
  void rotlwi(Register ra, Register rs, int sh, RCBit r = LeaveRC);
  void rotrwi(Register ra, Register rs, int sh, RCBit r = LeaveRC);

  void subi(Register dst, Register src1, const Operand& src2);

  void mov(Register dst, const Operand& src);
  void bitwise_mov(Register dst, intptr_t value);
  void bitwise_mov32(Register dst, int32_t value);
  void bitwise_add32(Register dst, Register src, int32_t value);

  // Load the position of the label relative to the generated code object
  // pointer in a register.
  void mov_label_offset(Register dst, Label* label);

  // dst = base + label position + delta
  void add_label_offset(Register dst, Register base, Label* label,
                        int delta = 0);

  // Load the address of the label in a register and associate with an
  // internal reference relocation.
  void mov_label_addr(Register dst, Label* label);

  // Emit the address of the label (i.e. a jump table entry) and associate with
  // an internal reference relocation.
  void emit_label_addr(Label* label);

  // Multiply instructions
  void mul(Register dst, Register src1, Register src2, OEBit s = LeaveOE,
           RCBit r = LeaveRC);

  // Miscellaneous arithmetic instructions

  // Special register access
  void crxor(int bt, int ba, int bb);
  void crclr(int bt) { crxor(bt, bt, bt); }
  void creqv(int bt, int ba, int bb);
  void crset(int bt) { creqv(bt, bt, bt); }
  void mflr(Register dst);
  void mtlr(Register src);
  void mtctr(Register src);
  void mtxer(Register src);
  void mcrfs(CRegister cr, FPSCRBit bit);
  void mfcr(Register dst);
#if V8_TARGET_ARCH_PPC64
  void mffprd(Register dst, DoubleRegister src);
  void mffprwz(Register dst, DoubleRegister src);
  void mtfprd(DoubleRegister dst, Register src);
  void mtfprwz(DoubleRegister dst, Register src);
  void mtfprwa(DoubleRegister dst, Register src);
#endif

  void function_descriptor();

  // Exception-generating instructions and debugging support
  void stop(const char* msg, Condition cond = al,
            int32_t code = kDefaultStopCode, CRegister cr = cr7);

  void bkpt(uint32_t imm16);  // v5 and above

  void dcbf(Register ra, Register rb);
  void sync();
  void lwsync();
  void icbi(Register ra, Register rb);
  void isync();

  // Support for floating point
  void lfd(const DoubleRegister frt, const MemOperand& src);
  void lfdu(const DoubleRegister frt, const MemOperand& src);
  void lfs(const DoubleRegister frt, const MemOperand& src);
  void lfsu(const DoubleRegister frt, const MemOperand& src);
  void stfd(const DoubleRegister frs, const MemOperand& src);
  void stfdu(const DoubleRegister frs, const MemOperand& src);
  void stfs(const DoubleRegister frs, const MemOperand& src);
  void stfsu(const DoubleRegister frs, const MemOperand& src);

  void fadd(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frb, RCBit rc = LeaveRC);
  void fsub(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frb, RCBit rc = LeaveRC);
  void fdiv(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frb, RCBit rc = LeaveRC);
  void fmul(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frc, RCBit rc = LeaveRC);
  void fcmpu(const DoubleRegister fra, const DoubleRegister frb,
             CRegister cr = cr7);
  void fmr(const DoubleRegister frt, const DoubleRegister frb,
           RCBit rc = LeaveRC);
  void fctiwz(const DoubleRegister frt, const DoubleRegister frb);
  void fctiw(const DoubleRegister frt, const DoubleRegister frb);
  void frin(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void friz(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void frip(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void frim(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void frsp(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void fcfid(const DoubleRegister frt, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fcfidu(const DoubleRegister frt, const DoubleRegister frb,
              RCBit rc = LeaveRC);
  void fcfidus(const DoubleRegister frt, const DoubleRegister frb,
               RCBit rc = LeaveRC);
  void fcfids(const DoubleRegister frt, const DoubleRegister frb,
              RCBit rc = LeaveRC);
  void fctid(const DoubleRegister frt, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fctidz(const DoubleRegister frt, const DoubleRegister frb,
              RCBit rc = LeaveRC);
  void fctidu(const DoubleRegister frt, const DoubleRegister frb,
              RCBit rc = LeaveRC);
  void fctiduz(const DoubleRegister frt, const DoubleRegister frb,
               RCBit rc = LeaveRC);
  void fsel(const DoubleRegister frt, const DoubleRegister fra,
            const DoubleRegister frc, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void fneg(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void mtfsb0(FPSCRBit bit, RCBit rc = LeaveRC);
  void mtfsb1(FPSCRBit bit, RCBit rc = LeaveRC);
  void mtfsfi(int bf, int immediate, RCBit rc = LeaveRC);
  void mffs(const DoubleRegister frt, RCBit rc = LeaveRC);
  void mtfsf(const DoubleRegister frb, bool L = 1, int FLM = 0, bool W = 0,
             RCBit rc = LeaveRC);
  void fsqrt(const DoubleRegister frt, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fabs(const DoubleRegister frt, const DoubleRegister frb,
            RCBit rc = LeaveRC);
  void fmadd(const DoubleRegister frt, const DoubleRegister fra,
             const DoubleRegister frc, const DoubleRegister frb,
             RCBit rc = LeaveRC);
  void fmsub(const DoubleRegister frt, const DoubleRegister fra,
             const DoubleRegister frc, const DoubleRegister frb,
             RCBit rc = LeaveRC);

  // Pseudo instructions

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    NON_MARKING_NOP = 0,
    GROUP_ENDING_NOP,
    DEBUG_BREAK_NOP,
    // IC markers.
    PROPERTY_ACCESS_INLINED,
    PROPERTY_ACCESS_INLINED_CONTEXT,
    PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE,
    // Helper values.
    LAST_CODE_MARKER,
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED
  };

  void nop(int type = 0);  // 0 is the default non-marking type.

  void push(Register src) {
#if V8_TARGET_ARCH_PPC64
    stdu(src, MemOperand(sp, -kPointerSize));
#else
    stwu(src, MemOperand(sp, -kPointerSize));
#endif
  }

  void pop(Register dst) {
#if V8_TARGET_ARCH_PPC64
    ld(dst, MemOperand(sp));
#else
    lwz(dst, MemOperand(sp));
#endif
    addi(sp, sp, Operand(kPointerSize));
  }

  void pop() { addi(sp, sp, Operand(kPointerSize)); }

  // Jump unconditionally to given label.
  void jmp(Label* L) { b(L); }

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Check the number of instructions generated from label to here.
  int InstructionsGeneratedSince(Label* label) {
    return SizeOfCodeGeneratedSince(label) / kInstrSize;
  }

  // Class for scoping postponing the trampoline pool generation.
  class BlockTrampolinePoolScope {
   public:
    explicit BlockTrampolinePoolScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockTrampolinePool();
    }
    ~BlockTrampolinePoolScope() { assem_->EndBlockTrampolinePool(); }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockTrampolinePoolScope);
  };

  // Class for scoping disabling constant pool entry merging
  class BlockConstantPoolEntrySharingScope {
   public:
    explicit BlockConstantPoolEntrySharingScope(Assembler* assem)
        : assem_(assem) {
      assem_->StartBlockConstantPoolEntrySharing();
    }
    ~BlockConstantPoolEntrySharingScope() {
      assem_->EndBlockConstantPoolEntrySharing();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockConstantPoolEntrySharingScope);
  };

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Record a deoptimization reason that can be used by a log or cpu profiler.
  // Use --trace-deopt to enable.
  void RecordDeoptReason(DeoptimizeReason reason, SourcePosition position,
                         int id);

  // Writes a single byte or word of data in the code stream.  Used
  // for inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void dq(uint64_t data);
  void dp(uintptr_t data);

  // Read/patch instructions
  Instr instr_at(int pos) { return *reinterpret_cast<Instr*>(buffer_ + pos); }
  void instr_at_put(int pos, Instr instr) {
    *reinterpret_cast<Instr*>(buffer_ + pos) = instr;
  }
  static Instr instr_at(Address pc) { return *reinterpret_cast<Instr*>(pc); }
  static void instr_at_put(Address pc, Instr instr) {
    *reinterpret_cast<Instr*>(pc) = instr;
  }
  static Condition GetCondition(Instr instr);

  static bool IsLis(Instr instr);
  static bool IsLi(Instr instr);
  static bool IsAddic(Instr instr);
  static bool IsOri(Instr instr);

  static bool IsBranch(Instr instr);
  static Register GetRA(Instr instr);
  static Register GetRB(Instr instr);
#if V8_TARGET_ARCH_PPC64
  static bool Is64BitLoadIntoR12(Instr instr1, Instr instr2, Instr instr3,
                                 Instr instr4, Instr instr5);
#else
  static bool Is32BitLoadIntoR12(Instr instr1, Instr instr2);
#endif

  static bool IsCmpRegister(Instr instr);
  static bool IsCmpImmediate(Instr instr);
  static bool IsRlwinm(Instr instr);
  static bool IsAndi(Instr instr);
#if V8_TARGET_ARCH_PPC64
  static bool IsRldicl(Instr instr);
#endif
  static bool IsCrSet(Instr instr);
  static Register GetCmpImmediateRegister(Instr instr);
  static int GetCmpImmediateRawImmediate(Instr instr);
  static bool IsNop(Instr instr, int type = NON_MARKING_NOP);

  // Postpone the generation of the trampoline pool for the specified number of
  // instructions.
  void BlockTrampolinePoolFor(int instructions);
  void CheckTrampolinePool();

  // For mov.  Return the number of actual instructions required to
  // load the operand into a register.  This can be anywhere from
  // one (constant pool small section) to five instructions (full
  // 64-bit sequence).
  //
  // The value returned is only valid as long as no entries are added to the
  // constant pool between this call and the actual instruction being emitted.
  int instructions_required_for_mov(Register dst, const Operand& src) const;

  // Decide between using the constant pool vs. a mov immediate sequence.
  bool use_constant_pool_for_mov(Register dst, const Operand& src,
                                 bool canOptimize) const;

  // The code currently calls CheckBuffer() too often. This has the side
  // effect of randomly growing the buffer in the middle of multi-instruction
  // sequences.
  //
  // This function allows outside callers to check and grow the buffer
  void EnsureSpaceFor(int space_needed);

  int EmitConstantPool() { return constant_pool_builder_.Emit(this); }

  bool ConstantPoolAccessIsInOverflow() const {
    return constant_pool_builder_.NextAccess(ConstantPoolEntry::INTPTR) ==
           ConstantPoolEntry::OVERFLOWED;
  }

  Label* ConstantPoolPosition() {
    return constant_pool_builder_.EmittedPosition();
  }

  void EmitRelocations();

 protected:
  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode instruction(s) at pos and return backchain to previous
  // label reference or kEndOfChain.
  int target_at(int pos);

  // Patch instruction(s) at pos to target target_pos (e.g. branch)
  void target_at_put(int pos, int target_pos, bool* is_branch = nullptr);

  // Record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);
  ConstantPoolEntry::Access ConstantPoolAddEntry(RelocInfo::Mode rmode,
                                                 intptr_t value) {
    bool sharing_ok = RelocInfo::IsNone(rmode) ||
                      (!options().record_reloc_info_for_serialization &&
                       RelocInfo::IsShareableRelocMode(rmode) &&
                       !is_constant_pool_entry_sharing_blocked());
    return constant_pool_builder_.AddEntry(pc_offset(), value, sharing_ok);
  }
  ConstantPoolEntry::Access ConstantPoolAddEntry(Double value) {
    return constant_pool_builder_.AddEntry(pc_offset(), value);
  }

  // Block the emission of the trampoline pool before pc_offset.
  void BlockTrampolinePoolBefore(int pc_offset) {
    if (no_trampoline_pool_before_ < pc_offset)
      no_trampoline_pool_before_ = pc_offset;
  }

  void StartBlockTrampolinePool() { trampoline_pool_blocked_nesting_++; }
  void EndBlockTrampolinePool() {
    int count = --trampoline_pool_blocked_nesting_;
    if (count == 0) CheckTrampolinePoolQuick();
  }
  bool is_trampoline_pool_blocked() const {
    return trampoline_pool_blocked_nesting_ > 0;
  }

  void StartBlockConstantPoolEntrySharing() {
    constant_pool_entry_sharing_blocked_nesting_++;
  }
  void EndBlockConstantPoolEntrySharing() {
    constant_pool_entry_sharing_blocked_nesting_--;
  }
  bool is_constant_pool_entry_sharing_blocked() const {
    return constant_pool_entry_sharing_blocked_nesting_ > 0;
  }

  bool has_exception() const { return internal_trampoline_exception_; }

  bool is_trampoline_emitted() const { return trampoline_emitted_; }

  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static constexpr int kGap = 32;

  RelocInfoWriter reloc_info_writer;

 private:
  // Avoid overflows for displacements etc.
  static const int kMaximalBufferSize = 512 * MB;

  // Repeated checking whether the trampoline pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated.
  int next_trampoline_check_;  // pc offset of next buffer check.

  // Emission of the trampoline pool may be blocked in some code sequences.
  int trampoline_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_trampoline_pool_before_;  // Block emission before this pc offset.

  // Do not share constant pool entries.
  int constant_pool_entry_sharing_blocked_nesting_;

  // Relocation info generation
  // Each relocation is encoded as a variable size value
  static constexpr int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  std::vector<DeferredRelocInfo> relocations_;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;
  // Optimizable cmpi information.
  int optimizable_cmpi_pos_;
  CRegister cmpi_cr_ = CRegister::no_reg();

  ConstantPoolBuilder constant_pool_builder_;

  void CheckBuffer() {
    if (buffer_space() <= kGap) {
      GrowBuffer();
    }
  }

  void GrowBuffer(int needed = 0);
  // Code emission
  void emit(Instr x) {
    CheckBuffer();
    *reinterpret_cast<Instr*>(pc_) = x;
    pc_ += kInstrSize;
    CheckTrampolinePoolQuick();
  }
  void TrackBranch() {
    DCHECK(!trampoline_emitted_);
    int count = tracked_branch_count_++;
    if (count == 0) {
      // We leave space (kMaxBlockTrampolineSectionSize)
      // for BlockTrampolinePoolScope buffer.
      next_trampoline_check_ =
          pc_offset() + kMaxCondBranchReach - kMaxBlockTrampolineSectionSize;
    } else {
      next_trampoline_check_ -= kTrampolineSlotsSize;
    }
  }

  inline void UntrackBranch();
  void CheckTrampolinePoolQuick() {
    if (pc_offset() >= next_trampoline_check_) {
      CheckTrampolinePool();
    }
  }

  // Instruction generation
  void a_form(Instr instr, DoubleRegister frt, DoubleRegister fra,
              DoubleRegister frb, RCBit r);
  void d_form(Instr instr, Register rt, Register ra, const intptr_t val,
              bool signed_disp);
  void xo_form(Instr instr, Register rt, Register ra, Register rb, OEBit o,
               RCBit r);
  void md_form(Instr instr, Register ra, Register rs, int shift, int maskbit,
               RCBit r);
  void mds_form(Instr instr, Register ra, Register rs, Register rb, int maskbit,
                RCBit r);

  // Labels
  void print(Label* L);
  int max_reach_from(int pos);
  void bind_to(Label* L, int pos);
  void next(Label* L);

  class Trampoline {
   public:
    Trampoline() {
      next_slot_ = 0;
      free_slot_count_ = 0;
    }
    Trampoline(int start, int slot_count) {
      next_slot_ = start;
      free_slot_count_ = slot_count;
    }
    int take_slot() {
      int trampoline_slot = kInvalidSlotPos;
      if (free_slot_count_ <= 0) {
        // We have run out of space on trampolines.
        // Make sure we fail in debug mode, so we become aware of each case
        // when this happens.
        DCHECK(0);
        // Internal exception will be caught.
      } else {
        trampoline_slot = next_slot_;
        free_slot_count_--;
        next_slot_ += kTrampolineSlotsSize;
      }
      return trampoline_slot;
    }

   private:
    int next_slot_;
    int free_slot_count_;
  };

  int32_t get_trampoline_entry();
  int tracked_branch_count_;
  // If trampoline is emitted, generated code is becoming large. As
  // this is already a slow case which can possibly break our code
  // generation for the extreme case, we use this information to
  // trigger different mode of branch instruction generation, where we
  // no longer use a single branch instruction.
  bool trampoline_emitted_;
  static constexpr int kTrampolineSlotsSize = kInstrSize;
  static constexpr int kMaxCondBranchReach = (1 << (16 - 1)) - 1;
  static constexpr int kMaxBlockTrampolineSectionSize = 64 * kInstrSize;
  static constexpr int kInvalidSlotPos = -1;

  Trampoline trampoline_;
  bool internal_trampoline_exception_;

  void AllocateAndInstallRequestedHeapObjects(Isolate* isolate);

  friend class RegExpMacroAssemblerPPC;
  friend class RelocInfo;
  friend class BlockTrampolinePoolScope;
  friend class EnsureSpace;
};

class EnsureSpace {
 public:
  explicit EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }
};

class PatchingAssembler : public Assembler {
 public:
  PatchingAssembler(const AssemblerOptions& options, byte* address,
                    int instructions);
  ~PatchingAssembler();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PPC_ASSEMBLER_PPC_H_
