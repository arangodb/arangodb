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

TEST(GeoDistanceFilterTest, options) {
  S2RegionTermIndexer::Options const s2opts;
  GeoDistanceFilterOptions const opts;
  ASSERT_TRUE(opts.prefix.empty());
  ASSERT_EQ(0., opts.range.min);
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.min_type);
  ASSERT_EQ(0., opts.range.max);
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.max_type);
  ASSERT_EQ(S2Point{}, opts.origin);
  ASSERT_EQ(s2opts.level_mod(), opts.options.level_mod());
  ASSERT_EQ(s2opts.min_level(), opts.options.min_level());
  ASSERT_EQ(s2opts.max_level(), opts.options.max_level());
  ASSERT_EQ(s2opts.max_cells(), opts.options.max_cells());
  ASSERT_EQ(s2opts.marker(), opts.options.marker());
  ASSERT_EQ(s2opts.index_contains_points_only(), opts.options.index_contains_points_only());
  ASSERT_EQ(s2opts.optimize_for_space(), opts.options.optimize_for_space());
}

TEST(GeoDistanceFilterTest, ctor) {
  GeoDistanceFilter q;
  ASSERT_EQ(irs::type<GeoDistanceFilter>::id(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::no_boost(), q.boost());
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
  ASSERT_EQ(GeoDistanceFilterOptions{}, q.options());
#endif
}

TEST(GeoDistanceFilterTest, equal) {
  GeoDistanceFilter q;
  q.mutable_options()->origin = S2Point{1., 2., 3.};
  q.mutable_options()->range.min = 5000.;
  q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q.mutable_options()->range.max = 7000.;
  q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
  *q.mutable_field() = "field";

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoDistanceFilter q1;
    q1.boost(1.5);
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoDistanceFilter q1;
    q1.boost(1.5);
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field1";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 6000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 6000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{2., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }
}

TEST(GeoDistanceFilterTest, boost) {
  {
    GeoDistanceFilter q;
    q.mutable_options()->origin = S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  {
    irs::boost_t boost = 1.5f;
    GeoDistanceFilter q;
    q.mutable_options()->origin = S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max = 6000.;
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }

  {
    irs::boost_t boost = 1.5f;
    GeoDistanceFilter q;
    q.mutable_options()->origin = S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }

  {
    irs::boost_t boost = 1.5f;
    GeoDistanceFilter q;
    q.mutable_options()->origin = S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max = 6000.;
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}
