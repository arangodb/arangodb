// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_JSREGEXP_H_
#define V8_REGEXP_JSREGEXP_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/isolate.h"
#include "src/objects/js-regexp.h"
#include "src/regexp/regexp-ast.h"
#include "src/regexp/regexp-macro-assembler.h"

namespace v8 {
namespace internal {

class NodeVisitor;
class RegExpCompiler;
class RegExpMacroAssembler;
class RegExpNode;
class RegExpTree;
class BoyerMooreLookahead;

inline bool IgnoreCase(JSRegExp::Flags flags) {
  return (flags & JSRegExp::kIgnoreCase) != 0;
}

inline bool IsUnicode(JSRegExp::Flags flags) {
  return (flags & JSRegExp::kUnicode) != 0;
}

inline bool IsSticky(JSRegExp::Flags flags) {
  return (flags & JSRegExp::kSticky) != 0;
}

inline bool IsGlobal(JSRegExp::Flags flags) {
  return (flags & JSRegExp::kGlobal) != 0;
}

inline bool DotAll(JSRegExp::Flags flags) {
  return (flags & JSRegExp::kDotAll) != 0;
}

inline bool Multiline(JSRegExp::Flags flags) {
  return (flags & JSRegExp::kMultiline) != 0;
}

inline bool NeedsUnicodeCaseEquivalents(JSRegExp::Flags flags) {
  // Both unicode and ignore_case flags are set. We need to use ICU to find
  // the closure over case equivalents.
  return IsUnicode(flags) && IgnoreCase(flags);
}

class RegExpImpl {
 public:
  // Whether V8 is compiled with native regexp support or not.
  static bool UsesNativeRegExp() {
#ifdef V8_INTERPRETED_REGEXP
    return false;
#else
    return true;
#endif
  }

  // Returns a string representation of a regular expression.
  // Implements RegExp.prototype.toString, see ECMA-262 section 15.10.6.4.
  // This function calls the garbage collector if necessary.
  static Handle<String> ToString(Handle<Object> value);

  // Parses the RegExp pattern and prepares the JSRegExp object with
  // generic data and choice of implementation - as well as what
  // the implementation wants to store in the data field.
  // Returns false if compilation fails.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Compile(
      Isolate* isolate, Handle<JSRegExp> re, Handle<String> pattern,
      JSRegExp::Flags flags);

  // See ECMA-262 section 15.10.6.2.
  // This function calls the garbage collector if necessary.
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Exec(
      Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
      int index, Handle<RegExpMatchInfo> last_match_info);

  // Prepares a JSRegExp object with Irregexp-specific data.
  static void IrregexpInitialize(Isolate* isolate, Handle<JSRegExp> re,
                                 Handle<String> pattern, JSRegExp::Flags flags,
                                 int capture_register_count);

  static void AtomCompile(Isolate* isolate, Handle<JSRegExp> re,
                          Handle<String> pattern, JSRegExp::Flags flags,
                          Handle<String> match_pattern);

  static int AtomExecRaw(Isolate* isolate, Handle<JSRegExp> regexp,
                         Handle<String> subject, int index, int32_t* output,
                         int output_size);

  static Handle<Object> AtomExec(Isolate* isolate, Handle<JSRegExp> regexp,
                                 Handle<String> subject, int index,
                                 Handle<RegExpMatchInfo> last_match_info);

  enum IrregexpResult { RE_FAILURE = 0, RE_SUCCESS = 1, RE_EXCEPTION = -1 };

  // Prepare a RegExp for being executed one or more times (using
  // IrregexpExecOnce) on the subject.
  // This ensures that the regexp is compiled for the subject, and that
  // the subject is flat.
  // Returns the number of integer spaces required by IrregexpExecOnce
  // as its "registers" argument.  If the regexp cannot be compiled,
  // an exception is set as pending, and this function returns negative.
  static int IrregexpPrepare(Isolate* isolate, Handle<JSRegExp> regexp,
                             Handle<String> subject);

  // Execute a regular expression on the subject, starting from index.
  // If matching succeeds, return the number of matches.  This can be larger
  // than one in the case of global regular expressions.
  // The captures and subcaptures are stored into the registers vector.
  // If matching fails, returns RE_FAILURE.
  // If execution fails, sets a pending exception and returns RE_EXCEPTION.
  static int IrregexpExecRaw(Isolate* isolate, Handle<JSRegExp> regexp,
                             Handle<String> subject, int index, int32_t* output,
                             int output_size);

  // Execute an Irregexp bytecode pattern.
  // On a successful match, the result is a JSArray containing
  // captured positions.  On a failure, the result is the null value.
  // Returns an empty handle in case of an exception.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> IrregexpExec(
      Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
      int index, Handle<RegExpMatchInfo> last_match_info);

  // Set last match info.  If match is nullptr, then setting captures is
  // omitted.
  static Handle<RegExpMatchInfo> SetLastMatchInfo(
      Isolate* isolate, Handle<RegExpMatchInfo> last_match_info,
      Handle<String> subject, int capture_count, int32_t* match);

  class GlobalCache {
   public:
    GlobalCache(Handle<JSRegExp> regexp,
                Handle<String> subject,
                Isolate* isolate);

    V8_INLINE ~GlobalCache();

    // Fetch the next entry in the cache for global regexp match results.
    // This does not set the last match info.  Upon failure, nullptr is
    // returned. The cause can be checked with Result().  The previous result is
    // still in available in memory when a failure happens.
    V8_INLINE int32_t* FetchNext();

    V8_INLINE int32_t* LastSuccessfulMatch();

    V8_INLINE bool HasException() { return num_matches_ < 0; }

   private:
    int AdvanceZeroLength(int last_index);

    int num_matches_;
    int max_matches_;
    int current_match_index_;
    int registers_per_match_;
    // Pointer to the last set of captures.
    int32_t* register_array_;
    int register_array_size_;
    Handle<JSRegExp> regexp_;
    Handle<String> subject_;
    Isolate* isolate_;
  };

  // For acting on the JSRegExp data FixedArray.
  static int IrregexpMaxRegisterCount(FixedArray* re);
  static void SetIrregexpMaxRegisterCount(FixedArray* re, int value);
  static void SetIrregexpCaptureNameMap(FixedArray* re,
                                        Handle<FixedArray> value);
  static int IrregexpNumberOfCaptures(FixedArray* re);
  static int IrregexpNumberOfRegisters(FixedArray* re);
  static ByteArray* IrregexpByteCode(FixedArray* re, bool is_one_byte);
  static Code* IrregexpNativeCode(FixedArray* re, bool is_one_byte);

  // Limit the space regexps take up on the heap.  In order to limit this we
  // would like to keep track of the amount of regexp code on the heap.  This
  // is not tracked, however.  As a conservative approximation we track the
  // total regexp code compiled including code that has subsequently been freed
  // and the total executable memory at any point.
  static const size_t kRegExpExecutableMemoryLimit = 16 * MB;
  static const size_t kRegExpCompiledLimit = 1 * MB;
  static const int kRegExpTooLargeToOptimize = 20 * KB;

 private:
  static bool CompileIrregexp(Isolate* isolate, Handle<JSRegExp> re,
                              Handle<String> sample_subject, bool is_one_byte);
  static inline bool EnsureCompiledIrregexp(Isolate* isolate,
                                            Handle<JSRegExp> re,
                                            Handle<String> sample_subject,
                                            bool is_one_byte);
};


// Represents the location of one element relative to the intersection of
// two sets. Corresponds to the four areas of a Venn diagram.
enum ElementInSetsRelation {
  kInsideNone = 0,
  kInsideFirst = 1,
  kInsideSecond = 2,
  kInsideBoth = 3
};


// A set of unsigned integers that behaves especially well on small
// integers (< 32).  May do zone-allocation.
class OutSet: public ZoneObject {
 public:
  OutSet() : first_(0), remaining_(nullptr), successors_(nullptr) {}
  OutSet* Extend(unsigned value, Zone* zone);
  bool Get(unsigned value) const;
  static const unsigned kFirstLimit = 32;

 private:
  // Destructively set a value in this set.  In most cases you want
  // to use Extend instead to ensure that only one instance exists
  // that contains the same values.
  void Set(unsigned value, Zone* zone);

  // The successors are a list of sets that contain the same values
  // as this set and the one more value that is not present in this
  // set.
  ZoneList<OutSet*>* successors(Zone* zone) { return successors_; }

  OutSet(uint32_t first, ZoneList<unsigned>* remaining)
      : first_(first), remaining_(remaining), successors_(nullptr) {}
  uint32_t first_;
  ZoneList<unsigned>* remaining_;
  ZoneList<OutSet*>* successors_;
  friend class Trace;
};


// A mapping from integers, specified as ranges, to a set of integers.
// Used for mapping character ranges to choices.
class DispatchTable : public ZoneObject {
 public:
  explicit DispatchTable(Zone* zone) : tree_(zone) { }

  class Entry {
   public:
    Entry() : from_(0), to_(0), out_set_(nullptr) {}
    Entry(uc32 from, uc32 to, OutSet* out_set)
        : from_(from), to_(to), out_set_(out_set) {
      DCHECK(from <= to);
    }
    uc32 from() { return from_; }
    uc32 to() { return to_; }
    void set_to(uc32 value) { to_ = value; }
    void AddValue(int value, Zone* zone) {
      out_set_ = out_set_->Extend(value, zone);
    }
    OutSet* out_set() { return out_set_; }
   private:
    uc32 from_;
    uc32 to_;
    OutSet* out_set_;
  };

  class Config {
   public:
    typedef uc32 Key;
    typedef Entry Value;
    static const uc32 kNoKey;
    static const Entry NoValue() { return Value(); }
    static inline int Compare(uc32 a, uc32 b) {
      if (a == b)
        return 0;
      else if (a < b)
        return -1;
      else
        return 1;
    }
  };

  void AddRange(CharacterRange range, int value, Zone* zone);
  OutSet* Get(uc32 value);
  void Dump();

  template <typename Callback>
  void ForEach(Callback* callback) {
    return tree()->ForEach(callback);
  }

 private:
  // There can't be a static empty set since it allocates its
  // successors in a zone and caches them.
  OutSet* empty() { return &empty_; }
  OutSet empty_;
  ZoneSplayTree<Config>* tree() { return &tree_; }
  ZoneSplayTree<Config> tree_;
};


// Categorizes character ranges into BMP, non-BMP, lead, and trail surrogates.
class UnicodeRangeSplitter {
 public:
  UnicodeRangeSplitter(Zone* zone, ZoneList<CharacterRange>* base);
  void Call(uc32 from, DispatchTable::Entry entry);

  ZoneList<CharacterRange>* bmp() { return bmp_; }
  ZoneList<CharacterRange>* lead_surrogates() { return lead_surrogates_; }
  ZoneList<CharacterRange>* trail_surrogates() { return trail_surrogates_; }
  ZoneList<CharacterRange>* non_bmp() const { return non_bmp_; }

 private:
  static const int kBase = 0;
  // Separate ranges into
  static const int kBmpCodePoints = 1;
  static const int kLeadSurrogates = 2;
  static const int kTrailSurrogates = 3;
  static const int kNonBmpCodePoints = 4;

  Zone* zone_;
  DispatchTable table_;
  ZoneList<CharacterRange>* bmp_;
  ZoneList<CharacterRange>* lead_surrogates_;
  ZoneList<CharacterRange>* trail_surrogates_;
  ZoneList<CharacterRange>* non_bmp_;
};


#define FOR_EACH_NODE_TYPE(VISIT)                                    \
  VISIT(End)                                                         \
  VISIT(Action)                                                      \
  VISIT(Choice)                                                      \
  VISIT(BackReference)                                               \
  VISIT(Assertion)                                                   \
  VISIT(Text)


class Trace;
struct PreloadState;
class GreedyLoopState;
class AlternativeGenerationList;

struct NodeInfo {
  NodeInfo()
      : being_analyzed(false),
        been_analyzed(false),
        follows_word_interest(false),
        follows_newline_interest(false),
        follows_start_interest(false),
        at_end(false),
        visited(false),
        replacement_calculated(false) { }

  // Returns true if the interests and assumptions of this node
  // matches the given one.
  bool Matches(NodeInfo* that) {
    return (at_end == that->at_end) &&
           (follows_word_interest == that->follows_word_interest) &&
           (follows_newline_interest == that->follows_newline_interest) &&
           (follows_start_interest == that->follows_start_interest);
  }

  // Updates the interests of this node given the interests of the
  // node preceding it.
  void AddFromPreceding(NodeInfo* that) {
    at_end |= that->at_end;
    follows_word_interest |= that->follows_word_interest;
    follows_newline_interest |= that->follows_newline_interest;
    follows_start_interest |= that->follows_start_interest;
  }

  bool HasLookbehind() {
    return follows_word_interest ||
           follows_newline_interest ||
           follows_start_interest;
  }

  // Sets the interests of this node to include the interests of the
  // following node.
  void AddFromFollowing(NodeInfo* that) {
    follows_word_interest |= that->follows_word_interest;
    follows_newline_interest |= that->follows_newline_interest;
    follows_start_interest |= that->follows_start_interest;
  }

  void ResetCompilationState() {
    being_analyzed = false;
    been_analyzed = false;
  }

  bool being_analyzed: 1;
  bool been_analyzed: 1;

  // These bits are set of this node has to know what the preceding
  // character was.
  bool follows_word_interest: 1;
  bool follows_newline_interest: 1;
  bool follows_start_interest: 1;

  bool at_end: 1;
  bool visited: 1;
  bool replacement_calculated: 1;
};


// Details of a quick mask-compare check that can look ahead in the
// input stream.
class QuickCheckDetails {
 public:
  QuickCheckDetails()
      : characters_(0),
        mask_(0),
        value_(0),
        cannot_match_(false) { }
  explicit QuickCheckDetails(int characters)
      : characters_(characters),
        mask_(0),
        value_(0),
        cannot_match_(false) { }
  bool Rationalize(bool one_byte);
  // Merge in the information from another branch of an alternation.
  void Merge(QuickCheckDetails* other, int from_index);
  // Advance the current position by some amount.
  void Advance(int by, bool one_byte);
  void Clear();
  bool cannot_match() { return cannot_match_; }
  void set_cannot_match() { cannot_match_ = true; }
  struct Position {
    Position() : mask(0), value(0), determines_perfectly(false) { }
    uc16 mask;
    uc16 value;
    bool determines_perfectly;
  };
  int characters() { return characters_; }
  void set_characters(int characters) { characters_ = characters; }
  Position* positions(int index) {
    DCHECK_LE(0, index);
    DCHECK_GT(characters_, index);
    return positions_ + index;
  }
  uint32_t mask() { return mask_; }
  uint32_t value() { return value_; }

 private:
  // How many characters do we have quick check information from.  This is
  // the same for all branches of a choice node.
  int characters_;
  Position positions_[4];
  // These values are the condensate of the above array after Rationalize().
  uint32_t mask_;
  uint32_t value_;
  // If set to true, there is no way this quick check can match at all.
  // E.g., if it requires to be at the start of the input, and isn't.
  bool cannot_match_;
};


extern int kUninitializedRegExpNodePlaceHolder;


class RegExpNode: public ZoneObject {
 public:
  explicit RegExpNode(Zone* zone)
      : replacement_(nullptr),
        on_work_list_(false),
        trace_count_(0),
        zone_(zone) {
    bm_info_[0] = bm_info_[1] = nullptr;
  }
  virtual ~RegExpNode();
  virtual void Accept(NodeVisitor* visitor) = 0;
  // Generates a goto to this node or actually generates the code at this point.
  virtual void Emit(RegExpCompiler* compiler, Trace* trace) = 0;
  // How many characters must this node consume at a minimum in order to
  // succeed.  If we have found at least 'still_to_find' characters that
  // must be consumed there is no need to ask any following nodes whether
  // they are sure to eat any more characters.  The not_at_start argument is
  // used to indicate that we know we are not at the start of the input.  In
  // this case anchored branches will always fail and can be ignored when
  // determining how many characters are consumed on success.
  virtual int EatsAtLeast(int still_to_find, int budget, bool not_at_start) = 0;
  // Emits some quick code that checks whether the preloaded characters match.
  // Falls through on certain failure, jumps to the label on possible success.
  // If the node cannot make a quick check it does nothing and returns false.
  bool EmitQuickCheck(RegExpCompiler* compiler,
                      Trace* bounds_check_trace,
                      Trace* trace,
                      bool preload_has_checked_bounds,
                      Label* on_possible_success,
                      QuickCheckDetails* details_return,
                      bool fall_through_on_failure);
  // For a given number of characters this returns a mask and a value.  The
  // next n characters are anded with the mask and compared with the value.
  // A comparison failure indicates the node cannot match the next n characters.
  // A comparison success indicates the node may match.
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start) = 0;
  static const int kNodeIsTooComplexForGreedyLoops = kMinInt;
  virtual int GreedyLoopTextLength() { return kNodeIsTooComplexForGreedyLoops; }
  // Only returns the successor for a text node of length 1 that matches any
  // character and that has no guards on it.
  virtual RegExpNode* GetSuccessorOfOmnivorousTextNode(
      RegExpCompiler* compiler) {
    return nullptr;
  }

  // Collects information on the possible code units (mod 128) that can match if
  // we look forward.  This is used for a Boyer-Moore-like string searching
  // implementation.  TODO(erikcorry):  This should share more code with
  // EatsAtLeast, GetQuickCheckDetails.  The budget argument is used to limit
  // the number of nodes we are willing to look at in order to create this data.
  static const int kRecursionBudget = 200;
  bool KeepRecursing(RegExpCompiler* compiler);
  virtual void FillInBMInfo(Isolate* isolate, int offset, int budget,
                            BoyerMooreLookahead* bm, bool not_at_start) {
    UNREACHABLE();
  }

  // If we know that the input is one-byte then there are some nodes that can
  // never match.  This method returns a node that can be substituted for
  // itself, or nullptr if the node can never match.
  virtual RegExpNode* FilterOneByte(int depth) { return this; }
  // Helper for FilterOneByte.
  RegExpNode* replacement() {
    DCHECK(info()->replacement_calculated);
    return replacement_;
  }
  RegExpNode* set_replacement(RegExpNode* replacement) {
    info()->replacement_calculated = true;
    replacement_ =  replacement;
    return replacement;  // For convenience.
  }

  // We want to avoid recalculating the lookahead info, so we store it on the
  // node.  Only info that is for this node is stored.  We can tell that the
  // info is for this node when offset == 0, so the information is calculated
  // relative to this node.
  void SaveBMInfo(BoyerMooreLookahead* bm, bool not_at_start, int offset) {
    if (offset == 0) set_bm_info(not_at_start, bm);
  }

  Label* label() { return &label_; }
  // If non-generic code is generated for a node (i.e. the node is not at the
  // start of the trace) then it cannot be reused.  This variable sets a limit
  // on how often we allow that to happen before we insist on starting a new
  // trace and generating generic code for a node that can be reused by flushing
  // the deferred actions in the current trace and generating a goto.
  static const int kMaxCopiesCodeGenerated = 10;

  bool on_work_list() { return on_work_list_; }
  void set_on_work_list(bool value) { on_work_list_ = value; }

  NodeInfo* info() { return &info_; }

  BoyerMooreLookahead* bm_info(bool not_at_start) {
    return bm_info_[not_at_start ? 1 : 0];
  }

  Zone* zone() const { return zone_; }

 protected:
  enum LimitResult { DONE, CONTINUE };
  RegExpNode* replacement_;

  LimitResult LimitVersions(RegExpCompiler* compiler, Trace* trace);

  void set_bm_info(bool not_at_start, BoyerMooreLookahead* bm) {
    bm_info_[not_at_start ? 1 : 0] = bm;
  }

 private:
  static const int kFirstCharBudget = 10;
  Label label_;
  bool on_work_list_;
  NodeInfo info_;
  // This variable keeps track of how many times code has been generated for
  // this node (in different traces).  We don't keep track of where the
  // generated code is located unless the code is generated at the start of
  // a trace, in which case it is generic and can be reused by flushing the
  // deferred operations in the current trace and generating a goto.
  int trace_count_;
  BoyerMooreLookahead* bm_info_[2];

  Zone* zone_;
};


class SeqRegExpNode: public RegExpNode {
 public:
  explicit SeqRegExpNode(RegExpNode* on_success)
      : RegExpNode(on_success->zone()), on_success_(on_success) { }
  RegExpNode* on_success() { return on_success_; }
  void set_on_success(RegExpNode* node) { on_success_ = node; }
  RegExpNode* FilterOneByte(int depth) override;
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override {
    on_success_->FillInBMInfo(isolate, offset, budget - 1, bm, not_at_start);
    if (offset == 0) set_bm_info(not_at_start, bm);
  }

 protected:
  RegExpNode* FilterSuccessor(int depth);

 private:
  RegExpNode* on_success_;
};


class ActionNode: public SeqRegExpNode {
 public:
  enum ActionType {
    SET_REGISTER,
    INCREMENT_REGISTER,
    STORE_POSITION,
    BEGIN_SUBMATCH,
    POSITIVE_SUBMATCH_SUCCESS,
    EMPTY_MATCH_CHECK,
    CLEAR_CAPTURES
  };
  static ActionNode* SetRegister(int reg, int val, RegExpNode* on_success);
  static ActionNode* IncrementRegister(int reg, RegExpNode* on_success);
  static ActionNode* StorePosition(int reg,
                                   bool is_capture,
                                   RegExpNode* on_success);
  static ActionNode* ClearCaptures(Interval range, RegExpNode* on_success);
  static ActionNode* BeginSubmatch(int stack_pointer_reg,
                                   int position_reg,
                                   RegExpNode* on_success);
  static ActionNode* PositiveSubmatchSuccess(int stack_pointer_reg,
                                             int restore_reg,
                                             int clear_capture_count,
                                             int clear_capture_from,
                                             RegExpNode* on_success);
  static ActionNode* EmptyMatchCheck(int start_register,
                                     int repetition_register,
                                     int repetition_limit,
                                     RegExpNode* on_success);
  void Accept(NodeVisitor* visitor) override;
  void Emit(RegExpCompiler* compiler, Trace* trace) override;
  int EatsAtLeast(int still_to_find, int budget, bool not_at_start) override;
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int filled_in,
                            bool not_at_start) override {
    return on_success()->GetQuickCheckDetails(
        details, compiler, filled_in, not_at_start);
  }
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override;
  ActionType action_type() { return action_type_; }
  // TODO(erikcorry): We should allow some action nodes in greedy loops.
  int GreedyLoopTextLength() override {
    return kNodeIsTooComplexForGreedyLoops;
  }

 private:
  union {
    struct {
      int reg;
      int value;
    } u_store_register;
    struct {
      int reg;
    } u_increment_register;
    struct {
      int reg;
      bool is_capture;
    } u_position_register;
    struct {
      int stack_pointer_register;
      int current_position_register;
      int clear_register_count;
      int clear_register_from;
    } u_submatch;
    struct {
      int start_register;
      int repetition_register;
      int repetition_limit;
    } u_empty_match_check;
    struct {
      int range_from;
      int range_to;
    } u_clear_captures;
  } data_;
  ActionNode(ActionType action_type, RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        action_type_(action_type) { }
  ActionType action_type_;
  friend class DotPrinter;
};


class TextNode: public SeqRegExpNode {
 public:
  TextNode(ZoneList<TextElement>* elms, bool read_backward,
           RegExpNode* on_success)
      : SeqRegExpNode(on_success), elms_(elms), read_backward_(read_backward) {}
  TextNode(RegExpCharacterClass* that, bool read_backward,
           RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        elms_(new (zone()) ZoneList<TextElement>(1, zone())),
        read_backward_(read_backward) {
    elms_->Add(TextElement::CharClass(that), zone());
  }
  // Create TextNode for a single character class for the given ranges.
  static TextNode* CreateForCharacterRanges(Zone* zone,
                                            ZoneList<CharacterRange>* ranges,
                                            bool read_backward,
                                            RegExpNode* on_success,
                                            JSRegExp::Flags flags);
  // Create TextNode for a surrogate pair with a range given for the
  // lead and the trail surrogate each.
  static TextNode* CreateForSurrogatePair(Zone* zone, CharacterRange lead,
                                          CharacterRange trail,
                                          bool read_backward,
                                          RegExpNode* on_success,
                                          JSRegExp::Flags flags);
  void Accept(NodeVisitor* visitor) override;
  void Emit(RegExpCompiler* compiler, Trace* trace) override;
  int EatsAtLeast(int still_to_find, int budget, bool not_at_start) override;
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int characters_filled_in,
                            bool not_at_start) override;
  ZoneList<TextElement>* elements() { return elms_; }
  bool read_backward() { return read_backward_; }
  void MakeCaseIndependent(Isolate* isolate, bool is_one_byte);
  int GreedyLoopTextLength() override;
  RegExpNode* GetSuccessorOfOmnivorousTextNode(
      RegExpCompiler* compiler) override;
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override;
  void CalculateOffsets();
  RegExpNode* FilterOneByte(int depth) override;

 private:
  enum TextEmitPassType {
    NON_LATIN1_MATCH,            // Check for characters that can't match.
    SIMPLE_CHARACTER_MATCH,      // Case-dependent single character check.
    NON_LETTER_CHARACTER_MATCH,  // Check characters that have no case equivs.
    CASE_CHARACTER_MATCH,        // Case-independent single character check.
    CHARACTER_CLASS_MATCH        // Character class.
  };
  static bool SkipPass(TextEmitPassType pass, bool ignore_case);
  static const int kFirstRealPass = SIMPLE_CHARACTER_MATCH;
  static const int kLastPass = CHARACTER_CLASS_MATCH;
  void TextEmitPass(RegExpCompiler* compiler,
                    TextEmitPassType pass,
                    bool preloaded,
                    Trace* trace,
                    bool first_element_checked,
                    int* checked_up_to);
  int Length();
  ZoneList<TextElement>* elms_;
  bool read_backward_;
};


class AssertionNode: public SeqRegExpNode {
 public:
  enum AssertionType {
    AT_END,
    AT_START,
    AT_BOUNDARY,
    AT_NON_BOUNDARY,
    AFTER_NEWLINE
  };
  static AssertionNode* AtEnd(RegExpNode* on_success) {
    return new(on_success->zone()) AssertionNode(AT_END, on_success);
  }
  static AssertionNode* AtStart(RegExpNode* on_success) {
    return new(on_success->zone()) AssertionNode(AT_START, on_success);
  }
  static AssertionNode* AtBoundary(RegExpNode* on_success) {
    return new(on_success->zone()) AssertionNode(AT_BOUNDARY, on_success);
  }
  static AssertionNode* AtNonBoundary(RegExpNode* on_success) {
    return new(on_success->zone()) AssertionNode(AT_NON_BOUNDARY, on_success);
  }
  static AssertionNode* AfterNewline(RegExpNode* on_success) {
    return new(on_success->zone()) AssertionNode(AFTER_NEWLINE, on_success);
  }
  void Accept(NodeVisitor* visitor) override;
  void Emit(RegExpCompiler* compiler, Trace* trace) override;
  int EatsAtLeast(int still_to_find, int budget, bool not_at_start) override;
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int filled_in,
                            bool not_at_start) override;
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override;
  AssertionType assertion_type() { return assertion_type_; }

 private:
  void EmitBoundaryCheck(RegExpCompiler* compiler, Trace* trace);
  enum IfPrevious { kIsNonWord, kIsWord };
  void BacktrackIfPrevious(RegExpCompiler* compiler,
                           Trace* trace,
                           IfPrevious backtrack_if_previous);
  AssertionNode(AssertionType t, RegExpNode* on_success)
      : SeqRegExpNode(on_success), assertion_type_(t) { }
  AssertionType assertion_type_;
};


class BackReferenceNode: public SeqRegExpNode {
 public:
  BackReferenceNode(int start_reg, int end_reg, JSRegExp::Flags flags,
                    bool read_backward, RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        start_reg_(start_reg),
        end_reg_(end_reg),
        flags_(flags),
        read_backward_(read_backward) {}
  void Accept(NodeVisitor* visitor) override;
  int start_register() { return start_reg_; }
  int end_register() { return end_reg_; }
  bool read_backward() { return read_backward_; }
  void Emit(RegExpCompiler* compiler, Trace* trace) override;
  int EatsAtLeast(int still_to_find, int recursion_depth,
                  bool not_at_start) override;
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int characters_filled_in,
                            bool not_at_start) override {
    return;
  }
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override;

 private:
  int start_reg_;
  int end_reg_;
  JSRegExp::Flags flags_;
  bool read_backward_;
};


class EndNode: public RegExpNode {
 public:
  enum Action { ACCEPT, BACKTRACK, NEGATIVE_SUBMATCH_SUCCESS };
  EndNode(Action action, Zone* zone) : RegExpNode(zone), action_(action) {}
  void Accept(NodeVisitor* visitor) override;
  void Emit(RegExpCompiler* compiler, Trace* trace) override;
  int EatsAtLeast(int still_to_find, int recursion_depth,
                  bool not_at_start) override {
    return 0;
  }
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int characters_filled_in,
                            bool not_at_start) override {
    // Returning 0 from EatsAtLeast should ensure we never get here.
    UNREACHABLE();
  }
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override {
    // Returning 0 from EatsAtLeast should ensure we never get here.
    UNREACHABLE();
  }

 private:
  Action action_;
};


class NegativeSubmatchSuccess: public EndNode {
 public:
  NegativeSubmatchSuccess(int stack_pointer_reg,
                          int position_reg,
                          int clear_capture_count,
                          int clear_capture_start,
                          Zone* zone)
      : EndNode(NEGATIVE_SUBMATCH_SUCCESS, zone),
        stack_pointer_register_(stack_pointer_reg),
        current_position_register_(position_reg),
        clear_capture_count_(clear_capture_count),
        clear_capture_start_(clear_capture_start) { }
  void Emit(RegExpCompiler* compiler, Trace* trace) override;

 private:
  int stack_pointer_register_;
  int current_position_register_;
  int clear_capture_count_;
  int clear_capture_start_;
};


class Guard: public ZoneObject {
 public:
  enum Relation { LT, GEQ };
  Guard(int reg, Relation op, int value)
      : reg_(reg),
        op_(op),
        value_(value) { }
  int reg() { return reg_; }
  Relation op() { return op_; }
  int value() { return value_; }

 private:
  int reg_;
  Relation op_;
  int value_;
};


class GuardedAlternative {
 public:
  explicit GuardedAlternative(RegExpNode* node)
      : node_(node), guards_(nullptr) {}
  void AddGuard(Guard* guard, Zone* zone);
  RegExpNode* node() { return node_; }
  void set_node(RegExpNode* node) { node_ = node; }
  ZoneList<Guard*>* guards() { return guards_; }

 private:
  RegExpNode* node_;
  ZoneList<Guard*>* guards_;
};


class AlternativeGeneration;


class ChoiceNode: public RegExpNode {
 public:
  explicit ChoiceNode(int expected_size, Zone* zone)
      : RegExpNode(zone),
        alternatives_(new (zone)
                          ZoneList<GuardedAlternative>(expected_size, zone)),
        table_(nullptr),
        not_at_start_(false),
        being_calculated_(false) {}
  void Accept(NodeVisitor* visitor) override;
  void AddAlternative(GuardedAlternative node) {
    alternatives()->Add(node, zone());
  }
  ZoneList<GuardedAlternative>* alternatives() { return alternatives_; }
  DispatchTable* GetTable(bool ignore_case);
  void Emit(RegExpCompiler* compiler, Trace* trace) override;
  int EatsAtLeast(int still_to_find, int budget, bool not_at_start) override;
  int EatsAtLeastHelper(int still_to_find,
                        int budget,
                        RegExpNode* ignore_this_node,
                        bool not_at_start);
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int characters_filled_in,
                            bool not_at_start) override;
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override;

  bool being_calculated() { return being_calculated_; }
  bool not_at_start() { return not_at_start_; }
  void set_not_at_start() { not_at_start_ = true; }
  void set_being_calculated(bool b) { being_calculated_ = b; }
  virtual bool try_to_emit_quick_check_for_alternative(bool is_first) {
    return true;
  }
  RegExpNode* FilterOneByte(int depth) override;
  virtual bool read_backward() { return false; }

 protected:
  int GreedyLoopTextLengthForAlternative(GuardedAlternative* alternative);
  ZoneList<GuardedAlternative>* alternatives_;

 private:
  friend class DispatchTableConstructor;
  friend class Analysis;
  void GenerateGuard(RegExpMacroAssembler* macro_assembler,
                     Guard* guard,
                     Trace* trace);
  int CalculatePreloadCharacters(RegExpCompiler* compiler, int eats_at_least);
  void EmitOutOfLineContinuation(RegExpCompiler* compiler,
                                 Trace* trace,
                                 GuardedAlternative alternative,
                                 AlternativeGeneration* alt_gen,
                                 int preload_characters,
                                 bool next_expects_preload);
  void SetUpPreLoad(RegExpCompiler* compiler,
                    Trace* current_trace,
                    PreloadState* preloads);
  void AssertGuardsMentionRegisters(Trace* trace);
  int EmitOptimizedUnanchoredSearch(RegExpCompiler* compiler, Trace* trace);
  Trace* EmitGreedyLoop(RegExpCompiler* compiler,
                        Trace* trace,
                        AlternativeGenerationList* alt_gens,
                        PreloadState* preloads,
                        GreedyLoopState* greedy_loop_state,
                        int text_length);
  void EmitChoices(RegExpCompiler* compiler,
                   AlternativeGenerationList* alt_gens,
                   int first_choice,
                   Trace* trace,
                   PreloadState* preloads);
  DispatchTable* table_;
  // If true, this node is never checked at the start of the input.
  // Allows a new trace to start with at_start() set to false.
  bool not_at_start_;
  bool being_calculated_;
};


class NegativeLookaroundChoiceNode : public ChoiceNode {
 public:
  explicit NegativeLookaroundChoiceNode(GuardedAlternative this_must_fail,
                                        GuardedAlternative then_do_this,
                                        Zone* zone)
      : ChoiceNode(2, zone) {
    AddAlternative(this_must_fail);
    AddAlternative(then_do_this);
  }
  int EatsAtLeast(int still_to_find, int budget, bool not_at_start) override;
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int characters_filled_in,
                            bool not_at_start) override;
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override {
    alternatives_->at(1).node()->FillInBMInfo(isolate, offset, budget - 1, bm,
                                              not_at_start);
    if (offset == 0) set_bm_info(not_at_start, bm);
  }
  // For a negative lookahead we don't emit the quick check for the
  // alternative that is expected to fail.  This is because quick check code
  // starts by loading enough characters for the alternative that takes fewest
  // characters, but on a negative lookahead the negative branch did not take
  // part in that calculation (EatsAtLeast) so the assumptions don't hold.
  bool try_to_emit_quick_check_for_alternative(bool is_first) override {
    return !is_first;
  }
  RegExpNode* FilterOneByte(int depth) override;
};


class LoopChoiceNode: public ChoiceNode {
 public:
  LoopChoiceNode(bool body_can_be_zero_length, bool read_backward, Zone* zone)
      : ChoiceNode(2, zone),
        loop_node_(nullptr),
        continue_node_(nullptr),
        body_can_be_zero_length_(body_can_be_zero_length),
        read_backward_(read_backward) {}
  void AddLoopAlternative(GuardedAlternative alt);
  void AddContinueAlternative(GuardedAlternative alt);
  void Emit(RegExpCompiler* compiler, Trace* trace) override;
  int EatsAtLeast(int still_to_find, int budget, bool not_at_start) override;
  void GetQuickCheckDetails(QuickCheckDetails* details,
                            RegExpCompiler* compiler, int characters_filled_in,
                            bool not_at_start) override;
  void FillInBMInfo(Isolate* isolate, int offset, int budget,
                    BoyerMooreLookahead* bm, bool not_at_start) override;
  RegExpNode* loop_node() { return loop_node_; }
  RegExpNode* continue_node() { return continue_node_; }
  bool body_can_be_zero_length() { return body_can_be_zero_length_; }
  bool read_backward() override { return read_backward_; }
  void Accept(NodeVisitor* visitor) override;
  RegExpNode* FilterOneByte(int depth) override;

 private:
  // AddAlternative is made private for loop nodes because alternatives
  // should not be added freely, we need to keep track of which node
  // goes back to the node itself.
  void AddAlternative(GuardedAlternative node) {
    ChoiceNode::AddAlternative(node);
  }

  RegExpNode* loop_node_;
  RegExpNode* continue_node_;
  bool body_can_be_zero_length_;
  bool read_backward_;
};


// Improve the speed that we scan for an initial point where a non-anchored
// regexp can match by using a Boyer-Moore-like table. This is done by
// identifying non-greedy non-capturing loops in the nodes that eat any
// character one at a time.  For example in the middle of the regexp
// /foo[\s\S]*?bar/ we find such a loop.  There is also such a loop implicitly
// inserted at the start of any non-anchored regexp.
//
// When we have found such a loop we look ahead in the nodes to find the set of
// characters that can come at given distances. For example for the regexp
// /.?foo/ we know that there are at least 3 characters ahead of us, and the
// sets of characters that can occur are [any, [f, o], [o]]. We find a range in
// the lookahead info where the set of characters is reasonably constrained. In
// our example this is from index 1 to 2 (0 is not constrained). We can now
// look 3 characters ahead and if we don't find one of [f, o] (the union of
// [f, o] and [o]) then we can skip forwards by the range size (in this case 2).
//
// For Unicode input strings we do the same, but modulo 128.
//
// We also look at the first string fed to the regexp and use that to get a hint
// of the character frequencies in the inputs. This affects the assessment of
// whether the set of characters is 'reasonably constrained'.
//
// We also have another lookahead mechanism (called quick check in the code),
// which uses a wide load of multiple characters followed by a mask and compare
// to determine whether a match is possible at this point.
enum ContainedInLattice {
  kNotYet = 0,
  kLatticeIn = 1,
  kLatticeOut = 2,
  kLatticeUnknown = 3  // Can also mean both in and out.
};


inline ContainedInLattice Combine(ContainedInLattice a, ContainedInLattice b) {
  return static_cast<ContainedInLattice>(a | b);
}


ContainedInLattice AddRange(ContainedInLattice a,
                            const int* ranges,
                            int ranges_size,
                            Interval new_range);


class BoyerMoorePositionInfo : public ZoneObject {
 public:
  explicit BoyerMoorePositionInfo(Zone* zone)
      : map_(new(zone) ZoneList<bool>(kMapSize, zone)),
        map_count_(0),
        w_(kNotYet),
        s_(kNotYet),
        d_(kNotYet),
        surrogate_(kNotYet) {
     for (int i = 0; i < kMapSize; i++) {
       map_->Add(false, zone);
     }
  }

  bool& at(int i) { return map_->at(i); }

  static const int kMapSize = 128;
  static const int kMask = kMapSize - 1;

  int map_count() const { return map_count_; }

  void Set(int character);
  void SetInterval(const Interval& interval);
  void SetAll();
  bool is_non_word() { return w_ == kLatticeOut; }
  bool is_word() { return w_ == kLatticeIn; }

 private:
  ZoneList<bool>* map_;
  int map_count_;  // Number of set bits in the map.
  ContainedInLattice w_;  // The \w character class.
  ContainedInLattice s_;  // The \s character class.
  ContainedInLattice d_;  // The \d character class.
  ContainedInLattice surrogate_;  // Surrogate UTF-16 code units.
};


class BoyerMooreLookahead : public ZoneObject {
 public:
  BoyerMooreLookahead(int length, RegExpCompiler* compiler, Zone* zone);

  int length() { return length_; }
  int max_char() { return max_char_; }
  RegExpCompiler* compiler() { return compiler_; }

  int Count(int map_number) {
    return bitmaps_->at(map_number)->map_count();
  }

  BoyerMoorePositionInfo* at(int i) { return bitmaps_->at(i); }

  void Set(int map_number, int character) {
    if (character > max_char_) return;
    BoyerMoorePositionInfo* info = bitmaps_->at(map_number);
    info->Set(character);
  }

  void SetInterval(int map_number, const Interval& interval) {
    if (interval.from() > max_char_) return;
    BoyerMoorePositionInfo* info = bitmaps_->at(map_number);
    if (interval.to() > max_char_) {
      info->SetInterval(Interval(interval.from(), max_char_));
    } else {
      info->SetInterval(interval);
    }
  }

  void SetAll(int map_number) {
    bitmaps_->at(map_number)->SetAll();
  }

  void SetRest(int from_map) {
    for (int i = from_map; i < length_; i++) SetAll(i);
  }
  void EmitSkipInstructions(RegExpMacroAssembler* masm);

 private:
  // This is the value obtained by EatsAtLeast.  If we do not have at least this
  // many characters left in the sample string then the match is bound to fail.
  // Therefore it is OK to read a character this far ahead of the current match
  // point.
  int length_;
  RegExpCompiler* compiler_;
  // 0xff for Latin1, 0xffff for UTF-16.
  int max_char_;
  ZoneList<BoyerMoorePositionInfo*>* bitmaps_;

  int GetSkipTable(int min_lookahead,
                   int max_lookahead,
                   Handle<ByteArray> boolean_skip_table);
  bool FindWorthwhileInterval(int* from, int* to);
  int FindBestInterval(
    int max_number_of_chars, int old_biggest_points, int* from, int* to);
};


// There are many ways to generate code for a node.  This class encapsulates
// the current way we should be generating.  In other words it encapsulates
// the current state of the code generator.  The effect of this is that we
// generate code for paths that the matcher can take through the regular
// expression.  A given node in the regexp can be code-generated several times
// as it can be part of several traces.  For example for the regexp:
// /foo(bar|ip)baz/ the code to match baz will be generated twice, once as part
// of the foo-bar-baz trace and once as part of the foo-ip-baz trace.  The code
// to match foo is generated only once (the traces have a common prefix).  The
// code to store the capture is deferred and generated (twice) after the places
// where baz has been matched.
class Trace {
 public:
  // A value for a property that is either known to be true, know to be false,
  // or not known.
  enum TriBool {
    UNKNOWN = -1, FALSE_VALUE = 0, TRUE_VALUE = 1
  };

  class DeferredAction {
   public:
    DeferredAction(ActionNode::ActionType action_type, int reg)
        : action_type_(action_type), reg_(reg), next_(nullptr) {}
    DeferredAction* next() { return next_; }
    bool Mentions(int reg);
    int reg() { return reg_; }
    ActionNode::ActionType action_type() { return action_type_; }
   private:
    ActionNode::ActionType action_type_;
    int reg_;
    DeferredAction* next_;
    friend class Trace;
  };

  class DeferredCapture : public DeferredAction {
   public:
    DeferredCapture(int reg, bool is_capture, Trace* trace)
        : DeferredAction(ActionNode::STORE_POSITION, reg),
          cp_offset_(trace->cp_offset()),
          is_capture_(is_capture) { }
    int cp_offset() { return cp_offset_; }
    bool is_capture() { return is_capture_; }
   private:
    int cp_offset_;
    bool is_capture_;
    void set_cp_offset(int cp_offset) { cp_offset_ = cp_offset; }
  };

  class DeferredSetRegister : public DeferredAction {
   public:
    DeferredSetRegister(int reg, int value)
        : DeferredAction(ActionNode::SET_REGISTER, reg),
          value_(value) { }
    int value() { return value_; }
   private:
    int value_;
  };

  class DeferredClearCaptures : public DeferredAction {
   public:
    explicit DeferredClearCaptures(Interval range)
        : DeferredAction(ActionNode::CLEAR_CAPTURES, -1),
          range_(range) { }
    Interval range() { return range_; }
   private:
    Interval range_;
  };

  class DeferredIncrementRegister : public DeferredAction {
   public:
    explicit DeferredIncrementRegister(int reg)
        : DeferredAction(ActionNode::INCREMENT_REGISTER, reg) { }
  };

  Trace()
      : cp_offset_(0),
        actions_(nullptr),
        backtrack_(nullptr),
        stop_node_(nullptr),
        loop_label_(nullptr),
        characters_preloaded_(0),
        bound_checked_up_to_(0),
        flush_budget_(100),
        at_start_(UNKNOWN) {}

  // End the trace.  This involves flushing the deferred actions in the trace
  // and pushing a backtrack location onto the backtrack stack.  Once this is
  // done we can start a new trace or go to one that has already been
  // generated.
  void Flush(RegExpCompiler* compiler, RegExpNode* successor);
  int cp_offset() { return cp_offset_; }
  DeferredAction* actions() { return actions_; }
  // A trivial trace is one that has no deferred actions or other state that
  // affects the assumptions used when generating code.  There is no recorded
  // backtrack location in a trivial trace, so with a trivial trace we will
  // generate code that, on a failure to match, gets the backtrack location
  // from the backtrack stack rather than using a direct jump instruction.  We
  // always start code generation with a trivial trace and non-trivial traces
  // are created as we emit code for nodes or add to the list of deferred
  // actions in the trace.  The location of the code generated for a node using
  // a trivial trace is recorded in a label in the node so that gotos can be
  // generated to that code.
  bool is_trivial() {
    return backtrack_ == nullptr && actions_ == nullptr && cp_offset_ == 0 &&
           characters_preloaded_ == 0 && bound_checked_up_to_ == 0 &&
           quick_check_performed_.characters() == 0 && at_start_ == UNKNOWN;
  }
  TriBool at_start() { return at_start_; }
  void set_at_start(TriBool at_start) { at_start_ = at_start; }
  Label* backtrack() { return backtrack_; }
  Label* loop_label() { return loop_label_; }
  RegExpNode* stop_node() { return stop_node_; }
  int characters_preloaded() { return characters_preloaded_; }
  int bound_checked_up_to() { return bound_checked_up_to_; }
  int flush_budget() { return flush_budget_; }
  QuickCheckDetails* quick_check_performed() { return &quick_check_performed_; }
  bool mentions_reg(int reg);
  // Returns true if a deferred position store exists to the specified
  // register and stores the offset in the out-parameter.  Otherwise
  // returns false.
  bool GetStoredPosition(int reg, int* cp_offset);
  // These set methods and AdvanceCurrentPositionInTrace should be used only on
  // new traces - the intention is that traces are immutable after creation.
  void add_action(DeferredAction* new_action) {
    DCHECK(new_action->next_ == nullptr);
    new_action->next_ = actions_;
    actions_ = new_action;
  }
  void set_backtrack(Label* backtrack) { backtrack_ = backtrack; }
  void set_stop_node(RegExpNode* node) { stop_node_ = node; }
  void set_loop_label(Label* label) { loop_label_ = label; }
  void set_characters_preloaded(int count) { characters_preloaded_ = count; }
  void set_bound_checked_up_to(int to) { bound_checked_up_to_ = to; }
  void set_flush_budget(int to) { flush_budget_ = to; }
  void set_quick_check_performed(QuickCheckDetails* d) {
    quick_check_performed_ = *d;
  }
  void InvalidateCurrentCharacter();
  void AdvanceCurrentPositionInTrace(int by, RegExpCompiler* compiler);

 private:
  int FindAffectedRegisters(OutSet* affected_registers, Zone* zone);
  void PerformDeferredActions(RegExpMacroAssembler* macro,
                              int max_register,
                              const OutSet& affected_registers,
                              OutSet* registers_to_pop,
                              OutSet* registers_to_clear,
                              Zone* zone);
  void RestoreAffectedRegisters(RegExpMacroAssembler* macro,
                                int max_register,
                                const OutSet& registers_to_pop,
                                const OutSet& registers_to_clear);
  int cp_offset_;
  DeferredAction* actions_;
  Label* backtrack_;
  RegExpNode* stop_node_;
  Label* loop_label_;
  int characters_preloaded_;
  int bound_checked_up_to_;
  QuickCheckDetails quick_check_performed_;
  int flush_budget_;
  TriBool at_start_;
};


class GreedyLoopState {
 public:
  explicit GreedyLoopState(bool not_at_start);

  Label* label() { return &label_; }
  Trace* counter_backtrack_trace() { return &counter_backtrack_trace_; }

 private:
  Label label_;
  Trace counter_backtrack_trace_;
};


struct PreloadState {
  static const int kEatsAtLeastNotYetInitialized = -1;
  bool preload_is_current_;
  bool preload_has_checked_bounds_;
  int preload_characters_;
  int eats_at_least_;
  void init() {
    eats_at_least_ = kEatsAtLeastNotYetInitialized;
  }
};


class NodeVisitor {
 public:
  virtual ~NodeVisitor() = default;
#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that) = 0;
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  virtual void VisitLoopChoice(LoopChoiceNode* that) { VisitChoice(that); }
};


// Node visitor used to add the start set of the alternatives to the
// dispatch table of a choice node.
class DispatchTableConstructor: public NodeVisitor {
 public:
  DispatchTableConstructor(DispatchTable* table, bool ignore_case,
                           Zone* zone)
      : table_(table),
        choice_index_(-1),
        ignore_case_(ignore_case),
        zone_(zone) { }

  void BuildTable(ChoiceNode* node);

  void AddRange(CharacterRange range) {
    table()->AddRange(range, choice_index_, zone_);
  }

  void AddInverse(ZoneList<CharacterRange>* ranges);

#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that);
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT

  DispatchTable* table() { return table_; }
  void set_choice_index(int value) { choice_index_ = value; }

 protected:
  DispatchTable* table_;
  int choice_index_;
  bool ignore_case_;
  Zone* zone_;
};


// Assertion propagation moves information about assertions such as
// \b to the affected nodes.  For instance, in /.\b./ information must
// be propagated to the first '.' that whatever follows needs to know
// if it matched a word or a non-word, and to the second '.' that it
// has to check if it succeeds a word or non-word.  In this case the
// result will be something like:
//
//   +-------+        +------------+
//   |   .   |        |      .     |
//   +-------+  --->  +------------+
//   | word? |        | check word |
//   +-------+        +------------+
class Analysis: public NodeVisitor {
 public:
  Analysis(Isolate* isolate, bool is_one_byte)
      : isolate_(isolate), is_one_byte_(is_one_byte), error_message_(nullptr) {}
  void EnsureAnalyzed(RegExpNode* node);

#define DECLARE_VISIT(Type) void Visit##Type(Type##Node* that) override;
  FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  void VisitLoopChoice(LoopChoiceNode* that) override;

  bool has_failed() { return error_message_ != nullptr; }
  const char* error_message() {
    DCHECK(error_message_ != nullptr);
    return error_message_;
  }
  void fail(const char* error_message) {
    error_message_ = error_message;
  }

  Isolate* isolate() const { return isolate_; }

 private:
  Isolate* isolate_;
  bool is_one_byte_;
  const char* error_message_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Analysis);
};


struct RegExpCompileData {
  RegExpCompileData()
      : tree(nullptr),
        node(nullptr),
        simple(true),
        contains_anchor(false),
        capture_count(0) {}
  RegExpTree* tree;
  RegExpNode* node;
  bool simple;
  bool contains_anchor;
  Handle<FixedArray> capture_name_map;
  Handle<String> error;
  int capture_count;
};


class RegExpEngine: public AllStatic {
 public:
  struct CompilationResult {
    CompilationResult(Isolate* isolate, const char* error_message)
        : error_message(error_message),
          code(ReadOnlyRoots(isolate).the_hole_value()),
          num_registers(0) {}
    CompilationResult(Object* code, int registers)
        : error_message(nullptr), code(code), num_registers(registers) {}
    const char* error_message;
    Object* code;
    int num_registers;
  };

  static CompilationResult Compile(Isolate* isolate, Zone* zone,
                                   RegExpCompileData* input,
                                   JSRegExp::Flags flags,
                                   Handle<String> pattern,
                                   Handle<String> sample_subject,
                                   bool is_one_byte);

  static bool TooMuchRegExpCode(Isolate* isolate, Handle<String> pattern);

  static void DotPrint(const char* label, RegExpNode* node, bool ignore_case);
};


class RegExpResultsCache : public AllStatic {
 public:
  enum ResultsCacheType { REGEXP_MULTIPLE_INDICES, STRING_SPLIT_SUBSTRINGS };

  // Attempt to retrieve a cached result.  On failure, 0 is returned as a Smi.
  // On success, the returned result is guaranteed to be a COW-array.
  static Object* Lookup(Heap* heap, String* key_string, Object* key_pattern,
                        FixedArray** last_match_out, ResultsCacheType type);
  // Attempt to add value_array to the cache specified by type.  On success,
  // value_array is turned into a COW-array.
  static void Enter(Isolate* isolate, Handle<String> key_string,
                    Handle<Object> key_pattern, Handle<FixedArray> value_array,
                    Handle<FixedArray> last_match_cache, ResultsCacheType type);
  static void Clear(FixedArray* cache);
  static const int kRegExpResultsCacheSize = 0x100;

 private:
  static const int kArrayEntriesPerCacheEntry = 4;
  static const int kStringOffset = 0;
  static const int kPatternOffset = 1;
  static const int kArrayOffset = 2;
  static const int kLastMatchOffset = 3;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_JSREGEXP_H_
