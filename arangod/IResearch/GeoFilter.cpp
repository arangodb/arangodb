////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/DownCast.h"

namespace arangodb::iresearch {
namespace {

// assume up to 2x machine epsilon in precision errors for singleton caps
constexpr auto kSingletonCapEps = 2 * std::numeric_limits<double>::epsilon();

using Disjunction =
    irs::disjunction_iterator<irs::doc_iterator::ptr, irs::NoopAggregator>;

// Return a filter matching all documents with a given geo field
irs::filter::prepared::ptr match_all(irs::IndexReader const& index,
                                     irs::Scorers const& order,
                                     std::string_view field,
                                     irs::score_t boost) {
  // Return everything we've stored
  irs::by_column_existence filter;
  *filter.mutable_field() = field;

  return filter.prepare(index, order, boost);
}

// Returns singleton S2Cap that tolerates precision errors
// TODO(MBkkt) Probably remove it
inline S2Cap fromPoint(S2Point const& origin) noexcept {
  return S2Cap{origin, S1Angle::Radians(kSingletonCapEps)};
}

inline S2Cap fromPoint(S2Point origin, double distance) noexcept {
  return {origin, S1Angle::Radians(geo::metersToRadians(distance))};
}

struct S2PointParser;

template<typename Parser, typename Acceptor>
class GeoIterator : public irs::doc_iterator {
  // Two phase iterator is heavier than a usual disjunction
  static constexpr irs::cost::cost_t kExtraCost = 2;

 public:
  GeoIterator(doc_iterator::ptr&& approx, doc_iterator::ptr&& columnIt,
              Parser& parser, Acceptor& acceptor, irs::SubReader const& reader,
              irs::term_reader const& field, irs::byte_type const* query_stats,
              irs::Scorers const& order, irs::score_t boost)
      : _approx{std::move(approx)},
        _columnIt{std::move(columnIt)},
        _storedValue{irs::get<irs::payload>(*_columnIt)},
        _parser{parser},
        _acceptor{acceptor} {
    std::get<irs::attribute_ptr<irs::document>>(_attrs) =
        irs::get_mutable<irs::document>(_approx.get());

    std::get<irs::cost>(_attrs).reset(
        [&]() noexcept { return kExtraCost * irs::cost::extract(*_approx); });

    if (!order.empty()) {
      auto& score = std::get<irs::score>(_attrs);
      score = irs::CompileScore(order.buckets(), reader, field, query_stats,
                                *this, boost);
    }
    if constexpr (std::is_same_v<std::decay_t<Parser>, S2PointParser>) {
      // random, stub value but it should be unit length because assert
      _shape.reset(S2Point{1, 0, 0});
    }
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(_attrs, type);
  }

  irs::doc_id_t value() const final {
    return std::get<irs::attribute_ptr<irs::document>>(_attrs).ptr->value;
  }

  bool next() final {
    for (;;) {
      if (!_approx->next()) {
        return false;
      }

      if (accept()) {
        return true;
      }
    }
  }

  irs::doc_id_t seek(irs::doc_id_t target) final {
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
      LOG_TOPIC("62a62", DEBUG, TOPIC)
          << "failed to find stored geo value, doc='" << doc->value << "'";
      return false;
    }
    return _parser(_storedValue->value, _shape) && _acceptor(_shape);
  }

  using Attributes =
      std::tuple<irs::attribute_ptr<irs::document>, irs::cost, irs::score>;

  geo::ShapeContainer _shape;
  irs::doc_iterator::ptr _approx;
  irs::doc_iterator::ptr _columnIt;
  irs::payload const* _storedValue;
  Attributes _attrs;
  Parser& _parser;
  Acceptor& _acceptor;
};

template<typename Parser, typename Acceptor>
irs::doc_iterator::ptr makeIterator(
    typename Disjunction::doc_iterators_t&& itrs,
    irs::doc_iterator::ptr&& columnIt, irs::SubReader const& reader,
    irs::term_reader const& field, irs::byte_type const* query_stats,
    irs::Scorers const& order, irs::score_t boost, Parser& parser,
    Acceptor& acceptor) {
  if (ADB_UNLIKELY(itrs.empty() || !columnIt)) {
    return irs::doc_iterator::empty();
  }

  return irs::memory::make_managed<GeoIterator<Parser, Acceptor>>(
      irs::MakeDisjunction<Disjunction>(std::move(itrs), irs::NoopAggregator{}),
      std::move(columnIt), parser, acceptor, reader, field, query_stats, order,
      boost);
}

// Cached per reader query state
struct GeoState {
  // Corresponding stored field
  irs::column_reader const* storedField{};

  // Reader using for iterate over the terms
  irs::term_reader const* reader{};

  // Geo term states
  std::vector<irs::seek_cookie::ptr> states;
};

using GeoStates = irs::StatesCache<GeoState>;

// Compiled GeoFilter
template<typename Parser, typename Acceptor>
class GeoQuery : public irs::filter::prepared {
 public:
  GeoQuery(GeoStates&& states, irs::bstring&& stats, Parser&& parser,
           Acceptor&& acceptor, irs::score_t boost) noexcept
      : prepared{boost},
        _states{std::move(states)},
        _stats{std::move(stats)},
        _parser{std::move(parser)},
        _acceptor{std::move(acceptor)} {}

  irs::doc_iterator::ptr execute(irs::ExecutionContext const& ctx) const final {
    // get term state for the specified reader
    auto& segment = ctx.segment;
    auto const* state = _states.find(segment);

    if (!state) {
      // invalid state
      return irs::doc_iterator::empty();
    }

    auto* field = state->reader;
    TRI_ASSERT(field);

    typename Disjunction::doc_iterators_t itrs;
    itrs.reserve(state->states.size());

    for (auto& entry : state->states) {
      TRI_ASSERT(entry);
      auto& it =
          itrs.emplace_back(field->postings(*entry, irs::IndexFeatures::NONE));

      if (ADB_UNLIKELY(!it || irs::doc_limits::eof(it->value()))) {
        itrs.pop_back();

        continue;
      }
    }

    auto columnIt = state->storedField->iterator(irs::ColumnHint::kNormal);

    return makeIterator(std::move(itrs), std::move(columnIt), segment,
                        *state->reader, _stats.c_str(), ctx.scorers, boost(),
                        _parser, _acceptor);
  }

  void visit(irs::SubReader const&, irs::PreparedStateVisitor&,
             irs::score_t) const final {
    // NOOP
  }

 private:
  GeoStates _states;
  irs::bstring _stats;
  IRS_NO_UNIQUE_ADDRESS Parser _parser;
  IRS_NO_UNIQUE_ADDRESS Acceptor _acceptor;
};

struct VPackParser {
  explicit VPackParser(bool legacy) : _legacy{legacy} {}

  bool operator()(irs::bytes_view value, geo::ShapeContainer& shape) const {
    TRI_ASSERT(!value.empty());
    return parseShape<Parsing::FromIndex>(slice(value), shape, _cache, _legacy,
                                          geo::coding::Options::kInvalid,
                                          nullptr);
  }

 private:
  mutable std::vector<S2LatLng> _cache;
  bool _legacy;
};

struct S2ShapeParser {
  bool operator()(irs::bytes_view value, geo::ShapeContainer& shape) const {
    TRI_ASSERT(!value.empty());
    Decoder decoder{value.data(), value.size()};
    auto r = shape.Decode(decoder, _cache);
    TRI_ASSERT(r);
    TRI_ASSERT(decoder.avail() == 0);
    return r;
  }

 private:
  mutable std::vector<S2Point> _cache;
};

struct S2PointParser {
  bool operator()(irs::bytes_view value, geo::ShapeContainer& shape) const {
    TRI_ASSERT(!value.empty());
    TRI_ASSERT(shape.type() == geo::ShapeContainer::Type::S2_POINT);
    Decoder decoder{value.data(), value.size()};
    S2Point point;
    uint8_t tag = 0;
    auto r = geo::decodePoint(decoder, point, &tag);
    TRI_ASSERT(r);
    TRI_ASSERT(decoder.avail() == 0);
    basics::downCast<S2PointRegion>(*shape.region()) = S2PointRegion{point};
    shape.setCoding(
        static_cast<geo::coding::Options>(geo::coding::toPoint(tag)));
    return r;
  }
};

// TODO(MBkkt) S2LaxShapeParser

template<bool MinIncl, bool MaxIncl>
struct GeoDistanceRangeAcceptor {
  S2Cap min;
  S2Cap max;

  bool operator()(geo::ShapeContainer const& shape) const {
    auto const point = shape.centroid();

    return !(MinIncl ? min.InteriorContains(point) : min.Contains(point)) &&
           (MaxIncl ? max.Contains(point) : max.InteriorContains(point));
  }
};

template<bool Incl>
struct GeoDistanceAcceptor {
  S2Cap filter;

  bool operator()(geo::ShapeContainer const& shape) const {
    auto const point = shape.centroid();

    return Incl ? filter.Contains(point) : filter.InteriorContains(point);
  }
};

template<typename Options, typename Acceptor>
irs::filter::prepared::ptr makeQuery(GeoStates&& states, irs::bstring&& stats,
                                     irs::score_t boost, Options const& options,
                                     Acceptor&& acceptor) {
  bool legacy = false;
  switch (options.stored) {
    case StoredType::VPackLegacy:
      legacy = true;
      [[fallthrough]];
    case StoredType::VPack:
      return irs::memory::make_managed<GeoQuery<VPackParser, Acceptor>>(
          std::move(states), std::move(stats), VPackParser{legacy},
          std::forward<Acceptor>(acceptor), boost);
    case StoredType::S2Region:
      return irs::memory::make_managed<GeoQuery<S2ShapeParser, Acceptor>>(
          std::move(states), std::move(stats), S2ShapeParser{},
          std::forward<Acceptor>(acceptor), boost);
    case StoredType::S2Point:
    case StoredType::S2Centroid:
      return irs::memory::make_managed<GeoQuery<S2PointParser, Acceptor>>(
          std::move(states), std::move(stats), S2PointParser{},
          std::forward<Acceptor>(acceptor), boost);
  }
  TRI_ASSERT(false);
  return irs::filter::prepared::empty();
}

std::pair<GeoStates, irs::bstring> prepareStates(
    irs::IndexReader const& index, irs::Scorers const& order,
    std::span<const std::string> geoTerms, std::string_view field) {
  TRI_ASSERT(!geoTerms.empty());

  std::vector<std::string_view> sortedTerms(geoTerms.begin(), geoTerms.end());
  std::sort(sortedTerms.begin(), sortedTerms.end());
  TRI_ASSERT(std::unique(sortedTerms.begin(), sortedTerms.end()) ==
             sortedTerms.end());

  std::pair<GeoStates, irs::bstring> res{
      std::piecewise_construct, std::forward_as_tuple(index.size()),
      std::forward_as_tuple(order.stats_size(), 0)};

  auto const size = sortedTerms.size();
  irs::field_collectors fieldStats{order};
  std::vector<irs::seek_cookie::ptr> termStates;

  for (auto const& segment : index) {
    auto const* reader = segment.field(field);
    if (!reader) {
      continue;
    }
    auto const* storedField = segment.column(field);
    if (!storedField) {
      continue;
    }
    auto terms = reader->iterator(irs::SeekMode::NORMAL);
    if (IRS_UNLIKELY(!terms)) {
      continue;
    }

    fieldStats.collect(segment, *reader);
    termStates.reserve(size);

    for (auto const term : sortedTerms) {
      if (!terms->seek(irs::ViewCast<irs::byte_type>(term))) {
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
    state.storedField = storedField;
    termStates.clear();
  }

  fieldStats.finish(const_cast<irs::byte_type*>(res.second.data()));

  return res;
}

std::pair<S2Cap, bool> getBound(irs::BoundType type, S2Point origin,
                                double distance) {
  if (irs::BoundType::UNBOUNDED == type) {
    return {S2Cap::Full(), true};
  }

  return {(0. == distance ? fromPoint(origin) : fromPoint(origin, distance)),
          irs::BoundType::INCLUSIVE == type};
}

irs::filter::prepared::ptr prepareOpenInterval(
    irs::IndexReader const& index, irs::Scorers const& order,
    irs::score_t boost, std::string_view field,
    GeoDistanceFilterOptions const& options, bool greater) {
  auto const& range = options.range;
  auto const& origin = options.origin;

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
            auto& column = root.add<irs::by_column_existence>();
            *column.mutable_field() = field;
          }
          {
            auto& excl = root.add<irs::Not>().filter<GeoDistanceFilter>();
            *excl.mutable_field() = field;
            auto& opts = *excl.mutable_options();
            opts = options;
            opts.range.min = 0;
            opts.range.min_type = irs::BoundType::INCLUSIVE;
            opts.range.max = 0;
            opts.range.max_type = irs::BoundType::INCLUSIVE;
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

  S2RegionTermIndexer indexer(options.options);

  auto const geoTerms = indexer.GetQueryTerms(bound, options.prefix);

  if (geoTerms.empty()) {
    return irs::filter::prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field);

  if (incl) {
    return makeQuery(std::move(states), std::move(stats), boost, options,
                     GeoDistanceAcceptor<true>{bound});
  } else {
    return makeQuery(std::move(states), std::move(stats), boost, options,
                     GeoDistanceAcceptor<false>{bound});
  }
}

irs::filter::prepared::ptr prepareInterval(
    irs::IndexReader const& index, irs::Scorers const& order,
    irs::score_t boost, std::string_view field,
    GeoDistanceFilterOptions const& options) {
  auto const& range = options.range;
  TRI_ASSERT(irs::BoundType::UNBOUNDED != range.min_type);
  TRI_ASSERT(irs::BoundType::UNBOUNDED != range.max_type);

  if (range.max < 0.) {
    return irs::filter::prepared::empty();
  } else if (range.min < 0.) {
    return prepareOpenInterval(index, order, boost, field, options, false);
  }

  bool const minIncl = range.min_type == irs::BoundType::INCLUSIVE;
  bool const maxIncl = range.max_type == irs::BoundType::INCLUSIVE;

  if (irs::math::approx_equals(range.min, range.max)) {
    if (!minIncl || !maxIncl) {
      return irs::filter::prepared::empty();
    }
  } else if (range.min > range.max) {
    return irs::filter::prepared::empty();
  }

  auto const& origin = options.origin;

  if (0. == range.max && 0. == range.min) {
    TRI_ASSERT(minIncl);
    TRI_ASSERT(maxIncl);

    S2RegionTermIndexer indexer(options.options);
    auto const geoTerms = indexer.GetQueryTerms(origin, options.prefix);

    if (geoTerms.empty()) {
      return irs::filter::prepared::empty();
    }

    auto [states, stats] = prepareStates(index, order, geoTerms, field);

    return makeQuery(
        std::move(states), std::move(stats), boost, options,
        [bound = fromPoint(origin)](geo::ShapeContainer const& shape) {
          return bound.InteriorContains(shape.centroid());
        });
  }

  auto minBound = fromPoint(origin, range.min);
  auto maxBound = fromPoint(origin, range.max);

  if (!minBound.is_valid() || !maxBound.is_valid()) {
    return irs::filter::prepared::empty();
  }

  S2RegionTermIndexer indexer(options.options);
  S2RegionCoverer coverer(options.options);

  TRI_ASSERT(!minBound.is_empty());
  TRI_ASSERT(!maxBound.is_empty());

  auto const ring = coverer.GetCovering(maxBound).Difference(
      coverer.GetInteriorCovering(minBound));
  auto const geoTerms =
      indexer.GetQueryTermsForCanonicalCovering(ring, options.prefix);

  if (geoTerms.empty()) {
    return irs::filter::prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field);

  switch (size_t(minIncl) + 2 * size_t(maxIncl)) {
    case 0:
      return makeQuery(
          std::move(states), std::move(stats), boost, options,
          GeoDistanceRangeAcceptor<false, false>{minBound, maxBound});
    case 1:
      return makeQuery(
          std::move(states), std::move(stats), boost, options,
          GeoDistanceRangeAcceptor<true, false>{minBound, maxBound});
    case 2:
      return makeQuery(
          std::move(states), std::move(stats), boost, options,
          GeoDistanceRangeAcceptor<false, true>{minBound, maxBound});
    case 3:
      return makeQuery(
          std::move(states), std::move(stats), boost, options,
          GeoDistanceRangeAcceptor<true, true>{minBound, maxBound});
    default:
      TRI_ASSERT(false);
      return irs::filter::prepared::empty();
  }
}

}  // namespace

irs::filter::prepared::ptr GeoFilter::prepare(
    irs::IndexReader const& index, irs::Scorers const& order,
    irs::score_t boost, irs::attribute_provider const* /*ctx*/) const {
  auto& shape = const_cast<geo::ShapeContainer&>(options().shape);
  if (shape.empty()) {
    return prepared::empty();
  }

  auto const& options = this->options();

  S2RegionTermIndexer indexer{options.options};
  std::vector<std::string> geoTerms;
  auto const type = shape.type();
  if (type == geo::ShapeContainer::Type::S2_POINT) {
    auto const& region = basics::downCast<S2PointRegion>(*shape.region());
    geoTerms = indexer.GetQueryTerms(region.point(), options.prefix);
  } else {
    geoTerms = indexer.GetQueryTerms(*shape.region(), {});
  }

  if (geoTerms.empty()) {
    return prepared::empty();
  }

  auto [states, stats] = prepareStates(index, order, geoTerms, field());

  boost *= this->boost();

  switch (options.type) {
    case GeoFilterType::INTERSECTS:
      return makeQuery(std::move(states), std::move(stats), boost, options,
                       [filterShape = std::move(shape)](
                           geo::ShapeContainer const& indexedShape) {
                         return filterShape.intersects(indexedShape);
                       });
    case GeoFilterType::CONTAINS:
      return makeQuery(std::move(states), std::move(stats), boost, options,
                       [filterShape = std::move(shape)](
                           geo::ShapeContainer const& indexedShape) {
                         return filterShape.contains(indexedShape);
                       });
    case GeoFilterType::IS_CONTAINED:
      return makeQuery(std::move(states), std::move(stats), boost, options,
                       [filterShape = std::move(shape)](
                           geo::ShapeContainer const& indexedShape) {
                         return indexedShape.contains(filterShape);
                       });
  }
  TRI_ASSERT(false);
  return prepared::empty();
}

irs::filter::prepared::ptr GeoDistanceFilter::prepare(
    irs::IndexReader const& index, irs::Scorers const& order,
    irs::score_t boost, irs::attribute_provider const* /*ctx*/) const {
  auto const& options = this->options();
  auto const& range = options.range;
  auto const lowerBound = irs::BoundType::UNBOUNDED != range.min_type;
  auto const upperBound = irs::BoundType::UNBOUNDED != range.max_type;

  boost *= this->boost();

  if (!lowerBound && !upperBound) {
    return match_all(index, order, field(), boost);
  }
  if (lowerBound && upperBound) {
    return prepareInterval(index, order, boost, field(), options);
  } else {
    return prepareOpenInterval(index, order, boost, field(), options,
                               lowerBound);
  }
}

}  // namespace arangodb::iresearch
