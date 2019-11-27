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

#ifndef IRESEARCH_AUTOMATON_DECL_H
#define IRESEARCH_AUTOMATON_DECL_H

#include <cstddef>

NS_BEGIN(fst)

template <class Arc, class Allocator>
class VectorState;

template <class Arc, class State>
class VectorFst;

template<typename F, size_t CacheSize, bool MatchInput>
class TableMatcher;

NS_BEGIN(fsa)

struct Transition;

using AutomatonState = VectorState<Transition, std::allocator<Transition>>;
using Automaton = VectorFst<Transition, AutomatonState>;

NS_END // fsa
NS_END // fst

NS_ROOT

using automaton = fst::fsa::Automaton;

using automaton_table_matcher = fst::TableMatcher<automaton, 256, true>;

NS_END

#endif // IRESEARCH_AUTOMATON_DECL_H

