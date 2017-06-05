//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "tests_shared.hpp"
#include "utils/attributes.hpp"

using namespace iresearch;

namespace tests {

struct attribute : iresearch::attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();

  virtual void clear() { value = 0; }

  attribute() : iresearch::attribute(attribute::type()) { }

  int value = 5;

  bool operator==(const attribute& rhs ) const {
    return value == rhs.value;
  }
};

DEFINE_ATTRIBUTE_TYPE(tests::attribute);
DEFINE_FACTORY_DEFAULT(attribute);

struct invalid_attribute : iresearch::attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();

  virtual void clear() { value = 0; }

  invalid_attribute() : attribute(invalid_attribute::type()) { }

  int value = 5;
};

DEFINE_ATTRIBUTE_TYPE(tests::invalid_attribute);
DEFINE_FACTORY_DEFAULT(invalid_attribute);

} // tests

TEST(attributes_tests, duplicate_register) {
  struct dummy_attribute: public irs::attribute {
    DECLARE_ATTRIBUTE_TYPE() { static irs::attribute::type_id type("dummy_attribute"); return type; }
    dummy_attribute(): irs::attribute(dummy_attribute::type()) { }
  };

  static bool initial_expected = true;
  irs::attribute_registrar initial(dummy_attribute::type());
  ASSERT_EQ(!initial_expected, !initial);
  initial_expected = false; // next test iteration will not be able to register the same attribute
  irs::attribute_registrar duplicate(dummy_attribute::type());
  ASSERT_TRUE(!duplicate);
}

TEST( attributes_tests, ctor) {
  attributes attrs;

  ASSERT_EQ(0, attrs.size());
  ASSERT_EQ(flags{}, attrs.features());

  {
    attrs.add<tests::attribute>();
    attributes attrs1( std::move(attrs));
    ASSERT_EQ(1, attrs1.size());
    ASSERT_EQ(flags{tests::attribute::type()}, attrs1.features());
    ASSERT_TRUE(attrs1.contains<tests::attribute>());
  }

  ASSERT_EQ(0, attrs.size());
  ASSERT_EQ(flags{}, attrs.features());
  ASSERT_FALSE(attrs.contains<tests::attribute>());
}

TEST( attributes_tests, copy) {
  attributes src;
  auto& added = src.add<tests::attribute>();
  added->value = 10;

  /* copy attribute */
  attributes dst;
  tests::attribute* copied = copy_attribute<tests::attribute>(dst, src);
  ASSERT_NE(nullptr, copied);
  ASSERT_NE(&*added, copied);
  ASSERT_EQ(*added, *copied);

  /* copy invalid attribute */
  tests::invalid_attribute* missed_copied = copy_attribute<tests::invalid_attribute>(src,dst);
  ASSERT_FALSE(missed_copied);
}

TEST( attributes_tests, add_get_clear_state_clear) {
  attributes attrs;
  auto& added = attrs.add<tests::attribute>();
  ASSERT_FALSE(!added);
  ASSERT_EQ(1, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::attribute>());
  ASSERT_EQ(flags{added->type()}, attrs.features());

  /* add attribute */
  {
    auto& added1 = attrs.add<tests::attribute>();
    ASSERT_EQ(&*added, &*added1);
    ASSERT_EQ(1, attrs.size());
    ASSERT_TRUE(attrs.contains<tests::attribute>());
    ASSERT_EQ(flags{added->type()}, attrs.features());
  }

  /* get attribute */
  {
    auto& added1 = attrs.get(added->type());
    ASSERT_FALSE(!added1);
    auto& added2 = attrs.get<tests::attribute>();
    ASSERT_FALSE(!added2);
    ASSERT_EQ(&*added, &*added2);
    ASSERT_EQ(&*added1, &*added2);
  }

  /* clear state */
  attrs.clear_state();
  ASSERT_EQ(0, added->value);

  /*add attribute */
  attrs.add<tests::invalid_attribute>();
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

TEST(attributes_tests, visit) {
  attributes attrs;

  // add first attribute
  ASSERT_FALSE(!attrs.add<tests::attribute>());
  ASSERT_EQ(1, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::attribute>());
  ASSERT_EQ(flags{tests::attribute::type()}, attrs.features());

  // add second attribute
  ASSERT_FALSE(!attrs.add<tests::invalid_attribute>());
  ASSERT_EQ(2, attrs.size());
  ASSERT_TRUE(attrs.contains<tests::invalid_attribute>());
  ASSERT_EQ(flags({tests::attribute::type(), tests::invalid_attribute::type()}), attrs.features());


  // visit 2 attributes
  {
    std::unordered_set<const attribute::type_id*> expected;
    auto visitor = [&expected](const attribute::type_id& type_id, const attribute_ref<attribute>&)->bool {
      return 1 == expected.erase(&type_id);
    };

    expected.insert(&(tests::attribute::type()));
    expected.insert(&(tests::invalid_attribute::type()));
    ASSERT_TRUE(attrs.visit(visitor));
    ASSERT_TRUE(expected.empty());
  }
}
