////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "wildcard_utils.hpp"

#include "fst/concat.h"

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wsign-compare"
  #pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "fstext/determinize-star.h"

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif


#include "automaton_utils.hpp"

namespace iresearch {

WildcardType wildcard_type(const bytes_ref& expr) noexcept {
  if (expr.empty()) {
    return WildcardType::TERM;
  }

  bool escaped = false;
  bool seen_escaped = false;
  size_t num_match_any_string = 0;
  size_t num_adjacent_match_any_string = 0;

  const auto* char_begin = expr.begin();
  const auto* end = expr.end();

  for (size_t i = 0; char_begin < end; ++i) {
    const auto char_length = utf8_utils::cp_length(*char_begin);
    const auto char_end = char_begin + char_length;

    if (!char_length || char_end > end) {
      return WildcardType::INVALID;
    }

    switch (*char_begin) {
      case WildcardMatch::ANY_STRING:
        num_adjacent_match_any_string += size_t(!escaped);
        num_match_any_string += size_t(!escaped);
        seen_escaped |= escaped;
        escaped = false;
        break;
      case WildcardMatch::ANY_CHAR:
        if (!escaped) {
          return WildcardType::WILDCARD;
        }
        seen_escaped = true;
        num_adjacent_match_any_string = 0;
        escaped = false;
        break;
      case WildcardMatch::ESCAPE:
        num_adjacent_match_any_string = 0;
        seen_escaped |= escaped;
        escaped = !escaped;
        break;
      default:
        num_adjacent_match_any_string = 0;
        escaped = false;
        break;
    }

    char_begin = char_end;
  }

  if (0 == num_match_any_string) {
    return seen_escaped ? WildcardType::TERM_ESCAPED
                        : WildcardType::TERM;
  }

  if (expr.size() == num_match_any_string) {
    return WildcardType::MATCH_ALL;
  }

  if (num_match_any_string == num_adjacent_match_any_string) {
    return seen_escaped ? WildcardType::PREFIX_ESCAPED
                        : WildcardType::PREFIX;
  }

  return WildcardType::WILDCARD;
}

automaton from_wildcard(const bytes_ref& expr) {
  // need this variable to preserve valid address
  // for cases with match all and  terminal escape
  // character (%\\)
  const byte_type c = WildcardMatch::ESCAPE;

  bool escaped = false;
  std::vector<automaton> parts;
  parts.reserve(expr.size() / 2); // reserve some space

  auto append_char = [&](const bytes_ref& label) {
    auto* begin = label.begin();

    parts.emplace_back(make_char(utf8_utils::next(begin)));
    escaped = false;
  };

  const auto* label_begin = expr.begin();
  const auto* end = expr.end();

  while (label_begin < end) {
    const auto label_length = utf8_utils::cp_length(*label_begin);
    const auto label_end = label_begin + label_length;

    if (!label_length || label_end > end) {
      // invalid UTF-8 sequence
      return {};
    }

    switch (*label_begin) {
      case WildcardMatch::ANY_STRING: {
        if (escaped) {
          append_char({label_begin, label_length});
        } else {
          parts.emplace_back(make_all());
        }
        break;
      }
      case WildcardMatch::ANY_CHAR: {
        if (escaped) {
          append_char({label_begin, label_length});
        } else {
          parts.emplace_back(make_any());
        }
        break;
      }
      case WildcardMatch::ESCAPE: {
        if (escaped) {
          append_char({label_begin, label_length});
        } else {
          escaped = !escaped;
        }
        break;
      }
      default: {
        if (escaped) {
          // a backslash followed by no special character
          parts.emplace_back(make_char({&c, 1}));
        }
        append_char({label_begin, label_length});
        break;
      }
    }

    label_begin = label_end;
  }

  if (escaped) {
    // a non-terminated escape sequence
    parts.emplace_back(make_char({&c, 1}));
  }

  automaton nfa;
  nfa.SetStart(nfa.AddState());
  nfa.SetFinal(0, true);

  for (auto begin = parts.rbegin(), end = parts.rend(); begin != end; ++begin) {
    // prefer prepending version of fst::Concat(...) as the cost of concatenation
    // is linear in the sum of the size of the input FSAs
    fst::Concat(*begin, &nfa);
  }

#ifdef IRESEARCH_DEBUG
  // ensure nfa is sorted
  static constexpr auto EXPECTED_NFA_PROPERTIES =
    fst::kILabelSorted | fst::kOLabelSorted |
    fst::kAcceptor | fst::kUnweighted;

  assert(EXPECTED_NFA_PROPERTIES == nfa.Properties(EXPECTED_NFA_PROPERTIES, true));
  UNUSED(EXPECTED_NFA_PROPERTIES);
#endif

  // nfa has only 1 arc per state
  nfa.SetProperties(fst::kILabelSorted, fst::kILabelSorted);

  automaton dfa;
  if (fst::DeterminizeStar(nfa, &dfa)) {
    // nfa isn't fully determinized
    return {};
  }

//FIXME???
//  fst::Minimize(&dfa);

  utf8_expand_labels(dfa);

#ifdef IRESEARCH_DEBUG
  // ensure resulting automaton is sorted and deterministic
  static constexpr auto EXPECTED_DFA_PROPERTIES =
    fst::kIDeterministic | EXPECTED_NFA_PROPERTIES;

  assert(EXPECTED_DFA_PROPERTIES == dfa.Properties(EXPECTED_DFA_PROPERTIES, true));
  UNUSED(EXPECTED_DFA_PROPERTIES);
#endif

  return dfa;
}

}
