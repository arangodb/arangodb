// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_REGEXP_GEN_H_
#define V8_BUILTINS_BUILTINS_REGEXP_GEN_H_

#include "src/base/optional.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/common/message-template.h"

namespace v8 {
namespace internal {

class RegExpBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit RegExpBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  // Create and initialize a RegExp object.
  TNode<Object> RegExpCreate(TNode<Context> context,
                             TNode<Context> native_context,
                             TNode<Object> regexp_string, TNode<String> flags);

  TNode<Object> RegExpCreate(TNode<Context> context, TNode<Map> initial_map,
                             TNode<Object> regexp_string, TNode<String> flags);

  TNode<Smi> SmiZero();
  TNode<IntPtrT> IntPtrZero();

  TNode<RawPtrT> LoadCodeObjectEntry(TNode<Code> code);

  // Allocate a RegExpResult with the given length (the number of captures,
  // including the match itself), index (the index where the match starts),
  // and input string.
  TNode<JSRegExpResult> AllocateRegExpResult(
      TNode<Context> context, TNode<Smi> length, TNode<Smi> index,
      TNode<String> input, TNode<RegExpMatchInfo> match_info,
      TNode<FixedArray>* elements_out = nullptr);

  TNode<Object> FastLoadLastIndexBeforeSmiCheck(TNode<JSRegExp> regexp);
  TNode<Smi> FastLoadLastIndex(TNode<JSRegExp> regexp) {
    return CAST(FastLoadLastIndexBeforeSmiCheck(regexp));
  }
  TNode<Object> SlowLoadLastIndex(TNode<Context> context, TNode<Object> regexp);
  TNode<Object> LoadLastIndex(TNode<Context> context, TNode<Object> regexp,
                              bool is_fastpath);

  void FastStoreLastIndex(TNode<JSRegExp> regexp, TNode<Smi> value);
  void SlowStoreLastIndex(SloppyTNode<Context> context,
                          SloppyTNode<Object> regexp,
                          SloppyTNode<Object> value);
  void StoreLastIndex(TNode<Context> context, TNode<Object> regexp,
                      TNode<Number> value, bool is_fastpath);

  // Loads {var_string_start} and {var_string_end} with the corresponding
  // offsets into the given {string_data}.
  void GetStringPointers(TNode<RawPtrT> string_data, TNode<IntPtrT> offset,
                         TNode<IntPtrT> last_index,
                         TNode<IntPtrT> string_length,
                         String::Encoding encoding,
                         TVariable<RawPtrT>* var_string_start,
                         TVariable<RawPtrT>* var_string_end);

  // Low level logic around the actual call into pattern matching code.
  TNode<HeapObject> RegExpExecInternal(TNode<Context> context,
                                       TNode<JSRegExp> regexp,
                                       TNode<String> string,
                                       TNode<Number> last_index,
                                       TNode<RegExpMatchInfo> match_info);

  TNode<JSRegExpResult> ConstructNewResultFromMatchInfo(
      TNode<Context> context, TNode<JSReceiver> maybe_regexp,
      TNode<RegExpMatchInfo> match_info, TNode<String> string);

  TNode<RegExpMatchInfo> RegExpPrototypeExecBodyWithoutResult(
      TNode<Context> context, TNode<JSReceiver> maybe_regexp,
      TNode<String> string, Label* if_didnotmatch, const bool is_fastpath);
  TNode<RegExpMatchInfo> RegExpPrototypeExecBodyWithoutResultFast(
      TNode<Context> context, TNode<JSRegExp> maybe_regexp,
      TNode<String> string, Label* if_didnotmatch);

  TNode<HeapObject> RegExpPrototypeExecBody(TNode<Context> context,
                                            TNode<JSReceiver> maybe_regexp,
                                            TNode<String> string,
                                            const bool is_fastpath);

  TNode<BoolT> IsReceiverInitialRegExpPrototype(SloppyTNode<Context> context,
                                                SloppyTNode<Object> receiver);

  // Fast path check logic.
  //
  // Are you afraid? If not, you should be.
  //
  // It's complicated. Fast path checks protect certain assumptions, e.g. that
  // relevant properties on the regexp prototype (such as exec, @@split, global)
  // are unmodified.
  //
  // These assumptions differ by callsite. For example, RegExpPrototypeExec
  // cares whether the exec property has been modified; but it's totally fine
  // to modify other prototype properties. On the other hand,
  // StringPrototypeSplit does care very much whether @@split has been changed.
  //
  // We want to keep regexp execution on the fast path as much as possible.
  // Ideally, we could simply check if the regexp prototype has been modified;
  // yet common web frameworks routinely mutate it for various reasons. But most
  // of these mutations should happen in a way that still allows us to remain
  // on the fast path. To support this, the fast path check logic necessarily
  // becomes more involved.
  //
  // There are multiple knobs to twiddle for regexp fast path checks. We support
  // checks that completely ignore the prototype, checks that verify specific
  // properties on the prototype (the caller must ensure it passes in the right
  // ones), and strict checks that additionally ensure the prototype is
  // unchanged (we use these when we'd have to check multiple properties we
  // don't care too much about, e.g. all individual flag getters).

  using DescriptorIndexNameValue =
      PrototypeCheckAssembler::DescriptorIndexNameValue;

  void BranchIfFastRegExp(
      TNode<Context> context, TNode<HeapObject> object, TNode<Map> map,
      PrototypeCheckAssembler::Flags prototype_check_flags,
      base::Optional<DescriptorIndexNameValue> additional_property_to_check,
      Label* if_isunmodified, Label* if_ismodified);

  // Strict: Does not tolerate any changes to the prototype map.
  // Permissive: Allows changes to the prototype map except for the exec
  //             property.
  void BranchIfFastRegExp_Strict(TNode<Context> context,
                                 TNode<HeapObject> object,
                                 Label* if_isunmodified, Label* if_ismodified);
  void BranchIfFastRegExp_Permissive(TNode<Context> context,
                                     TNode<HeapObject> object,
                                     Label* if_isunmodified,
                                     Label* if_ismodified);

  // Performs fast path checks on the given object itself, but omits prototype
  // checks.
  TNode<BoolT> IsFastRegExpNoPrototype(TNode<Context> context,
                                       TNode<Object> object);
  TNode<BoolT> IsFastRegExpNoPrototype(TNode<Context> context,
                                       TNode<Object> object, TNode<Map> map);

  // For debugging only. Uses a slow GetProperty call to fetch object.exec.
  TNode<BoolT> IsFastRegExpWithOriginalExec(TNode<Context> context,
                                            TNode<JSRegExp> object);

  void BranchIfFastRegExpResult(const TNode<Context> context,
                                const TNode<Object> object,
                                Label* if_isunmodified, Label* if_ismodified);

  TNode<String> FlagsGetter(TNode<Context> context, TNode<Object> regexp,
                            const bool is_fastpath);

  TNode<BoolT> FastFlagGetter(TNode<JSRegExp> regexp, JSRegExp::Flag flag);
  TNode<BoolT> FastFlagGetterGlobal(TNode<JSRegExp> regexp) {
    return FastFlagGetter(regexp, JSRegExp::kGlobal);
  }
  TNode<BoolT> FastFlagGetterUnicode(TNode<JSRegExp> regexp) {
    return FastFlagGetter(regexp, JSRegExp::kUnicode);
  }
  TNode<BoolT> SlowFlagGetter(TNode<Context> context, TNode<Object> regexp,
                              JSRegExp::Flag flag);
  TNode<BoolT> FlagGetter(TNode<Context> context, TNode<Object> regexp,
                          JSRegExp::Flag flag, bool is_fastpath);

  TNode<Object> RegExpInitialize(const TNode<Context> context,
                                 const TNode<JSRegExp> regexp,
                                 const TNode<Object> maybe_pattern,
                                 const TNode<Object> maybe_flags);

  TNode<Number> AdvanceStringIndex(SloppyTNode<String> string,
                                   SloppyTNode<Number> index,
                                   SloppyTNode<BoolT> is_unicode,
                                   bool is_fastpath);

  TNode<Smi> AdvanceStringIndexFast(TNode<String> string, TNode<Smi> index,
                                    TNode<BoolT> is_unicode) {
    return CAST(AdvanceStringIndex(string, index, is_unicode, true));
  }

  TNode<Smi> AdvanceStringIndexSlow(TNode<String> string, TNode<Number> index,
                                    TNode<BoolT> is_unicode) {
    return CAST(AdvanceStringIndex(string, index, is_unicode, false));
  }

  TNode<Object> RegExpPrototypeMatchBody(TNode<Context> context,
                                         TNode<Object> regexp,
                                         TNode<String> const string,
                                         const bool is_fastpath);

  TNode<JSArray> RegExpPrototypeSplitBody(TNode<Context> context,
                                          TNode<JSRegExp> regexp,
                                          TNode<String> const string,
                                          TNode<Smi> const limit);
};

class RegExpMatchAllAssembler : public RegExpBuiltinsAssembler {
 public:
  explicit RegExpMatchAllAssembler(compiler::CodeAssemblerState* state)
      : RegExpBuiltinsAssembler(state) {}

  TNode<Object> CreateRegExpStringIterator(TNode<NativeContext> native_context,
                                           TNode<Object> regexp,
                                           TNode<String> string,
                                           TNode<BoolT> global,
                                           TNode<BoolT> full_unicode);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_REGEXP_GEN_H_
