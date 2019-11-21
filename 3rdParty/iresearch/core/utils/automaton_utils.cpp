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

#include "automaton_utils.hpp"

#include "index/index_reader.hpp"
#include "search/limited_sample_scorer.hpp"
#include "search/multiterm_query.hpp"
#include "utils/fst_table_matcher.hpp"

NS_ROOT

filter::prepared::ptr prepare_automaton_filter(const string_ref& field,
                                               const automaton& acceptor,
                                               size_t scored_terms_limit,
                                               const index_reader& index,
                                               const order::prepared& order,
                                               boost_t boost) {
  automaton_table_matcher matcher(acceptor, fst::fsa::kRho);

  if (fst::kError == matcher.Properties(0)) {
    IR_FRMT_ERROR("Expected deterministic, epsilon-free acceptor, "
                  "got the following properties " IR_UINT64_T_SPECIFIER "",
                  acceptor.Properties(automaton_table_matcher::FST_PROPERTIES, false));

    return filter::prepared::empty();
  }

  limited_sample_scorer scorer(order.empty() ? 0 : scored_terms_limit); // object for collecting order stats
  multiterm_query::states_t states(index.size());

  for (const auto& segment : index) {
    // get term dictionary for field
    const term_reader* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    auto it = reader->iterator(matcher);

    auto& meta = it->attributes().get<term_meta>(); // get term metadata
    const decltype(irs::term_meta::docs_count) NO_DOCS = 0;
    const auto& docs_count = meta ? meta->docs_count : NO_DOCS;

    if (it->next()) {
      auto& state = states.insert(segment);
      state.reader = reader;

      do {
        it->read(); // read term attributes

        state.estimation += docs_count;
        scorer.collect(docs_count, state.count++, state, segment, *it);
      } while (it->next());
    }
  }

  std::vector<bstring> stats;
  scorer.score(index, order, stats);

  return memory::make_shared<multiterm_query>(std::move(states), std::move(stats), boost);
}

NS_END
