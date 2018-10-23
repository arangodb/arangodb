////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include "analysis/token_attributes.hpp"
#include "index/segment_writer.hpp"
#include "index/index_tests.hpp"
#include "store/memory_directory.hpp"

NS_LOCAL

class segment_writer_tests: public test_base {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).

    test_base::SetUp();
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).

    test_base::TearDown();
  }
};

NS_END

TEST_F(segment_writer_tests, memory) {
  struct token_stream_t: public irs::token_stream {
    irs::attribute_view attrs;
    size_t token_count;
    virtual const irs::attribute_view& attributes() const NOEXCEPT override { return attrs; }
    virtual bool next() override { return --token_count; }
  };

  struct field_t {
    irs::token_stream& token_stream;
    field_t(irs::token_stream& stream): token_stream(stream) {}
    float_t boost() const { return 1.f; }
    const irs::flags& features() const { return irs::flags::empty_instance(); }
    irs::token_stream& get_tokens() { return token_stream; }
    irs::string_ref& name() const { static irs::string_ref value("test_field"); return value; }
  };

  irs::boolean_token_stream stream;
  stream.reset(true);
  field_t field(stream);

  irs::memory_directory dir;
  auto writer = irs::segment_writer::make(dir);
  ASSERT_EQ(0, writer->memory_active());

  for (size_t i = 0; i < 100; ++i) {
    irs::segment_writer::update_context ctx;
    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_TRUE(writer->insert(irs::action::index, field));
    ASSERT_TRUE(writer->valid());
    writer->commit();
  }

  ASSERT_LT(0, writer->memory_active());

  writer->reset();

  ASSERT_EQ(0, writer->memory_active());
}

TEST_F(segment_writer_tests, index_field) {
  struct token_stream_t: public irs::token_stream {
    irs::attribute_view attrs;
    size_t token_count;
    virtual const irs::attribute_view& attributes() const NOEXCEPT override { return attrs; }
    virtual bool next() override { return --token_count; }
  };

  struct field_t {
    irs::token_stream& token_stream;
    field_t(irs::token_stream& stream): token_stream(stream) {}
    float_t boost() const { return 1.f; }
    const irs::flags& features() const { return irs::flags::empty_instance(); }
    irs::token_stream& get_tokens() { return token_stream; }
    irs::string_ref& name() const { static irs::string_ref value("test_field"); return value; }
  };

  // test missing token_stream attributes (increment)
  {
    irs::memory_directory dir;
    auto writer = irs::segment_writer::make(dir);
    irs::segment_writer::update_context ctx;
    token_stream_t stream;
    field_t field(stream);
    irs::term_attribute term;

    stream.attrs.emplace<irs::term_attribute>(term);
    stream.token_count = 10;

    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_FALSE(writer->insert(irs::action::index, field));
    ASSERT_FALSE(writer->valid());
    writer->commit();
  }

  // test missing token_stream attributes (term_attribute)
  {
    irs::memory_directory dir;
    auto writer = irs::segment_writer::make(dir);
    irs::segment_writer::update_context ctx;
    token_stream_t stream;
    field_t field(stream);
    irs::increment inc;

    stream.attrs.emplace<irs::increment>(inc);
    stream.token_count = 10;

    writer->begin(ctx);
    ASSERT_TRUE(writer->valid());
    ASSERT_FALSE(writer->insert(irs::action::index, field));
    ASSERT_FALSE(writer->valid());
    writer->commit();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------