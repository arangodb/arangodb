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

#include "s2/s2point_region.h"
#include "s2/s2polygon.h"

#include "index/index_reader.hpp"

#include "IResearch/GeoFilter.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

extern const char* ARGV0;  // defined in main.cpp

using namespace arangodb;
using namespace arangodb::iresearch;

TEST(GeoFilterTest, options) {
  S2RegionTermIndexer::Options const s2opts;
  GeoFilterOptions const opts;
  ASSERT_TRUE(opts.prefix.empty());
  ASSERT_TRUE(opts.storedField.empty());
  ASSERT_TRUE(opts.shape.empty());
  ASSERT_EQ(s2opts.level_mod(), opts.options.level_mod());
  ASSERT_EQ(s2opts.min_level(), opts.options.min_level());
  ASSERT_EQ(s2opts.max_level(), opts.options.max_level());
  ASSERT_EQ(s2opts.max_cells(), opts.options.max_cells());
  ASSERT_EQ(s2opts.marker(), opts.options.marker());
  ASSERT_EQ(s2opts.index_contains_points_only(), opts.options.index_contains_points_only());
  ASSERT_EQ(s2opts.optimize_for_space(), opts.options.optimize_for_space());
  ASSERT_EQ(arangodb::iresearch::GeoFilterType::NEAR, opts.type);
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

//#include "tests_shared.hpp"
//
//#include "rapidjson/document.h"
//#include "s2/s2latlng.h"
//#include "s2/s2text_format.h"
//#include <s2/s2earth.h>
//#include <s2/s2cap.h>
//
//#include "index/doc_generator.hpp"
//#include "filter_test_case_base.hpp"
//
//#include "analysis/geo_token_stream.hpp"
//#include "search/geo_filter.hpp"
//
//NS_LOCAL
//
//struct geo_point_field final : tests::field_base {
//  virtual irs::token_stream& get_tokens() const override {
//    stream.reset(point);
//    return stream;
//  }
//
//  virtual bool write(irs::data_output& out) const override {
//    const auto str = s2textformat::ToString(point);
//    irs::write_string(out, str);
//    return true;
//  }
//
//  mutable irs::analysis::geo_token_stream stream{{}, ""};
//  S2Point point;
//};
//
//struct geo_region_field final : tests::field_base {
//  virtual irs::token_stream& get_tokens() const override {
//    stream.reset(*region);
//    return stream;
//  }
//
//  virtual bool write(irs::data_output&) const override {
//    return false;
//  }
//
//  mutable irs::analysis::geo_token_stream stream{{}, ""};
//  std::unique_ptr<S2Region> region;
//};
//
//class geo_terms_filter_test_case : public tests::filter_test_case_base { };
//
//TEST_P(geo_terms_filter_test_case, test_region) {
//  // add segment
//  double_t lnglat[2];
//  double_t* coord = lnglat;
//
//  tests::json_doc_generator gen(
//    resource("simple_sequential_geo.json"),
//    [&](tests::document& doc,
//       const std::string& name,
//       const tests::json_doc_generator::json_value& data) {
//     if (name == "type") {
//       coord = lnglat;
//       ASSERT_TRUE(data.is_string());
//       ASSERT_EQ(data.str, irs::string_ref("Point"));
//     } else if (name == "coordinates") {
//       ASSERT_TRUE(data.is_number());
//       *coord++ = data.dbl;
//     } else if (name == "name") {
//       auto geo_field = std::make_shared<::geo_point_field>();
//       geo_field->name("point");
//       geo_field->point = S2Point(S2LatLng::FromDegrees(lnglat[1], lnglat[0]));
//       auto name_field = std::make_shared<tests::templates::string_field>("name");
//       name_field->value(data.str);
//
//       doc.insert(geo_field, true, true);
//       doc.insert(name_field, true, true);
//     }
//  });
//
//  add_segment(gen);
//
//  auto reader = open_reader();
//  ASSERT_EQ(1, reader.size());
//  auto& segment = reader[0];
//  auto* column = segment.column_reader("name");
//  ASSERT_NE(nullptr, column);
//  auto values = column->values();
//
//  const double_t distance = 0.000005;
//  const S1ChordAngle radius(S1Angle::Radians(S2Earth::MetersToRadians(distance)));
//
//  gen.reset();
//  irs::doc_id_t doc_id = irs::doc_limits::min();
//  while (auto* doc = gen.next()) {
//    auto& geo_field = static_cast<const geo_point_field&>(doc->indexed.front());
//    auto& name_field = static_cast<const tests::templates::string_field&>(doc->indexed.back());
//
//    irs::by_geo_terms q;
//    *q.mutable_field() = "point";
//    q.mutable_options()->reset(irs::GeoFilterType::INTERSECTS, S2Cap(geo_field.point, radius));
//
//    auto prepared = q.prepare(reader);
//    ASSERT_NE(nullptr, prepared);
//    auto docs = prepared->execute(segment);
//    ASSERT_NE(nullptr, docs);
//    ASSERT_TRUE(docs->next());
//    ASSERT_EQ(doc_id, docs->value());
//    ASSERT_FALSE(docs->next());
//    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
//
//    irs::bytes_ref value;
//    ASSERT_TRUE(values(doc_id, value));
//    ASSERT_EQ(name_field.value(), irs::to_string<irs::string_ref>(value.c_str()));
//    ++doc_id;
//  }
//}
//
//TEST_P(geo_terms_filter_test_case, test_point) {
//  // add segment
//  double_t lnglat[2];
//  double_t* coord = lnglat;
//
//  tests::json_doc_generator gen(
//    resource("simple_sequential_geo.json"),
//    [&](tests::document& doc,
//       const std::string& name,
//       const tests::json_doc_generator::json_value& data) {
//     if (name == "type") {
//       coord = lnglat;
//       ASSERT_TRUE(data.is_string());
//       ASSERT_EQ(data.str, irs::string_ref("Point"));
//     } else if (name == "coordinates") {
//       ASSERT_TRUE(data.is_number());
//       *coord++ = data.dbl;
//     } else if (name == "name") {
//       auto geo_field = std::make_shared<::geo_point_field>();
//       geo_field->name("point");
//       geo_field->point = S2Point(S2LatLng::FromDegrees(lnglat[1], lnglat[0]));
//       auto name_field = std::make_shared<tests::templates::string_field>("name");
//       name_field->value(data.str);
//
//       doc.insert(geo_field, true, true);
//       doc.insert(name_field, true, true);
//     }
//  });
//
//  add_segment(gen);
//
//  auto reader = open_reader();
//  ASSERT_EQ(1, reader.size());
//  auto& segment = reader[0];
//  auto* column = segment.column_reader("name");
//  ASSERT_NE(nullptr, column);
//  auto values = column->values();
//
//  gen.reset();
//  irs::doc_id_t doc_id = irs::doc_limits::min();
//  while (auto* doc = gen.next()) {
//    auto& geo_field = static_cast<const geo_point_field&>(doc->indexed.front());
//    auto& name_field = static_cast<const tests::templates::string_field&>(doc->indexed.back());
//
//    irs::by_geo_terms q;
//    *q.mutable_field() = "point";
//    q.mutable_options()->mutable_options()->set_index_contains_points_only(true);
//    q.mutable_options()->reset(geo_field.point);
//
//    auto prepared = q.prepare(reader);
//    ASSERT_NE(nullptr, prepared);
//    auto docs = prepared->execute(segment);
//    ASSERT_NE(nullptr, docs);
//    ASSERT_TRUE(docs->next());
//    ASSERT_EQ(doc_id, docs->value());
//    ASSERT_FALSE(docs->next());
//    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
//
//    irs::bytes_ref value;
//    ASSERT_TRUE(values(doc_id, value));
//    ASSERT_EQ(name_field.value(), irs::to_string<irs::string_ref>(value.c_str()));
//    ++doc_id;
//  }
//}
//
////#ifndef IRESEARCH_DLL
////TEST_P(term_filter_test_case, visit) {
////  // add segment
////  {
////    tests::json_doc_generator gen(
////      resource("simple_sequential.json"),
////      &tests::generic_json_field_factory);
////    add_segment(gen);
////  }
////  tests::empty_filter_visitor visitor;
////  std::string fld = "prefix";
////  irs::string_ref field = irs::string_ref(fld);
////  auto term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
////  // read segment
////  auto index = open_reader();
////  for (const auto& segment : index) {
////    // get term dictionary for field
////    const auto* reader = segment.field(field);
////    ASSERT_TRUE(reader != nullptr);
////
////    irs::term_query::visit(*reader, term, visitor);
////    ASSERT_EQ(1, visitor.prepare_calls_counter());
////    ASSERT_EQ(1, visitor.visit_calls_counter());
////    visitor.reset();
////  }
////}
////#endif
////
//
//
//INSTANTIATE_TEST_CASE_P(
//  geo_terms_filter_test,
//  geo_terms_filter_test_case,
//  ::testing::Combine(
//    ::testing::Values(
//      &tests::memory_directory,
//      &tests::fs_directory,
//      &tests::mmap_directory
//    ),
//    ::testing::Values("1_0")
//  ),
//  tests::to_string
//);
//
//NS_END
