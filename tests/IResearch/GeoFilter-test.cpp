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

#include "gtest/gtest.h"

#include <set>

#include "s2/s2point_region.h"
#include "s2/s2polygon.h"

#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/memory_directory.hpp"

#include "IResearch/GeoFilter.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFields.h"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::iresearch;
using namespace arangodb::tests;

TEST(GeoFilterTest, options) {
  S2RegionTermIndexer::Options const s2opts;
  GeoFilterOptions const opts;
  ASSERT_TRUE(opts.prefix.empty());
  ASSERT_TRUE(opts.shape.empty());
  ASSERT_EQ(s2opts.level_mod(), opts.options.level_mod());
  ASSERT_EQ(s2opts.min_level(), opts.options.min_level());
  ASSERT_EQ(s2opts.max_level(), opts.options.max_level());
  ASSERT_EQ(s2opts.max_cells(), opts.options.max_cells());
  ASSERT_EQ(s2opts.marker(), opts.options.marker());
  ASSERT_EQ(s2opts.index_contains_points_only(), opts.options.index_contains_points_only());
  ASSERT_EQ(s2opts.optimize_for_space(), opts.options.optimize_for_space());
  ASSERT_EQ(arangodb::iresearch::GeoFilterType::NEARBY, opts.type);
}

TEST(GeoFilterTest, ctor) {
  GeoFilter q;
  ASSERT_EQ(irs::type<GeoFilter>::id(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::no_boost(), q.boost());
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
  ASSERT_EQ(GeoFilterOptions{}, q.options());
#endif
}

TEST(GeoFilterTest, equal) {
  GeoFilter q;
  q.mutable_options()->type = GeoFilterType::INTERSECTS;
  q.mutable_options()->shape.reset(std::make_unique<S2PointRegion>(S2Point{1., 2., 3.}),
                                   geo::ShapeContainer::Type::S2_POINT);
  *q.mutable_field() = "field";

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::INTERSECTS;
    q1.mutable_options()->shape.reset(std::make_unique<S2PointRegion>(S2Point{1., 2., 3.}),
                                      geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field";
    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoFilter q1;
    q1.boost(1.5);
    q1.mutable_options()->type = GeoFilterType::INTERSECTS;
    q1.mutable_options()->shape.reset(std::make_unique<S2PointRegion>(S2Point{1., 2., 3.}),
                                      geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field";
    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::INTERSECTS;
    q1.mutable_options()->shape.reset(std::make_unique<S2PointRegion>(S2Point{1., 2., 3.}),
                                      geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field1";
    ASSERT_NE(q, q1);
  }

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::CONTAINS;
    q1.mutable_options()->shape.reset(std::make_unique<S2PointRegion>(S2Point{1., 2., 3.}),
                                      geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field";
    ASSERT_NE(q, q1);
  }

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::CONTAINS;
    q1.mutable_options()->shape.reset(std::make_unique<S2Polygon>(),
                                      geo::ShapeContainer::Type::S2_POLYGON);
    *q1.mutable_field() = "field";
    ASSERT_NE(q, q1);
  }
}

TEST(GeoFilterTest, boost) {
  // no boost
  {
    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    q.mutable_options()->shape.reset(std::make_unique<S2PointRegion>(S2Point{1., 2., 3.}),
                                     geo::ShapeContainer::Type::S2_POINT);
    *q.mutable_field() = "field";

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;
    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    q.mutable_options()->shape.reset(std::make_unique<S2PointRegion>(S2Point{1., 2., 3.}),
                                     geo::ShapeContainer::Type::S2_POINT);
    *q.mutable_field() = "field";
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

TEST(GeoFilterTest, query) {
  auto docs = VPackParser::fromJson(R"([
    { "name": "A", "geometry": { "type": "Point", "coordinates": [ 37.615895, 55.7039   ] } },
    { "name": "B", "geometry": { "type": "Point", "coordinates": [ 37.615315, 55.703915 ] } },
    { "name": "C", "geometry": { "type": "Point", "coordinates": [ 37.61509, 55.703537  ] } },
    { "name": "D", "geometry": { "type": "Point", "coordinates": [ 37.614183, 55.703806 ] } },
    { "name": "E", "geometry": { "type": "Point", "coordinates": [ 37.613792, 55.704405 ] } },
    { "name": "F", "geometry": { "type": "Point", "coordinates": [ 37.614956, 55.704695 ] } },
    { "name": "G", "geometry": { "type": "Point", "coordinates": [ 37.616297, 55.704831 ] } },
    { "name": "H", "geometry": { "type": "Point", "coordinates": [ 37.617053, 55.70461  ] } },
    { "name": "I", "geometry": { "type": "Point", "coordinates": [ 37.61582, 55.704459  ] } },
    { "name": "J", "geometry": { "type": "Point", "coordinates": [ 37.614634, 55.704338 ] } },
    { "name": "K", "geometry": { "type": "Point", "coordinates": [ 37.613121, 55.704193 ] } },
    { "name": "L", "geometry": { "type": "Point", "coordinates": [ 37.614135, 55.703298 ] } },
    { "name": "M", "geometry": { "type": "Point", "coordinates": [ 37.613663, 55.704002 ] } },
    { "name": "N", "geometry": { "type": "Point", "coordinates": [ 37.616522, 55.704235 ] } },
    { "name": "O", "geometry": { "type": "Point", "coordinates": [ 37.615508, 55.704172 ] } },
    { "name": "P", "geometry": { "type": "Point", "coordinates": [ 37.614629, 55.704081 ] } },
    { "name": "Q", "geometry": { "type": "Point", "coordinates": [ 37.610235, 55.709754 ] } },
    { "name": "R", "geometry": { "type": "Point", "coordinates": [ 37.605,    55.707917 ] } },
    { "name": "S", "geometry": { "type": "Point", "coordinates": [ 37.545776, 55.722083 ] } },
    { "name": "T", "geometry": { "type": "Point", "coordinates": [ 37.559509, 55.715895 ] } },
    { "name": "U", "geometry": { "type": "Point", "coordinates": [ 37.701645, 55.832144 ] } },
    { "name": "V", "geometry": { "type": "Point", "coordinates": [ 37.73735,  55.816715 ] } },
    { "name": "W", "geometry": { "type": "Point", "coordinates": [ 37.75589,  55.798193 ] } },
    { "name": "X", "geometry": { "type": "Point", "coordinates": [ 37.659073, 55.843711 ] } },
    { "name": "Y", "geometry": { "type": "Point", "coordinates": [ 37.778549, 55.823659 ] } },
    { "name": "Z", "geometry": { "type": "Point", "coordinates": [ 37.729797, 55.853733 ] } },
    { "name": "1", "geometry": { "type": "Point", "coordinates": [ 37.608261, 55.784682 ] } },
    { "name": "2", "geometry": { "type": "Point", "coordinates": [ 37.525177, 55.802825 ] } }
  ])");

  irs::memory_directory dir;

  // index data
  {
    auto codec = irs::formats::get(arangodb::iresearch::LATEST_FORMAT);
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);
    GeoField geoField;
    geoField.fieldName = "geometry";
    StringField nameField;
    nameField.fieldName = "name";
    {
      auto segment0 = writer->documents();
      auto segment1 = writer->documents();
      {
        size_t i = 0;
        for (auto docSlice: VPackArrayIterator(docs->slice())) {
          geoField.shapeSlice = docSlice.get("geometry");
          nameField.value = getStringRef(docSlice.get("name"));

          auto doc = (i++ % 2 ? segment0 : segment1).insert();
          ASSERT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(nameField));
          ASSERT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(geoField));
        }
      }
    }
    writer->commit();
  }

  auto reader = irs::directory_reader::open(dir);
  ASSERT_NE(nullptr, reader);
  ASSERT_EQ(2, reader->size());
  ASSERT_EQ(docs->slice().length(), reader->docs_count());
  ASSERT_EQ(docs->slice().length(), reader->live_docs_count());

  auto executeQuery = [&reader](irs::filter const& q) {
    std::set<std::string> actualResults;

    auto prepared = q.prepare(*reader);
    EXPECT_NE(nullptr, prepared);
    for (auto& segment : *reader) {
      auto column = segment.column_reader("name");
      EXPECT_NE(nullptr, column);
      auto values = column->values();
      auto it = prepared->execute(segment);
      EXPECT_NE(nullptr, it);

      while (it->next()) {
        auto docId = it->value();
        irs::bytes_ref value;
        EXPECT_TRUE(values(docId, value));
        EXPECT_FALSE(value.null());

        actualResults.emplace(irs::to_string<std::string>(value.c_str()));
      }
    }

    return actualResults;
  };

  {
    std::set<std::string> const expected{
      "Q", "R"
    };

    auto json = VPackParser::fromJson(R"({
      "type": "Polygon",
      "coordinates": [
          [
              [37.602682, 55.706853],
              [37.613025, 55.706853],
              [37.613025, 55.711906],
              [37.602682, 55.711906],
              [37.602682, 55.706853]
          ]
      ]
    })");

    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    ASSERT_TRUE(geo::geojson::parseRegion(json->slice(), q.mutable_options()->shape).ok());
    ASSERT_EQ(geo::ShapeContainer::Type::S2_LATLNGRECT, q.mutable_options()->shape.type());
    *q.mutable_field() = "geometry";

    ASSERT_EQ(expected, executeQuery(q));
  }
}
