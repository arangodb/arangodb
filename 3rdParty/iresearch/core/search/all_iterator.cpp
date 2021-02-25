////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "all_iterator.hpp"
#include "formats/empty_term_reader.hpp"

namespace iresearch {

all_iterator::all_iterator(
    const sub_reader& reader,
    const byte_type* query_stats,
    const order::prepared& order,
    uint64_t docs_count,
    boost_t boost)
  : attributes{{
      { type<document>::id(), &doc_   },
      { type<cost>::id(),     &cost_  },
      { type<score>::id(),    &score_ },
    }},
    score_(order),
    max_doc_(doc_id_t(doc_limits::min() + docs_count - 1)),
    cost_(max_doc_) {
  if (!order.empty()) {
    order::prepared::scorers scorers(
      order, reader, irs::empty_term_reader(docs_count),
      query_stats, score_.data(),
      *this, boost);

    irs::reset(score_, std::move(scorers));
  }
}

} // ROOT
