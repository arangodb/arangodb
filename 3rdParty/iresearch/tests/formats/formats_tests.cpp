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
#include "formats/formats.hpp"

TEST(formats_tests, duplicate_register) {
  struct dummy_format: public irs::format {
    static ptr make() { return ptr(new dummy_format()); }
    dummy_format(): irs::format(dummy_format::type()) { }
    virtual irs::column_meta_writer::ptr get_column_meta_writer() const override { return nullptr; }
    virtual irs::column_meta_reader::ptr get_column_meta_reader() const override { return nullptr; }
    virtual irs::columnstore_writer::ptr get_columnstore_writer() const override { return nullptr; }
    virtual irs::columnstore_reader::ptr get_columnstore_reader() const override { return nullptr; }
    virtual irs::document_mask_writer::ptr get_document_mask_writer() const override { return nullptr; }
    virtual irs::document_mask_reader::ptr get_document_mask_reader() const override { return nullptr; }
    virtual irs::field_writer::ptr get_field_writer(bool) const override { return nullptr; }
    virtual irs::field_reader::ptr get_field_reader() const override { return nullptr; }
    virtual irs::index_meta_writer::ptr get_index_meta_writer() const override { return nullptr; }
    virtual irs::index_meta_reader::ptr get_index_meta_reader() const override { return nullptr; }
    virtual irs::segment_meta_writer::ptr get_segment_meta_writer() const override { return nullptr; }
    virtual irs::segment_meta_reader::ptr get_segment_meta_reader() const override { return nullptr; }
  };

  static bool initial_expected = true;

  // check required for tests with repeat (static maps are not cleared between runs)
  if (initial_expected) {
    ASSERT_FALSE(irs::formats::exists(irs::type<dummy_format>::name()));
    ASSERT_EQ(nullptr, irs::formats::get(irs::type<dummy_format>::name()));

    irs::format_registrar initial(irs::type<dummy_format>::get(), irs::string_ref::NIL, &dummy_format::make);
    ASSERT_EQ(!initial_expected, !initial);
  }

  initial_expected = false; // next test iteration will not be able to register the same format
  irs::format_registrar duplicate(irs::type<dummy_format>::get(), "foo", &dummy_format::make);
  ASSERT_TRUE(!duplicate);

  ASSERT_TRUE(irs::formats::exists(irs::type<dummy_format>::name()));
  ASSERT_NE(nullptr, irs::formats::get(irs::type<dummy_format>::name()));
}
