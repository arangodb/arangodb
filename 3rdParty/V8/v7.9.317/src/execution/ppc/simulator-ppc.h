// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Declares a Simulator for PPC instructions if we are not generating a native
// PPC binary. This Simulator allows us to run and debug PPC code generation on
// regular desktop machines.
// V8 calls into generated code via the GeneratedCode wrapper,
// which will start execution in the Simulator or forwards to the real entry
// on a PPC HW platform.

#ifndef V8_EXECUTION_PPC_SIMULATOR_PPC_H_
#define V8_EXECUTION_PPC_SIMULATOR_PPC_H_

// globals.h defines USE_SIMULATOR.
#include "src/common/globals.h"

#if defined(USE_SIMULATOR)
// Running with a simulator.

#include "src/base/lazy-instance.h"
#include "src/base/platform/mutex.h"
#include "src/utils/allocation.h"

#include "src/base/hashmap.h"
#include "src/codegen/assembler.h"
#include "src/codegen/ppc/constants-ppc.h"
#include "src/execution/simulator-base.h"

namespace v8 {
namespace internal {

class CachePage {
 public:
  static const int LINE_VALID = 0;
  static const int LINE_INVALID = 1;

  static const int kPageShift = 12;
  static const int kPageSize = 1 << kPageShift;
  static const int kPageMask = kPageSize - 1;
  static const int kLineShift = 2;  // The cache line is only 4 bytes right now.
  static const int kLineLength = 1 << kLineShift;
  static const int kLineMask = kLineLength - 1;

  CachePage() { memset(&validity_map_, LINE_INVALID, sizeof(validity_map_)); }

  char* ValidityByte(int offset) {
    return &validity_map_[offset >> kLineShift];
  }

  char* CachedData(int offset) { return &data_[offset]; }

 private:
  char data_[kPageSize];  // The cached data.
  static const int kValidityMapSize = kPageSize >> kLineShift;
  char validity_map_[kValidityMapSize];  // One byte per line.
};

class Simulator : public SimulatorBase {
 public:
  friend class PPCDebugger;
  enum Register {
    no_reg = -1,
    r0 = 0,
    sp,
    r2,
    r3,
    r4,
    r5,
    r6,
    r7,
    r8,
    r9,
    r10,
    r11,
    r12,
    r13,
    r14,
    r15,
    r16,
    r17,
    r18,
    r19,
    r20,
    r21,
    r22,
    r23,
    r24,
    r25,
    r26,
    r27,
    r28,
    r29,
    r30,
    fp,
    kNumGPRs = 32,
    d0 = 0,
    d1,
    d2,
    d3,
    d4,
    d5,
    d6,
    d7,
    d8,
    d9,
    d10,
    d11,
    d12,
    d13,
    d14,
    d15,
    d16,
    d17,
    d18,
    d19,
    d20,
    d21,
    d22,
    d23,
    d24,
    d25,
    d26,
    d27,
    d28,
    d29,
    d30,
    d31,
    kNumFPRs = 32
  };

  explicit Simulator(Isolate* isolate);
  ~Simulator();

  // The currently executing Simulator instance. Potentially there can be one
  // for each native thread.
  static Simulator* current(v8::internal::Isolate* isolate);

  // Accessors for register state.
  void set_register(int reg, intptr_t value);
  intptr_t get_register(int reg) const;
  double get_double_from_register_pair(int reg);
  void set_d_register_from_double(int dreg, const double dbl) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    *bit_cast<double*>(&fp_registers_[dreg]) = dbl;
  }
  double get_double_from_d_register(int dreg) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    return *bit_cast<double*>(&fp_registers_[dreg]);
  }
  void set_d_register(int dreg, int64_t value) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    fp_registers_[dreg] = value;
  }
  int64_t get_d_register(int dreg) {
    DCHECK(dreg >= 0 && dreg < kNumFPRs);
    return fp_registers_[dreg];
  }

  // Special case of set_register and get_register to access the raw PC value.
  void set_pc(intptr_t value);
  intptr_t get_pc() const;

  Address get_sp() const { return static_cast<Address>(get_register(sp)); }

  // Accessor to the internal Link Register
  intptr_t get_lr() const;

  // Accessor to the internal simulator stack area.
  uintptr_t StackLimit(uintptr_t c_limit) const;

  // Executes PPC instructions until the PC reaches end_sim_pc.
  void Execute();

  template <typename Return, typename... Args>
  Return Call(Address entry, Args... args) {
    return VariadicCall<Return>(this, &Simulator::CallImpl, entry, args...);
  }

  // Alternative: call a 2-argument double function.
  void CallFP(Address entry, double d0, double d1);
  int32_t CallFPReturnsInt(Address entry, double d0, double d1);
  double CallFPReturnsDouble(Address entry, double d0, double d1);

  // Push an address onto the JS stack.
  uintptr_t PushAddress(uintptr_t address);

  // Pop an address from the JS stack.
  uintptr_t PopAddress();

  // Debugger input.
  void set_last_debugger_input(char* input);
  char* last_debugger_input() { return last_debugger_input_; }

  // Redirection support.
  static void SetRedirectInstruction(Instruction* instruction);

  // ICache checking.
  static bool ICacheMatch(void* one, void* two);
  static void FlushICache(base::CustomMatcherHashMap* i_cache, void* start,
                          size_t size);

  // Returns true if pc register contains one of the 'special_values' defined
  // below (bad_lr, end_sim_pc).
  bool has_bad_pc() const;

 private:
  enum special_values {
    // Known bad pc value to ensure that the simulator does not execute
    // without being properly setup.
    bad_lr = -1,
    // A pc value used to signal the simulator to stop execution.  Generally
    // the lr is set to this value on transition from native C code to
    // simulated execution, so that the simulator can "return" to the native
    // C code.
    end_sim_pc = -2
  };

  intptr_t CallImpl(Address entry, int argument_count,
                    const intptr_t* arguments);

  enum BCType { BC_OFFSET, BC_LINK_REG, BC_CTR_REG };

  // Unsupported instructions use Format to print an error and stop execution.
  void Format(Instruction* instr, const char* format);

  // Helper functions to set the conditional flags in the architecture state.
  bool CarryFrom(int32_t left, int32_t right, int32_t carry = 0);
  bool BorrowFrom(int32_t left, int32_t right);
  bool OverflowFrom(int32_t alu_out, int32_t left, int32_t right,
                    bool addition);

  // Helper functions to decode common "addressing" modes
  int32_t GetShiftRm(Instruction* instr, bool* carry_out);
  int32_t GetImm(Instruction* instr, bool* carry_out);
  void ProcessPUW(Instruction* instr, int num_regs, int operand_size,
                  intptr_t* start_address, intptr_t* end_address);
  void HandleRList(Instruction* instr, bool load);
  void HandleVList(Instruction* inst);
  void SoftwareInterrupt(Instruction* instr);

  // Stop helper functions.
  inline bool isStopInstruction(Instruction* instr);
  inline bool isWatchedStop(uint32_t bkpt_code);
  inline bool isEnabledStop(uint32_t bkpt_code);
  inline void EnableStop(uint32_t bkpt_code);
  inline void DisableStop(uint32_t bkpt_code);
  inline void IncreaseStopCounter(uint32_t bkpt_code);
  void PrintStopInfo(uint32_t code);

  // Read and write memory.
  template <typename T>
  inline void Read(uintptr_t address, T* value) {
    base::MutexGuard lock_guard(&GlobalMonitor::Get()->mutex);
    memcpy(value, reinterpret_cast<const char*>(address), sizeof(T));
  }

  template <typename T>
  inline void ReadEx(uintptr_t address, T* value) {
    base::MutexGuard lock_guard(&GlobalMonitor::Get()->mutex);
    GlobalMonitor::Get()->NotifyLoadExcl(
        address, static_cast<TransactionSize>(sizeof(T)),
        isolate_->thread_id());
    memcpy(value, reinterpret_cast<const char*>(address), sizeof(T));
  }

  template <typename T>
  inline void Write(uintptr_t address, T value) {
    base::MutexGuard lock_guard(&GlobalMonitor::Get()->mutex);
    GlobalMonitor::Get()->NotifyStore(address,
                                      static_cast<TransactionSize>(sizeof(T)),
                                      isolate_->thread_id());
    memcpy(reinterpret_cast<char*>(address), &value, sizeof(T));
  }

  template <typename T>
  inline int32_t WriteEx(uintptr_t address, T value) {
    base::MutexGuard lock_guard(&GlobalMonitor::Get()->mutex);
    if (GlobalMonitor::Get()->NotifyStoreExcl(
            address, static_cast<TransactionSize>(sizeof(T)),
            isolate_->thread_id())) {
      memcpy(reinterpret_cast<char*>(address), &value, sizeof(T));
      return 0;
    } else {
      return 1;
    }
  }

#define RW_VAR_LIST(V) \
  V(DWU, uint64_t)     \
  V(DW, int64_t)       \
  V(WU, uint32_t)      \
  V(W, int32_t) V(HU, uint16_t) V(H, int16_t) V(BU, uint8_t) V(B, int8_t)

#define GENERATE_RW_FUNC(size, type)                   \
  inline type Read##size(uintptr_t addr);              \
  inline type ReadEx##size(uintptr_t addr);            \
  inline void Write##size(uintptr_t addr, type value); \
  inline int32_t WriteEx##size(uintptr_t addr, type value);

  RW_VAR_LIST(GENERATE_RW_FUNC)
#undef GENERATE_RW_FUNC

  void Trace(Instruction* instr);
  void SetCR0(intptr_t result, bool setSO = false);
  void ExecuteBranchConditional(Instruction* instr, BCType type);
  void ExecuteGeneric(Instruction* instr);

  void SetFPSCR(int bit) { fp_condition_reg_ |= (1 << (31 - bit)); }
  void ClearFPSCR(int bit) { fp_condition_reg_ &= ~(1 << (31 - bit)); }

  // Executes one instruction.
  void ExecuteInstruction(Instruction* instr);

  // ICache.
  static void CheckICache(base::CustomMatcherHashMap* i_cache,
                          Instruction* instr);
  static void FlushOnePage(base::CustomMatcherHashMap* i_cache, intptr_t start,
                           int size);
  static CachePage* GetCachePage(base::CustomMatcherHashMap* i_cache,
                                 void* page);

  // Handle arguments and return value for runtime FP functions.
  void GetFpArgs(double* x, double* y, intptr_t* z);
  void SetFpResult(const double& result);
  void TrashCallerSaveRegisters();

  void CallInternal(Address entry);

  // Architecture state.
  // Saturating instructions require a Q flag to indicate saturation.
  // There is currently no way to read the CPSR directly, and thus read the Q
  // flag, so this is left unimplemented.
  intptr_t registers_[kNumGPRs];
  int32_t condition_reg_;
  int32_t fp_condition_reg_;
  intptr_t special_reg_lr_;
  intptr_t special_reg_pc_;
  intptr_t special_reg_ctr_;
  int32_t special_reg_xer_;

  int64_t fp_registers_[kNumFPRs];

  // Simulator support.
  char* stack_;
  static const size_t stack_protection_size_ = 256 * kPointerSize;
  bool pc_modified_;
  int icount_;

  // Debugger input.
  char* last_debugger_input_;

  // Registered breakpoints.
  Instruction* break_pc_;
  Instr break_instr_;

  v8::internal::Isolate* isolate_;

  // A stop is watched if its code is less than kNumOfWatchedStops.
  // Only watched stops support enabling/disabling and the counter feature.
  static const uint32_t kNumOfWatchedStops = 256;

  // Breakpoint is disabled if bit 31 is set.
  static const uint32_t kStopDisabledBit = 1 << 31;

  // A stop is enabled, meaning the simulator will stop when meeting the
  // instruction, if bit 31 of watched_stops_[code].count is unset.
  // The value watched_stops_[code].count & ~(1 << 31) indicates how many times
  // the breakpoint was hit or gone through.
  struct StopCountAndDesc {
    uint32_t count;
    char* desc;
  };
  StopCountAndDesc watched_stops_[kNumOfWatchedStops];

  // Synchronization primitives. See ARM DDI 0406C.b, A2.9.
  enum class MonitorAccess {
    Open,
    Exclusive,
  };

  enum class TransactionSize {
    None = 0,
    Byte = 1,
    HalfWord = 2,
    Word = 4,
    DWord = 8,
  };

  class GlobalMonitor {
   public:
    // Exposed so it can be accessed by Simulator::{Read,Write}Ex*.
    base::Mutex mutex;

    void NotifyLoadExcl(uintptr_t addr, TransactionSize size,
                        ThreadId thread_id);
    void NotifyStore(uintptr_t addr, TransactionSize size, ThreadId thread_id);
    bool NotifyStoreExcl(uintptr_t addr, TransactionSize size,
                         ThreadId thread_id);

    static GlobalMonitor* Get();

   private:
    // Private constructor. Call {GlobalMonitor::Get()} to get the singleton.
    GlobalMonitor() = default;
    friend class base::LeakyObject<GlobalMonitor>;

    void Clear();

    MonitorAccess access_state_ = MonitorAccess::Open;
    uintptr_t tagged_addr_ = 0;
    TransactionSize size_ = TransactionSize::None;
    ThreadId thread_id_ = ThreadId::Invalid();
  };
};

}  // namespace internal
}  // namespace v8

#endif  // defined(USE_SIMULATOR)
#endif  // V8_EXECUTION_PPC_SIMULATOR_PPC_H_
