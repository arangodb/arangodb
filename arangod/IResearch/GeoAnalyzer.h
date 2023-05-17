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

#pragma once

#include <s2/s2region_term_indexer.h>
#include <s2/s2latlng.h>
#include <s2/util/coding/coder.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/analyzer.hpp"
#include "utils/attribute_helper.hpp"

#include "Geo/ShapeContainer.h"
#include "IResearch/Geo.h"

namespace arangodb::iresearch {

struct GeoFilterOptionsBase;

class GeoAnalyzer : public irs::analysis::analyzer,
                    private irs::util::noncopyable {
 public:
  bool next() noexcept final;
  using irs::analysis::analyzer::reset;

  virtual void prepare(GeoFilterOptionsBase& options) const = 0;

  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return irs::get_mutable(_attrs, id);
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  auto const& options() const noexcept { return _indexer.options(); }
#endif

 protected:
  explicit GeoAnalyzer(irs::type_info const& type,
                       S2RegionTermIndexer::Options const& options);
  void reset(std::vector<std::string>&& terms) noexcept;
  void reset() noexcept {
    _begin = _terms.data();
    _end = _begin;
  }

  S2RegionTermIndexer _indexer;
  // We already have S2RegionCoverer in S2RegionTermIndexer
  // but it's private. TODO(MBkkt) make PR to s2
  S2RegionCoverer _coverer;

 private:
  using attributes = std::tuple<irs::increment, irs::term_attribute>;

  std::vector<std::string> _terms;
  std::string const* _begin{_terms.data()};
  std::string const* _end{_begin};
  irs::offset _offset;
  attributes _attrs;
};

////////////////////////////////////////////////////////////////////////////////
/// @class GeoPointAnalyzer
/// @brief an analyzer capable of breaking up a valid geo point input into a set
///        of tokens for further indexing
////////////////////////////////////////////////////////////////////////////////
class GeoPointAnalyzer final : public GeoAnalyzer {
 public:
  struct Options {
    GeoOptions options;
    std::vector<std::string> latitude;
    std::vector<std::string> longitude;
  };

  static constexpr std::string_view type_name() noexcept { return "geopoint"; }
  static bool normalize(std::string_view args, std::string& out);
  static irs::analysis::analyzer::ptr make(std::string_view args);

  // store point as [lng, lat] array to be GeoJSON compliant
  static irs::bytes_view store(irs::token_stream* ctx, velocypack::Slice slice);

  explicit GeoPointAnalyzer(Options const& options);

  irs::type_info::type_id type() const noexcept final {
    return irs::type<GeoPointAnalyzer>::id();
  }

  bool reset(std::string_view value) final;

  void prepare(GeoFilterOptionsBase& options) const final;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  auto const& latitude() const noexcept { return _latitude; }
  auto const& longitude() const noexcept { return _longitude; }
#endif

 private:
  bool parsePoint(VPackSlice slice, S2LatLng& out) const;

  S2LatLng _point;
  bool _fromArray;
  std::vector<std::string> _latitude;
  std::vector<std::string> _longitude;
  velocypack::Builder _builder;
};

class GeoJsonAnalyzerBase : public GeoAnalyzer {
 public:
  enum class Type : uint8_t {
    // analyzer accepts any valid GeoJson input
    // and produces tokens denoting an approximation for a given shape
    SHAPE = 0,
    // analyzer accepts any valid GeoJson shape
    // but produces tokens denoting a centroid of a given shape
    CENTROID,
    // analyzer accepts points only
    POINT,
  };

  struct OptionsBase {
    GeoOptions options;
    Type type{Type::SHAPE};
  };

#ifdef ARANGODB_USE_GOOGLE_TESTS
  auto shapeType() const noexcept { return _type; }
#endif

 protected:
  explicit GeoJsonAnalyzerBase(irs::type_info const& type,
                               OptionsBase const& options);

  bool resetImpl(std::string_view value, bool legacy,
                 geo::coding::Options options, Encoder* encoder);

  geo::ShapeContainer _shape;
  S2Point _centroid;
  std::vector<S2LatLng> _cache;
  Type _type;
};

/// The analyzer capable of breaking up a valid GeoJson input
/// into a set of tokens for further indexing. Stores vpack.
class GeoVPackAnalyzer final : public GeoJsonAnalyzerBase {
 public:
  struct Options : OptionsBase {
    bool legacy{false};
  };

  static constexpr std::string_view type_name() noexcept { return "geojson"; }
  static bool normalize(std::string_view args, std::string& out);
  static irs::analysis::analyzer::ptr make(std::string_view args);

  static irs::bytes_view store(irs::token_stream* ctx, velocypack::Slice slice);

  explicit GeoVPackAnalyzer(Options const& opts);

  irs::type_info::type_id type() const noexcept final {
    return irs::type<GeoVPackAnalyzer>::id();
  }

  bool reset(std::string_view value) final;

  void prepare(GeoFilterOptionsBase& options) const final;

 private:
  velocypack::Builder _builder;
  bool _legacy{false};
};

Result fromVelocyPackBase(velocypack::Slice object,
                          GeoJsonAnalyzerBase::OptionsBase& options);
void toVelocyPackBase(velocypack::Builder& builder,
                      GeoJsonAnalyzerBase::OptionsBase const& options);

void toVelocyPack(velocypack::Builder& builder,
                  GeoPointAnalyzer::Options const& options);
void toVelocyPack(velocypack::Builder& builder,
                  GeoVPackAnalyzer::Options const& options);

}  // namespace arangodb::iresearch
