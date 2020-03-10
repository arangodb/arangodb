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

#ifndef IRESEARCH_WILDCARD_UTILS_H
#define IRESEARCH_WILDCARD_UTILS_H

#include "automaton.hpp"

NS_ROOT

template<typename Char>
struct wildcard_traits {
  using char_type = Char;

  // match any string or empty string
  static constexpr Char MATCH_ANY_STRING= Char('%');

  // match any char
  static constexpr Char MATCH_ANY_CHAR = Char('_');

  // denotes beginning of escape sequence
  static constexpr Char ESCAPE = Char('\\');
};

template<
  typename Char,
  typename Traits = wildcard_traits<Char>,
  // brackets over condition are for circumventing MSVC parser bug
  typename = typename std::enable_if<(sizeof(Char) < sizeof(fst::fsa::kMaxLabel))>::type, 
  typename = typename std::enable_if<Traits::MATCH_ANY_CHAR != Traits::MATCH_ANY_STRING>::type>
automaton from_wildcard(const irs::basic_string_ref<Char>& expr) {
  automaton a;
  a.ReserveStates(expr.size() + 1);

  automaton::StateId from = a.AddState();
  automaton::StateId to = from;
  a.SetStart(from);

  automaton::StateId match_all_state = fst::kNoStateId;
  bool escaped = false;
  auto appendChar = [&match_all_state, &escaped, &a, &to, &from](Char c) {
    to = a.AddState();
    a.EmplaceArc(from, c, to);
    from = to;
    escaped = false;
    if (match_all_state != fst::kNoStateId) {
      auto state = a.AddState();
      a.ReserveArcs(state, 2);
      a.EmplaceArc(match_all_state, fst::fsa::kRho, state);
      a.EmplaceArc(state, fst::fsa::kRho, state);
      a.EmplaceArc(state, c, to);
      a.EmplaceArc(to, c, to);
      a.EmplaceArc(to, fst::fsa::kRho, state);
      match_all_state = fst::kNoStateId;
    }
  };

  for (const auto c : expr) {
    switch (c) {
      case Traits::MATCH_ANY_STRING: {
        if (escaped) {
          appendChar(c);
        } else {
          match_all_state = from;
        }
        break;
      }
      case Traits::MATCH_ANY_CHAR: {
        if (escaped) {
          appendChar(c);
        } else {
          to = a.AddState();
          a.EmplaceArc(from, fst::fsa::kRho, to);
          from = to;
        }
      } break;
      case Traits::ESCAPE: {
       if (escaped) {
         appendChar(c);
       } else {
         escaped = !escaped;
       }
       break;
      }
      default: {
        appendChar(c);
        break;
      }
    }
  }

  // non-terminated escape sequence
  if (escaped) {
    appendChar(Traits::ESCAPE);
  }

  if (match_all_state != fst::kNoLabel) {
    a.EmplaceArc(to, fst::fsa::kRho, to);
  }

  a.SetFinal(to);

  fst::ArcSort(&a, fst::ILabelCompare<fst::fsa::Transition>());

  automaton res;
  fst::Determinize(a, &res);

  return res;
}

NS_END

#endif // IRESEARCH_WILDCARD_UTILS_H

