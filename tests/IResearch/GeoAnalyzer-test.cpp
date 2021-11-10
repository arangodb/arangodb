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

#include <gtest/gtest.h>

#include <s2/s2point_region.h>

#include "IResearch/common.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/VelocyPackHelper.h"
#include "Geo/GeoJson.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace arangodb::iresearch;

// -----------------------------------------------------------------------------
// --SECTION--                                             GeoOptions test suite
// -----------------------------------------------------------------------------

TEST(GeoOptionsTest, constants) {
  static_assert(S2RegionCoverer::Options::kDefaultMaxCells == GeoOptions::MAX_CELLS);
  static_assert(0 == GeoOptions::MIN_LEVEL);
  static_assert(S2CellId::kMaxLevel == GeoOptions::MAX_LEVEL);
  static_assert(20 == GeoOptions::DEFAULT_MAX_CELLS);
  static_assert(4 == GeoOptions::DEFAULT_MIN_LEVEL);
  static_assert(23 == GeoOptions::DEFAULT_MAX_LEVEL);
}

TEST(GeoOptionsTest, options) {
  GeoOptions opts;
  ASSERT_EQ(GeoOptions::DEFAULT_MAX_CELLS, opts.maxCells);
  ASSERT_EQ(GeoOptions::DEFAULT_MIN_LEVEL, opts.minLevel);
  ASSERT_EQ(GeoOptions::DEFAULT_MAX_LEVEL, opts.maxLevel);
}

// -----------------------------------------------------------------------------
// --SECTION--                                       GeoPointAnalyzer test suite
// -----------------------------------------------------------------------------

TEST(GeoPointAnalyzerTest, constants) {
  static_assert("geopoint" == GeoPointAnalyzer::type_name());
}

TEST(GeoPointAnalyzerTest, options) {
  GeoPointAnalyzer::Options opts;
  ASSERT_TRUE(opts.latitude.empty());
  ASSERT_TRUE(opts.longitude.empty());
  ASSERT_EQ(GeoOptions{}.maxCells, opts.options.maxCells);
  ASSERT_EQ(GeoOptions{}.minLevel, opts.options.minLevel);
  ASSERT_EQ(GeoOptions{}.maxLevel, opts.options.maxLevel);
}

TEST(GeoPointAnalyzerTest, prepareQuery) {
  {
    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    opts.latitude = {"foo"};
    opts.latitude = {"bar"};
    GeoPointAnalyzer a(opts);

    S2RegionTermIndexer::Options s2opts;
    a.prepare(s2opts);

    ASSERT_EQ(1, s2opts.level_mod());
    ASSERT_FALSE(s2opts.optimize_for_space());
    ASSERT_EQ("$", s2opts.marker());
    ASSERT_EQ(opts.options.minLevel, s2opts.min_level());
    ASSERT_EQ(opts.options.maxLevel, s2opts.max_level());
    ASSERT_EQ(opts.options.maxCells, s2opts.max_cells());
    ASSERT_TRUE(s2opts.index_contains_points_only());
  }

  {
    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    GeoPointAnalyzer a(opts);

    S2RegionTermIndexer::Options s2opts;
    a.prepare(s2opts);

    ASSERT_EQ(1, s2opts.level_mod());
    ASSERT_FALSE(s2opts.optimize_for_space());
    ASSERT_EQ("$", s2opts.marker());
    ASSERT_EQ(opts.options.minLevel, s2opts.min_level());
    ASSERT_EQ(opts.options.maxLevel, s2opts.max_level());
    ASSERT_EQ(opts.options.maxCells, s2opts.max_cells());
    ASSERT_TRUE(s2opts.index_contains_points_only());
  }
}

TEST(GeoPointAnalyzerTest, ctor) {
  {
    GeoPointAnalyzer::Options opts;
    GeoPointAnalyzer a(opts);
    ASSERT_TRUE(opts.latitude.empty());
    ASSERT_TRUE(opts.longitude.empty());
    {
      auto* inc = irs::get<irs::increment>(a);
      ASSERT_NE(nullptr, inc);
      ASSERT_EQ(1, inc->value);
    }
    {
      auto* term = irs::get<irs::term_attribute>(a);
      ASSERT_NE(nullptr, term);
      ASSERT_TRUE(term->value.null());
    }
    ASSERT_EQ(irs::type<GeoPointAnalyzer>::id(), a.type());
    ASSERT_FALSE(a.next());

  }

  {
    GeoPointAnalyzer::Options opts;
    opts.latitude = {"foo"};
    GeoPointAnalyzer a(opts);
    ASSERT_TRUE(a.latitude().empty());
    ASSERT_TRUE(a.longitude().empty());
    {
      auto* inc = irs::get<irs::increment>(a);
      ASSERT_NE(nullptr, inc);
      ASSERT_EQ(1, inc->value);
    }
    {
      auto* term = irs::get<irs::term_attribute>(a);
      ASSERT_NE(nullptr, term);
      ASSERT_TRUE(term->value.null());
    }
    ASSERT_EQ(irs::type<GeoPointAnalyzer>::id(), a.type());
    ASSERT_FALSE(a.next());
  }

  {
    GeoPointAnalyzer::Options opts;
    opts.latitude = {"foo"};
    opts.longitude = {"bar"};
    GeoPointAnalyzer a(opts);
    ASSERT_EQ(std::vector<std::string>{"foo"}, a.latitude());
    ASSERT_EQ(std::vector<std::string>{"bar"}, a.longitude());
    {
      auto* inc = irs::get<irs::increment>(a);
      ASSERT_NE(nullptr, inc);
      ASSERT_EQ(1, inc->value);
    }
    {
      auto* term = irs::get<irs::term_attribute>(a);
      ASSERT_NE(nullptr, term);
      ASSERT_TRUE(term->value.null());
    }
    ASSERT_EQ(irs::type<GeoPointAnalyzer>::id(), a.type());
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoPointAnalyzerTest, tokenizePointFromArray) {
  auto json = VPackParser::fromJson(R"([ 63.57789956676574, 53.72314453125 ])");

  geo::ShapeContainer shape;
  ASSERT_TRUE(shape.parseCoordinates(json->slice(), false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize point
  {
    GeoPointAnalyzer::Options opts;
    GeoPointAnalyzer a(opts);
    ASSERT_TRUE(a.latitude().empty());
    ASSERT_TRUE(a.longitude().empty());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point, custom options
  {
    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoPointAnalyzer a(opts);
    ASSERT_TRUE(a.latitude().empty());
    ASSERT_TRUE(a.longitude().empty());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoPointAnalyzerTest, tokenizePointFromObject) {
  auto json = VPackParser::fromJson(R"([ 63.57789956676574, 53.72314453125 ])");
  auto jsonObject = VPackParser::fromJson(R"({ "lat": 63.57789956676574, "lon": 53.72314453125 })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(shape.parseCoordinates(json->slice(), false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize point
  {
    GeoPointAnalyzer::Options opts;
    opts.latitude = {"lat"};
    opts.longitude = {"lon"};
    GeoPointAnalyzer a(opts);
    ASSERT_EQ(std::vector<std::string>{"lat"}, a.latitude());
    ASSERT_EQ(std::vector<std::string>{"lon"}, a.longitude());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point, custom options
  {
    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.latitude = {"lat"};
    opts.longitude = {"lon"};
    GeoPointAnalyzer a(opts);
    ASSERT_EQ(std::vector<std::string>{"lat"}, a.latitude());
    ASSERT_EQ(std::vector<std::string>{"lon"}, a.longitude());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoPointAnalyzerTest, tokenizePointFromObjectComplexPath) {
  auto json = VPackParser::fromJson(R"([ 63.57789956676574, 53.72314453125 ])");
  auto jsonObject = VPackParser::fromJson(R"({ "subObj": { "lat": 63.57789956676574, "lon": 53.72314453125 } })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(shape.parseCoordinates(json->slice(), false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize point
  {
    GeoPointAnalyzer::Options opts;
    opts.latitude = {"subObj", "lat"};
    opts.longitude = {"subObj", "lon"};
    GeoPointAnalyzer a(opts);
    ASSERT_EQ((std::vector<std::string>{"subObj", "lat"}), a.latitude());
    ASSERT_EQ((std::vector<std::string>{"subObj", "lon"}), a.longitude());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point, custom options
  {
    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.latitude = {"subObj", "lat"};
    opts.longitude = {"subObj", "lon"};
    GeoPointAnalyzer a(opts);
    ASSERT_EQ((std::vector<std::string>{"subObj", "lat"}), a.latitude());
    ASSERT_EQ((std::vector<std::string>{"subObj", "lon"}), a.longitude());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoPointAnalyzerTest, createFromSlice) {
  {
    auto json = VPackParser::fromJson(R"({})");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    ASSERT_TRUE(impl.longitude().empty());
    ASSERT_TRUE(impl.latitude().empty());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": 1000
      }
    })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    ASSERT_TRUE(impl.longitude().empty());
    ASSERT_TRUE(impl.latitude().empty());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": 1000,
        "minLevel": 2,
        "maxLevel": 22
      }
    })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    ASSERT_TRUE(impl.longitude().empty());
    ASSERT_TRUE(impl.latitude().empty());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "latitude": ["foo"], "longitude":["bar"] })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    ASSERT_EQ(std::vector<std::string>{"bar"}, impl.longitude());
    ASSERT_EQ(std::vector<std::string>{"foo"}, impl.latitude());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "latitude": ["subObj", "foo"], "longitude":["subObj", "bar"] })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    ASSERT_EQ((std::vector<std::string>{"subObj", "foo"}), impl.latitude());
    ASSERT_EQ((std::vector<std::string>{"subObj", "bar"}), impl.longitude());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "unknownField": "anything", "latitude": ["subObj", "foo"], "longitude":["subObj", "bar"] })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    ASSERT_EQ((std::vector<std::string>{"subObj", "foo"}), impl.latitude());
    ASSERT_EQ((std::vector<std::string>{"subObj", "bar"}), impl.longitude());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  // latitude field is not set
  {
    auto json = VPackParser::fromJson(R"({
      "longitude": ["foo"]
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // longitude is not set
  {
    auto json = VPackParser::fromJson(R"({
      "latitude": ["foo"]
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // minLevel > maxLevel
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minLevel": 22,
        "maxLevel": 2
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minLevel": -2,
        "maxLevel": 22
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minLevel": -22,
        "maxLevel": -2
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": -2
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // nan
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": "2"
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // higher than max GeoOptions::MAX_LEVEL
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxLevel": 31
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }

  // higher than max GeoOptions::MAX_LEVEL
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minCells": 31,
        "maxCells": 31
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        GeoJSONAnalyzer test suite
// -----------------------------------------------------------------------------

TEST(GeoJSONAnalyzerTest, constants) {
  static_assert("geojson" == GeoJSONAnalyzer::type_name());
}

TEST(GeoJSONAnalyzerTest, options) {
  GeoJSONAnalyzer::Options opts;
  ASSERT_EQ(GeoJSONAnalyzer::Type::SHAPE, opts.type);
  ASSERT_EQ(GeoOptions{}.maxCells, opts.options.maxCells);
  ASSERT_EQ(GeoOptions{}.minLevel, opts.options.minLevel);
  ASSERT_EQ(GeoOptions{}.maxLevel, opts.options.maxLevel);
}

TEST(GeoJSONAnalyzerTest, ctor) {
  GeoJSONAnalyzer a({});
  {
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    ASSERT_EQ(1, inc->value);
  }
  {
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(term->value.null());
  }
  ASSERT_EQ(irs::type<GeoJSONAnalyzer>::id(), a.type());
  ASSERT_FALSE(a.next());
}

TEST(GeoJSONAnalyzerTest, tokenizeLatLngRect) {
  auto json = VPackParser::fromJson(R"({
    "type": "Polygon",
    "coordinates": [
      [
        [
          50.361328125,
          61.501734289732326
        ],
        [
          51.2841796875,
          61.501734289732326
        ],
        [
          51.2841796875,
          61.907926072709756
        ],
        [
          50.361328125,
          61.907926072709756
        ],
        [
          50.361328125,
          61.501734289732326
        ]
      ]
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::geojson::parsePolygon(json->slice(), shape).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_LATLNGRECT, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoJSONAnalyzerTest, tokenizePolygon) {
  auto json = VPackParser::fromJson(R"({
    "type": "Polygon",
    "coordinates": [
      [
        [
          52.44873046875,
          64.33039136366138
        ],
        [
          50.73486328125,
          63.792191443824464
        ],
        [
          51.5478515625,
          63.104699747121074
        ],
        [
          52.6904296875,
          62.825055614564306
        ],
        [
          54.95361328125,
          63.203925767041305
        ],
        [
          55.37109374999999,
          63.82128765261384
        ],
        [
          54.7998046875,
          64.37794095121995
        ],
        [
          53.525390625,
          64.44437240555092
        ],
        [
          52.44873046875,
          64.33039136366138
        ]
      ]
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::geojson::parsePolygon(json->slice(), shape).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoJSONAnalyzerTest, tokenizeLineString) {
  auto json = VPackParser::fromJson(R"({
    "type": "LineString",
    "coordinates": [
      [
        37.615908086299896,
        55.704700721216476
      ],
      [
        37.61495590209961,
        55.70460097444075
      ],
      [
        37.614915668964386,
        55.704266972019845
      ],
      [
        37.61498004198074,
        55.70365336737268
      ],
      [
        37.61568009853363,
        55.7036518560193
      ],
      [
        37.61656254529953,
        55.7041400201247
      ],
      [
        37.61668860912323,
        55.70447251230901
      ],
      [
        37.615661323070526,
        55.704404502774175
      ],
      [
        37.61548697948456,
        55.70397830699434
      ],
      [
        37.61526703834534,
        55.70439090085301
      ]
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::geojson::parseRegion(json->slice(), shape).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYLINE, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoJSONAnalyzerTest, tokenizeMultiPolygon) {
  auto json = VPackParser::fromJson(R"({
    "type": "MultiPolygon",
    "coordinates": [
        [
            [
                [
                    107,
                    7
                ],
                [
                    108,
                    7
                ],
                [
                    108,
                    8
                ],
                [
                    107,
                    8
                ],
                [
                    107,
                    7
                ]
            ]
        ],
        [
            [
                [
                    100,
                    0
                ],
                [
                    101,
                    0
                ],
                [
                    101,
                    1
                ],
                [
                    100,
                    1
                ],
                [
                    100,
                    0
                ]
            ]
        ]
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(json->slice(), shape).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoJSONAnalyzerTest, tokenizeMultiPoint) {
  auto json = VPackParser::fromJson(R"({
    "type": "MultiPoint",
    "coordinates": [
        [
            -105.01621,
            39.57422
        ],
        [
            -80.666513,
            35.053994
        ]
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::geojson::parseMultiPoint(json->slice(), shape).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_MULTIPOINT, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoJSONAnalyzerTest, tokenizeMultiPolyLine) {
  auto json = VPackParser::fromJson(R"({
    "type": "MultiLineString",
    "coordinates": [
        [
            [
                -105.021443,
                39.578057
            ],
            [
                -105.021507,
                39.577809
            ],
            [
                -105.021572,
                39.577495
            ],
            [
                -105.021572,
                39.577164
            ],
            [
                -105.021572,
                39.577032
            ],
            [
                -105.021529,
                39.576784
            ]
        ],
        [
            [
                -105.019898,
                39.574997
            ],
            [
                -105.019598,
                39.574898
            ],
            [
                -105.019061,
                39.574782
            ]
        ],
        [
            [
                -105.017173,
                39.574402
            ],
            [
                -105.01698,
                39.574385
            ],
            [
                -105.016636,
                39.574385
            ],
            [
                -105.016508,
                39.574402
            ],
            [
                -105.01595,
                39.57427
            ]
        ],
        [
            [
                -105.014276,
                39.573972
            ],
            [
                -105.014126,
                39.574038
            ],
            [
                -105.013825,
                39.57417
            ],
            [
                -105.01331,
                39.574452
            ]
        ]
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::geojson::parseRegion(json->slice(), shape).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_MULTIPOLYLINE, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoJSONAnalyzerTest, tokenizePoint) {
  auto json = VPackParser::fromJson(R"({
    "type": "Point",
    "coordinates": [
      53.72314453125,
      63.57789956676574
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::geojson::parseRegion(json->slice(), shape).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::SHAPE, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::SHAPE, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::CENTROID, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::CENTROID, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::POINT, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::POINT, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoJSONAnalyzerTest, tokenizePointGeoJSONArray) {
  auto json = VPackParser::fromJson(R"([ 53.72314453125, 63.57789956676574 ])");

  geo::ShapeContainer shape;
  ASSERT_TRUE(parseShape(json->slice(), shape, true));
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::SHAPE, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::SHAPE, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::CENTROID, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::CENTROID, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::POINT, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point, custom options
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    ASSERT_EQ(GeoJSONAnalyzer::Type::POINT, a.shapeType());
    ASSERT_EQ(1, a.options().level_mod());
    ASSERT_FALSE(a.options().optimize_for_space());
    ASSERT_EQ("$", a.options().marker());
    ASSERT_EQ(opts.options.minLevel, a.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, a.options().max_level());
    ASSERT_EQ(opts.options.maxCells, a.options().max_cells());
    ASSERT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ref_cast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoJSONAnalyzerTest, invalidGeoJson) {
  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyArraySlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::noneSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::illegalSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::falseSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::trueSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::zeroSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::nullSlice())));
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyArraySlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::noneSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::illegalSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::falseSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::trueSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::zeroSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::nullSlice())));
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyArraySlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::noneSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::illegalSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::falseSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::trueSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::zeroSlice())));
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(VPackSlice::nullSlice())));
  }
}

TEST(GeoJSONAnalyzerTest, prepareQuery) {
  // tokenize shape
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    GeoJSONAnalyzer a(opts);

    S2RegionTermIndexer::Options s2opts;
    a.prepare(s2opts);

    ASSERT_EQ(1, s2opts.level_mod());
    ASSERT_FALSE(s2opts.optimize_for_space());
    ASSERT_EQ("$", s2opts.marker());
    ASSERT_EQ(opts.options.minLevel, s2opts.min_level());
    ASSERT_EQ(opts.options.maxLevel, s2opts.max_level());
    ASSERT_EQ(opts.options.maxCells, s2opts.max_cells());
    ASSERT_FALSE(s2opts.index_contains_points_only());
  }

  // tokenize centroid
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    GeoJSONAnalyzer a(opts);

    S2RegionTermIndexer::Options s2opts;
    a.prepare(s2opts);

    ASSERT_EQ(1, s2opts.level_mod());
    ASSERT_FALSE(s2opts.optimize_for_space());
    ASSERT_EQ("$", s2opts.marker());
    ASSERT_EQ(opts.options.minLevel, s2opts.min_level());
    ASSERT_EQ(opts.options.maxLevel, s2opts.max_level());
    ASSERT_EQ(opts.options.maxCells, s2opts.max_cells());
    ASSERT_TRUE(s2opts.index_contains_points_only());
  }

  // tokenize point
  {
    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    GeoJSONAnalyzer a(opts);

    S2RegionTermIndexer::Options s2opts;
    a.prepare(s2opts);

    ASSERT_EQ(1, s2opts.level_mod());
    ASSERT_FALSE(s2opts.optimize_for_space());
    ASSERT_EQ("$", s2opts.marker());
    ASSERT_EQ(opts.options.minLevel, s2opts.min_level());
    ASSERT_EQ(opts.options.maxLevel, s2opts.max_level());
    ASSERT_EQ(opts.options.maxCells, s2opts.max_cells());
    ASSERT_TRUE(s2opts.index_contains_points_only());
  }
}

TEST(GeoJSONAnalyzerTest, createFromSlice) {
  // no type supplied
  {
    auto json = VPackParser::fromJson(R"({})");
    auto a = GeoJSONAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoJSONAnalyzer&>(*a);

    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::SHAPE;
    ASSERT_EQ(opts.type, impl.shapeType());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "type": "shape" })");
    auto a = GeoJSONAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoJSONAnalyzer&>(*a);

    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::SHAPE;
    ASSERT_EQ(opts.type, impl.shapeType());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "shape",
      "options" : {
        "maxCells": 1000
      }
    })");
    auto a = GeoJSONAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoJSONAnalyzer&>(*a);

    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.type = GeoJSONAnalyzer::Type::SHAPE;
    ASSERT_EQ(opts.type, impl.shapeType());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "shape",
      "options" : {
        "maxCells": 1000,
        "minLevel": 2,
        "maxLevel": 22
      }
    })");
    auto a = GeoJSONAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoJSONAnalyzer&>(*a);

    GeoJSONAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    opts.type = GeoJSONAnalyzer::Type::SHAPE;
    ASSERT_EQ(opts.type, impl.shapeType());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "type": "centroid" })");
    auto a = GeoJSONAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoJSONAnalyzer&>(*a);

    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::CENTROID;
    ASSERT_EQ(opts.type, impl.shapeType());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "type": "point" })");
    auto a = GeoJSONAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoJSONAnalyzer&>(*a);

    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    ASSERT_EQ(opts.type, impl.shapeType());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "type": "point", "unknownField":"anything" })");
    auto a = GeoJSONAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoJSONAnalyzer&>(*a);

    GeoJSONAnalyzer::Options opts;
    opts.type = GeoJSONAnalyzer::Type::POINT;
    ASSERT_EQ(opts.type, impl.shapeType());
    ASSERT_EQ(1, impl.options().level_mod());
    ASSERT_FALSE(impl.options().optimize_for_space());
    ASSERT_EQ("$", impl.options().marker());
    ASSERT_EQ(opts.options.minLevel, impl.options().min_level());
    ASSERT_EQ(opts.options.maxLevel, impl.options().max_level());
    ASSERT_EQ(opts.options.maxCells, impl.options().max_cells());
    ASSERT_FALSE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "Shape"
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "Centroid"
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "Point"
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  // minLevel > maxLevel
  {
    auto json = VPackParser::fromJson(R"({
      "type": "shape",
      "options" : {
        "minLevel": 22,
        "maxLevel": 2
      }
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minLevel": -2,
        "maxLevel": 22
      }
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minLevel": -22,
        "maxLevel": -2
      }
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": -2
      }
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  // nan
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": "2"
      }
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  // higher than max GeoOptions::MAX_LEVEL
  {
    auto json = VPackParser::fromJson(R"({
      "type": "shape",
      "options" : {
        "maxLevel": 31
      }
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }

  // higher than max GeoOptions::MAX_LEVEL
  {
    auto json = VPackParser::fromJson(R"({
      "type": "shape",
      "options" : {
        "minCells": 31,
        "maxCells": 31
      }
    })");
    ASSERT_EQ(nullptr, GeoJSONAnalyzer::make(ref<char>(json->slice())));
  }
}
