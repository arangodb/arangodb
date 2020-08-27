////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "geo_filter.hpp"

#include "index/index_reader.hpp"
#include "search/collectors.hpp"
#include "search/conjunction.hpp"
#include "search/disjunction.hpp"
#include "search/filter_visitor.hpp"
#include "search/multiterm_query.hpp"
#include "search/term_filter.hpp"

NS_LOCAL

using namespace irs;

template<typename Iterator>
class geo_terms_iterator {
 public:

 private:
  Iterator it_;
};

//////////////////////////////////////////////////////////////////////////////
/// @struct multiterm_state
/// @brief cached per reader state
//////////////////////////////////////////////////////////////////////////////
struct geo_state {
  using term_state = seek_term_iterator::seek_cookie::ptr;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reader using for iterate over the terms
  //////////////////////////////////////////////////////////////////////////////
  const term_reader* reader{};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief geo term states
  //////////////////////////////////////////////////////////////////////////////
  std::vector<seek_term_iterator::seek_cookie::ptr> states;
}; // geo_state

class geo_terms_query : public filter::prepared {
 public:
  using states_t = states_cache<geo_state>;

  geo_terms_query(
      states_t&& states,
      bstring&& stats,
      boost_t boost,
      bool contains) noexcept
    : prepared(boost), states_(std::move(states)),
      stats_(std::move(stats)), contains_(contains) {
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& segment,
      const order::prepared& ord,
      const attribute_provider* /*ctx*/) const override {
    using doc_iterator_t = score_iterator_adapter<doc_iterator::ptr>;

    // get term state for the specified reader
    auto state = states_.find(segment);

    if (!state) {
      // invalid state
      return doc_iterator::empty();
    }

    // get terms iterator
    auto terms = state->reader->iterator();

    if (IRS_UNLIKELY(!terms)) {
      return doc_iterator::empty();
    }

    std::vector<doc_iterator_t> itrs;
    itrs.reserve(state->states.size());

    const auto& features = irs::flags::empty_instance();

    for (auto& entry : state->states) {
      assert(entry);
      if (!terms->seek(bytes_ref::NIL, *entry)) {
        return doc_iterator::empty(); // internal error
      }

      auto docs = terms->postings(features);

      itrs.emplace_back(std::move(docs));

      if (IRS_UNLIKELY(!itrs.back().it)) {
        itrs.pop_back();

        if (contains_) {
          return doc_iterator::empty();
        }

        continue;
      }
    }

    doc_iterator::ptr it;
    if (contains_) {
      using conjunction_t = conjunction<doc_iterator::ptr>;

      it = make_conjunction<conjunction_t>(std::move(itrs));
    } else {
      using disjunction_t = disjunction_iterator<doc_iterator::ptr>;

      it = make_disjunction<disjunction_t>(std::move(itrs));
    }

    return it;

    //if (ord.empty()) {
    //  return make_disjunction<disjunction_t>(
    //    std::move(itrs), ord, merge_type_, state->estimation());
    //}

    //return make_disjunction<scored_disjunction_t>(
    //  std::move(itrs), ord, merge_type_, state->estimation());
  }

 private:
  states_t states_;
  bstring stats_;
  bool contains_;
};

NS_END

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                             by_geo_terms_options
// ----------------------------------------------------------------------------

void by_geo_terms_options::reset(GeoFilterType type, const S2Region& region) {
  terms_ = indexer_.GetQueryTerms(region, prefix_);
  type_ = type;
}

void by_geo_terms_options::reset(const S2Point& point) {
  terms_ = indexer_.GetQueryTerms(point, prefix_);
  type_ = GeoFilterType::INTERSECTS;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                     by_geo_terms
// ----------------------------------------------------------------------------

filter::prepared::ptr by_geo_terms::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const attribute_provider* /*ctx*/) const {
  const string_ref field = this->field();

  boost *= this->boost();
  const auto& geo_terms = options().terms();
  const size_t size = geo_terms.size();

  if (0 == size) {
    return prepared::empty();
  }

  if (1 == size) {
    return by_term::prepare(index, order, boost, field, ref_cast<byte_type>(geo_terms.front()));
  }

  const bool contains = options().type() == GeoFilterType::CONTAINS;
  field_collectors field_stats(order);
  geo_terms_query::states_t states(index.size());
  std::vector<geo_state::term_state> term_states;

  for (auto& segment : index) {
    auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    auto terms = reader->iterator();

    if (IRS_UNLIKELY(!terms)) {
      continue;
    }

    field_stats.collect(segment, *reader);
    term_states.reserve(size);

    for (auto& term : geo_terms) {
      if (!terms->seek(ref_cast<byte_type>(term))) {
        if (contains) {
          break;
        }

        continue;
      }

      terms->read();

      term_states.emplace_back(terms->cookie());
    }

    if (contains && term_states.size() != geo_terms.size()) {
      continue;
    }

    auto& state = states.insert(segment);
    state.reader = reader;
    state.states = std::move(term_states);
  }

  bstring stats(order.stats_size(), 0);
  field_stats.finish(const_cast<byte_type*>(stats.data()), index);

  return memory::make_managed<geo_terms_query>(
    std::move(states), std::move(stats),
    boost, contains);
}

DEFINE_FACTORY_DEFAULT(by_geo_terms)

NS_END
