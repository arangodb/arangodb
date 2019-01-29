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
#include "utils/attributes.hpp"

NS_LOCAL

template< typename T >
inline T* copy_attribute(irs::attribute_store& dst, const irs::attribute_store& src) {
  typedef typename std::enable_if<std::is_base_of<irs::attribute, T>::value, T> ::type type;

  type* dsta = nullptr;
  const type* srca = src.get<type>().get();
  
  if( srca) {
    *(dsta = dst.emplace<type>().get()) = *srca;
  }

  return dsta;
}

NS_END

using namespace iresearch;

namespace tests {

struct attribute : iresearch::stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();

  virtual void clear() { value = 0; }

  int value = 5;

  bool operator==(const attribute& rhs ) const {
    return value == rhs.value;
  }
};

DEFINE_ATTRIBUTE_TYPE(tests::attribute)
DEFINE_FACTORY_DEFAULT(attribute)

struct invalid_attribute : iresearch::stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();

  virtual void clear() { value = 0; }

  int value = 5;
};

DEFINE_ATTRIBUTE_TYPE(tests::invalid_attribute)
DEFINE_FACTORY_DEFAULT(invalid_attribute)

} // tests

TEST(attributes_tests, duplicate_register) {
  struct dummy_attribute: public irs::attribute {
    DECLARE_ATTRIBUTE_TYPE() { static irs::attribute::type_id type("dummy_attribute"); return type; }
  };

  static bool initial_expected = true;

  // check required for tests with repeat (static maps are not cleared between runs)
  if (initial_expected) {
    ASSERT_FALSE(irs::attribute::type_id::exists("dummy_attribute"));
    ASSERT_EQ(nullptr, irs::attribute::type_id::get("dummy_attribute"));

    irs::attribute_registrar initial(dummy_attribute::type());
    ASSERT_EQ(!initial_expected, !initial);
  }

  initial_expected = false; // next test iteration will not be able to register the same attribute
  irs::attribute_registrar duplicate(dummy_attribute::type());
  ASSERT_TRUE(!duplicate);

  ASSERT_TRUE(irs::attribute::type_id::exists("dummy_attribute"));
  ASSERT_NE(nullptr, irs::attribute::type_id::get("dummy_attribute"));
}

TEST(attributes_tests, store_ctor) {
  irs::attribute_store attrs;

  ASSERT_EQ(0, attrs.size());
  ASSERT_EQ(flags{}, attrs.features());

  {
    attrs.emplace<tests::attribute>();
    irs::attribute_store attrs1( std::move(attrs));
    ASSERT_EQ(1, attrs1.size());
    ASSERT_EQ(flags{tests::attribute::type()}, attrs1.features());
    ASSERT_TRUE(attrs1.contains<tests::attribute>());
  }

  ASSERT_EQ(0, attrs.size());
  ASSERT_EQ(flags{}, attrs.features());
  ASSERT_FALSE(attrs.contains<tests::attribute>());
}

TEST(attributes_tests, store_copy) {
  irs::attribute_store src;
  auto& added = src.emplace<tests::attribute>();
  added->value = 10;

  // copy attribute
  irs::attribute_store dst;
  tests::attribute* copied = copy_attribute<tests::attribute>(dst, src);
  ASSERT_NE(nullptr, copied);
  ASSERT_NE(&*added, copied);
  ASSERT_EQ(*added, *copied);

  /* copy invalid attribute */
  tests::invalid_attribute* missed_copied = copy_attribute<tests::invalid_attribute>(src,dst);
  ASSERT_FALSE(missed_copied);
}

TEST(attributes_tests, store_add_get_clear_state_clear) {
  irs::attribute_store attrs;
  auto& added = attrs.emplace<tests::attribute>();
  ASSERT_FALSE(!added);
  ASSERT_EQ(1, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::attribute>());
  ASSERT_EQ(flags{added->type()}, attrs.features());

  // add attribute
  {
    auto& added1 = attrs.emplace<tests::attribute>();
    ASSERT_EQ(&*added, &*added1);
    ASSERT_EQ(1, attrs.size());
    ASSERT_TRUE(attrs.contains<tests::attribute>());
    ASSERT_EQ(flags{added->type()}, attrs.features());
  }

  // get attribute
  {
    auto& added1 = added; //const_cast<const irs::attribute_store&>(attrs).get(added->type());
    ASSERT_FALSE(!added1);
    auto& added2 = const_cast<const irs::attribute_store&>(attrs).get<tests::attribute>();
    ASSERT_FALSE(!added2);
    ASSERT_EQ(added.get(), added2.get());
    ASSERT_EQ(reinterpret_cast<void*>(added1.get()), added2.get());
  }

//  // clear state
//  attrs.visit([](
//      const attribute::type_id&,
//      attribute_store::ref<void>& ref
//  )->bool {
//    reinterpret_cast<attribute*>(ref.get())->clear();
//    return true;
//  });
//  ASSERT_EQ(0, added->value);

  //add attribute
  attrs.emplace<tests::invalid_attribute>();
  ASSERT_EQ(2, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::invalid_attribute>());
  ASSERT_EQ(flags({tests::attribute::type(), tests::invalid_attribute::type()}), attrs.features());

  /* remove attribute */
  attrs.remove<tests::invalid_attribute>();
  ASSERT_EQ(1, attrs.size());
  ASSERT_FALSE(attrs.contains<tests::invalid_attribute>());
  ASSERT_EQ(flags{added->type()}, attrs.features());

  /* clear */
  attrs.clear();
  ASSERT_EQ(0, attrs.size());
  ASSERT_FALSE(attrs.contains<tests::attribute>());
  ASSERT_EQ(flags{}, attrs.features());
}

TEST(attributes_tests, store_visit) {
  irs::attribute_store attrs;

  // add first attribute
  ASSERT_FALSE(!attrs.emplace<tests::attribute>());
  ASSERT_EQ(1, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::attribute>());
  ASSERT_EQ(flags{tests::attribute::type()}, attrs.features());

  // add second attribute
  ASSERT_FALSE(!attrs.emplace<tests::invalid_attribute>());
  ASSERT_EQ(2, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::invalid_attribute>());
  ASSERT_EQ(flags({tests::attribute::type(), tests::invalid_attribute::type()}), attrs.features());

  // visit 2 attributes
  {
    std::unordered_set<const attribute::type_id*> expected;
    auto visitor = [&expected](const attribute::type_id& type_id, const attribute_store::ref<attribute>::type&)->bool {
      return 1 == expected.erase(&type_id);
    };

    expected.insert(&(tests::attribute::type()));
    expected.insert(&(tests::invalid_attribute::type()));
    ASSERT_TRUE(attrs.visit(visitor));
    ASSERT_TRUE(expected.empty());
  }
}

TEST(attributes_tests, view_ctor) {
  irs::attribute_view attrs;

  ASSERT_EQ(0, attrs.size());
  ASSERT_EQ(flags{}, attrs.features());

  {
    tests::attribute value;
    attrs.emplace(value);
    irs::attribute_view attrs1(std::move(attrs));
    ASSERT_EQ(1, attrs1.size());
    ASSERT_EQ(flags{tests::attribute::type()}, attrs1.features());
    ASSERT_TRUE(attrs1.contains<tests::attribute>());
    ASSERT_EQ(&value, attrs1.get<tests::attribute>()->get());
  }

  ASSERT_EQ(0, attrs.size());
  ASSERT_EQ(flags{}, attrs.features());
  ASSERT_FALSE(attrs.contains<tests::attribute>());
}

TEST(attributes_tests, view_add_placeholder) {
  irs::attribute_view attrs;
  auto& added = attrs.emplace<tests::attribute>();
  ASSERT_TRUE(!added);
  ASSERT_EQ(1, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::attribute>());
  ASSERT_EQ(nullptr, attrs.get<tests::attribute>()->get());

  tests::attribute value;
  added = &value;
  ASSERT_FALSE(!added);
  ASSERT_EQ(&value, attrs.get<tests::attribute>()->get());
}

TEST(attributes_tests, view_add_get_clear_state_clear) {
  irs::attribute_view attrs;
  tests::attribute value0;
  auto& added = attrs.emplace(value0);
  ASSERT_FALSE(!added);
  ASSERT_EQ(1, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::attribute>());
  ASSERT_EQ(&value0, attrs.get<tests::attribute>()->get());
  ASSERT_EQ(flags{added->type()}, attrs.features());

  // add attribute
  {
    tests::attribute value;
    auto& added1 = attrs.emplace(value);
    ASSERT_EQ(added.get(), added1.get());
    ASSERT_EQ(1, attrs.size());
    ASSERT_TRUE(attrs.contains<tests::attribute>());
    ASSERT_EQ(&value0, attrs.get<tests::attribute>()->get());
    ASSERT_EQ(flags{added->type()}, attrs.features());
  }

  // get attribute
  {
    auto& added1 = added; //const_cast<const irs::attribute_view&>(attrs).get(added->type());
    ASSERT_FALSE(!added1);
    auto& added2 = const_cast<const irs::attribute_view&>(attrs).get<tests::attribute>();
    ASSERT_FALSE(!added2);
    ASSERT_EQ(added.get(), added2.get());
    ASSERT_EQ(reinterpret_cast<const void*>(added1.get()), added2.get());
  }

//  // clear state
//  attrs.visit([](
//      const attribute::type_id&,
//      attribute_view::ref<void>& ref
//  )->bool {
//    reinterpret_cast<attribute*>(ref.get())->clear();
//    return true;
//  });
//  ASSERT_EQ(0, added->value);

  // add attribute
  tests::invalid_attribute value1;
  attrs.emplace(value1);
  ASSERT_EQ(2, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::invalid_attribute>());
  ASSERT_EQ(&value1, attrs.get<tests::invalid_attribute>()->get());
  ASSERT_EQ(flags({tests::attribute::type(), tests::invalid_attribute::type()}), attrs.features());

  // remove attribute
  attrs.remove<tests::invalid_attribute>();
  ASSERT_EQ(1, attrs.size());
  ASSERT_FALSE(attrs.contains<tests::invalid_attribute>());
  ASSERT_EQ(flags{added->type()}, attrs.features());

  // clear
  attrs.clear();
  ASSERT_EQ(0, attrs.size());
  ASSERT_FALSE(attrs.contains<tests::attribute>());
  ASSERT_EQ(flags{}, attrs.features());
}

TEST(attributes_tests, view_visit) {
  irs::attribute_view attrs;
  tests::attribute value0;
  tests::invalid_attribute value1;

  // add first attribute
  ASSERT_FALSE(!attrs.emplace(value0));
  ASSERT_EQ(1, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::attribute>());
  ASSERT_EQ(&value0, attrs.get<tests::attribute>()->get());
  ASSERT_EQ(flags{tests::attribute::type()}, attrs.features());

  // add second attribute
  ASSERT_FALSE(!attrs.emplace(value1));
  ASSERT_EQ(2, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::invalid_attribute>());
  ASSERT_EQ(&value1, attrs.get<tests::invalid_attribute>()->get());
  ASSERT_EQ(flags({tests::attribute::type(), tests::invalid_attribute::type()}), attrs.features());

  // visit 2 attributes
  {
    std::unordered_set<const attribute::type_id*> expected;
    auto visitor = [&expected](const attribute::type_id& type_id, const attribute_view::ref<attribute>::type&)->bool {
      return 1 == expected.erase(&type_id);
    };

    expected.insert(&(tests::attribute::type()));
    expected.insert(&(tests::invalid_attribute::type()));
    ASSERT_TRUE(attrs.visit(visitor));
    ASSERT_TRUE(expected.empty());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------