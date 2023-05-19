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
#ifdef USE_ENTERPRISE
#include "Enterprise/IResearch/GeoAnalyzerEE.h"
#endif
#include "IResearch/GeoFilter.h"
#include "Basics/DownCast.h"
#include "IResearch/VelocyPackHelper.h"
#include "Geo/GeoJson.h"
#include "velocypack/Parser.h"

using namespace arangodb;
using namespace arangodb::iresearch;

// -----------------------------------------------------------------------------
// --SECTION--                                             GeoOptions test suite
// -----------------------------------------------------------------------------

TEST(GeoOptionsTest, constants) {
  static_assert(20 == GeoOptions::kDefaultMaxCells);
  static_assert(4 == GeoOptions::kDefaultMinLevel);
  static_assert(23 == GeoOptions::kDefaultMaxLevel);
}

TEST(GeoOptionsTest, options) {
  GeoOptions opts;
  ASSERT_EQ(GeoOptions::kDefaultMaxCells, opts.maxCells);
  ASSERT_EQ(GeoOptions::kDefaultMinLevel, opts.minLevel);
  ASSERT_EQ(GeoOptions::kDefaultMaxLevel, opts.maxLevel);
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
    opts.longitude = {"bar"};
    GeoPointAnalyzer a(opts);

    GeoFilterOptionsBase options;
    a.prepare(options);

    EXPECT_EQ(options.prefix, "");
    EXPECT_EQ(options.stored, StoredType::VPack);
    EXPECT_EQ(1, options.options.level_mod());
    EXPECT_FALSE(options.options.optimize_for_space());
    EXPECT_EQ("$", options.options.marker());
    EXPECT_EQ(opts.options.minLevel, options.options.min_level());
    EXPECT_EQ(opts.options.maxLevel, options.options.max_level());
    EXPECT_EQ(opts.options.maxCells, options.options.max_cells());
    EXPECT_TRUE(options.options.index_contains_points_only());
  }

  {
    GeoPointAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    GeoPointAnalyzer a(opts);

    GeoFilterOptionsBase options;
    a.prepare(options);

    EXPECT_EQ(options.prefix, "");
    EXPECT_EQ(options.stored, StoredType::VPack);
    EXPECT_EQ(1, options.options.level_mod());
    EXPECT_FALSE(options.options.optimize_for_space());
    EXPECT_EQ("$", options.options.marker());
    EXPECT_EQ(opts.options.minLevel, options.options.min_level());
    EXPECT_EQ(opts.options.maxLevel, options.options.max_level());
    EXPECT_EQ(opts.options.maxCells, options.options.max_cells());
    EXPECT_TRUE(options.options.index_contains_points_only());
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
      ASSERT_EQ(1U, inc->value);
    }
    {
      auto* term = irs::get<irs::term_attribute>(a);
      ASSERT_NE(nullptr, term);
      ASSERT_TRUE(irs::IsNull(term->value));
    }
    ASSERT_EQ(irs::type<GeoPointAnalyzer>::id(), a.type());
    ASSERT_FALSE(a.next());
  }

  {
    GeoPointAnalyzer::Options opts;
    opts.latitude = {"foo"};
    velocypack::Builder builder;
    toVelocyPack(builder, opts);
    auto a = GeoPointAnalyzer::make(ref<char>(builder.slice()));
    EXPECT_TRUE(a == nullptr);
  }

  {
    GeoPointAnalyzer::Options opts;
    opts.longitude = {"foo"};
    velocypack::Builder builder;
    toVelocyPack(builder, opts);
    auto a = GeoPointAnalyzer::make(ref<char>(builder.slice()));
    EXPECT_TRUE(a == nullptr);
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
      ASSERT_TRUE(irs::IsNull(term->value));
    }
    ASSERT_EQ(irs::type<GeoPointAnalyzer>::id(), a.type());
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoPointAnalyzerTest, tokenizePointFromArray) {
  auto json = VPackParser::fromJson(R"([ 63.57789956676574, 53.72314453125 ])");

  geo::ShapeContainer shape;
  ASSERT_TRUE(
      geo::json::parseCoordinates<true>(json->slice(), shape, false).ok());
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
    ASSERT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
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
    EXPECT_TRUE(a.latitude().empty());
    EXPECT_TRUE(a.longitude().empty());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoPointAnalyzerTest, tokenizePointFromObject) {
  auto json = VPackParser::fromJson(R"([ 63.57789956676574, 53.72314453125 ])");
  auto jsonObject = VPackParser::fromJson(
      R"({ "lat": 63.57789956676574, "lon": 53.72314453125 })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(
      geo::json::parseCoordinates<true>(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize point
  {
    GeoPointAnalyzer::Options opts;
    opts.latitude = {"lat"};
    opts.longitude = {"lon"};
    GeoPointAnalyzer a(opts);
    EXPECT_EQ(std::vector<std::string>{"lat"}, a.latitude());
    EXPECT_EQ(std::vector<std::string>{"lon"}, a.longitude());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
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
    EXPECT_EQ(std::vector<std::string>{"lat"}, a.latitude());
    EXPECT_EQ(std::vector<std::string>{"lon"}, a.longitude());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoPointAnalyzerTest, tokenizePointFromObjectComplexPath) {
  auto json = VPackParser::fromJson(R"([ 63.57789956676574, 53.72314453125 ])");
  auto jsonObject = VPackParser::fromJson(
      R"({ "subObj": { "lat": 63.57789956676574, "lon": 53.72314453125 } })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(
      geo::json::parseCoordinates<true>(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize point
  {
    GeoPointAnalyzer::Options opts;
    opts.latitude = {"subObj", "lat"};
    opts.longitude = {"subObj", "lon"};
    GeoPointAnalyzer a(opts);
    EXPECT_EQ((std::vector<std::string>{"subObj", "lat"}), a.latitude());
    EXPECT_EQ((std::vector<std::string>{"subObj", "lon"}), a.longitude());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
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
    EXPECT_EQ((std::vector<std::string>{"subObj", "lat"}), a.latitude());
    EXPECT_EQ((std::vector<std::string>{"subObj", "lon"}), a.longitude());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(jsonObject->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, true));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
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
    EXPECT_TRUE(impl.longitude().empty());
    EXPECT_TRUE(impl.latitude().empty());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
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
    EXPECT_TRUE(impl.longitude().empty());
    EXPECT_TRUE(impl.latitude().empty());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
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
    EXPECT_TRUE(impl.longitude().empty());
    EXPECT_TRUE(impl.latitude().empty());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(
        R"({ "latitude": ["foo"], "longitude":["bar"] })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    EXPECT_EQ(std::vector<std::string>{"bar"}, impl.longitude());
    EXPECT_EQ(std::vector<std::string>{"foo"}, impl.latitude());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(
        R"({ "latitude": ["subObj", "foo"], "longitude":["subObj", "bar"] })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    EXPECT_EQ((std::vector<std::string>{"subObj", "foo"}), impl.latitude());
    EXPECT_EQ((std::vector<std::string>{"subObj", "bar"}), impl.longitude());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(
        R"({ "unknownField": "anything", "latitude": ["subObj", "foo"], "longitude":["subObj", "bar"] })");
    auto a = GeoPointAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoPointAnalyzer&>(*a);

    GeoPointAnalyzer::Options opts;
    EXPECT_EQ((std::vector<std::string>{"subObj", "foo"}), impl.latitude());
    EXPECT_EQ((std::vector<std::string>{"subObj", "bar"}), impl.longitude());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
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
        "minLevel": 31,
        "maxLevel": 31
      }
    })");
    ASSERT_EQ(nullptr, GeoPointAnalyzer::make(ref<char>(json->slice())));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        GeoVPackAnalyzer test
// suite
// -----------------------------------------------------------------------------

TEST(GeoVPackAnalyzerTest, constants) {
  static_assert("geojson" == GeoVPackAnalyzer::type_name());
}

TEST(GeoVPackAnalyzerTest, options) {
  GeoVPackAnalyzer::Options opts;
  ASSERT_EQ(GeoVPackAnalyzer::Type::SHAPE, opts.type);
  ASSERT_EQ(GeoOptions{}.maxCells, opts.options.maxCells);
  ASSERT_EQ(GeoOptions{}.minLevel, opts.options.minLevel);
  ASSERT_EQ(GeoOptions{}.maxLevel, opts.options.maxLevel);
}

TEST(GeoVPackAnalyzerTest, ctor) {
  GeoVPackAnalyzer a({});
  {
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    ASSERT_EQ(1, inc->value);
  }
  {
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(irs::IsNull(term->value));
  }
  ASSERT_EQ(irs::type<GeoVPackAnalyzer>::id(), a.type());
  ASSERT_FALSE(a.next());
}

TEST(GeoVPackAnalyzerTest, tokenizeLatLngRect) {
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
  ASSERT_TRUE(geo::json::parseRegion(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoVPackAnalyzerTest, tokenizePolygon) {
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
  ASSERT_TRUE(geo::json::parseRegion(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoVPackAnalyzerTest, tokenizeLineString) {
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
  ASSERT_TRUE(geo::json::parseRegion(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYLINE, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    auto end = terms.end();
    for (; a.next() && begin != end; ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, end);
    while (a.next()) {  // centroid terms
    }
  }

  // tokenize shape, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    auto end = terms.end();
    for (; a.next() && begin != end; ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, end);
    while (a.next()) {  // centroid terms
    }
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoVPackAnalyzerTest, tokenizeMultiPolygon) {
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
  ASSERT_TRUE(geo::json::parseRegion(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    auto end = terms.end();
    for (; a.next() && begin != end; ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, end);
    while (a.next()) {  // centroid terms
    }
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoVPackAnalyzerTest, tokenizeMultiPoint) {
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
  ASSERT_TRUE(geo::json::parseRegion(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_MULTIPOINT, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    auto end = terms.end();
    for (; a.next() && begin != end; ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, end);
    while (a.next()) {  // centroid terms
    }
  }

  // tokenize shape, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    auto end = terms.end();
    for (; a.next() && begin != end; ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, end);
    while (a.next()) {  // centroid terms
    }
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoVPackAnalyzerTest, tokenizeMultiPolyLine) {
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
  ASSERT_TRUE(geo::json::parseRegion(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_MULTIPOLYLINE, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    auto end = terms.end();
    for (; a.next() && begin != end; ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, end);
    while (a.next()) {  // centroid terms
    }
  }

  // tokenize shape, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(*shape.region(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    auto end = terms.end();
    for (; a.next() && begin != end; ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, end);
    while (a.next()) {  // centroid terms
    }
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(arangodb::iresearch::ref<char>(json->slice())));
    ASSERT_FALSE(a.next());
  }
}

TEST(GeoVPackAnalyzerTest, tokenizePoint) {
  auto json = VPackParser::fromJson(R"({
    "type": "Point",
    "coordinates": [
      53.72314453125,
      63.57789956676574
    ]
  })");

  geo::ShapeContainer shape;
  ASSERT_TRUE(geo::json::parseRegion(json->slice(), shape, false).ok());
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::SHAPE, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::SHAPE, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_FALSE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::CENTROID, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, true));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::CENTROID, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, true));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::POINT, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::POINT, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, true));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoVPackAnalyzerTest, tokenizePointGeoJSONArray) {
  auto json = VPackParser::fromJson(R"([ 53.72314453125, 63.57789956676574 ])");

  geo::ShapeContainer shape;
  std::vector<S2LatLng> cache;
  ASSERT_TRUE(parseShape<arangodb::iresearch::Parsing::OnlyPoint>(
      json->slice(), shape, cache, false, geo::coding::Options::kInvalid,
      nullptr));
  ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT, shape.type());

  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    ASSERT_EQ(GeoVPackAnalyzer::Type::SHAPE, a.shapeType());
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

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize shape, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);
    ASSERT_EQ(GeoVPackAnalyzer::Type::SHAPE, a.shapeType());
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

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::CENTROID, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize centroid, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::CENTROID, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::POINT, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }

  // tokenize point, custom options
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 3;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    EXPECT_EQ(GeoVPackAnalyzer::Type::POINT, a.shapeType());
    EXPECT_EQ(1, a.options().level_mod());
    EXPECT_FALSE(a.options().optimize_for_space());
    EXPECT_EQ("$", a.options().marker());
    EXPECT_EQ(opts.options.minLevel, a.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, a.options().max_level());
    EXPECT_EQ(opts.options.maxCells, a.options().max_cells());
    EXPECT_TRUE(a.options().index_contains_points_only());

    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_TRUE(a.reset(arangodb::iresearch::ref<char>(json->slice())));

    S2RegionTermIndexer indexer(S2Options(opts.options, false));
    auto terms = indexer.GetIndexTerms(shape.centroid(), {});
    ASSERT_FALSE(terms.empty());

    auto begin = terms.begin();
    for (; a.next(); ++begin) {
      ASSERT_EQ(1, inc->value);
      ASSERT_EQ(*begin, irs::ViewCast<char>(term->value));
    }
    ASSERT_EQ(begin, terms.end());
  }
}

TEST(GeoVPackAnalyzerTest, invalidGeoJson) {
  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyArraySlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::noneSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::illegalSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::falseSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::trueSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::zeroSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::nullSlice())));
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyArraySlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::noneSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::illegalSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::falseSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::trueSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::zeroSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::nullSlice())));
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);
    auto* inc = irs::get<irs::increment>(a);
    ASSERT_NE(nullptr, inc);
    auto* term = irs::get<irs::term_attribute>(a);
    ASSERT_NE(nullptr, term);
    ASSERT_FALSE(a.reset(
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::emptyArraySlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::noneSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::illegalSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::falseSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::trueSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::zeroSlice())));
    ASSERT_FALSE(
        a.reset(arangodb::iresearch::ref<char>(VPackSlice::nullSlice())));
  }
}

TEST(GeoVPackAnalyzerTest, prepareQuery) {
  // tokenize shape
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    GeoVPackAnalyzer a(opts);

    GeoFilterOptionsBase options;
    a.prepare(options);

    EXPECT_EQ(options.prefix, "");
    EXPECT_EQ(options.stored, StoredType::VPack);
    EXPECT_EQ(1, options.options.level_mod());
    EXPECT_FALSE(options.options.optimize_for_space());
    EXPECT_EQ("$", options.options.marker());
    EXPECT_EQ(opts.options.minLevel, options.options.min_level());
    EXPECT_EQ(opts.options.maxLevel, options.options.max_level());
    EXPECT_EQ(opts.options.maxCells, options.options.max_cells());
    EXPECT_FALSE(options.options.index_contains_points_only());
  }

  // tokenize centroid
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    GeoVPackAnalyzer a(opts);

    GeoFilterOptionsBase options;
    a.prepare(options);

    EXPECT_EQ(options.prefix, "");
    EXPECT_EQ(options.stored, StoredType::VPack);
    EXPECT_EQ(1, options.options.level_mod());
    EXPECT_FALSE(options.options.optimize_for_space());
    EXPECT_EQ("$", options.options.marker());
    EXPECT_EQ(opts.options.minLevel, options.options.min_level());
    EXPECT_EQ(opts.options.maxLevel, options.options.max_level());
    EXPECT_EQ(opts.options.maxCells, options.options.max_cells());
    EXPECT_TRUE(options.options.index_contains_points_only());
  }

  // tokenize point
  {
    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    GeoVPackAnalyzer a(opts);

    GeoFilterOptionsBase options;
    a.prepare(options);

    EXPECT_EQ(options.prefix, "");
    EXPECT_EQ(options.stored, StoredType::VPack);
    EXPECT_EQ(1, options.options.level_mod());
    EXPECT_FALSE(options.options.optimize_for_space());
    EXPECT_EQ("$", options.options.marker());
    EXPECT_EQ(opts.options.minLevel, options.options.min_level());
    EXPECT_EQ(opts.options.maxLevel, options.options.max_level());
    EXPECT_EQ(opts.options.maxCells, options.options.max_cells());
    EXPECT_TRUE(options.options.index_contains_points_only());
  }
}

TEST(GeoVPackAnalyzerTest, createFromSlice) {
  // no type supplied
  {
    auto json = VPackParser::fromJson(R"({})");
    auto a = GeoVPackAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoVPackAnalyzer&>(*a);

    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::SHAPE;
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
    auto a = GeoVPackAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoVPackAnalyzer&>(*a);

    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::SHAPE;
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
    auto a = GeoVPackAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoVPackAnalyzer&>(*a);

    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.type = GeoVPackAnalyzer::Type::SHAPE;
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
    auto a = GeoVPackAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoVPackAnalyzer&>(*a);

    GeoVPackAnalyzer::Options opts;
    opts.options.maxCells = 1000;
    opts.options.minLevel = 2;
    opts.options.maxLevel = 22;
    opts.type = GeoVPackAnalyzer::Type::SHAPE;
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
    auto a = GeoVPackAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoVPackAnalyzer&>(*a);

    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::CENTROID;
    EXPECT_EQ(opts.type, impl.shapeType());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({ "type": "point" })");
    auto a = GeoVPackAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoVPackAnalyzer&>(*a);

    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    EXPECT_EQ(opts.type, impl.shapeType());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(
        R"({ "type": "point", "unknownField":"anything" })");
    auto a = GeoVPackAnalyzer::make(ref<char>(json->slice()));
    ASSERT_NE(nullptr, a);
    auto& impl = dynamic_cast<GeoVPackAnalyzer&>(*a);

    GeoVPackAnalyzer::Options opts;
    opts.type = GeoVPackAnalyzer::Type::POINT;
    EXPECT_EQ(opts.type, impl.shapeType());
    EXPECT_EQ(1, impl.options().level_mod());
    EXPECT_FALSE(impl.options().optimize_for_space());
    EXPECT_EQ("$", impl.options().marker());
    EXPECT_EQ(opts.options.minLevel, impl.options().min_level());
    EXPECT_EQ(opts.options.maxLevel, impl.options().max_level());
    EXPECT_EQ(opts.options.maxCells, impl.options().max_cells());
    EXPECT_TRUE(impl.options().index_contains_points_only());
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "Shape"
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "Centroid"
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  {
    auto json = VPackParser::fromJson(R"({
      "type": "Point"
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
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
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minLevel": -2,
        "maxLevel": 22
      }
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "minLevel": -22,
        "maxLevel": -2
      }
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  // negative value
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": -2
      }
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  // nan
  {
    auto json = VPackParser::fromJson(R"({
      "options" : {
        "maxCells": "2"
      }
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  // higher than max GeoOptions::MAX_LEVEL
  {
    auto json = VPackParser::fromJson(R"({
      "type": "shape",
      "options" : {
        "maxLevel": 31
      }
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }

  // higher than max GeoOptions::MAX_LEVEL
  {
    auto json = VPackParser::fromJson(R"({
      "type": "shape",
      "options" : {
        "minLevel": 31,
        "maxLevel": 31
      }
    })");
    ASSERT_EQ(nullptr, GeoVPackAnalyzer::make(ref<char>(json->slice())));
  }
}
