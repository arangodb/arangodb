// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Utilities to convert strings into FSTs.

#ifndef FST_STRING_H_
#define FST_STRING_H_

#include <cstdint>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include <fst/flags.h>
#include <fst/log.h>

#include <fst/compact-fst.h>
#include <fst/icu.h>
#include <fst/mutable-fst.h>
#include <fst/properties.h>
#include <fst/symbol-table.h>
#include <fst/util.h>

#include <fst/compat.h>
#include <string_view>

DECLARE_string(fst_field_separator);

namespace fst {

enum class TokenType : uint8_t { SYMBOL = 1, BYTE = 2, UTF8 = 3 };

inline std::ostream &operator<<(std::ostream &strm,
                                const TokenType &token_type) {
  switch (token_type) {
    case TokenType::BYTE:
      return strm << "byte";
    case TokenType::UTF8:
      return strm << "utf8";
    case TokenType::SYMBOL:
      return strm << "symbol";
  }
  return strm;  // unreachable
}

namespace internal {

template <class Label>
bool ConvertSymbolToLabel(std::string_view str, const SymbolTable *syms,
                          Label unknown_label, bool allow_negative,
                          Label *output) {
  int64_t n;
  if (syms) {
    n = syms->Find(str);
    if ((n == kNoSymbol) && (unknown_label != kNoLabel)) n = unknown_label;
    if (n == kNoSymbol || (!allow_negative && n < 0)) {
      LOG(ERROR) << "ConvertSymbolToLabel: Symbol \"" << str
                 << "\" is not mapped to any integer label, symbol table = "
                 << syms->Name();
      return false;
    }
  } else {
    const auto maybe_n = ParseInt64(str);
    if (!maybe_n.has_value() || (!allow_negative && *maybe_n < 0)) {
      LOG(ERROR) << "ConvertSymbolToLabel: Bad label integer "
                 << "= \"" << str << "\"";
      return false;
    }
    n = *maybe_n;
  }
  *output = n;
  return true;
}

template <class Label>
bool ConvertStringToLabels(
    std::string_view str, TokenType token_type, const SymbolTable *syms,
    Label unknown_label, bool allow_negative, std::vector<Label> *labels,
    const std::string &sep = FST_FLAGS_fst_field_separator) {
  labels->clear();
  switch (token_type) {
    case TokenType::BYTE: {
      labels->reserve(str.size());
      return ByteStringToLabels(str, labels);
    }
    case TokenType::UTF8: {
      return UTF8StringToLabels(str, labels);
    }
    case TokenType::SYMBOL: {
      const std::string separator = fst::StrCat("\n", sep);
      for (std::string_view c :
           StrSplit(str, ByAnyChar(separator), SkipEmpty())) {
        Label label;
        if (!ConvertSymbolToLabel(c, syms, unknown_label, allow_negative,
                                  &label)) {
          return false;
        }
        labels->push_back(label);
      }
      return true;
    }
  }
  return false;  // Unreachable.
}

// The last character of 'sep' is used as a separator between symbols.
// Additionally, epsilon symbols will be printed only if omit_epsilon
// is false.
template <class Label>
bool LabelsToSymbolString(const std::vector<Label> &labels, std::string *str,
                          const SymbolTable &syms, std::string_view sep,
                          bool omit_epsilon) {
  std::stringstream ostrm;
  sep.remove_prefix(sep.size() - 1);  // We only respect the final char of sep.
  std::string_view delim = "";
  for (auto label : labels) {
    if (omit_epsilon && !label) continue;
    ostrm << delim;
    const std::string &symbol = syms.Find(label);
    if (symbol.empty()) {
      LOG(ERROR) << "LabelsToSymbolString: Label " << label
                 << " is not mapped onto any textual symbol in symbol table "
                 << syms.Name();
      return false;
    }
    ostrm << symbol;
    delim = sep;
  }
  *str = ostrm.str();
  return !!ostrm;
}

// The last character of 'sep' is used as a separator between symbols.
// Additionally, epsilon symbols will be printed only if omit_epsilon
// is false.
template <class Label>
bool LabelsToNumericString(const std::vector<Label> &labels, std::string *str,
                           std::string_view sep, bool omit_epsilon) {
  std::stringstream ostrm;
  sep.remove_prefix(sep.size() - 1);  // We only respect the final char of sep.
  std::string_view delim = "";
  for (auto label : labels) {
    if (omit_epsilon && !label) continue;
    ostrm << delim;
    ostrm << label;
    delim = sep;
  }
  *str = ostrm.str();
  return !!ostrm;
}

}  // namespace internal

// Functor for compiling a string in an FST.
template <class Arc>
class StringCompiler {
 public:
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;

  explicit StringCompiler(TokenType token_type = TokenType::BYTE,
                          const SymbolTable *syms = nullptr,
                          Label unknown_label = kNoLabel,
                          bool allow_negative = false)
      : token_type_(token_type),
        syms_(syms),
        unknown_label_(unknown_label),
        allow_negative_(allow_negative) {}

  // Compiles string into an FST. With SYMBOL token type, sep is used to
  // specify the set of char separators between symbols, in addition
  // of '\n' which is always treated as a separator.
  // Returns true on success.
  template <class FST>
  bool operator()(
      std::string_view str, FST *fst,
      const std::string &sep = FST_FLAGS_fst_field_separator) const {
    std::vector<Label> labels;
    if (!internal::ConvertStringToLabels(str, token_type_, syms_,
                                         unknown_label_, allow_negative_,
                                         &labels, sep)) {
      return false;
    }
    Compile(labels, fst);
    return true;
  }

  // Same as above but allows to specify a weight for the string.
  template <class FST>
  bool operator()(
      std::string_view str, FST *fst, Weight weight,
      const std::string &sep = FST_FLAGS_fst_field_separator) const {
    std::vector<Label> labels;
    if (!internal::ConvertStringToLabels(str, token_type_, syms_,
                                         unknown_label_, allow_negative_,
                                         &labels, sep)) {
      return false;
    }
    Compile(labels, fst, std::move(weight));
    return true;
  }

 private:
  void Compile(const std::vector<Label> &labels, MutableFst<Arc> *fst,
               Weight weight = Weight::One()) const {
    fst->DeleteStates();
    auto state = fst->AddState();
    fst->SetStart(state);
    fst->AddStates(labels.size());
    for (auto label : labels) {
      fst->AddArc(state, Arc(label, label, state + 1));
      ++state;
    }
    fst->SetFinal(state, std::move(weight));
    fst->SetProperties(kCompiledStringProperties, kCompiledStringProperties);
  }

  template <class Unsigned>
  void Compile(const std::vector<Label> &labels,
               CompactStringFst<Arc, Unsigned> *fst) const {
    using Compactor = typename CompactStringFst<Arc, Unsigned>::Compactor;
    fst->SetCompactor(
        std::make_shared<Compactor>(labels.begin(), labels.end()));
  }

  template <class Unsigned>
  void Compile(const std::vector<Label> &labels,
               CompactWeightedStringFst<Arc, Unsigned> *fst,
               Weight weight = Weight::One()) const {
    std::vector<std::pair<Label, Weight>> compacts;
    compacts.reserve(labels.size() + 1);
    for (StateId i = 0; i < static_cast<StateId>(labels.size()) - 1; ++i) {
      compacts.emplace_back(labels[i], Weight::One());
    }
    compacts.emplace_back(!labels.empty() ? labels.back() : kNoLabel, weight);
    using Compactor =
        typename CompactWeightedStringFst<Arc, Unsigned>::Compactor;
    fst->SetCompactor(
        std::make_shared<Compactor>(compacts.begin(), compacts.end()));
  }

  const TokenType token_type_;
  const SymbolTable *syms_;    // Symbol table (used when token type is symbol).
  const Label unknown_label_;  // Label for token missing from symbol table.
  const bool allow_negative_;  // Negative labels allowed?

  StringCompiler(const StringCompiler &) = delete;
  StringCompiler &operator=(const StringCompiler &) = delete;
};

// A useful alias when using StdArc.
using StdStringCompiler = StringCompiler<StdArc>;

// Helpers for StringPrinter.

// Converts an FST to a vector of output labels. To get input labels, use
// Project or Invert. Returns true on success. Use only with string FSTs; may
// loop for non-string FSTs.
template <class Arc>
bool StringFstToOutputLabels(const Fst<Arc> &fst,
                             std::vector<typename Arc::Label> *labels) {
  labels->clear();
  auto s = fst.Start();
  if (s == kNoStateId) {
    LOG(ERROR) << "StringFstToOutputLabels: Invalid start state";
    return false;
  }
  while (fst.Final(s) == Arc::Weight::Zero()) {
    ArcIterator<Fst<Arc>> aiter(fst, s);
    if (aiter.Done()) {
      LOG(ERROR) << "StringFstToOutputLabels: Does not reach final state";
      return false;
    }
    const auto &arc = aiter.Value();
    labels->push_back(arc.olabel);
    s = arc.nextstate;
    aiter.Next();
    if (!aiter.Done()) {
      LOG(ERROR) << "StringFstToOutputLabels: State " << s
                 << " has multiple outgoing arcs";
      return false;
    }
  }
  if (fst.NumArcs(s) != 0) {
    LOG(ERROR) << "StringFstToOutputLabels: Final state " << s
               << " has outgoing arc(s)";
    return false;
  }
  return true;
}

// Converts a list of symbols to a string. If the token type is SYMBOL, the last
// character of sep is used to separate textual symbols. Additionally, if the
// token type is SYMBOL, epsilon symbols will be printed only if omit_epsilon
// is false. Returns true on success.
template <class Label>
bool LabelsToString(
    const std::vector<Label> &labels, std::string *str,
    TokenType ttype = TokenType::BYTE, const SymbolTable *syms = nullptr,
    const std::string &sep = FST_FLAGS_fst_field_separator,
    bool omit_epsilon = true) {
  switch (ttype) {
    case TokenType::BYTE: {
      return LabelsToByteString(labels, str);
    }
    case TokenType::UTF8: {
      return LabelsToUTF8String(labels, str);
    }
    case TokenType::SYMBOL: {
      return syms ? internal::LabelsToSymbolString(labels, str, *syms, sep,
                                                   omit_epsilon)
                  : internal::LabelsToNumericString(labels, str, sep,
                                                    omit_epsilon);
    }
  }
  return false;
}

// Functor for printing a string FST as a string.
template <class Arc>
class StringPrinter {
 public:
  using Label = typename Arc::Label;

  explicit StringPrinter(TokenType token_type = TokenType::BYTE,
                         const SymbolTable *syms = nullptr,
                         bool omit_epsilon = true)
      : token_type_(token_type), syms_(syms), omit_epsilon_(omit_epsilon) {}

  // Converts the FST into a string. With SYMBOL token type, the last character
  // of sep is used as a separator between symbols. Returns true on success.
  bool operator()(
      const Fst<Arc> &fst, std::string *str,
      const std::string &sep = FST_FLAGS_fst_field_separator) const {
    std::vector<Label> labels;
    return StringFstToOutputLabels(fst, &labels) &&
           LabelsToString(labels, str, token_type_, syms_, sep, omit_epsilon_);
  }

 private:
  const TokenType token_type_;
  const SymbolTable *syms_;
  const bool omit_epsilon_;

  StringPrinter(const StringPrinter &) = delete;
  StringPrinter &operator=(const StringPrinter &) = delete;
};

// A useful alias when using StdArc.
using StdStringPrinter = StringPrinter<StdArc>;

}  // namespace fst

#endif  // FST_STRING_H_
