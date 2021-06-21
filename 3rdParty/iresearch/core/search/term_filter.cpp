////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "term_filter.hpp"

#include "index/index_reader.hpp"
#include "search/filter_visitor.hpp"
#include "search/collectors.hpp"
#include "search/term_query.hpp"

namespace {

using namespace irs;

//////////////////////////////////////////////////////////////////////////////
/// @class term_visitor
/// @brief filter visitor for term queries
//////////////////////////////////////////////////////////////////////////////
class term_visitor : private util::noncopyable {
 public:
  term_visitor(
      const term_collectors& term_stats,
      term_query::states_t& states)
    : term_stats_(term_stats),
      states_(states) {
  }

  void prepare(
      const sub_reader& segment,
      const term_reader& field,
      const seek_term_iterator& terms) noexcept {
    segment_ = &segment;
    reader_ = &field;
    terms_ = &terms;
  }

  void visit(boost_t /*boost*/) {
    // collect statistics
    assert(segment_ && reader_ && terms_);
    term_stats_.collect(*segment_, *reader_, 0, *terms_);

    // Cache term state in prepared query attributes.
    // Later, using cached state we could easily "jump" to
    // postings without relatively expensive FST traversal
    auto& state = states_.insert(*segment_);
    state.reader = reader_;
    state.cookie = terms_->cookie();
  }

 private:
  const term_collectors& term_stats_;
  term_query::states_t& states_;
  const sub_reader* segment_{};
  const term_reader* reader_{};
  const seek_term_iterator* terms_{};
};

template<typename Visitor>
void visit(
    const sub_reader& segment,
    const term_reader& field,
    const bytes_ref& term,
    Visitor& visitor) {
  // find term
  auto terms = field.iterator();

  if (IRS_UNLIKELY(!terms) || !terms->seek(term)) {
    return;
  }

  visitor.prepare(segment, field, *terms);

  // read term attributes
  terms->read();

  visitor.visit(no_boost());
}

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                            by_term implementation
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(by_term)

/*static*/ void by_term::visit(
    const sub_reader& segment,
    const term_reader& field,
    const bytes_ref& term,
    filter_visitor& visitor) {
  ::visit(segment, field, term, visitor);
}

/*static*/ filter::prepared::ptr by_term::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term) {
  term_query::states_t states(index);
  field_collectors field_stats(ord);
  term_collectors term_stats(ord, 1);

  term_visitor visitor(term_stats, states);

  // iterate over the segments
  for (const auto& segment : index) {
    // get field
    const auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    field_stats.collect(segment, *reader); // collect field statistics once per segment

    ::visit(segment, *reader, term, visitor);
  }

  bstring stats(ord.stats_size(), 0);
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  term_stats.finish(stats_buf, 0, field_stats, index);

  return memory::make_managed<term_query>(
    std::move(states), std::move(stats), boost);
}

} // ROOT
