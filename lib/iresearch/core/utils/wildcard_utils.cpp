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
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "fstext/determinize-star.h"

#if defined(_MSC_VER)
// NOOP
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "automaton_utils.hpp"

namespace irs {

WildcardType ComputeWildcardType(bytes_view pattern) noexcept {
  if (pattern.empty()) {
    return WildcardType::kTerm;
  }

  bool escaped = false;
  bool seen_escaped = false;
  size_t size_any_str = 0;
  size_t curr_any_str = 0;

  const auto* it = pattern.data();
  const auto* end = it + pattern.size();
  for (; it != end; it = utf8_utils::Next(it, end)) {
    auto prev_any_str = std::exchange(curr_any_str, 0);
    if (escaped) {
      escaped = false;
      continue;
    }
    switch (*it) {
      case WildcardMatch::kAnyStr:
        if (prev_any_str == size_any_str) {
          curr_any_str = ++size_any_str;
          break;
        }
        [[fallthrough]];
      case WildcardMatch::kAnyChr:
        return WildcardType::kWildcard;
      case WildcardMatch::kEscape:
        escaped = true;
        seen_escaped = true;
        break;
      default:
        break;
    }
  }
  if (size_any_str == 0) {
    return seen_escaped ? WildcardType::kTermEscaped : WildcardType::kTerm;
  }
  if (size_any_str == curr_any_str) {
    return seen_escaped ? WildcardType::kPrefixEscaped : WildcardType::kPrefix;
  }
  return WildcardType::kWildcard;
}

automaton FromWildcard(bytes_view expr) {
  bool escaped = false;
  std::vector<automaton> parts;
  parts.reserve(expr.size());

  const auto* it = expr.data();
  const auto* end = it + expr.size();
  while (it != end) {
    const auto curr = *it;
    const auto* next = utf8_utils::Next(it, end);
    // TODO(MBkkt) remove manual size compute, needed for some apple libc++
    bytes_view label{it, static_cast<size_t>(next - it)};
    it = next;

    if (escaped) {
      parts.emplace_back(MakeChar(label));
      escaped = false;
      continue;
    }
    switch (curr) {
      case WildcardMatch::kAnyStr:
        parts.emplace_back(MakeAll());
        break;
      case WildcardMatch::kAnyChr:
        parts.emplace_back(MakeAny());
        break;
      case WildcardMatch::kEscape:
        escaped = true;
        break;
      default:
        parts.emplace_back(MakeChar(label));
        break;
    }
  }

  automaton nfa;
  nfa.SetStart(nfa.AddState());
  nfa.SetFinal(0, true);

  auto states = nfa.NumStates();
  for (const auto& part : parts) {
    states += fst::CountStates(part);
  }
  nfa.ReserveStates(states);
  for (auto begin = parts.rbegin(), end = parts.rend(); begin != end; ++begin) {
    // prefer prepending version of fst::Concat(...) as the cost of
    // concatenation is linear in the sum of the size of the input FSAs
    fst::Concat(*begin, &nfa);
  }

#ifdef IRESEARCH_DEBUG
  // ensure nfa is sorted
  static constexpr auto kExpectedNfaProperties =
    fst::kILabelSorted | fst::kOLabelSorted | fst::kAcceptor | fst::kUnweighted;

  IRS_ASSERT(kExpectedNfaProperties ==
             nfa.Properties(kExpectedNfaProperties, true));
#endif

  // nfa is sorted
  nfa.SetProperties(fst::kILabelSorted, fst::kILabelSorted);

  automaton dfa;
  if (fst::DeterminizeStar(nfa, &dfa)) {
    // nfa isn't fully determinized
    return {};
  }

#ifdef IRESEARCH_DEBUG
  // ensure resulting automaton is sorted and deterministic
  static constexpr auto kExpectedDfaProperties =
    kExpectedNfaProperties | fst::kIDeterministic;

  IRS_ASSERT(kExpectedDfaProperties ==
             dfa.Properties(kExpectedDfaProperties, true));
#endif

  return dfa;
}

}  // namespace irs
