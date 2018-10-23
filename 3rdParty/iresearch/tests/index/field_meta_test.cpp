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
    ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::field_id_t>::valid(fm.norm));
    ASSERT_EQ(iresearch::flags::empty_instance(), fm.features);
  }

  {
    iresearch::flags features;
    features.add<iresearch::offset>();
    features.add<increment>();
    features.add<offset>();

    const std::string name("name");
    const field_meta fm(name, features, 5);
    ASSERT_EQ(name, fm.name);
    ASSERT_EQ(features, fm.features);
    ASSERT_EQ(5, fm.norm);
  }
}

TEST(field_meta_test, move) {
  iresearch::flags features;
  features.add<iresearch::offset>();
  features.add<increment>();
  features.add<offset>();

  const field_id norm = 10;
  const std::string name("name");

  // ctor
  {
    field_meta moved;
    moved.name = name;
    moved.features = features;
    moved.norm = norm;

    field_meta fm(std::move(moved));
    ASSERT_EQ(name, fm.name);
    ASSERT_EQ(features, fm.features);
    ASSERT_EQ(norm, fm.norm);

    ASSERT_EQ("", moved.name);
    ASSERT_EQ(iresearch::flags::empty_instance(), moved.features);
    ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::field_id_t>::valid(moved.norm));
  }

  // assign operator
  {
    field_meta moved;
    moved.name = name;
    moved.features = features;
    moved.norm = norm;

    field_meta fm;
    ASSERT_EQ("", fm.name);
    ASSERT_EQ(iresearch::flags::empty_instance(), fm.features);
    ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::field_id_t>::valid(fm.norm));

    fm = std::move(moved);
    ASSERT_EQ(name, fm.name);
    ASSERT_EQ(features, fm.features);
    ASSERT_EQ(norm, fm.norm);

    ASSERT_EQ("", moved.name);
    ASSERT_EQ(iresearch::flags::empty_instance(), moved.features);
    ASSERT_FALSE(iresearch::type_limits<iresearch::type_t::field_id_t>::valid(moved.norm));
  }
}

TEST(field_meta_test, compare) {
  iresearch::flags features;
  features.add<offset>();
  features.add<increment>();
  features.add<offset>();
  features.add<document>();

  const field_id id = 5;
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
