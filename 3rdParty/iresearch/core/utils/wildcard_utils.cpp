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

#include "automaton_utils.hpp"

NS_ROOT

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
  struct {
   automaton::StateId from;
   automaton::StateId to;
   automaton::StateId match_all_from{ fst::kNoStateId };
   automaton::StateId match_all_to{ fst::kNoStateId };
   bytes_ref match_all_label{};
   bool escaped{ false };
   bool match_all{ false };
  } state;

  utf8_transitions_builder builder;
  std::pair<bytes_ref, automaton::StateId> arcs[2];

  automaton a;
  state.from = a.AddState();
  state.to = state.from;
  a.SetStart(state.from);

  auto appendChar = [&a, &builder, &arcs, &state](const bytes_ref& c) {
    state.to = a.AddState();
    if (!state.match_all) {
      if (state.match_all_label.null()) {
        utf8_emplace_arc(a, state.from, c, state.to);
      } else {
        const auto r = compare(c, state.match_all_label);

        if (!r) {
          utf8_emplace_arc(a, state.from, state.match_all_from, c, state.to);
          state.match_all_to = state.to;
        } else {
          arcs[0] = { c, state.to };
          arcs[1] = { state.match_all_label, state.match_all_to };

          if (r > 0) {
            std::swap(arcs[0], arcs[1]);
          }

          builder.insert(a, state.from, state.match_all_from,
                         std::begin(arcs), std::end(arcs));
        }
      }
    } else {
      utf8_emplace_arc(a, state.from, state.from, c, state.to);

      state.match_all_from = state.from;
      state.match_all_to = state.to;
      state.match_all_label = c;
      state.match_all = false;
    }

    state.from = state.to;
    state.escaped = false;
  };

  const auto* label_begin = expr.begin();
  const auto* end = expr.end();

  while (label_begin < end) {
    const auto label_length = utf8_utils::cp_length(*label_begin);
    const auto label_end = label_begin + label_length;

    if (!label_length || label_end > end) {
      // invalid UTF-8 sequence
      a.DeleteStates();
      return a;
    }

    switch (*label_begin) {
      case WildcardMatch::ANY_STRING: {
        if (state.escaped) {
          appendChar({label_begin, label_length});
        } else {
          state.match_all = true;
        }
        break;
      }
      case WildcardMatch::ANY_CHAR: {
        if (state.escaped) {
          appendChar({label_begin, label_length});
        } else {
          state.to = a.AddState();
          utf8_emplace_rho_arc(a, state.from, state.to);
          state.from = state.to;
        }
        break;
      }
      case WildcardMatch::ESCAPE: {
        if (state.escaped) {
          appendChar({label_begin, label_length});
        } else {
          state.escaped = !state.escaped;
        }
        break;
      }
      default: {
        appendChar({label_begin, label_length});
        break;
      }
    }

    label_begin = label_end;
  }

  // need this variable to preserve valid address
  // for cases with match all and  terminal escape
  // character (%\\)
  const byte_type c = WildcardMatch::ESCAPE;

  if (state.escaped) {
    // non-terminated escape sequence
    appendChar({&c, 1});
  } if (state.match_all) {
    // terminal MATCH_ALL
    utf8_emplace_rho_arc(a, state.to, state.to);
    state.match_all_from = fst::kNoStateId;
  }

  if (state.match_all_from != fst::kNoStateId) {
    // non-terminal MATCH_ALL
    utf8_emplace_arc(a, state.to, state.match_all_from,
                     state.match_all_label, state.match_all_to);
  }

  a.SetFinal(state.to);

#ifdef IRESEARCH_DEBUG
  // ensure resulting automaton is sorted and deterministic
  static constexpr auto EXPECTED_PROPERTIES =
    fst::kIDeterministic | fst::kODeterministic |
    fst::kILabelSorted | fst::kOLabelSorted |
    fst::kAcceptor;
  assert(EXPECTED_PROPERTIES == a.Properties(EXPECTED_PROPERTIES, true));
#endif

  return a;
}

NS_END
