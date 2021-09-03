////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "index/field_meta.hpp"
#include "analysis/token_attributes.hpp"

using namespace iresearch;

TEST(field_meta_test, ctor) {
  {
    const field_meta fm;
    ASSERT_EQ("", fm.name);
    ASSERT_EQ(irs::IndexFeatures::NONE, fm.index_features);
    ASSERT_TRUE(fm.features.empty());
  }

  {
    const std::string name("name");
    const field_meta fm(name, irs::IndexFeatures::OFFS);
    ASSERT_EQ(name, fm.name);
    ASSERT_TRUE(fm.features.empty());
    ASSERT_EQ(irs::IndexFeatures::OFFS, fm.index_features);
  }
}

TEST(field_meta_test, move) {
  const field_id norm = 10;
  const std::string name("name");

  irs::feature_map_t features;
  features[irs::type<irs::offset>::id()] = irs::field_limits::invalid();
  features[irs::type<irs::increment>::id()] = irs::field_limits::invalid();

  // ctor
  {
    field_meta moved;
    moved.name = name;
    moved.index_features = irs::IndexFeatures::FREQ;
    moved.features = features;

    field_meta fm(std::move(moved));
    ASSERT_EQ(name, fm.name);
    ASSERT_EQ(features, fm.features);
    ASSERT_EQ(irs::IndexFeatures::FREQ, fm.index_features);

    ASSERT_TRUE(moved.name.empty());
    ASSERT_TRUE(moved.features.empty());
    ASSERT_EQ(irs::IndexFeatures::NONE, moved.index_features);
  }

  // assign operator
  {
    field_meta moved;
    moved.name = name;
    moved.index_features = irs::IndexFeatures::FREQ;
    moved.features = features;

    field_meta fm;
    ASSERT_TRUE(fm.name.empty());
    ASSERT_TRUE(fm.features.empty());
    ASSERT_EQ(irs::IndexFeatures::NONE, fm.index_features);

    fm = std::move(moved);
    ASSERT_EQ(name, fm.name);
    ASSERT_EQ(features, fm.features);
    ASSERT_EQ(irs::IndexFeatures::FREQ, fm.index_features);

    ASSERT_TRUE(moved.name.empty());
    ASSERT_TRUE(moved.features.empty());
    ASSERT_EQ(irs::IndexFeatures::NONE, moved.index_features);
  }
}

TEST(field_meta_test, compare) {
  irs::feature_map_t features;
  features[irs::type<irs::offset>::id()]    = irs::field_limits::invalid();
  features[irs::type<irs::increment>::id()] = irs::field_limits::invalid();
  features[irs::type<irs::document>::id()]  = irs::field_limits::invalid();

  const std::string name("name");

  field_meta lhs;
  lhs.name = name;
  lhs.features = features;
  field_meta rhs = lhs;
  ASSERT_EQ(lhs, rhs);

  rhs.name = "test";
  ASSERT_NE(lhs, rhs);
  lhs.name = "test";
  ASSERT_EQ(lhs, rhs);
}
