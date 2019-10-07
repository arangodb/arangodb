// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/disassembler.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "src/assembler-inl.h"
#include "src/code-reference.h"
#include "src/code-stubs.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/disasm.h"
#include "src/ic/ic.h"
#include "src/instruction-stream.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/snapshot/serializer-common.h"
#include "src/string-stream.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_DISASSEMBLER

class V8NameConverter: public disasm::NameConverter {
 public:
  explicit V8NameConverter(Isolate* isolate, CodeReference code = {})
      : isolate_(isolate), code_(code) {}
  const char* NameOfAddress(byte* pc) const override;
  const char* NameInCode(byte* addr) const override;
  const char* RootRelativeName(int offset) const override;

  const CodeReference& code() const { return code_; }

 private:
  void InitExternalRefsCache() const;

  Isolate* isolate_;
  CodeReference code_;

  EmbeddedVector<char, 128> v8_buffer_;

  // Map from root-register relative offset of the external reference value to
  // the external reference name (stored in the external reference table).
  // This cache is used to recognize [root_reg + offs] patterns as direct
  // access to certain external reference's value.
  mutable std::unordered_map<int, const char*> directly_accessed_external_refs_;
};

void V8NameConverter::InitExternalRefsCache() const {
  ExternalReferenceTable* external_reference_table =
      isolate_->heap()->external_reference_table();
  if (!external_reference_table->is_initialized()) return;

  base::AddressRegion addressable_region =
      isolate_->root_register_addressable_region();
  Address roots_start =
      reinterpret_cast<Address>(isolate_->heap()->roots_array_start());

  for (uint32_t i = 0; i < external_reference_table->size(); i++) {
    Address address = external_reference_table->address(i);
    if (addressable_region.contains(address)) {
      int offset = static_cast<int>(address - roots_start);
      const char* name = external_reference_table->name(i);
      directly_accessed_external_refs_.insert({offset, name});
    }
  }
}

const char* V8NameConverter::NameOfAddress(byte* pc) const {
  if (!code_.is_null()) {
    const char* name =
        isolate_ ? isolate_->builtins()->Lookup(reinterpret_cast<Address>(pc))
                 : nullptr;

    if (name != nullptr) {
      SNPrintF(v8_buffer_, "%p  (%s)", static_cast<void*>(pc), name);
      return v8_buffer_.start();
    }

    int offs = static_cast<int>(reinterpret_cast<Address>(pc) -
                                code_.instruction_start());
    // print as code offset, if it seems reasonable
    if (0 <= offs && offs < code_.instruction_size()) {
      SNPrintF(v8_buffer_, "%p  <+0x%x>", static_cast<void*>(pc), offs);
      return v8_buffer_.start();
    }

    wasm::WasmCode* wasm_code =
        isolate_ ? isolate_->wasm_engine()->code_manager()->LookupCode(
                       reinterpret_cast<Address>(pc))
                 : nullptr;
    if (wasm_code != nullptr) {
      SNPrintF(v8_buffer_, "%p  (%s)", static_cast<void*>(pc),
               wasm::GetWasmCodeKindAsString(wasm_code->kind()));
      return v8_buffer_.start();
    }
  }

  return disasm::NameConverter::NameOfAddress(pc);
}


const char* V8NameConverter::NameInCode(byte* addr) const {
  // The V8NameConverter is used for well known code, so we can "safely"
  // dereference pointers in generated code.
  return code_.is_null() ? "" : reinterpret_cast<const char*>(addr);
}

const char* V8NameConverter::RootRelativeName(int offset) const {
  if (isolate_ == nullptr) return nullptr;

  const int kRootsStart = 0;
  const int kRootsEnd = Heap::roots_to_external_reference_table_offset();
  const int kExtRefsStart = kRootsEnd;
  const int kExtRefsEnd = Heap::roots_to_builtins_offset();
  const int kBuiltinsStart = kExtRefsEnd;
  const int kBuiltinsEnd =
      kBuiltinsStart + Builtins::builtin_count * kPointerSize;

  if (kRootsStart <= offset && offset < kRootsEnd) {
    uint32_t offset_in_roots_table = offset - kRootsStart;

    // Fail safe in the unlikely case of an arbitrary root-relative offset.
    if (offset_in_roots_table % kPointerSize != 0) return nullptr;

    RootIndex root_index =
        static_cast<RootIndex>(offset_in_roots_table / kPointerSize);

    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    isolate_->heap()->root(root_index)->ShortPrint(&accumulator);
    std::unique_ptr<char[]> obj_name = accumulator.ToCString();

    SNPrintF(v8_buffer_, "root (%s)", obj_name.get());
    return v8_buffer_.start();

  } else if (kExtRefsStart <= offset && offset < kExtRefsEnd) {
    uint32_t offset_in_extref_table = offset - kExtRefsStart;

    // Fail safe in the unlikely case of an arbitrary root-relative offset.
    if (offset_in_extref_table % ExternalReferenceTable::EntrySize() != 0) {
      return nullptr;
    }

    // Likewise if the external reference table is uninitialized.
    if (!isolate_->heap()->external_reference_table()->is_initialized()) {
      return nullptr;
    }

    SNPrintF(v8_buffer_, "external reference (%s)",
             isolate_->heap()->external_reference_table()->NameFromOffset(
                 offset_in_extref_table));
    return v8_buffer_.start();

  } else if (kBuiltinsStart <= offset && offset < kBuiltinsEnd) {
    uint32_t offset_in_builtins_table = (offset - kBuiltinsStart);

    Builtins::Name builtin_id =
        static_cast<Builtins::Name>(offset_in_builtins_table / kPointerSize);

    const char* name = Builtins::name(builtin_id);
    SNPrintF(v8_buffer_, "builtin (%s)", name);
    return v8_buffer_.start();

  } else {
    // It must be a direct access to one of the external values.
    if (directly_accessed_external_refs_.empty()) {
      InitExternalRefsCache();
    }

    auto iter = directly_accessed_external_refs_.find(offset);
    if (iter != directly_accessed_external_refs_.end()) {
      SNPrintF(v8_buffer_, "external value (%s)", iter->second);
      return v8_buffer_.start();
    }
    return "WAAT??? What are we accessing here???";
  }
}

static void DumpBuffer(std::ostream* os, StringBuilder* out) {
  (*os) << out->Finalize() << std::endl;
  out->Reset();
}


static const int kOutBufferSize = 2048 + String::kMaxShortPrintLength;
static const int kRelocInfoPosition = 57;

static void PrintRelocInfo(StringBuilder* out, Isolate* isolate,
                           const ExternalReferenceEncoder* ref_encoder,
                           std::ostream* os, CodeReference host,
                           RelocInfo* relocinfo, bool first_reloc_info = true) {
  // Indent the printing of the reloc info.
  if (first_reloc_info) {
    // The first reloc info is printed after the disassembled instruction.
    out->AddPadding(' ', kRelocInfoPosition - out->position());
  } else {
    // Additional reloc infos are printed on separate lines.
    DumpBuffer(os, out);
    out->AddPadding(' ', kRelocInfoPosition);
  }

  RelocInfo::Mode rmode = relocinfo->rmode();
  if (rmode == RelocInfo::DEOPT_SCRIPT_OFFSET) {
    out->AddFormatted("    ;; debug: deopt position, script offset '%d'",
                      static_cast<int>(relocinfo->data()));
  } else if (rmode == RelocInfo::DEOPT_INLINING_ID) {
    out->AddFormatted("    ;; debug: deopt position, inlining id '%d'",
                      static_cast<int>(relocinfo->data()));
  } else if (rmode == RelocInfo::DEOPT_REASON) {
    DeoptimizeReason reason = static_cast<DeoptimizeReason>(relocinfo->data());
    out->AddFormatted("    ;; debug: deopt reason '%s'",
                      DeoptimizeReasonToString(reason));
  } else if (rmode == RelocInfo::DEOPT_ID) {
    out->AddFormatted("    ;; debug: deopt index %d",
                      static_cast<int>(relocinfo->data()));
  } else if (rmode == RelocInfo::EMBEDDED_OBJECT) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    relocinfo->target_object()->ShortPrint(&accumulator);
    std::unique_ptr<char[]> obj_name = accumulator.ToCString();
    out->AddFormatted("    ;; object: %s", obj_name.get());
  } else if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
    const char* reference_name =
        ref_encoder ? ref_encoder->NameOfAddress(
                          isolate, relocinfo->target_external_reference())
                    : "unknown";
    out->AddFormatted("    ;; external reference (%s)", reference_name);
  } else if (RelocInfo::IsCodeTargetMode(rmode)) {
    out->AddFormatted("    ;; code:");
    Code* code = isolate->heap()->GcSafeFindCodeForInnerPointer(
        relocinfo->target_address());
    Code::Kind kind = code->kind();
    if (kind == Code::STUB) {
      // Get the STUB key and extract major and minor key.
      uint32_t key = code->stub_key();
      uint32_t minor_key = CodeStub::MinorKeyFromKey(key);
      CodeStub::Major major_key = CodeStub::GetMajorKey(code);
      DCHECK(major_key == CodeStub::MajorKeyFromKey(key));
      out->AddFormatted(" %s, %s, ", Code::Kind2String(kind),
                        CodeStub::MajorName(major_key));
      out->AddFormatted("minor: %d", minor_key);
    } else if (code->is_builtin()) {
      out->AddFormatted(" Builtin::%s", Builtins::name(code->builtin_index()));
    } else {
      out->AddFormatted(" %s", Code::Kind2String(kind));
    }
  } else if (RelocInfo::IsWasmStubCall(rmode) && !isolate) {
    // Host is isolate-independent, try wasm native module instead.
    wasm::WasmCode* code = host.as_wasm_code()->native_module()->Lookup(
        relocinfo->wasm_stub_call_address());
    out->AddFormatted("    ;; wasm stub: %s", code->GetRuntimeStubName());
  } else if (RelocInfo::IsRuntimeEntry(rmode) && isolate &&
             isolate->deoptimizer_data() != nullptr) {
    // A runtime entry relocinfo might be a deoptimization bailout.
    Address addr = relocinfo->target_address();
    DeoptimizeKind type;
    if (Deoptimizer::IsDeoptimizationEntry(isolate, addr, &type)) {
      int id = relocinfo->GetDeoptimizationId(isolate, type);
      out->AddFormatted("    ;; %s deoptimization bailout %d",
                        Deoptimizer::MessageFor(type), id);
    } else {
      out->AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
    }
  } else {
    out->AddFormatted("    ;; %s", RelocInfo::RelocModeName(rmode));
  }
}

static int DecodeIt(Isolate* isolate, ExternalReferenceEncoder* ref_encoder,
                    std::ostream* os, CodeReference code,
                    const V8NameConverter& converter, byte* begin, byte* end,
                    Address current_pc) {
  v8::internal::EmbeddedVector<char, 128> decode_buffer;
  v8::internal::EmbeddedVector<char, kOutBufferSize> out_buffer;
  StringBuilder out(out_buffer.start(), out_buffer.length());
  byte* pc = begin;
  disasm::Disassembler d(converter,
                         disasm::Disassembler::kContinueOnUnimplementedOpcode);
  RelocIterator* it = nullptr;
  if (!code.is_null()) {
    it = new RelocIterator(code);
  } else {
    // No relocation information when printing code stubs.
  }
  int constants = -1;  // no constants being decoded at the start

  while (pc < end) {
    // First decode instruction so that we know its length.
    byte* prev_pc = pc;
    if (constants > 0) {
      SNPrintF(decode_buffer,
               "%08x       constant",
               *reinterpret_cast<int32_t*>(pc));
      constants--;
      pc += 4;
    } else {
      int num_const = d.ConstantPoolSizeAt(pc);
      if (num_const >= 0) {
        SNPrintF(decode_buffer,
                 "%08x       constant pool begin (num_const = %d)",
                 *reinterpret_cast<int32_t*>(pc), num_const);
        constants = num_const;
        pc += 4;
      } else if (it != nullptr && !it->done() &&
                 it->rinfo()->pc() == reinterpret_cast<Address>(pc) &&
                 it->rinfo()->rmode() == RelocInfo::INTERNAL_REFERENCE) {
        // raw pointer embedded in code stream, e.g., jump table
        byte* ptr = *reinterpret_cast<byte**>(pc);
        SNPrintF(
            decode_buffer, "%08" V8PRIxPTR "      jump table entry %4" PRIuS,
            reinterpret_cast<intptr_t>(ptr), static_cast<size_t>(ptr - begin));
        pc += sizeof(ptr);
      } else {
        decode_buffer[0] = '\0';
        pc += d.InstructionDecode(decode_buffer, pc);
      }
    }

    // Collect RelocInfo for this instruction (prev_pc .. pc-1)
    std::vector<const char*> comments;
    std::vector<Address> pcs;
    std::vector<RelocInfo::Mode> rmodes;
    std::vector<intptr_t> datas;
    if (it != nullptr) {
      while (!it->done() && it->rinfo()->pc() < reinterpret_cast<Address>(pc)) {
        if (RelocInfo::IsComment(it->rinfo()->rmode())) {
          // For comments just collect the text.
          comments.push_back(
              reinterpret_cast<const char*>(it->rinfo()->data()));
        } else {
          // For other reloc info collect all data.
          pcs.push_back(it->rinfo()->pc());
          rmodes.push_back(it->rinfo()->rmode());
          datas.push_back(it->rinfo()->data());
        }
        it->next();
      }
    }

    // Comments.
    for (size_t i = 0; i < comments.size(); i++) {
      out.AddFormatted("                  %s", comments[i]);
      DumpBuffer(os, &out);
    }

    // Instruction address and instruction offset.
    if (FLAG_log_colour && reinterpret_cast<Address>(prev_pc) == current_pc) {
      // If this is the given "current" pc, make it yellow and bold.
      out.AddFormatted("\033[33;1m");
    }
    out.AddFormatted("%p  %4" V8PRIxPTRDIFF "  ", static_cast<void*>(prev_pc),
                     prev_pc - begin);

    // Instruction.
    out.AddFormatted("%s", decode_buffer.start());

    // Print all the reloc info for this instruction which are not comments.
    for (size_t i = 0; i < pcs.size(); i++) {
      // Put together the reloc info
      const CodeReference& host = code;
      Address constant_pool =
          host.is_null() ? kNullAddress : host.constant_pool();
      RelocInfo relocinfo(pcs[i], rmodes[i], datas[i], nullptr, constant_pool);

      bool first_reloc_info = (i == 0);
      PrintRelocInfo(&out, isolate, ref_encoder, os, code, &relocinfo,
                     first_reloc_info);
    }

    // If this is a constant pool load and we haven't found any RelocInfo
    // already, check if we can find some RelocInfo for the target address in
    // the constant pool.
    if (pcs.empty() && !code.is_null()) {
      RelocInfo dummy_rinfo(reinterpret_cast<Address>(prev_pc), RelocInfo::NONE,
                            0, nullptr);
      if (dummy_rinfo.IsInConstantPool()) {
        Address constant_pool_entry_address =
            dummy_rinfo.constant_pool_entry_address();
        RelocIterator reloc_it(code);
        while (!reloc_it.done()) {
          if (reloc_it.rinfo()->IsInConstantPool() &&
              (reloc_it.rinfo()->constant_pool_entry_address() ==
               constant_pool_entry_address)) {
            PrintRelocInfo(&out, isolate, ref_encoder, os, code,
                           reloc_it.rinfo());
            break;
          }
          reloc_it.next();
        }
      }
    }

    if (FLAG_log_colour && reinterpret_cast<Address>(prev_pc) == current_pc) {
      out.AddFormatted("\033[m");
    }

    DumpBuffer(os, &out);
  }

  // Emit comments following the last instruction (if any).
  if (it != nullptr) {
    for ( ; !it->done(); it->next()) {
      if (RelocInfo::IsComment(it->rinfo()->rmode())) {
        out.AddFormatted("                  %s",
                         reinterpret_cast<const char*>(it->rinfo()->data()));
        DumpBuffer(os, &out);
      }
    }
  }

  delete it;
  return static_cast<int>(pc - begin);
}

int Disassembler::Decode(Isolate* isolate, std::ostream* os, byte* begin,
                         byte* end, CodeReference code, Address current_pc) {
  V8NameConverter v8NameConverter(isolate, code);
  bool decode_off_heap = isolate && InstructionStream::PcIsOffHeap(
                                        isolate, bit_cast<Address>(begin));
  CodeReference code_ref = decode_off_heap ? CodeReference() : code;
  if (isolate) {
    // We have an isolate, so support external reference names.
    SealHandleScope shs(isolate);
    DisallowHeapAllocation no_alloc;
    ExternalReferenceEncoder ref_encoder(isolate);
    return DecodeIt(isolate, &ref_encoder, os, code_ref, v8NameConverter, begin,
                    end, current_pc);
  } else {
    // No isolate => isolate-independent code. No external reference names.
    return DecodeIt(nullptr, nullptr, os, code_ref, v8NameConverter, begin, end,
                    current_pc);
  }
}

#else  // ENABLE_DISASSEMBLER

int Disassembler::Decode(Isolate* isolate, std::ostream* os, byte* begin,
                         byte* end, CodeReference code, Address current_pc) {
  return 0;
}

#endif  // ENABLE_DISASSEMBLER

}  // namespace internal
}  // namespace v8
