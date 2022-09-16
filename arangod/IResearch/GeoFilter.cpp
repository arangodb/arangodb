////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "search/boolean_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/collectors.hpp"
#include "search/disjunction.hpp"
#include "search/multiterm_query.hpp"
#include "utils/memory.hpp"

#include "Basics/voc-errors.h"
#include "Geo/GeoParams.h"
#include "Geo/GeoJson.h"
#include "IResearch/Geo.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"

namespace {

// assume up to 2x machine epsilon in precision errors for signleton caps
constexpr auto SIGNLETON_CAP_EPS = 2 * std::numeric_limits<double_t>::epsilon();

using namespace arangodb;
using namespace arangodb::iresearch;

using Disjunction =
    irs::disjunction_iterator<irs::doc_iterator::ptr, irs::NoopAggregator>;

// Return a filter matching all documents with a given geo field
irs::filter::prepared::ptr match_all(irs::index_reader const& index,
                                     irs::Order const& order,
                                     irs::string_ref field,
                                     irs::score_t boost) {
  // Return everything we've stored
  irs::by_column_existence filter;
  *filter.mutable_field() = field;

  return filter.prepare(index, order, boost);
}

// Returns singleton S2Cap that tolerates precision errors
inline S2Cap fromPoint(S2Point const& origin) {
  return S2Cap(origin, S1Angle::Radians(SIGNLETON_CAP_EPS));
}

template<typename Acceptor>
class GeoIterator : public irs::doc_iterator {
 private:
  // Two phase iterator is heavier than a usual disjunction
  static constexpr irs::cost::cost_t EXTRA_COST = 2;

 public:
  GeoIterator(doc_iterator::ptr&& approx, doc_iterator::ptr&& columnIt,
              Acceptor& acceptor, const irs::sub_reader& reader,
              const irs::term_reader& field, const irs::byte_type* query_stats,
              irs::Order const& order, irs::score_t boost)
      : _approx(std::move(approx)),
        _columnIt(std::move(columnIt)),
        _storedValue(irs::get<irs::payload>(*_columnIt)),
        _acceptor(acceptor) {
    std::get<irs::attribute_ptr<irs::document>>(_attrs) =
        irs::get_mutable<irs::document>(_approx.get());

    std::get<irs::cost>(_attrs).reset([this]() noexcept {
      return EXTRA_COST * irs::cost::extract(*_approx);
    });

    if (!order.empty()) {
      auto& score = std::get<irs::score>(_attrs);
      score = irs::CompileScore(order.buckets(), reader, field, query_stats,
                                *this, boost);
    }
  }

  virtual irs::attribute* get_mutable(
      irs::type_info::type_id type) noexcept override {
    return irs::get_mutable(_attrs, type);
  }

  irs::doc_id_t value() const final {
    return std::get<irs::attribute_ptr<irs::document>>(_attrs).ptr->value;
  }

  virtual bool next() override {
    for (;;) {
      if (!_approx->next()) {
        return false;
      }

      if (accept()) {
        return true;
      }
    }
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    auto* doc = std::get<irs::attribute_ptr<irs::document>>(_attrs).ptr;

    if (target <= doc->value) {
      return doc->value;
    }

    if (irs::doc_limits::eof(_approx->seek(target))) {
      return irs::doc_limits::eof();
    }

    if (!accept()) {
      next();
    }

    return doc->value;
  }

 private:
  bool accept() {
    auto* doc = std::get<irs::attribute_ptr<irs::document>>(_attrs).ptr;
    TRI_ASSERT(_columnIt->value() < doc->value);

    if (doc->value != _columnIt->seek(doc->value) ||
        _storedValue->value.empty()) {
      LOG_TOPIC("62a62", DEBUG, arangodb::iresearch::TOPIC)
          << "failed to find stored geo value, doc='" << doc->value << "'";
      return false;
    }

    if (!parseShape(slice(_storedValue->value), _shape, false)) {
      LOG_TOPIC("62a65", DEBUG, arangodb::iresearch::TOPIC)
          << "failed to parse stored geo value, value='"
          << slice(_storedValue->value).toHex() << ", doc='" << doc->value
          << "'";
      return false;
    }

    return _acceptor(_shape);
  }

  using attributes =
      std::tuple<irs::attribute_ptr<irs::document>, irs::cost, irs::score>;

  geo::ShapeContainer _shape;
  irs::doc_iterator::ptr _approx;
  irs::doc_iterator::ptr _columnIt;
  irs::payload const* _storedValue;
  attributes _attrs;
  Acceptor& _acceptor;
};

template<typename Acceptor>
irs::doc_iterator::ptr make_iterator(
    typename Disjunction::doc_iterators_t&& itrs,
    irs::doc_iterator::ptr&& columnIt, const irs::sub_reader& reader,
    const irs::term_reader& field, const irs::byte_type* query_stats,
    irs::Order const& order, irs::score_t boost, Acceptor& acceptor) {
  return irs::memory::make_managed<GeoIterator<Acceptor>>(
      irs::MakeDisjunction<Disjunction>(std::move(itrs), irs::NoopAggregator{}),
      std::move(columnIt), acceptor, reader, field, query_stats, order, boost);
}

// Cached per reader query state
struct GeoState {
  using TermState = irs::seek_cookie::ptr;

  // Corresponding stored field
  const irs::column_reader* storedField;

  // Reader using for iterate over the terms
  const irs::term_reader* reader;

  // Geo term states
  std::vector<irs::seek_cookie::ptr> states;
};

using GeoStates = irs::states_cache<GeoState>;

// Compiled GeoFilter
template<typename Acceptor>
class GeoQuery final : public irs::filter::prepared {
 public:
  GeoQuery(GeoStates&& states, irs::bstring&& stats, Acceptor&& acceptor,
           irs::score_t boost) noexcept
      : prepared(boost),
        _states(std::move(states)),
        _stats(std::move(stats)),
        _acceptor(std::move(acceptor)) {}

  irs::doc_iterator::ptr execute(
      irs::ExecutionContext const& ctx) const override {
    // get term state for the specified reader
    auto& segment = ctx.segment;
    auto state = _states.find(segment);

    if (!state) {
      // invalid state
      return irs::doc_iterator::empty();
    }

    auto field = state->reader;
    TRI_ASSERT(field);

    typename Disjunction::doc_iterators_t itrs;
    itrs.reserve(state->states.size());

    for (auto& entry : state->states) {
      assert(entry);
      auto docs = field->postings(*entry, irs::IndexFeatures::NONE);

      itrs.emplace_back(std::move(docs));

      if (IRS_UNLIKELY(!itrs.back().it)) {
        itrs.pop_back();

        continue;
      }
    }

    auto* reader = state->storedField;
    auto columnIt =
        reader ? reader->iterator(irs::ColumnHint::kNormal) : nullptr;

    if (!columnIt) {
      return irs::doc_iterator::empty();
    }

    return make_iterator(std::move(itrs), std::move(columnIt), segment,
                         *state->reader, _stats.c_str(), ctx.scorers, boost(),
                         _acceptor);
  }

  void visit(irs::sub_reader const&, irs::PreparedStateVisitor&,
             irs::score_t) const override {
    // NOOP
  }

 private:
  GeoStates _states;
  irs::bstring _stats;
  Acceptor _acceptor;
};

template<bool MinIncl, bool MaxIncl>
struct GeoDistanceRangeAcceptor {
  S2Cap min;
  S2Cap max;

  bool operator()(geo::ShapeContainer const& shape) const {
    auto const point = shape.centroid();

    return (MinIncl ? min.Contains(point) : min.InteriorContains(point)) &&
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
irs::filter::prepared::ptr make_query(GeoStates&& states, irs::bstring&& stats,
                                      irs::score_t boost, Acceptor&& acceptor) {
  return irs::memory::make_managed<GeoQuery<Acceptor>>(
      std::move(states), std::move(stats), std::move(acceptor), boost);
}

std::pair<GeoStates, irs::bstring> prepareStates(
    irs::index_reader const& index, irs::Order const& order,
    std::vector<std::string> const& geoTerms, irs::string_ref field) {
  assert(!geoTerms.empty());

  std::vector<std::string_view> sortedTerms(geoTerms.begin(), geoTerms.end());
  std::sort(sortedTerms.begin(), sortedTerms.end());

  std::pair<GeoStates, irs::bstring> res(
      std::piecewise_construct, std::forward_as_tuple(index),
      std::forward_as_tuple(order.stats_size(), 0));

  auto const size = sortedTerms.size();
  irs::field_collectors fieldStats(order);
  std::vector<GeoState::TermState> termStates;

  for (auto& segment : index) {
    auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    auto terms = reader->iterator(irs::SeekMode::NORMAL);

    if (IRS_UNLIKELY(!terms)) {
      continue;
    }

    fieldStats.collect(segment, *reader);
    termStates.reserve(size);

    for (auto& term : sortedTerms) {
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
    state.storedField = segment.column(field);
    termStates.clear();
  }

  fieldStats.finish(const_cast<irs::byte_type*>(res.second.data()), index);

  return res;
}

std::pair<S2Cap, bool> getBound(irs::BoundType type, S2Point origin,
                                double_t distance) {
  if (irs::BoundType::UNBOUNDED == type) {
    return {S2Cap::Full(), true};
  }

  return {
      (0. == distance
           ? fromPoint(origin)
           : S2Cap(origin, S1Angle::Radians(geo::metersToRadians(distance)))),
      irs::BoundType::INCLUSIVE == type};
}

irs::filter::prepared::ptr prepareOpenInterval(
    irs::index_reader const& index, irs::Order const& order, irs::score_t boost,
    irs::string_ref field, GeoDistanceFilterOptions const& opts, bool greater) {
  auto const& range = opts.range;
  auto const& origin = opts.origin;

  auto const [dist, type] =
      greater ? std::forward_as_tuple(range.min, range.min_type)
              : std::forward_as_tuple(range.max, range.max_type);

  S2Cap bound;
  // the actual initialization value does not matter. the proper
  // value for incl will be set below. the initialization is here
  // just to please the compiler, which may otherwise warn about
  // uninitialized values
  bool incl = false;

  if (dist < 0.) {
    bound = greater ? S2Cap::Full() : S2Cap::Empty();
  } else if (0. == dist) {
    switch (type) {
      case irs::BoundType::UNBOUNDED:
        incl = false;
        TRI_ASSERT(false);
        break;
      case irs::BoundType::INCLUSIVE:
        bound = greater ? S2Cap::Full() : fromPoint(origin);

        if (!bound.is_valid()) {
          return irs::filter::prepared::empty();
        }

        incl = true;
        break;
      case irs::BoundType::EXCLUSIVE:
        if (greater) {
          // a full cap without a center
          irs::And root;
          {
            auto& incl = root.add<irs::by_column_existence>();
            *incl.mutable_field() = field;
          }
          {
            auto& excl = root.add<irs::Not>().filter<GeoDistanceFilter>();
            *excl.mutable_field() = field;
            auto* options = excl.mutable_options();
            options->prefix = opts.prefix;
            options->options = opts.options;
            options->range.min = 0;
            options->range.min_type = irs::BoundType::INCLUSIVE;
            options->range.max = 0;
            options->range.max_type = irs::BoundType::INCLUSIVE;
            options->origin = origin;
          }

          return root.prepare(index, order, boost);
        } else {
          bound = S2Cap::Empty();
        }

        incl = false;
        break;
    }
  } else {
    std::tie(bound, incl) = getBound(type, origin, dist);

    if (!bound.is_valid()) {
      return irs::filter::prepared::empty();
    }

    if (greater) {
      bound = bound.Complement();
    }
  }

  TRI_ASSERT(bound.is_valid());

  if (bound.is_full()) {
    return match_all(index, order, field, boost);
  }

  if (bound.is_empty()) {
    return irs::filter::prepared::empty();
  }

  S2RegionTermIndexer indexer(opts.options);

  auto const geoTerms = indexer.GetQueryTerms(bound, opts.prefix);

  if (geoTerms.empty()) {
    return irs::filter::prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field);

  if (incl) {
    return ::make_query(std::move(states), std::move(stats), boost,
                        GeoDistanceAcceptor<true>{bound});
  } else {
    return ::make_query(std::move(states), std::move(stats), boost,
                        GeoDistanceAcceptor<false>{bound});
  }
}

irs::filter::prepared::ptr prepareInterval(
    irs::index_reader const& index, irs::Order const& order, irs::score_t boost,
    irs::string_ref field, GeoDistanceFilterOptions const& opts) {
  auto const& range = opts.range;
  TRI_ASSERT(irs::BoundType::UNBOUNDED != range.min_type);
  TRI_ASSERT(irs::BoundType::UNBOUNDED != range.max_type);

  if (range.max < 0.) {
    return irs::filter::prepared::empty();
  } else if (range.min < 0.) {
    return ::prepareOpenInterval(index, order, boost, field, opts, false);
  }

  if (irs::math::approx_equals(range.min, range.max)) {
    if (irs::BoundType::INCLUSIVE != range.min_type ||
        irs::BoundType::INCLUSIVE != range.max_type) {
      return irs::filter::prepared::empty();
    }
  }

  auto const& origin = opts.origin;

  if (0. == range.max && 0. == range.min) {
    TRI_ASSERT(irs::BoundType::INCLUSIVE == range.min_type);
    TRI_ASSERT(irs::BoundType::INCLUSIVE == range.max_type);

    S2RegionTermIndexer indexer(opts.options);
    auto const geoTerms = indexer.GetQueryTerms(origin, opts.prefix);

    if (geoTerms.empty()) {
      return irs::filter::prepared::empty();
    }

    auto [states, stats] = prepareStates(index, order, geoTerms, field);

    return ::make_query(
        std::move(states), std::move(stats), boost,
        [bound = fromPoint(origin)](geo::ShapeContainer const& shape) {
          return bound.InteriorContains(shape.centroid());
        });
  }

  auto [minBound, minIncl] = getBound(range.min_type, origin, range.min);
  auto [maxBound, maxIncl] = getBound(range.max_type, origin, range.max);

  if (!minBound.is_valid() || !maxBound.is_valid()) {
    return irs::filter::prepared::empty();
  }

  // complement is not bijective => complement of singleton cap is an empty cap
  minBound = minBound.Complement();

  if ((maxIncl && !maxBound.Intersects(minBound)) ||
      (!maxIncl && !maxBound.InteriorIntersects(minBound))) {
    return irs::filter::prepared::empty();
  }

  TRI_ASSERT(!minBound.is_empty());
  TRI_ASSERT(!maxBound.is_empty());

  S2RegionTermIndexer indexer(opts.options);
  S2RegionCoverer coverer(opts.options);

  auto const ring =
      coverer.GetCovering(maxBound).Intersection(coverer.GetCovering(minBound));
  auto const geoTerms =
      indexer.GetQueryTermsForCanonicalCovering(ring, opts.prefix);

  if (geoTerms.empty()) {
    return irs::filter::prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field);

  switch (size_t(minIncl) + 2 * size_t(maxIncl)) {
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

}  // namespace

namespace arangodb {
namespace iresearch {

irs::filter::prepared::ptr GeoFilter::prepare(
    irs::index_reader const& index, irs::Order const& order, irs::score_t boost,
    irs::attribute_provider const* /*ctx*/) const {
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

irs::filter::prepared::ptr GeoDistanceFilter::prepare(
    irs::index_reader const& index, irs::Order const& order, irs::score_t boost,
    irs::attribute_provider const* /*ctx*/) const {
  auto const& range = options().range;
  auto const lowerBound = irs::BoundType::UNBOUNDED != range.min_type;
  auto const upperBound = irs::BoundType::UNBOUNDED != range.max_type;

  boost *= this->boost();

  if (!lowerBound && !upperBound) {
    return match_all(index, order, field(), boost);
  }
  if (lowerBound && upperBound) {
    return ::prepareInterval(index, order, boost, field(), options());
  } else {
    return ::prepareOpenInterval(index, order, boost, field(), options(),
                                 lowerBound);
  }
}

}  // namespace iresearch
}  // namespace arangodb
