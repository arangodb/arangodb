////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "multi_delimited_token_stream.hpp"

#include <fst/union.h>
#include <fstext/determinize-star.h>
#include <velocypack/Iterator.h>

#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "utils/automaton_utils.hpp"
#include "utils/fstext/fst_draw.hpp"
#include "utils/vpack_utils.hpp"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

using namespace irs;
using namespace irs::analysis;

namespace {

template<typename Derived>
class MultiDelimitedTokenStreamBase : public MultiDelimitedAnalyser {
 public:
  MultiDelimitedTokenStreamBase() = default;

  bool next() override {
    while (true) {
      if (data.begin() == data.end()) {
        return false;
      }

      auto [next, skip] = static_cast<Derived*>(this)->FindNextDelim();

      if (next == data.begin()) {
        // skip empty terms
        IRS_ASSERT(skip <= data.size());
        data = bytes_view(data.data() + skip, data.size() - skip);
        continue;
      }

      auto& term = std::get<term_attribute>(attrs);
      term.value = bytes_view(data.data(), std::distance(data.begin(), next));
      auto& offset = std::get<irs::offset>(attrs);
      offset.start = std::distance(start, data.data());
      offset.end = offset.start + term.value.size();

      if (next == data.end()) {
        data = {};
      } else {
        data =
          bytes_view(&(*next) + skip, std::distance(next, data.end()) - skip);
      }

      return true;
    }
  }
};

template<typename Derived>
class MultiDelimitedTokenStreamSingleCharsBase
  : public MultiDelimitedTokenStreamBase<
      MultiDelimitedTokenStreamSingleCharsBase<Derived>> {
 public:
  auto FindNextDelim() {
    auto where = static_cast<Derived*>(this)->FindNextDelim();
    return std::make_pair(where, size_t{1});
  }
};

template<std::size_t N>
class MultiDelimitedTokenStreamSingleChars final
  : public MultiDelimitedTokenStreamSingleCharsBase<
      MultiDelimitedTokenStreamSingleChars<N>> {
 public:
  explicit MultiDelimitedTokenStreamSingleChars(
    const MultiDelimitedAnalyser::Options&) {
    // according to benchmarks "table" version is
    // ~1.5x faster except cases for 0 and 1.
    // So this should not be used.
    IRS_ASSERT(false);
  }

  auto FindNextDelim() { return this->data.end(); }
};

template<>
class MultiDelimitedTokenStreamSingleChars<1> final
  : public MultiDelimitedTokenStreamSingleCharsBase<
      MultiDelimitedTokenStreamSingleChars<1>> {
 public:
  explicit MultiDelimitedTokenStreamSingleChars(
    const MultiDelimitedAnalyser::Options& opts) {
    IRS_ASSERT(opts.delimiters.size() == 1);
    IRS_ASSERT(opts.delimiters[0].size() == 1);
    delim = opts.delimiters[0][0];
  }

  auto FindNextDelim() {
    if (auto pos = this->data.find(delim); pos != bstring::npos) {
      return this->data.begin() + pos;
    }
    return this->data.end();
  }

  byte_type delim;
};

template<>
class MultiDelimitedTokenStreamSingleChars<0> final
  : public MultiDelimitedTokenStreamSingleCharsBase<
      MultiDelimitedTokenStreamSingleChars<0>> {
 public:
  explicit MultiDelimitedTokenStreamSingleChars(
    const MultiDelimitedAnalyser::Options& opts) {
    IRS_ASSERT(opts.delimiters.empty());
  }

  auto FindNextDelim() { return this->data.end(); }
};

class MultiDelimitedTokenStreamGenericSingleChars final
  : public MultiDelimitedTokenStreamSingleCharsBase<
      MultiDelimitedTokenStreamGenericSingleChars> {
 public:
  explicit MultiDelimitedTokenStreamGenericSingleChars(const Options& opts) {
    for (const auto& delim : opts.delimiters) {
      IRS_ASSERT(delim.size() == 1);
      bytes[delim[0]] = true;
    }
  }

  auto FindNextDelim() {
    return std::find_if(data.begin(), data.end(), [&](auto c) {
      if (c > SCHAR_MAX) {
        return false;
      }
      IRS_ASSERT(c <= SCHAR_MAX);
      return bytes[c];
    });
  }
  // TODO maybe use a bitset instead?
  std::array<bool, SCHAR_MAX + 1> bytes{};
};

struct TrieNode {
  explicit TrieNode(int32_t id, int32_t depth) : state_id(id), depth(depth) {}
  int32_t state_id;
  int32_t depth;
  bool is_leaf{false};
  absl::flat_hash_map<byte_type, TrieNode*> simple_trie;
  absl::flat_hash_map<byte_type, TrieNode*> real_trie;
};

bytes_view FindLongestPrefixThatIsSuffix(bytes_view s, bytes_view str) {
  // TODO this algorithm is quadratic. Probably OK for small strings.
  for (std::size_t n = s.length() - 1; n > 0; n--) {
    auto prefix = s.substr(0, n);
    if (str.ends_with(prefix)) {
      return prefix;
    }
  }
  return {};
}

bytes_view FindLongestPrefixThatIsSuffix(const std::vector<bstring>& strings,
                                         std::string_view str) {
  bytes_view result = {};
  for (const auto& s : strings) {
    auto other = FindLongestPrefixThatIsSuffix(s, ViewCast<byte_type>(str));
    if (other.length() > result.length()) {
      result = other;
    }
  }
  return result;
}

void InsertErrorTransitions(const std::vector<bstring>& strings,
                            std::string& matched_word, TrieNode* node,
                            TrieNode* root) {
  if (node->is_leaf) {
    return;
  }

  for (size_t k = 0; k <= std::numeric_limits<byte_type>::max(); k++) {
    if (auto it = node->simple_trie.find(k); it != node->simple_trie.end()) {
      node->real_trie.emplace(k, it->second);
      matched_word.push_back(k);
      InsertErrorTransitions(strings, matched_word, it->second, root);
      matched_word.pop_back();
    } else {
      // if we find a character c that we don't expect, we have to find
      // the longest prefix of `str` that is a suffix of the already matched
      // text including c. then go to that state.
      matched_word.push_back(k);
      auto prefix = FindLongestPrefixThatIsSuffix(strings, matched_word);
      if (prefix.empty()) {
        matched_word.pop_back();
        continue;  // no prefix found implies going to the initial state
      }

      auto* dest = root;
      for (auto c : prefix) {
        auto itt = dest->simple_trie.find(c);
        IRS_ASSERT(itt != dest->simple_trie.end());
        dest = itt->second;
      }
      node->real_trie.emplace(k, dest);
      matched_word.pop_back();
    }
  }
}

automaton MakeStringTrie(const std::vector<bstring>& strings) {
  std::vector<std::unique_ptr<TrieNode>> nodes;
  nodes.emplace_back(std::make_unique<TrieNode>(0, 0));

  for (const auto& str : strings) {
    TrieNode* current = nodes.front().get();

    for (size_t k = 0; k < str.length(); k++) {
      auto c = str[k];
      if (current->is_leaf) {
        break;
      }

      if (auto it = current->simple_trie.find(c);
          it != current->simple_trie.end()) {
        current = it->second;
        continue;
      }

      auto& new_node =
        nodes.emplace_back(std::make_unique<TrieNode>(nodes.size(), k));
      current->simple_trie.emplace(c, new_node.get());
      current = new_node.get();
    }

    current->is_leaf = true;
  }

  std::string matched_word;
  auto* root = nodes.front().get();
  InsertErrorTransitions(strings, matched_word, root, root);

  automaton a;
  a.AddStates(nodes.size());
  a.SetStart(0);

  for (auto& n : nodes) {
    int64_t last_state = -1;
    size_t last_char = 0;

    if (n->is_leaf) {
      a.SetFinal(n->state_id, {1, static_cast<byte_type>(n->depth)});
      continue;
    }

    for (size_t k = 0; k < 256; k++) {
      int64_t next_state = root->state_id;
      if (auto it = n->real_trie.find(k); it != n->real_trie.end()) {
        next_state = it->second->state_id;
      }

      if (last_state == -1) {
        last_state = next_state;
        last_char = k;
      } else if (last_state != next_state) {
        a.EmplaceArc(n->state_id, RangeLabel::From(last_char, k - 1),
                     last_state);
        last_state = next_state;
        last_char = k;
      }
    }

    a.EmplaceArc(n->state_id, RangeLabel::From(last_char, 255), last_state);
  }

  return a;
}

class MultiDelimitedTokenStreamGeneric final
  : public MultiDelimitedTokenStreamBase<MultiDelimitedTokenStreamGeneric> {
 public:
  explicit MultiDelimitedTokenStreamGeneric(const Options& opts)
    : autom(MakeStringTrie(opts.delimiters)),
      matcher(MakeAutomatonMatcher(autom)) {
    // fst::drawFst(automaton_, std::cout);

#ifdef IRESEARCH_DEBUG
    // ensure nfa is sorted
    static constexpr auto EXPECTED_NFA_PROPERTIES =
      fst::kILabelSorted | fst::kOLabelSorted | fst::kAcceptor |
      fst::kUnweighted;

    IRS_ASSERT(EXPECTED_NFA_PROPERTIES ==
               autom.Properties(EXPECTED_NFA_PROPERTIES, true));
#endif
  }

  auto FindNextDelim() {
    auto state = matcher.GetFst().Start();
    matcher.SetState(state);
    for (size_t k = 0; k < data.length(); k++) {
      matcher.Find(data[k]);

      state = matcher.Value().nextstate;

      if (matcher.Final(state)) {
        auto length = matcher.Final(state).Payload();
        IRS_ASSERT(length <= k);

        return std::make_pair(data.begin() + (k - length),
                              static_cast<size_t>(length + 1));
      }

      matcher.SetState(state);
    }

    return std::make_pair(data.end(), size_t{0});
  }

  automaton autom;
  automaton_table_matcher matcher;
};

#ifdef __APPLE__
class MultiDelimitedTokenStreamSingle final
  : public MultiDelimitedTokenStreamBase<MultiDelimitedTokenStreamSingle> {
 public:
  explicit MultiDelimitedTokenStreamSingle(Options& opts)
    : delim(std::move(opts.delimiters[0])) {}

  auto FindNextDelim() {
    auto next = data.end();
    if (auto pos = this->data.find(delim); pos != bstring::npos) {
      next = this->data.begin() + pos;
    }
    return std::make_pair(next, delim.size());
  }

  bstring delim;
};
#else

class MultiDelimitedTokenStreamSingle final
  : public MultiDelimitedTokenStreamBase<MultiDelimitedTokenStreamSingle> {
 public:
  explicit MultiDelimitedTokenStreamSingle(Options& opts)
    : delim(std::move(opts.delimiters[0])),
      searcher(delim.begin(), delim.end()) {}

  auto FindNextDelim() {
    auto next = std::search(data.begin(), data.end(), searcher);
    return std::make_pair(next, delim.size());
  }

  bstring delim;
  std::boyer_moore_searcher<bstring::iterator> searcher;
};

#endif

template<std::size_t N>
irs::analysis::analyzer::ptr MakeSingleChar(
  MultiDelimitedAnalyser::Options&& opts) {
  if constexpr (N >= 2) {
    return std::make_unique<MultiDelimitedTokenStreamGenericSingleChars>(
      std::move(opts));
  } else if (opts.delimiters.size() == N) {
    return std::make_unique<MultiDelimitedTokenStreamSingleChars<N>>(
      std::move(opts));
  } else {
    return MakeSingleChar<N + 1>(std::move(opts));
  }
}

irs::analysis::analyzer::ptr Make(MultiDelimitedAnalyser::Options&& opts) {
  const bool single_character_case =
    std::all_of(opts.delimiters.begin(), opts.delimiters.end(),
                [](const auto& delim) { return delim.size() == 1; });
  if (single_character_case) {
    return MakeSingleChar<0>(std::move(opts));
  }
  if (opts.delimiters.size() == 1) {
    return std::make_unique<MultiDelimitedTokenStreamSingle>(opts);
  }
  return std::make_unique<MultiDelimitedTokenStreamGeneric>(std::move(opts));
}

constexpr std::string_view kDelimiterParamName{"delimiters"};

bool ParseVpackOptions(VPackSlice slice,
                       MultiDelimitedAnalyser::Options& options) {
  if (!slice.isObject()) {
    IRS_LOG_ERROR(
      "Slice for multi_delimited_token_stream is not an object or string");
    return false;
  }
  auto delim_array_slice = slice.get(kDelimiterParamName);
  if (!delim_array_slice.isArray()) {
    IRS_LOG_WARN(
      absl::StrCat("Invalid type or missing '", kDelimiterParamName,
                   "' (array expected) for multi_delimited_token_stream from "
                   "VPack arguments"));
    return false;
  }

  for (auto delim : VPackArrayIterator(delim_array_slice)) {
    if (!delim.isString()) {
      IRS_LOG_WARN(absl::StrCat(
        "Invalid type in '", kDelimiterParamName,
        "' (string expected) for multi_delimited_token_stream from "
        "VPack arguments"));
      return false;
    }
    auto view = ViewCast<byte_type>(delim.stringView());

    if (view.empty()) {
      IRS_LOG_ERROR("Delimiter list contains an empty string.");
      return false;
    }

    for (const auto& known : options.delimiters) {
      if (view.starts_with(known) || known.starts_with(view)) {
        IRS_LOG_ERROR(
          absl::StrCat("Some delimiters are a prefix of others. See `",
                       ViewCast<char>(bytes_view{known}), "` and `",
                       delim.stringView(), "`"));
        return false;
      }
    }

    options.delimiters.emplace_back(view);
  }
  return true;
}

bool MakeVpackConfig(const MultiDelimitedAnalyser::Options& options,
                     VPackBuilder* vpack_builder) {
  VPackObjectBuilder object(vpack_builder);
  {
    VPackArrayBuilder array(vpack_builder, kDelimiterParamName);
    for (const auto& delim : options.delimiters) {
      auto view = ViewCast<char>(bytes_view{delim});
      vpack_builder->add(VPackValue(view));
    }
  }

  return true;
}

irs::analysis::analyzer::ptr MakeVpack(VPackSlice slice) {
  MultiDelimitedAnalyser::Options options;
  if (ParseVpackOptions(slice, options)) {
    return irs::analysis::MultiDelimitedAnalyser::Make(std::move(options));
  }
  return nullptr;
}

irs::analysis::analyzer::ptr MakeVpack(std::string_view args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  return MakeVpack(slice);
}

bool NormalizeVpackConfig(VPackSlice slice, VPackBuilder* vpack_builder) {
  MultiDelimitedAnalyser::Options options;
  if (ParseVpackOptions(slice, options)) {
    return MakeVpackConfig(options, vpack_builder);
  }
  return false;
}

bool NormalizeVpackConfig(std::string_view args, std::string& definition) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  VPackBuilder builder;
  bool res = NormalizeVpackConfig(slice, &builder);
  if (res) {
    definition.assign(builder.slice().startAs<char>(),
                      builder.slice().byteSize());
  }
  return res;
}

REGISTER_ANALYZER_VPACK(irs::analysis::MultiDelimitedAnalyser, MakeVpack,
                        NormalizeVpackConfig);
/*
REGISTER_ANALYZER_JSON(irs::analysis::multi_delimited_token_stream, make_json,
                       normalize_json_config);
*/
}  // namespace

namespace irs::analysis {

void MultiDelimitedAnalyser::init() {
  REGISTER_ANALYZER_VPACK(MultiDelimitedAnalyser, MakeVpack,
                          NormalizeVpackConfig);  // match registration above
  // REGISTER_ANALYZER_JSON(multi_delimited_token_stream, make_json,
  //                        normalize_json_config);  // match registration above
}

analyzer::ptr MultiDelimitedAnalyser::Make(
  MultiDelimitedAnalyser::Options&& opts) {
  return ::Make(std::move(opts));
}

}  // namespace irs::analysis
