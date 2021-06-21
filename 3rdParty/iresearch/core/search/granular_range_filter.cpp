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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "granular_range_filter.hpp"

#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "search/boolean_filter.hpp"
#include "search/range_filter.hpp"
#include "search/multiterm_query.hpp"
#include "search/term_filter.hpp"
#include "search/limited_sample_collector.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"
#include "search/filter_visitor.hpp"

namespace {

//////////////////////////////////////////////////////////////////////////////
// example term structure, in order of term iteration/comparison, N = 4:
// all/each token _must_ produce N terms
// min/max term ranges may have more/less that N terms
//         V- granularity level, 0 being most precise
//         3 * * * * * *                                                      
//         2 | | | | | | * * * *                                              
//         1 | | | | | | | | | | * * * * * * *                                
//         0 | | | | | | | | | | | | | | | | | * * * * * * * * * * * * * * * *
// min_term (with e.g. N=2)----------^-------------------------^              
//                           ^-------------^--------------^                   
// max_term (with e.g. N=3)-/                                                 
//////////////////////////////////////////////////////////////////////////////

// return the granularity portion of the term
irs::bytes_ref mask_granularity(const irs::bytes_ref& term, size_t prefix_size) {
  return term.size() > prefix_size
    ? irs::bytes_ref(term.c_str(), prefix_size)
    : term;
}

// return the value portion of the term
irs::bytes_ref mask_value(const irs::bytes_ref& term, size_t prefix_size) {
  if (term.null()) {
    return term;
  }

  return term.size() > prefix_size
    ? irs::bytes_ref(term.c_str() + prefix_size, term.size() - prefix_size)
    : irs::bytes_ref();
}

// collect terms while they are accepted by Comparer
template<typename Visitor, typename Comparer>
void collect_terms(
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    irs::seek_term_iterator& terms,
    Visitor& visitor,
    const Comparer& cmp) {
  terms.read(); // read attributes (needed for cookie())
  visitor.prepare(segment, field, terms);

  do {
    terms.read(); // read attributes

    if (!cmp(terms)) {
      break; // terminate traversal
    }

    visitor.visit(irs::no_boost());
  } while (terms.next());
}

// collect all terms for a granularity range (min .. max), granularity level for max is ingored during comparison
// null min/max are _always_ inclusive, i.e.: [null == current .. max), (min .. null == end of granularity range]
template<typename Visitor>
void collect_terms_between(
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    irs::seek_term_iterator& terms,
    size_t prefix_size,
    const irs::bytes_ref& begin_term,
    const irs::bytes_ref& end_term, // granularity level for end_term is ingored during comparison
    bool include_begin_term, // should begin_term also be included
    bool include_end_term, /* should end_term also be included*/
    Visitor& visitor) {
  auto masked_begin_level = mask_granularity(terms.value(), prefix_size); // the starting range granularity level

  // seek to start of term range for collection
  if (!begin_term.null()) {
    auto res = terms.seek_ge(begin_term); // seek to start

    if (irs::SeekResult::END == res) {
      return; // have reached the end of terms in segment
    }

    if (irs::SeekResult::FOUND == res) {
      if (!include_begin_term) {
        if (!terms.next()) {
          return; // skipped current term and no more terms in segment
        }
      } else if (!include_end_term
                 && !end_term.null() // (begin .. end of granularity range]
                 && !(mask_value(begin_term, prefix_size) < mask_value(end_term, prefix_size))) { // don't use '==' since it compares size
        return; // empty range because end > begin
      }
    }

    masked_begin_level = mask_granularity(begin_term, prefix_size); // update level after seek
  } else if (!include_begin_term && !terms.next()) {
    return; // skipped current term and no more terms in segment
  }

  const auto& masked_end_term = mask_value(end_term, prefix_size); // the ending term for range collection

  collect_terms(
    segment, field, terms, visitor, [&prefix_size, &masked_begin_level, &masked_end_term, include_end_term](
      const irs::term_iterator& itr
    )->bool {
      const auto& masked_current_level = mask_granularity(itr.value(), prefix_size);
      const auto& masked_current_term = mask_value(itr.value(), prefix_size);

      // collect to end, end is when end of terms or masked_current_term < masked_begin_term or masked_current_term >= masked_end_term
      // i.e. end or granularity level boundary passed or term already covered by a less-granular range
      return masked_current_level == masked_begin_level
             && (masked_end_term.null() // (begin .. end of granularity range]
                 || (include_end_term && masked_current_term <= masked_end_term) // (being .. end]
                 || (!include_end_term && masked_current_term < masked_end_term) // (begin .. end)
                 )
             ;
    }
  );
}

// collect all terms starting from the min_term granularity range
template<typename Visitor>
void collect_terms_from(
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    irs::seek_term_iterator& terms,
    size_t prefix_size,
    const irs::by_granular_range::options_type::terms& min_term,
    bool min_term_inclusive,
    Visitor& visitor) {
  auto min_term_itr = min_term.rbegin(); // start with least granular

  // for the case where there is no min_term, include remaining range at the current granularity level
  if (min_term_itr == min_term.rend()) {
    collect_terms_between(
      segment, field,
      terms, prefix_size,
      irs::bytes_ref::NIL, // collect full granularity range
      irs::bytes_ref::NIL, // collect full granularity range
      true, true,
      visitor
    );

    return; // done
  }

  // ...........................................................................
  // now we have a min_term,
  // collect the min_term if requested and the least-granular term range
  // ...........................................................................

  auto* exact_min_term = &(*min_term.begin());

  // seek to least-granular term, advance by one and seek to end, (end is when masked next term is < masked current term)
  collect_terms_between(
    segment, field,
    terms, prefix_size,
    *min_term_itr, // the min term for the current granularity level
    irs::bytes_ref::NIL, // collect full granularity range
    min_term_inclusive && exact_min_term == &(*min_term_itr), true, // add min_term if requested
    visitor
  );

  // ...........................................................................
  // now we collected the min_term if requested and the least-granular range
  // collect the remaining more-granular ranges of the min_term
  // ...........................................................................

  // seek to next more-granular term, advance by one and seek to masked current term <= masked lesser-granular term
  for (auto current_min_term_itr = min_term_itr, end = min_term.rend();
       ++current_min_term_itr != end;
       ++min_term_itr
  ) {
    // seek to the same term at a lower granularity level than current level
    auto res = terms.seek_ge(*min_term_itr);

    if (irs::SeekResult::END == res) {
      continue;
    }

    auto end_term =
      (irs::SeekResult::NOT_FOUND == res || (irs::SeekResult::FOUND == res && terms.next()))     // have next term
      && mask_granularity(terms.value(), prefix_size) == mask_granularity(*min_term_itr, prefix_size) // on same level
      ? terms.value() : irs::bytes_ref::NIL
    ;
    irs::bstring end_term_copy;
    auto is_most_granular_term = exact_min_term == &(*current_min_term_itr);

    // need a copy of the term since bytes_ref changes on terms.seek(...)
    if (!end_term.null()) {
      end_term_copy.assign(end_term.c_str(), end_term.size());
      end_term = irs::bytes_ref(end_term_copy);
    }

    collect_terms_between(
      segment, field,
      terms, prefix_size,
      *current_min_term_itr, // the min term for the current granularity level
      end_term, // the min term for the previous lesser granularity level
      min_term_inclusive && is_most_granular_term, // add min_term if requested
      end_term.null() && is_most_granular_term, // add end term if required (for most granular)
      visitor
    );
  }
}

// collect terms only starting from the current granularity level and ending with granularity range, include/exclude end term
template<typename Visitor>
void collect_terms_until(
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    irs::seek_term_iterator& terms,
    size_t prefix_size,
    const irs::by_granular_range::options_type::terms& max_term,
    bool max_term_inclusive,
    Visitor& visitor) {
  auto max_term_itr = max_term.rbegin(); // start with least granular

  // for the case where there is no max_term, remaining range at the current granularity level
  if (max_term_itr == max_term.rend()) {
    collect_terms_between(
      segment, field,
      terms, prefix_size,
      irs::bytes_ref::NIL, // collect full granularity range
      irs::bytes_ref::NIL, // collect full granularity range
      true, true,
      visitor
    );

    return; // done
  }

  // align current granularity level to be == max_term_itr (to ensure current term is not a superset of max_term)
  {
    const auto& current_level = mask_granularity(terms.value(), prefix_size);

    for (auto end = max_term.rend(); current_level != mask_granularity(*max_term_itr, prefix_size);) {
      if (++max_term_itr == end) {
        return; // cannot find granularity level in max_term matching current term
      }
    }
  }

  // ...........................................................................
  // now we alligned max_term granularity level to match current term,
  // collect the least-granular term range
  // ...........................................................................

  auto* exact_max_term = &(*max_term.begin());

  // advance by one and collect all terms excluding the current max_term
  collect_terms_between(
    segment, field,
    terms, prefix_size,
    irs::bytes_ref::NIL, // collect full granularity range
    *max_term_itr, // the max term for the current granularity level
    true, max_term_inclusive && exact_max_term == &(*max_term_itr), // add max_term if requested
    visitor
  );

  // ...........................................................................
  // now we collected the least-granular range
  // collect the remaining more-granular ranges of the min_term
  // ...........................................................................

  irs::bstring tmp_term;

  // advance by one and collect all terms excluding the current max_term, repeat for all remaining granularity levels
  for (auto current_max_term_itr = max_term_itr, end = max_term.rend();
       ++current_max_term_itr != end;
       ++max_term_itr) {
    tmp_term = *max_term_itr;

    // build starting term from current granularity_level + value of less granular term
    if (max_term_itr->size() > prefix_size) {
      tmp_term.replace(0, prefix_size, *current_max_term_itr, 0, prefix_size);
    }

    collect_terms_between(
      segment, field,
      terms, prefix_size,
      tmp_term, // the max term for the previous lesser granularity level
      *current_max_term_itr, // the max term for the current granularity level
      true, max_term_inclusive && exact_max_term == &(*current_max_term_itr), // add max_term if requested
      visitor
    );
  }
}

// collect all terms starting from the min_term granularity range and max_term granularity range
template<typename Visitor>
void collect_terms_within(
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    irs::seek_term_iterator& terms,
    size_t prefix_size,
    const irs::by_granular_range::options_type::terms& min_term,
    const irs::by_granular_range::options_type::terms& max_term,
    bool min_term_inclusive,
    bool max_term_inclusive,
    Visitor& visitor) {
  auto min_term_itr = min_term.rbegin(); // start with least granular

  // for the case where there is no min_term, include remaining range at the current granularity level
  if (min_term_itr == min_term.rend()) {
    collect_terms_until(
      segment, field, terms, prefix_size, max_term, max_term_inclusive, visitor
    );

    return; // done
  }

  // ...........................................................................
  // now we have a min_term,
  // collect min_term if requested
  // ...........................................................................

  if (min_term_inclusive && !min_term.empty()) {
    auto& exact_min_term = min_term.front();
    bool single_term = !max_term.empty() && exact_min_term == max_term.front();

    if ((!single_term || max_term_inclusive)
        && exact_min_term > max_term.front()) {
      return; // empty range because min > max
    }

    if (single_term && min_term_inclusive != max_term_inclusive) {
      min_term_inclusive = false; // min term should not be included
    }
  }

  // ...........................................................................
  // now we collected the min_term if requested,
  // align the min_term granularity level with max_term granularity level
  // ...........................................................................

  auto* exact_min_term = min_term.empty() ? nullptr : &(min_term.front());
  auto max_term_itr = max_term.rbegin();

  // align min_term granularity level to be <= max_term_itr (to ensure min_term term is not a superset of max_term)
  if (!max_term.empty()) {
    auto min_end = min_term.rend();
    auto max_end = max_term.rend();

    for(;;) {
      auto& min_term_value = *min_term_itr;
      auto& max_term_value = *max_term_itr;
      const auto& min_term_level = mask_granularity(min_term_value, prefix_size);
      const auto& max_term_level = mask_granularity(max_term_value, prefix_size);

      if (min_term_level == max_term_level) {
        if (min_term_value != max_term_value || exact_min_term == &min_term_value) {
          break; // aligned matching granularity levels with terms in different ranges
        }

        ++min_term_itr; // min_term and max_term are in the same granularity range
        ++max_term_itr;
      } else if (min_term_level > max_term_level && ++min_term_itr == min_end) {
        return; // all granularities of min_term include max_term (valid to compare levels if they are for the same data type)
      } else if (min_term_level < max_term_level && ++max_term_itr == max_end) {
        return; // all granularities of max_term include min_term (valid to compare levels if they are for the same data type)
      }
    }
  }

  // ...........................................................................
  // now min_term_itr is aligned with some granularity value in max_term
  // collect the least-granular term range
  // ...........................................................................

  // seek to least-granular term, advance by one and seek to end, (end is when masked next term is < masked current term)
  collect_terms_between(
    segment, field,
    terms, prefix_size,
    *min_term_itr, // the min term for the current granularity level
    max_term.empty() ? irs::bytes_ref::NIL : irs::bytes_ref(*max_term_itr), // collect up to max term at same granularity range
    min_term_inclusive && exact_min_term == &(*min_term_itr), false, // add min_term if requested, end_term already covered by a less-granular range
    visitor
  );

  // ...........................................................................
  // now we collected the min_term if requested and the least-granular range
  // collect the remaining more-granular ranges of the min_term
  // ...........................................................................

  // seek to next more-granular term, advance by one and seek to masked current term <= masked lesser-granular term
  for (auto current_min_term_itr = min_term_itr, end = min_term.rend();
       ++current_min_term_itr != end;
       ++min_term_itr
  ) {
    auto res = terms.seek_ge(*min_term_itr);

    if (irs::SeekResult::END == res) {
      continue;
    }

    auto end_term =
      (irs::SeekResult::NOT_FOUND == res || (irs::SeekResult::FOUND == res && terms.next()))     // have next term
      && mask_granularity(terms.value(), prefix_size) == mask_granularity(*min_term_itr, prefix_size) // on same level
      ? terms.value() : irs::bytes_ref::NIL
    ;
    irs::bstring end_term_copy;

    // need a copy of the term since bytes_ref changes on terms.seek(...)
    if (!end_term.null()) {
      end_term_copy.assign(end_term.c_str(), end_term.size());
      end_term = irs::bytes_ref(end_term_copy);
    }

    collect_terms_between(
      segment, field,
      terms, prefix_size,
      *current_min_term_itr, // the min term for the current granularity level
      end_term, // the min term for the previous lesser granularity level
      min_term_inclusive && exact_min_term == &(*current_min_term_itr), false, // add min_term if requested, end_term already covered by a less-granular range
      visitor
    );
  }

  // ...........................................................................
  // now we collected the min_term range up to least-granular common max_term
  // collect the max-term range (if defined)
  // skip the current least-granular term since it contains min_term range
  // ...........................................................................

  // if max is a defined range then seek to max_term that was collected above and collect max_term range
  if (!max_term.empty() && terms.seek(*max_term_itr)) {
    collect_terms_until(
      segment, field, terms, prefix_size, max_term, max_term_inclusive, visitor
    );
  }
}

template<typename Visitor>
void visit(
    const irs::sub_reader& segment,
    const irs::term_reader& reader,
    const irs::by_granular_range::options_type::range_type& rng,
    Visitor& visitor) {

  auto terms = reader.iterator();

  if (IRS_UNLIKELY(!terms) || !terms->next()) {
    return; // no terms to collect
  }

  size_t prefix_size = reader.meta().features.check<irs::granularity_prefix>() ? 1 : 0;

  assert(!rng.min.empty() || irs::BoundType::UNBOUNDED == rng.min_type);
  assert(!rng.max.empty() || irs::BoundType::UNBOUNDED == rng.max_type);

  if (rng.min.empty()) { // open min range
    if (rng.max.empty()) { // open max range
      // collect all terms
      static const irs::by_granular_range::options_type::terms empty;
      collect_terms_from(segment, reader, *terms, prefix_size, empty, true, visitor);
      return;
    }

    auto& max_term = *rng.max.rbegin();
    irs::bytes_ref smallest_term(max_term.c_str(), std::min(max_term.size(), prefix_size)); // smallest least granular term

    // collect terms ending with max granularity range, include/exclude max term
    if (irs::SeekResult::END != terms->seek_ge(smallest_term)) {
      collect_terms_until(segment, reader, *terms, prefix_size, rng.max, irs::BoundType::INCLUSIVE == rng.max_type, visitor);
    }

    return;
  }

  if (rng.max.empty()) { // open max range
    // collect terms starting with min granularity range, include/exclude min term
    collect_terms_from(segment, reader, *terms, prefix_size, rng.min, irs::BoundType::INCLUSIVE == rng.min_type, visitor);
    return;
  }

  // collect terms starting with min granularity range and ending with max granularity range, include/exclude min/max term
  collect_terms_within(segment, reader, *terms, prefix_size, rng.min, rng.max, irs::BoundType::INCLUSIVE == rng.min_type, irs::BoundType::INCLUSIVE == rng.max_type, visitor);
}

struct granular_states {
  granular_states(size_t size) : states(size) {}

  irs::multiterm_state& insert(const irs::sub_reader& segment) {
    return states.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(&segment),
      std::forward_as_tuple()
    )->second; // create a new range state
  }

  std::unordered_multimap<const irs::sub_reader*, irs::multiterm_state> states;
};

} // namespace {

namespace iresearch {

// sequential 'granularity_level' value, cannot use 'iresearch::increment' since it can be 0
void set_granular_term(by_granular_range_options::terms& boundary,
                       numeric_token_stream& term) {
  boundary.clear();

  for (auto* term_attr = get<term_attribute>(term); term.next(); ) {
    assert(term_attr);
    boundary.emplace_back(term_attr->value);
  }
}

DEFINE_FACTORY_DEFAULT(by_granular_range)

/*static*/ filter::prepared::ptr by_granular_range::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const options_type::range_type& rng,
    size_t scored_terms_limit) {
  if (!rng.min.empty() && !rng.max.empty()) {
    const auto& min = rng.min.front();
    const auto& max = rng.max.front();

    if (min == max) { // compare the most precise terms
      if (rng.min_type == rng.max_type && rng.min_type == BoundType::INCLUSIVE) {
        // degenerated case
        return by_term::prepare(index, ord, boost, field, min);
      }

      // can't satisfy condition
      return prepared::empty();
    }
  }

  limited_sample_collector<term_frequency> collector(ord.empty() ? 0 : scored_terms_limit); // object for collecting order stats
  granular_states states(index.size());
  multiterm_visitor<granular_states> mtv(collector, states);

  // iterate over the segments
  for (const auto& segment: index) {
    // get term dictionary for field
    const term_reader* reader = segment.field(field);

    if (!reader) {
      continue; // no such field in this reader
    }


    ::visit(segment, *reader, rng, mtv);
  }

  std::vector<bstring> stats;
  collector.score(index, ord, stats);

  // ...........................................................................
  // group the range states into a minimal number of groups per sub_reader
  // ...........................................................................

  std::vector<multiterm_query::states_t> range_states;
  size_t current_states = 0;
  const sub_reader* previous_reader = nullptr;

  // build a set of regular range query states
  for (auto& entry: states.states) {
    auto& reader = entry.first;
    auto& state = entry.second;

    if (reader != previous_reader) {
      current_states = 0;
      previous_reader = reader;
    }

    if (state.empty()) {
      continue; // skip empty ranges
    }

    if (current_states >= range_states.size()) {
      range_states.emplace_back(index);
    }

    range_states[current_states++].insert(*reader) = std::move(state);
  }

  // ...........................................................................
  // build up a disjunction of range_queries each of the grouped states
  // ...........................................................................

  auto shared_stats = std::make_shared<decltype(stats)>(std::move(stats));

  // dummy class for returning the stored prepared query on a call to prepare(...)
  class multiterm_filter_proxy: public filter {
   public:
    static ptr make() { return memory::make_unique<multiterm_filter_proxy>(); }

    multiterm_filter_proxy()
      : filter(irs::type<by_range>::get()) {
    }

    virtual filter::prepared::ptr prepare(
        const index_reader&, const order::prepared&,
        boost_t, const attribute_provider*) const override {
      return std::move(query_);
    }

    mutable multiterm_query::ptr query_;
  };

  Or multirange_filter;

  for (auto& range_state: range_states) {
    multirange_filter.add<multiterm_filter_proxy>().query_
        = memory::make_managed<multiterm_query>(std::move(range_state), shared_stats,
                                               no_boost(), sort::MergeType::AGGREGATE);
  }

  return multirange_filter.boost(boost).prepare(index, ord, 1);
}

/*static*/ void by_granular_range::visit(
    const sub_reader& segment,
    const term_reader& reader,
    const options_type::range_type& rng,
    filter_visitor& visitor) {
  ::visit(segment, reader, rng, visitor);
}

} // ROOT
