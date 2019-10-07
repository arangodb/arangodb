// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RELOC_INFO_H_
#define V8_RELOC_INFO_H_

#include "src/globals.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class CodeReference;
class EmbeddedData;

// Specifies whether to perform icache flush operations on RelocInfo updates.
// If FLUSH_ICACHE_IF_NEEDED, the icache will always be flushed if an
// instruction was modified. If SKIP_ICACHE_FLUSH the flush will always be
// skipped (only use this if you will flush the icache manually before it is
// executed).
enum ICacheFlushMode { FLUSH_ICACHE_IF_NEEDED, SKIP_ICACHE_FLUSH };

// -----------------------------------------------------------------------------
// Relocation information

// Relocation information consists of the address (pc) of the datum
// to which the relocation information applies, the relocation mode
// (rmode), and an optional data field. The relocation mode may be
// "descriptive" and not indicate a need for relocation, but simply
// describe a property of the datum. Such rmodes are useful for GC
// and nice disassembly output.

class RelocInfo {
 public:
  // This string is used to add padding comments to the reloc info in cases
  // where we are not sure to have enough space for patching in during
  // lazy deoptimization. This is the case if we have indirect calls for which
  // we do not normally record relocation info.
  static const char* const kFillerCommentString;

  // The minimum size of a comment is equal to two bytes for the extra tagged
  // pc and kPointerSize for the actual pointer to the comment.
  static const int kMinRelocCommentSize = 2 + kPointerSize;

  // The maximum size for a call instruction including pc-jump.
  static const int kMaxCallSize = 6;

  // The maximum pc delta that will use the short encoding.
  static const int kMaxSmallPCDelta;

  enum Mode : int8_t {
    // Please note the order is important (see IsRealRelocMode, IsGCRelocMode,
    // and IsShareableRelocMode predicates below).

    CODE_TARGET,
    RELATIVE_CODE_TARGET,  // LAST_CODE_TARGET_MODE
    EMBEDDED_OBJECT,       // LAST_GCED_ENUM

    JS_TO_WASM_CALL,
    WASM_CALL,  // FIRST_SHAREABLE_RELOC_MODE
    WASM_STUB_CALL,

    RUNTIME_ENTRY,
    COMMENT,

    EXTERNAL_REFERENCE,  // The address of an external C++ function.
    INTERNAL_REFERENCE,  // An address inside the same function.

    // Encoded internal reference, used only on MIPS, MIPS64 and PPC.
    INTERNAL_REFERENCE_ENCODED,

    // An off-heap instruction stream target. See http://goo.gl/Z2HUiM.
    OFF_HEAP_TARGET,

    // Marks constant and veneer pools. Only used on ARM and ARM64.
    // They use a custom noncompact encoding.
    CONST_POOL,
    VENEER_POOL,

    DEOPT_SCRIPT_OFFSET,
    DEOPT_INLINING_ID,  // Deoptimization source position.
    DEOPT_REASON,       // Deoptimization reason index.
    DEOPT_ID,           // Deoptimization inlining id.

    // This is not an actual reloc mode, but used to encode a long pc jump that
    // cannot be encoded as part of another record.
    PC_JUMP,

    // Pseudo-types
    NUMBER_OF_MODES,
    NONE,  // never recorded value

    LAST_CODE_TARGET_MODE = RELATIVE_CODE_TARGET,
    FIRST_REAL_RELOC_MODE = CODE_TARGET,
    LAST_REAL_RELOC_MODE = VENEER_POOL,
    LAST_GCED_ENUM = EMBEDDED_OBJECT,
    FIRST_SHAREABLE_RELOC_MODE = WASM_CALL,
  };

  STATIC_ASSERT(NUMBER_OF_MODES <= kBitsPerInt);

  RelocInfo() = default;

  RelocInfo(Address pc, Mode rmode, intptr_t data, Code* host,
            Address constant_pool = kNullAddress)
      : pc_(pc),
        rmode_(rmode),
        data_(data),
        host_(host),
        constant_pool_(constant_pool) {}

  static constexpr bool IsRealRelocMode(Mode mode) {
    return mode >= FIRST_REAL_RELOC_MODE && mode <= LAST_REAL_RELOC_MODE;
  }
  // Is the relocation mode affected by GC?
  static constexpr bool IsGCRelocMode(Mode mode) {
    return mode <= LAST_GCED_ENUM;
  }
  static constexpr bool IsShareableRelocMode(Mode mode) {
    static_assert(RelocInfo::NONE >= RelocInfo::FIRST_SHAREABLE_RELOC_MODE,
                  "Users of this function rely on NONE being a sharable "
                  "relocation mode.");
    return mode >= RelocInfo::FIRST_SHAREABLE_RELOC_MODE;
  }
  static constexpr bool IsCodeTarget(Mode mode) { return mode == CODE_TARGET; }
  static constexpr bool IsCodeTargetMode(Mode mode) {
    return mode <= LAST_CODE_TARGET_MODE;
  }
  static constexpr bool IsRelativeCodeTarget(Mode mode) {
    return mode == RELATIVE_CODE_TARGET;
  }
  static constexpr bool IsEmbeddedObject(Mode mode) {
    return mode == EMBEDDED_OBJECT;
  }
  static constexpr bool IsRuntimeEntry(Mode mode) {
    return mode == RUNTIME_ENTRY;
  }
  static constexpr bool IsWasmCall(Mode mode) { return mode == WASM_CALL; }
  static constexpr bool IsWasmStubCall(Mode mode) {
    return mode == WASM_STUB_CALL;
  }
  static constexpr bool IsComment(Mode mode) { return mode == COMMENT; }
  static constexpr bool IsConstPool(Mode mode) { return mode == CONST_POOL; }
  static constexpr bool IsVeneerPool(Mode mode) { return mode == VENEER_POOL; }
  static constexpr bool IsDeoptPosition(Mode mode) {
    return mode == DEOPT_SCRIPT_OFFSET || mode == DEOPT_INLINING_ID;
  }
  static constexpr bool IsDeoptReason(Mode mode) {
    return mode == DEOPT_REASON;
  }
  static constexpr bool IsDeoptId(Mode mode) { return mode == DEOPT_ID; }
  static constexpr bool IsExternalReference(Mode mode) {
    return mode == EXTERNAL_REFERENCE;
  }
  static constexpr bool IsInternalReference(Mode mode) {
    return mode == INTERNAL_REFERENCE;
  }
  static constexpr bool IsInternalReferenceEncoded(Mode mode) {
    return mode == INTERNAL_REFERENCE_ENCODED;
  }
  static constexpr bool IsOffHeapTarget(Mode mode) {
    return mode == OFF_HEAP_TARGET;
  }
  static constexpr bool IsNone(Mode mode) { return mode == NONE; }
  static constexpr bool IsWasmReference(Mode mode) {
    return IsWasmPtrReference(mode);
  }
  static constexpr bool IsJsToWasmCall(Mode mode) {
    return mode == JS_TO_WASM_CALL;
  }
  static constexpr bool IsWasmPtrReference(Mode mode) {
    return mode == WASM_CALL || mode == JS_TO_WASM_CALL;
  }

  static bool IsOnlyForSerializer(Mode mode) {
#ifdef V8_TARGET_ARCH_IA32
    // On ia32, inlined off-heap trampolines must be relocated.
    DCHECK_NE((kApplyMask & ModeMask(OFF_HEAP_TARGET)), 0);
    DCHECK_EQ((kApplyMask & ModeMask(EXTERNAL_REFERENCE)), 0);
    return mode == EXTERNAL_REFERENCE;
#else
    DCHECK_EQ((kApplyMask & ModeMask(OFF_HEAP_TARGET)), 0);
    DCHECK_EQ((kApplyMask & ModeMask(EXTERNAL_REFERENCE)), 0);
    return mode == EXTERNAL_REFERENCE || mode == OFF_HEAP_TARGET;
#endif
  }

  static constexpr int ModeMask(Mode mode) { return 1 << mode; }

  // Accessors
  Address pc() const { return pc_; }
  Mode rmode() const { return rmode_; }
  intptr_t data() const { return data_; }
  Code* host() const { return host_; }
  Address constant_pool() const { return constant_pool_; }

  // Apply a relocation by delta bytes. When the code object is moved, PC
  // relative addresses have to be updated as well as absolute addresses
  // inside the code (internal references).
  // Do not forget to flush the icache afterwards!
  V8_INLINE void apply(intptr_t delta);

  // Is the pointer this relocation info refers to coded like a plain pointer
  // or is it strange in some way (e.g. relative or patched into a series of
  // instructions).
  bool IsCodedSpecially();

  // The static pendant to IsCodedSpecially, just for off-heap targets. Used
  // during deserialization, when we don't actually have a RelocInfo handy.
  static bool OffHeapTargetIsCodedSpecially();

  // If true, the pointer this relocation info refers to is an entry in the
  // constant pool, otherwise the pointer is embedded in the instruction stream.
  bool IsInConstantPool();

  // Returns the deoptimization id for the entry associated with the reloc info
  // where {kind} is the deoptimization kind.
  // This is only used for printing RUNTIME_ENTRY relocation info.
  int GetDeoptimizationId(Isolate* isolate, DeoptimizeKind kind);

  Address wasm_call_address() const;
  Address wasm_stub_call_address() const;
  Address js_to_wasm_address() const;

  uint32_t wasm_call_tag() const;

  void set_wasm_call_address(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  void set_wasm_stub_call_address(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  void set_js_to_wasm_address(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  void set_target_address(
      Address target,
      WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // this relocation applies to;
  // can only be called if IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)
  V8_INLINE Address target_address();
  V8_INLINE HeapObject* target_object();
  V8_INLINE Handle<HeapObject> target_object_handle(Assembler* origin);
  V8_INLINE void set_target_object(
      Heap* heap, HeapObject* target,
      WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  V8_INLINE Address target_runtime_entry(Assembler* origin);
  V8_INLINE void set_target_runtime_entry(
      Address target,
      WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  V8_INLINE Address target_off_heap_target();
  V8_INLINE Cell* target_cell();
  V8_INLINE Handle<Cell> target_cell_handle();
  V8_INLINE void set_target_cell(
      Cell* cell, WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  V8_INLINE void set_target_external_reference(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // Returns the address of the constant pool entry where the target address
  // is held.  This should only be called if IsInConstantPool returns true.
  V8_INLINE Address constant_pool_entry_address();

  // Read the address of the word containing the target_address in an
  // instruction stream.  What this means exactly is architecture-independent.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.  Architecture-independent code shouldn't
  // dereference the pointer it gets back from this.
  V8_INLINE Address target_address_address();

  // This indicates how much space a target takes up when deserializing a code
  // stream.  For most architectures this is just the size of a pointer.  For
  // an instruction like movw/movt where the target bits are mixed into the
  // instruction bits the size of the target will be zero, indicating that the
  // serializer should not step forwards in memory after a target is resolved
  // and written.  In this case the target_address_address function above
  // should return the end of the instructions to be patched, allowing the
  // deserializer to deserialize the instructions as raw bytes and put them in
  // place, ready to be patched with the target.
  V8_INLINE int target_address_size();

  // Read the reference in the instruction this relocation
  // applies to; can only be called if rmode_ is EXTERNAL_REFERENCE.
  V8_INLINE Address target_external_reference();

  // Read the reference in the instruction this relocation
  // applies to; can only be called if rmode_ is INTERNAL_REFERENCE.
  V8_INLINE Address target_internal_reference();

  // Return the reference address this relocation applies to;
  // can only be called if rmode_ is INTERNAL_REFERENCE.
  V8_INLINE Address target_internal_reference_address();

  // Wipe out a relocation to a fixed value, used for making snapshots
  // reproducible.
  V8_INLINE void WipeOut();

  template <typename ObjectVisitor>
  inline void Visit(ObjectVisitor* v);

  // Check whether the given code contains relocation information that
  // either is position-relative or movable by the garbage collector.
  static bool RequiresRelocationAfterCodegen(const CodeDesc& desc);
  static bool RequiresRelocation(Code* code);

#ifdef ENABLE_DISASSEMBLER
  // Printing
  static const char* RelocModeName(Mode rmode);
  void Print(Isolate* isolate, std::ostream& os);  // NOLINT
#endif                                             // ENABLE_DISASSEMBLER
#ifdef VERIFY_HEAP
  void Verify(Isolate* isolate);
#endif

  static const int kApplyMask;  // Modes affected by apply.  Depends on arch.

  // In addition to modes covered by the apply mask (which is applied at GC
  // time, among others), this covers all modes that are relocated by
  // Code::CopyFromNoFlush after code generation.
  static int PostCodegenRelocationMask() {
    return ModeMask(RelocInfo::CODE_TARGET) |
           ModeMask(RelocInfo::EMBEDDED_OBJECT) |
           ModeMask(RelocInfo::RUNTIME_ENTRY) |
           ModeMask(RelocInfo::RELATIVE_CODE_TARGET) | kApplyMask;
  }

 private:
  // On ARM/ARM64, note that pc_ is the address of the instruction referencing
  // the constant pool and not the address of the constant pool entry.
  Address pc_;
  Mode rmode_;
  intptr_t data_ = 0;
  Code* host_;
  Address constant_pool_ = kNullAddress;
  friend class RelocIterator;
};

// RelocInfoWriter serializes a stream of relocation info. It writes towards
// lower addresses.
class RelocInfoWriter {
 public:
  RelocInfoWriter() : pos_(nullptr), last_pc_(nullptr) {}

  byte* pos() const { return pos_; }
  byte* last_pc() const { return last_pc_; }

  void Write(const RelocInfo* rinfo);

  // Update the state of the stream after reloc info buffer
  // and/or code is moved while the stream is active.
  void Reposition(byte* pos, byte* pc) {
    pos_ = pos;
    last_pc_ = pc;
  }

  // Max size (bytes) of a written RelocInfo. Longest encoding is
  // ExtraTag, VariableLengthPCJump, ExtraTag, pc_delta, data_delta.
  static constexpr int kMaxSize = 1 + 4 + 1 + 1 + kPointerSize;

 private:
  inline uint32_t WriteLongPCJump(uint32_t pc_delta);

  inline void WriteShortTaggedPC(uint32_t pc_delta, int tag);
  inline void WriteShortData(intptr_t data_delta);

  inline void WriteMode(RelocInfo::Mode rmode);
  inline void WriteModeAndPC(uint32_t pc_delta, RelocInfo::Mode rmode);
  inline void WriteIntData(int data_delta);
  inline void WriteData(intptr_t data_delta);

  byte* pos_;
  byte* last_pc_;

  DISALLOW_COPY_AND_ASSIGN(RelocInfoWriter);
};

// A RelocIterator iterates over relocation information.
// Typical use:
//
//   for (RelocIterator it(code); !it.done(); it.next()) {
//     // do something with it.rinfo() here
//   }
//
// A mask can be specified to skip unwanted modes.
class RelocIterator : public Malloced {
 public:
  // Create a new iterator positioned at
  // the beginning of the reloc info.
  // Relocation information with mode k is included in the
  // iteration iff bit k of mode_mask is set.
  explicit RelocIterator(Code* code, int mode_mask = -1);
  explicit RelocIterator(EmbeddedData* embedded_data, Code* code,
                         int mode_mask);
  explicit RelocIterator(const CodeDesc& desc, int mode_mask = -1);
  explicit RelocIterator(const CodeReference code_reference,
                         int mode_mask = -1);
  explicit RelocIterator(Vector<byte> instructions,
                         Vector<const byte> reloc_info, Address const_pool,
                         int mode_mask = -1);
  RelocIterator(RelocIterator&&) = default;

  // Iteration
  bool done() const { return done_; }
  void next();

  // Return pointer valid until next next().
  RelocInfo* rinfo() {
    DCHECK(!done());
    return &rinfo_;
  }

 private:
  RelocIterator(Code* host, Address pc, Address constant_pool, const byte* pos,
                const byte* end, int mode_mask);

  // Advance* moves the position before/after reading.
  // *Read* reads from current byte(s) into rinfo_.
  // *Get* just reads and returns info on current byte.
  void Advance(int bytes = 1) { pos_ -= bytes; }
  int AdvanceGetTag();
  RelocInfo::Mode GetMode();

  void AdvanceReadLongPCJump();

  void ReadShortTaggedPC();
  void ReadShortData();

  void AdvanceReadPC();
  void AdvanceReadInt();
  void AdvanceReadData();

  // If the given mode is wanted, set it in rinfo_ and return true.
  // Else return false. Used for efficiently skipping unwanted modes.
  bool SetMode(RelocInfo::Mode mode) {
    return (mode_mask_ & (1 << mode)) ? (rinfo_.rmode_ = mode, true) : false;
  }

  const byte* pos_;
  const byte* end_;
  RelocInfo rinfo_;
  bool done_ = false;
  const int mode_mask_;

  DISALLOW_COPY_AND_ASSIGN(RelocIterator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_RELOC_INFO_H_
