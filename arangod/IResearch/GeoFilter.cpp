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

#include "GeoFilter.h"

#include "s2/s2cap.h"
#include "s2/s2earth.h"
#include "s2/s2point_region.h"

#include "index/index_reader.hpp"
#include "search/all_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/collectors.hpp"
#include "search/disjunction.hpp"
#include "search/multiterm_query.hpp"

#include "Basics/voc-errors.h"
#include "Geo/GeoJson.h"
#include "IResearch/Geo.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

using Disjunction = irs::disjunction_iterator<irs::doc_iterator::ptr>;

constexpr double_t EXTRA_COST = 1.5;

irs::filter::prepared::ptr match_all(
    irs::index_reader const& index,
    irs::order::prepared const& order,
    irs::string_ref const& field,
    irs::boost_t boost) {
  // Return everything we've stored
  irs::by_column_existence filter;
  *filter.mutable_field() = field;

  return filter.prepare(index, order, boost);
}

//////////////////////////////////////////////////////////////////////////////
/// @class GeoIterator
//////////////////////////////////////////////////////////////////////////////
template<typename Acceptor>
class GeoIterator : public irs::doc_iterator {
 public:
  GeoIterator(
      doc_iterator::ptr&& approx,
      doc_iterator::ptr&& columnIt,
      Acceptor& acceptor,
      const irs::sub_reader& reader,
      const irs::term_reader& field,
      const irs::byte_type* query_stats,
      irs::order::prepared const& order,
      irs::boost_t boost)
    : _approx(std::move(approx)),
      _columnIt(std::move(columnIt)),
      _storedValue(irs::get<irs::payload>(*_columnIt)),
      _doc(irs::get_mutable<irs::document>(_approx.get())),
      _cost([this](){
        // FIXME find a better estimation
        return static_cast<irs::cost::cost_t>(EXTRA_COST*irs::cost::extract(*_approx));
      }),
      _attrs{{
        { irs::type<irs::document>::id(),  _doc    },
        { irs::type<irs::cost>::id(),      &_cost  },
        { irs::type<irs::score>::id(),     &_score },
      }},
      _acceptor(acceptor) {
    if (!order.empty()) {
      irs::order::prepared::scorers scorers(
        order, reader, field,
        query_stats, _score.data(),
        *this, boost);

      irs::reset(_score, std::move(scorers));
    }
  }

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return _attrs.get_mutable(type);
  }

  virtual irs::doc_id_t value() const override final {
    return _doc->value;
  }

  virtual bool next() override {
    bool next = false;
    while ((next = _approx->next()) && !accept()) {}

    return next;
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    const auto prev = _doc->value;
    const auto doc = _approx->seek(target);

    if (prev == doc || accept()) {
      return doc;
    }

    next();

    return _doc->value;
  }

 private:
  bool accept() {
    if (_doc->value != _columnIt->seek(_doc->value) ||
        _storedValue->value.empty()) {
      return false;
    }

    if (!parseShape(slice(_storedValue->value), _shape, false)) {
      return false;
    }

    return _acceptor(_shape);
  }

  geo::ShapeContainer _shape;
  irs::doc_iterator::ptr _approx;
  irs::doc_iterator::ptr _columnIt;
  irs::payload const* _storedValue;
  irs::document* _doc;
  irs::cost _cost;
  irs::score _score;
  irs::frozen_attributes<3, attribute_provider> _attrs;
  Acceptor& _acceptor;
}; // GeoIterator

template<typename Acceptor>
irs::doc_iterator::ptr make_iterator(
    typename Disjunction::doc_iterators_t&& itrs,
    irs::doc_iterator::ptr&& columnIt,
    const irs::sub_reader& reader,
    const irs::term_reader& field,
    const irs::byte_type* query_stats,
    irs::order::prepared const& order,
    irs::boost_t boost,
    Acceptor& acceptor) {
  return irs::memory::make_managed<GeoIterator<Acceptor>>(
    irs::make_disjunction<Disjunction>(std::move(itrs)),
    std::move(columnIt), acceptor,
    reader, field, query_stats, order, boost);
}

//////////////////////////////////////////////////////////////////////////////
/// @struct GeoState
/// @brief cached per reader state
//////////////////////////////////////////////////////////////////////////////
struct GeoState {
  using TermState = irs::seek_term_iterator::seek_cookie::ptr;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief corresponding stored field
  //////////////////////////////////////////////////////////////////////////////
  const irs::columnstore_reader::column_reader* storedField;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reader using for iterate over the terms
  //////////////////////////////////////////////////////////////////////////////
  const irs::term_reader* reader;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief geo term states
  //////////////////////////////////////////////////////////////////////////////
  std::vector<irs::seek_term_iterator::seek_cookie::ptr> states;
}; // GeoState

using GeoStates = irs::states_cache<GeoState>;

//////////////////////////////////////////////////////////////////////////////
/// @class GeoQuery
/// @brief compiled GeoFilter
//////////////////////////////////////////////////////////////////////////////
template<typename Acceptor>
class GeoQuery final : public irs::filter::prepared {
 public:
  GeoQuery(
      GeoStates&& states,
      irs::bstring&& stats,
      Acceptor&& acceptor,
      irs::boost_t boost) noexcept
    : prepared(boost),
      _states(std::move(states)),
      _stats(std::move(stats)),
      _acceptor(std::move(acceptor)) {
  }

  virtual irs::doc_iterator::ptr execute(
      const irs::sub_reader& segment,
      const irs::order::prepared& ord,
      const irs::attribute_provider* /*ctx*/) const override {
    // get term state for the specified reader
    auto state = _states.find(segment);

    if (!state) {
      // invalid state
      return irs::doc_iterator::empty();
    }

    // get terms iterator
    TRI_ASSERT(state->reader);
    auto terms = state->reader->iterator();

    if (IRS_UNLIKELY(!terms)) {
      return irs::doc_iterator::empty();
    }

    typename Disjunction::doc_iterators_t itrs;
    itrs.reserve(state->states.size());

    const auto& features = irs::flags::empty_instance();

    for (auto& entry : state->states) {
      assert(entry);
      if (!terms->seek(irs::bytes_ref::NIL, *entry)) {
        return irs::doc_iterator::empty(); // internal error
      }

      auto docs = terms->postings(features);

      itrs.emplace_back(std::move(docs));

      if (IRS_UNLIKELY(!itrs.back().it)) {
        itrs.pop_back();

        continue;
      }
    }

    auto* reader = state->storedField;
    auto columnIt = reader ? reader->iterator() : nullptr;

    if (!columnIt) {
      return irs::doc_iterator::empty();
    }

    return make_iterator(
      std::move(itrs), std::move(columnIt),
      segment, *state->reader, _stats.c_str(),
      ord, boost(), _acceptor);
  }

 private:
  GeoStates _states;
  irs::bstring _stats;
  Acceptor _acceptor;
}; // GeoQuery

template<bool MinIncl, bool MaxIncl>
struct GeoDistanceRangeAcceptor {
  S2Cap min;
  S2Cap max;

  bool operator()(geo::ShapeContainer const& shape) const {
    auto const point = shape.centroid();

    return (MinIncl ? min.InteriorContains(point) : min.Contains(point)) &&
           (MaxIncl ? max.Contains(point) : max.InteriorContains(point));
  }
};

template<bool Incl>
struct GeoDistanceAcceptor {
  S2Cap filter;

  bool operator()(geo::ShapeContainer const& shape) const {
    auto const point = shape.centroid();

    return (Incl ? filter.Contains(point) : filter.InteriorContains(point));
  }
};

template<typename Acceptor>
irs::filter::prepared::ptr make_query(
    GeoStates&& states, irs::bstring&& stats,
    irs::boost_t boost, Acceptor&& acceptor) {
  return irs::memory::make_managed<GeoQuery<Acceptor>>(
    std::move(states), std::move(stats),
    std::move(acceptor), boost);
}

std::pair<GeoStates, irs::bstring> prepareStates(
    irs::index_reader const& index,
    irs::order::prepared const& order,
    std::vector<std::string> const& geoTerms,
    irs::string_ref const& field) {
  assert(!geoTerms.empty());

  std::pair<GeoStates, irs::bstring> res(
    std::piecewise_construct,
    std::forward_as_tuple(index.size()),
    std::forward_as_tuple(order.stats_size(), 0));

  auto const size = geoTerms.size();
  irs::field_collectors fieldStats(order);
  std::vector<GeoState::TermState> termStates;

  for (auto& segment : index) {
    auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    auto terms = reader->iterator();

    if (IRS_UNLIKELY(!terms)) {
      continue;
    }

    fieldStats.collect(segment, *reader);
    termStates.reserve(size);

    for (auto& term : geoTerms) {
      if (!terms->seek(irs::ref_cast<irs::byte_type>(term))) {
        continue;
      }

      terms->read();

      termStates.emplace_back(terms->cookie());
    }

    if (termStates.empty()) {
      continue;
    }

    auto& state = res.first.insert(segment);
    state.reader = reader;
    state.states = std::move(termStates);
    state.storedField = segment.column_reader(field);
  }

  fieldStats.finish(const_cast<irs::byte_type*>(res.second.data()), index);

  return res;
}

std::pair<S2Cap, bool> getBound(irs::BoundType type,
                                S2Point origin,
                                double_t distance) {
  if (irs::BoundType::UNBOUNDED == type) {
    return { S2Cap::Full(), true };
  }

  return {
    (0. == distance
      ? S2Cap::Empty()
      : S2Cap(origin, S1Angle::Radians(S2Earth::MetersToRadians(distance)))),
    irs::BoundType::INCLUSIVE == type
  };
}

irs::filter::prepared::ptr prepareInterval(
    irs::index_reader const& index,
    irs::order::prepared const& order,
    irs::boost_t boost,
    irs::string_ref const& field,
    GeoDistanceFilterOptions const& opts) {
  auto const& range = opts.range;
  auto const& origin = opts.origin;

  auto [minBound, minIncl] = getBound(range.min_type, origin, range.min);
  auto [maxBound, maxIncl] = getBound(range.max_type, origin, range.max);

  if (!minBound.is_valid() || !maxBound.is_valid()) {
    return irs::filter::prepared::empty();
  }

  if ((maxIncl && !maxBound.Intersects(minBound)) ||
      (!maxIncl && !maxBound.InteriorIntersects(minBound))) {
    return irs::filter::prepared::empty();
  }

  TRI_ASSERT(!minBound.is_empty());
  TRI_ASSERT(!maxBound.is_empty());

  // FIXME points only???
  S2RegionTermIndexer indexer(opts.options);
  S2RegionCoverer coverer(opts.options);

  // FIXME or Difference?
  minBound = minBound.Complement();
  auto const ring = coverer.GetCovering(maxBound).Intersection(coverer.GetCovering(maxBound));
  auto const geoTerms = indexer.GetQueryTermsForCanonicalCovering(ring, opts.prefix);

  if (geoTerms.empty()) {
    return irs::filter::prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field);

  switch (size_t(minIncl) + 2*size_t(maxIncl)) {
    case 0:
      return ::make_query(
        std::move(states), std::move(stats), boost,
        GeoDistanceRangeAcceptor<false, false>{minBound, maxBound});
    case 1:
      return ::make_query(
        std::move(states), std::move(stats), boost,
        GeoDistanceRangeAcceptor<true, false>{minBound, maxBound});
    case 2:
      return ::make_query(
        std::move(states), std::move(stats), boost,
        GeoDistanceRangeAcceptor<false, true>{minBound, maxBound});
    case 3:
      return ::make_query(
        std::move(states), std::move(stats), boost,
        GeoDistanceRangeAcceptor<true, true>{minBound, maxBound});
    default:
      TRI_ASSERT(false);
      return irs::filter::prepared::empty();
  }
}

irs::filter::prepared::ptr prepareOpenInterval(
    irs::index_reader const& index,
    irs::order::prepared const& order,
    irs::boost_t boost,
    irs::string_ref const& field,
    GeoDistanceFilterOptions const& opts,
    bool greater) {
  auto const& range = opts.range;

  auto [bound, incl] = greater
    ? getBound(range.min_type, opts.origin, range.min)
    : getBound(range.max_type, opts.origin, range.max);

  if (!bound.is_valid()) {
    return irs::filter::prepared::empty();
  }

  if (bound.is_full()) {
    return greater ? irs::filter::prepared::empty()
                   : match_all(index, order, field, boost);
  }

  if (bound.is_empty()) {
    return greater ? match_all(index, order, field, boost)
                   : irs::filter::prepared::empty();
  }

  // FIXME points only???
  S2RegionTermIndexer indexer(opts.options);

  if (greater) {
    bound = bound.Complement();
  }

  auto const geoTerms = indexer.GetQueryTerms(bound, opts.prefix);

  if (geoTerms.empty()) {
    return irs::filter::prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field);

  if (incl) {
    return ::make_query(
      std::move(states), std::move(stats), boost,
      GeoDistanceAcceptor<true>{bound});
  } else {
    return ::make_query(
      std::move(states), std::move(stats), boost,
      GeoDistanceAcceptor<false>{bound});
  }
}

}

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                                        GeoFilter
// ----------------------------------------------------------------------------

irs::filter::prepared::ptr GeoFilter::prepare(
    const irs::index_reader& index,
    const irs::order::prepared& order,
    irs::boost_t boost,
    const irs::attribute_provider* /*ctx*/) const {
  auto& shape = const_cast<geo::ShapeContainer&>(options().shape);

  if (shape.empty()) {
    return prepared::empty();
  }

  TRI_ASSERT(shape.region());

  S2RegionTermIndexer indexer(options().options);
  auto const geoTerms = geo::ShapeContainer::Type::S2_POINT == shape.type()
    ? [&](){ auto const* region = static_cast<S2PointRegion const*>(shape.region());
             return indexer.GetQueryTerms(region->point(), options().prefix); }()
    : [&](){ return indexer.GetQueryTerms(*shape.region(), options().prefix); }();

  if (geoTerms.empty()) {
    return prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field());

  boost *= this->boost();

  switch (options().type) {
    case GeoFilterType::INTERSECTS: {
      return ::make_query(
        std::move(states), std::move(stats), boost,
        [filterShape = std::move(shape)](geo::ShapeContainer const& shape) {
          return filterShape.intersects(&shape);
      });
    }
    case GeoFilterType::CONTAINS:
      return ::make_query(
        std::move(states), std::move(stats), boost,
        [filterShape = std::move(shape)](geo::ShapeContainer const& shape) {
          return filterShape.contains(&shape);
      });
    case GeoFilterType::IS_CONTAINED:
      return ::make_query(
        std::move(states), std::move(stats), boost,
        [filterShape = std::move(shape)](geo::ShapeContainer const& shape) {
          return shape.contains(&filterShape);
      });
    default:
      TRI_ASSERT(false);
      return prepared::empty();
  }
}

DEFINE_FACTORY_DEFAULT(GeoFilter)

// ----------------------------------------------------------------------------
// --SECTION--                                                GeoDistanceFilter
// ----------------------------------------------------------------------------

irs::filter::prepared::ptr GeoDistanceFilter::prepare(
    irs::index_reader const& index,
    irs::order::prepared const& order,
    irs::boost_t boost,
    irs::attribute_provider const* /*ctx*/) const {
  auto const& range = options().range;
  auto const lowerBound = irs::BoundType::UNBOUNDED != range.min_type;
  auto const upperBound = irs::BoundType::UNBOUNDED != range.max_type;

  boost *= this->boost();

  if (!lowerBound && !upperBound) {
    return match_all(index, order, field(), boost);
  } else if (lowerBound && upperBound) {
    return ::prepareInterval(index, order, boost, field(), options());
  } else {
    return ::prepareOpenInterval(index, order, boost, field(), options(), lowerBound);
  }
}

DEFINE_FACTORY_DEFAULT(GeoDistanceFilter)

} // iresearch
} // arangodb
