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

#include <boost/functional/hash.hpp>

#include "boolean_filter.hpp"
#include "range_filter.hpp"
#include "range_query.hpp"
#include "term_query.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"

#include "granular_range_filter.hpp"

NS_LOCAL

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

typedef std::unordered_multimap<const iresearch::sub_reader*, iresearch::range_state> granular_states_t;

// return the granularity portion of the term
iresearch::bytes_ref mask_granularity(
  const iresearch::bytes_ref& term, size_t prefix_size
) {
  return term.size() > prefix_size
    ? iresearch::bytes_ref(term.c_str(), prefix_size)
    : term;
}

// return the value portion of the term
iresearch::bytes_ref mask_value(
  const iresearch::bytes_ref& term, size_t prefix_size
) {
  if (term.null()) {
    return term;
  }

  return term.size() > prefix_size
    ? iresearch::bytes_ref(term.c_str() + prefix_size, term.size() - prefix_size)
    : iresearch::bytes_ref();
}

// collect terms while they are accepted by Comparer
template<typename Comparer>
iresearch::range_state& collect_terms(
  granular_states_t& states,
  const iresearch::sub_reader& reader,
  const iresearch::term_reader& tr,
  iresearch::seek_term_iterator& terms,
  irs::limited_sample_scorer& scorer,
  const Comparer& cmp
) {
  auto& state = states.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(&reader),
    std::forward_as_tuple()
  )->second; // create a new range state

  // initialize range state
  terms.read(); // read attributes (needed for cookie())
  state.reader = &tr;
  state.min_term = terms.value();
  state.min_cookie = terms.cookie();
  state.unscored_docs.reset((irs::doc_limits::min)() + reader.docs_count()); // highest valid doc_id in reader

  auto& meta = terms.attributes().get<iresearch::term_meta>(); // get term metadata

  do {
    terms.read(); // read attributes

    if (!cmp(terms)) {
      break; // terminate traversal
    }


    // fill scoring candidates
    scorer.collect(
      meta ? meta->docs_count : 0,
      state.count, // current term offset in state
      state,
      reader,
      terms
    );
    ++(state.count);

    if (meta) {
      state.estimation += meta->docs_count;
    }
  } while (terms.next());

  return state;
}

// collect all terms for a granularity range (min .. max), granularity level for max is ingored during comparison
// null min/max are _always_ inclusive, i.e.: [null == current .. max), (min .. null == end of granularity range]
void collect_terms_between(
  granular_states_t& states,
  const iresearch::sub_reader& sr,
  const iresearch::term_reader& tr,
  iresearch::seek_term_iterator& terms,
  size_t prefix_size,
  irs::limited_sample_scorer& scorer,
  const iresearch::bytes_ref& begin_term,
  const iresearch::bytes_ref& end_term, // granularity level for end_term is ingored during comparison
  bool include_begin_term, // should begin_term also be included
  bool include_end_term // should end_term also be included
) {
  auto masked_begin_level = mask_granularity(terms.value(), prefix_size); // the starting range granularity level

  // seek to start of term range for collection
  if (!begin_term.null()) {
    auto res = terms.seek_ge(begin_term); // seek to start

    if (iresearch::SeekResult::END == res) {
      return; // have reached the end of terms in segment
    }

    if (iresearch::SeekResult::FOUND == res) {
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

  const auto& masked_begin_term = mask_value(terms.value(), prefix_size); // the starting term for range collection
  const auto& masked_end_term = mask_value(end_term, prefix_size); // the ending term for range collection

  collect_terms(
    states, sr, tr, terms, scorer, [&prefix_size, &masked_begin_level, &masked_begin_term, &masked_end_term, include_end_term](
      const iresearch::term_iterator& itr
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
void collect_terms_from(
  granular_states_t& states,
  const iresearch::sub_reader& sr,
  const iresearch::term_reader& tr,
  iresearch::seek_term_iterator& terms,
  size_t prefix_size,
  const iresearch::by_granular_range::terms_t& min_term,
  bool min_term_inclusive,
  irs::limited_sample_scorer& scorer
) {
  auto min_term_itr = min_term.rbegin(); // start with least granular

  // for the case where there is no min_term, include remaining range at the current granularity level
  if (min_term_itr == min_term.rend()) {
    collect_terms_between(
      states, sr, tr, terms, prefix_size, scorer,
      iresearch::bytes_ref::NIL, // collect full granularity range
      iresearch::bytes_ref::NIL, // collect full granularity range
      true, true
    );

    return; // done
  }

  // ...........................................................................
  // now we have a min_term,
  // collect the min_term if requested and the least-granular term range
  // ...........................................................................

  auto* exact_min_term = &(min_term.begin()->second);

  // seek to least-granular term, advance by one and seek to end, (end is when masked next term is < masked current term)
  collect_terms_between(
    states, sr, tr, terms, prefix_size, scorer,
    min_term_itr->second, // the min term for the current granularity level
    iresearch::bytes_ref::NIL, // collect full granularity range
    min_term_inclusive && exact_min_term == &(min_term_itr->second), true // add min_term if requested
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
    auto res = terms.seek_ge(min_term_itr->second);

    if (iresearch::SeekResult::END == res) {
      continue;
    }

    auto end_term =
      (iresearch::SeekResult::NOT_FOUND == res || (iresearch::SeekResult::FOUND == res && terms.next()))     // have next term
      && mask_granularity(terms.value(), prefix_size) == mask_granularity(min_term_itr->second, prefix_size) // on same level
      ? terms.value() : iresearch::bytes_ref::NIL
    ;
    iresearch::bstring end_term_copy;
    auto is_most_granular_term = exact_min_term == &(current_min_term_itr->second);

    // need a copy of the term since bytes_ref changes on terms.seek(...)
    if (!end_term.null()) {
      end_term_copy.assign(end_term.c_str(), end_term.size());
      end_term = iresearch::bytes_ref(end_term_copy);
    }

    collect_terms_between(
      states, sr, tr, terms, prefix_size, scorer,
      current_min_term_itr->second, // the min term for the current granularity level
      end_term, // the min term for the previous lesser granularity level
      min_term_inclusive && is_most_granular_term, // add min_term if requested
      end_term.null() && is_most_granular_term // add end term if required (for most granular)
    );
  }
}

// collect terms only starting from the current granularity level and ending with granularity range, include/exclude end term
void collect_terms_until(
  granular_states_t& states,
  const iresearch::sub_reader& sr,
  const iresearch::term_reader& tr,
  iresearch::seek_term_iterator& terms,
  size_t prefix_size,
  const iresearch::by_granular_range::terms_t& max_term,
  bool max_term_inclusive,
  irs::limited_sample_scorer& scorer
) {
  auto max_term_itr = max_term.rbegin(); // start with least granular

  // for the case where there is no max_term, remaining range at the current granularity level
  if (max_term_itr == max_term.rend()) {
    collect_terms_between(
      states, sr, tr, terms, prefix_size, scorer,
      iresearch::bytes_ref::NIL, // collect full granularity range
      iresearch::bytes_ref::NIL, // collect full granularity range
      true, true
    );

    return; // done
  }

  // align current granularity level to be == max_term_itr (to ensure current term is not a superset of max_term)
  {
    const auto& current_level = mask_granularity(terms.value(), prefix_size);

    for (auto end = max_term.rend(); current_level != mask_granularity(max_term_itr->second, prefix_size);) {
      if (++max_term_itr == end) {
        return; // cannot find granularity level in max_term matching current term
      }
    }
  }

  // ...........................................................................
  // now we alligned max_term granularity level to match current term,
  // collect the least-granular term range
  // ...........................................................................

  auto* exact_max_term = &(max_term.begin()->second);

  // advance by one and collect all terms excluding the current max_term
  collect_terms_between(
    states, sr, tr, terms, prefix_size, scorer,
    iresearch::bytes_ref::NIL, // collect full granularity range
    max_term_itr->second, // the max term for the current granularity level
    true, max_term_inclusive && exact_max_term == &(max_term_itr->second) // add max_term if requested
  );

  // ...........................................................................
  // now we collected the least-granular range
  // collect the remaining more-granular ranges of the min_term
  // ...........................................................................

  iresearch::bstring tmp_term;

  // advance by one and collect all terms excluding the current max_term, repeat for all remaining granularity levels
  for (auto current_max_term_itr = max_term_itr, end = max_term.rend();
       ++current_max_term_itr != end;
       ++max_term_itr
  ) {
    tmp_term = max_term_itr->second;

    // build starting term from current granularity_level + value of less granular term
    if (max_term_itr->second.size() > prefix_size) {
      tmp_term.replace(0, prefix_size, current_max_term_itr->second, 0, prefix_size);
    }

    collect_terms_between(
      states, sr, tr, terms, prefix_size, scorer,
      tmp_term, // the max term for the previous lesser granularity level
      current_max_term_itr->second, // the max term for the current granularity level
      true, max_term_inclusive && exact_max_term == &(current_max_term_itr->second) // add max_term if requested
    );
  }
}

// collect all terms starting from the min_term granularity range and max_term granularity range
void collect_terms_within(
  granular_states_t& states,
  const iresearch::sub_reader& sr,
  const iresearch::term_reader& tr,
  iresearch::seek_term_iterator& terms,
  size_t prefix_size,
  const iresearch::by_granular_range::terms_t& min_term,
  const iresearch::by_granular_range::terms_t& max_term,
  bool min_term_inclusive,
  bool max_term_inclusive,
  irs::limited_sample_scorer& scorer
) {
  auto min_term_itr = min_term.rbegin(); // start with least granular

  // for the case where there is no min_term, include remaining range at the current granularity level
  if (min_term_itr == min_term.rend()) {
    collect_terms_until(
      states, sr, tr, terms, prefix_size, max_term, max_term_inclusive, scorer
    );

    return; // done
  }

  // ...........................................................................
  // now we have a min_term,
  // collect min_term if requested
  // ...........................................................................

  if (min_term_inclusive && !min_term.empty()) {
    auto& exact_min_term = min_term.begin()->second;
    bool single_term = !max_term.empty() && exact_min_term == max_term.begin()->second;

    if ((!single_term || max_term_inclusive)
        && exact_min_term > max_term.begin()->second) {
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

  auto* exact_min_term = min_term.empty() ? nullptr : &(min_term.begin()->second);
  auto max_term_itr = max_term.rbegin();

  // align min_term granularity level to be <= max_term_itr (to ensure min_term term is not a superset of max_term)
  if (!max_term.empty()) {
    auto min_end = min_term.rend();
    auto max_end = max_term.rend();

    for(;;) {
      auto& min_term_value = min_term_itr->second;
      auto& max_term_value = max_term_itr->second;
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
    states, sr, tr, terms, prefix_size, scorer,
    min_term_itr->second, // the min term for the current granularity level
    max_term.empty() ? iresearch::bytes_ref::NIL : iresearch::bytes_ref(max_term_itr->second), // collect up to max term at same granularity range
    min_term_inclusive && exact_min_term == &(min_term_itr->second), false // add min_term if requested, end_term already covered by a less-granular range
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
    auto res = terms.seek_ge(min_term_itr->second);

    if (iresearch::SeekResult::END == res) {
      continue;
    }

    auto end_term =
      (iresearch::SeekResult::NOT_FOUND == res || (iresearch::SeekResult::FOUND == res && terms.next()))     // have next term
      && mask_granularity(terms.value(), prefix_size) == mask_granularity(min_term_itr->second, prefix_size) // on same level
      ? terms.value() : iresearch::bytes_ref::NIL
    ;
    iresearch::bstring end_term_copy;

    // need a copy of the term since bytes_ref changes on terms.seek(...)
    if (!end_term.null()) {
      end_term_copy.assign(end_term.c_str(), end_term.size());
      end_term = iresearch::bytes_ref(end_term_copy);
    }

    collect_terms_between(
      states, sr, tr, terms, prefix_size, scorer,
      current_min_term_itr->second, // the min term for the current granularity level
      end_term, // the min term for the previous lesser granularity level
      min_term_inclusive && exact_min_term == &(current_min_term_itr->second), false // add min_term if requested, end_term already covered by a less-granular range
    );
  }

  // ...........................................................................
  // now we collected the min_term range up to least-granular common max_term
  // collect the max-term range (if defined)
  // skip the current least-granular term since it contains min_term range
  // ...........................................................................

  // if max is a defined range then seek to max_term that was collected above and collect max_term range
  if (!max_term.empty() && terms.seek(max_term_itr->second)) {
    collect_terms_until(
      states, sr, tr, terms, prefix_size, max_term, max_term_inclusive, scorer
    );
  }
}

NS_END // NS_LOCAL

NS_ROOT

DEFINE_FILTER_TYPE(by_granular_range)
DEFINE_FACTORY_DEFAULT(by_granular_range)

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors & distructors
// -----------------------------------------------------------------------------

by_granular_range::by_granular_range():
  filter(by_granular_range::type()) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

by_granular_range& by_granular_range::field(std::string fld) {
  fld_ = std::move(fld); 
  return *this;
}

size_t by_granular_range::hash() const NOEXCEPT {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, fld_);
  ::boost::hash_combine(seed, rng_.min);
  ::boost::hash_combine(seed, rng_.min_type);
  ::boost::hash_combine(seed, rng_.max);
  ::boost::hash_combine(seed, rng_.max_type);
  return seed;
}

filter::prepared::ptr by_granular_range::prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& //ctx
) const {
  if (!rng_.min.empty() && !rng_.max.empty()) {
    const auto& min = rng_.min.begin()->second;
    const auto& max = rng_.max.begin()->second;

    if (min == max) { // compare the most precise terms
      if (rng_.min_type == rng_.max_type && rng_.min_type == Bound_Type::INCLUSIVE) {
        // degenerated case
        return term_query::make(rdr, ord, boost*this->boost(), fld_, min);
      }

      // can't satisfy condition
      return prepared::empty();
    }
  }


  limited_sample_scorer scorer(ord.empty() ? 0 : scored_terms_limit_); // object for collecting order stats
  granular_states_t states(rdr.size());

  // iterate over the segments
  for (const auto& sr: rdr) {
    // get term dictionary for field
    const term_reader* tr = sr.field(fld_);

    if (!tr) {
      continue; // no such field in this reader
    }

    size_t prefix_size = tr->meta().features.check<granularity_prefix>() ? 1 : 0;
    seek_term_iterator::ptr terms = tr->iterator();

    if (!terms->next()) {
      continue; // no terms to collect
    }

    assert(!rng_.min.empty() || Bound_Type::UNBOUNDED == rng_.min_type);
    assert(!rng_.max.empty() || Bound_Type::UNBOUNDED == rng_.max_type);

    if (rng_.min.empty()) { // open min range
      if (rng_.max.empty()) { // open max range
        // collect all terms
        static const terms_t empty;
        collect_terms_from(states, sr, *tr, *terms, prefix_size, empty, true, scorer);
        continue;
      }

      auto& max_term = rng_.max.rbegin()->second;
      bytes_ref smallest_term(max_term.c_str(), std::min(max_term.size(), prefix_size)); // smallest least granular term

      // collect terms ending with max granularity range, include/exclude max term
      if (iresearch::SeekResult::END != terms->seek_ge(smallest_term)) {
        collect_terms_until(states, sr, *tr, *terms, prefix_size, rng_.max, Bound_Type::INCLUSIVE == rng_.max_type, scorer);
      }

      continue;
    }

    if (rng_.max.empty()) { // open max range
      // collect terms starting with min granularity range, include/exclude min term
      collect_terms_from(states, sr, *tr, *terms, prefix_size, rng_.min, Bound_Type::INCLUSIVE == rng_.min_type, scorer);
      continue;
    }

    // collect terms starting with min granularity range and ending with max granularity range, include/exclude min/max term
    collect_terms_within(states, sr, *tr, *terms, prefix_size, rng_.min, rng_.max, Bound_Type::INCLUSIVE == rng_.min_type, Bound_Type::INCLUSIVE == rng_.max_type, scorer);
  }

  scorer.score(rdr, ord);

  // ...........................................................................
  // group the range states into a minimal number of groups per sub_reader
  // ...........................................................................

  std::vector<range_query::states_t> range_states;
  size_t current_states = 0;
  const sub_reader* previous_reader = nullptr;

  // build a set of regular range query states
  for (auto& entry: states) {
    auto& reader = entry.first;
    auto& state = entry.second;

    if (reader != previous_reader) {
      current_states = 0;
      previous_reader = reader;
    }

    if (!state.count) {
      continue; // skip empty ranges
    }

    if (current_states >= range_states.size()) {
      range_states.emplace_back(rdr.size());
    }

    range_states[current_states++].insert(*reader) = std::move(state);
  }

  // ...........................................................................
  // build up a disjunction of range_queries each of the grouped states
  // ...........................................................................

  // dummy class for returning the stored prepared query on a call to prepare(...)
  class range_filter_proxy: public filter {
   public:
    range_query::ptr query_;
    range_filter_proxy(): filter(by_range::type()) {}
    static ptr make() { return memory::make_unique<range_filter_proxy>(); }
    virtual filter::prepared::ptr prepare(
      const index_reader&,
      const order::prepared&,
      boost_t,
      const attribute_view&) const override {
      return query_;
    }
  };

  Or multirange_filter;

  for (auto& range_state: range_states) {
    multirange_filter.add<range_filter_proxy>().query_ = memory::make_shared<range_query>(std::move(range_state));
  }

  return multirange_filter.boost(this->boost()).prepare(rdr, ord, boost);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

bool by_granular_range::equals(const filter& rhs) const NOEXCEPT {
  const by_granular_range& trhs = static_cast<const by_granular_range&>(rhs);
  return filter::equals(rhs) && fld_ == trhs.fld_ && rng_ == trhs.rng_;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

bstring& by_granular_range::insert(
  terms_t& terms, const level_t& granularity_level
) {
  return terms[granularity_level];
}

bstring& by_granular_range::insert(
  terms_t& terms, const level_t& granularity_level, bstring&& term
) {
  return terms[granularity_level] = std::move(term);
}

bstring& by_granular_range::insert(
  terms_t& terms, const level_t& granularity_level, const bytes_ref& term
) {
  return terms[granularity_level] = term;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
