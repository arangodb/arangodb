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
#include "formats/skip_list.hpp"
#include "store/memory_directory.hpp"
#include "index/iterators.hpp"
#include "index/index_tests.hpp"

using namespace std::chrono_literals;

namespace {

class skip_writer_test : public test_base {
  virtual void SetUp() {
    test_base::SetUp();
  }

  virtual void TearDown() {
    test_base::TearDown();
  }

 protected:
  void write_flush(size_t count, size_t max_levels, size_t skip) {
    std::vector<std::vector<int>> levels(max_levels);

    irs::skip_writer writer(skip, skip);
    ASSERT_FALSE(writer);
    irs::memory_directory dir;

    // write data
    {
      size_t cur_doc = irs::integer_traits<size_t>::const_max;

      writer.prepare(
        max_levels, count,
        [&levels, &cur_doc](size_t level, irs::index_output& out) {
          levels[level].push_back(irs::doc_id_t(cur_doc));
          out.write_vlong(irs::doc_id_t(cur_doc));
      });
      ASSERT_TRUE(static_cast<bool>(writer));

      for (size_t doc = 0; doc < count; ++doc) {
        cur_doc = doc;
        // skip every "skip" document
        if (doc && 0 == doc % skip) {
          writer.skip(doc);
        }
      }

      auto out = dir.create("docs");
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }

    // check levels data
    {
      size_t step = skip;
      for (auto& level : levels) {
        int expected_doc = 0;

        for (auto doc : level) {
          expected_doc += irs::doc_id_t(step);
          ASSERT_EQ(expected_doc, doc);
        }
        step *= skip;
      }
    }

    // check data in stream
    // we write levels in reverse order into a stream (from n downto 0)
    {
      auto in = dir.open("docs", irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);

      // skip number of levels
      in->read_vint();

      // check levels from n downto 1
      for (size_t i = writer.num_levels(); i > 1; --i) {
        // skip level size
        in->read_vlong();
        auto& level = levels[i-1];
        for (auto expected_doc : level) {
          auto doc = in->read_vint();
          ASSERT_EQ(expected_doc, doc);
          // skip child pointer
          in->read_vlong();
        }
      }

      // check level 0
      {
        // skip level size
        in->read_vlong();
        auto& level = levels[0];
        for (auto expected_doc : level) {
          auto doc = in->read_vint();
          ASSERT_EQ(expected_doc, doc);
        }
      }
    }
  }
};

class skip_reader_test : public test_base {
  virtual void SetUp() {
    test_base::SetUp();
  }

  virtual void TearDown() {
    test_base::TearDown();
  }
};

}

TEST_F(skip_writer_test, prepare) {
  // empty doc count
  {
    const size_t max_levels = 10;
    const size_t doc_count = 0;
    const size_t skip_n = 8;
    const size_t skip_0 = 16;
    irs::skip_writer writer(skip_0, skip_n);
    ASSERT_FALSE(writer);
    writer.prepare(max_levels, doc_count);
    ASSERT_TRUE(static_cast<bool>(writer));
    ASSERT_EQ(skip_0, writer.skip_0());
    ASSERT_EQ(skip_n, writer.skip_n());
    ASSERT_EQ(0, writer.num_levels());
  }

  // at least 1 level must exists
  {
    const size_t max_levels = 0;
    const size_t doc_count = 0;
    const size_t skip_n = 8;
    const size_t skip_0 = 16;
    irs::skip_writer writer(skip_0, skip_n);
    ASSERT_FALSE(writer);
    writer.prepare(max_levels, doc_count);
    ASSERT_TRUE(static_cast<bool>(writer));
    ASSERT_EQ(skip_0, writer.skip_0());
    ASSERT_EQ(skip_n, writer.skip_n());
    ASSERT_EQ(0, writer.num_levels());
  }

  // less than max levels
  {
    const size_t doc_count = 1923;
    const size_t skip = 8;
    const size_t max_levels = 10;
    irs::skip_writer writer(skip, skip);
    ASSERT_FALSE(writer);
    writer.prepare(max_levels, doc_count);
    ASSERT_TRUE(static_cast<bool>(writer));
    ASSERT_EQ(skip, writer.skip_0());
    ASSERT_EQ(skip, writer.skip_n());
    ASSERT_EQ(3, writer.num_levels());
  }

  // more than max levels 
  {
    const size_t doc_count = 1923000;
    const size_t skip = 8;
    const size_t max_levels = 5;
    irs::skip_writer writer(skip, skip);
    ASSERT_FALSE(writer);
    writer.prepare(max_levels, doc_count);
    ASSERT_TRUE(static_cast<bool>(writer));
    ASSERT_EQ(skip, writer.skip_0());
    ASSERT_EQ(skip, writer.skip_n());
    ASSERT_EQ(5, writer.num_levels());
  }
}

TEST_F(skip_writer_test, write_flush) {
  skip_writer_test::write_flush(1923, 5, 8);
}

TEST_F(skip_writer_test, reset) {
  const size_t skip = 13;
  const size_t max_levels = 10;
  const size_t count = 2873;

  std::vector<int> levels[max_levels];
  size_t cur_doc = irs::integer_traits<size_t>::const_max;

  // prepare writer
  irs::skip_writer writer(skip, skip);
  ASSERT_FALSE(writer);
  writer.prepare(
    max_levels, count,
    [&cur_doc, &levels](size_t level, irs::index_output& out) {
      levels[level].push_back(cur_doc);
      out.write_vlong(cur_doc);
  });
  ASSERT_TRUE(static_cast<bool>(writer));

  // prepare directory
  irs::memory_directory dir;

  // write initial data
  {
    const std::string file = "docs_0";

    // write data
    {
      for (size_t i = 0; i < count; ++i) {
        cur_doc = i;
        if (i && 0 == i % skip) {
          writer.skip(i);
        }
      }

      auto out = dir.create(file);
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }
  
    // check levels data
    {
      size_t step = skip;
      for (auto& level : levels) {
        int expected_doc = 0;

        for (auto doc : level) {
          expected_doc += irs::doc_id_t(step);
          ASSERT_EQ(expected_doc, doc);
        }
        step *= skip;
      }
    }

    // check data in stream
    // we write levels in reverse order into a stream (from n downto 0)
    {
      auto in = dir.open(file, irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);

      // skip number of levels
      in->read_vint();

      // check levels from n downto 1
      for (size_t i = writer.num_levels(); i > 1; --i) {
        // skip level size
        in->read_vlong();
        auto& level = levels[i-1];
        for (auto expected_doc : level) {
          auto doc = in->read_vint();
          ASSERT_EQ(expected_doc, doc);
          // skip child pointer
          in->read_vlong();
        }
      }

      // check level 0
      {
        // skip level size
        in->read_vlong();
        auto& level = levels[0];
        for (auto expected_doc : level) {
          auto doc = in->read_vint();
          ASSERT_EQ(expected_doc, doc);
        }
      }
    }
  }

  // reset writer and write another docs into another stream
  {
    writer.reset();
    for (auto& level : levels) {
      level.clear();
    }

    const std::string file = "docs_1";

    // write data
    {
      for (size_t i = 0; i < count; ++i) {
        cur_doc = 2*i;
        // skip every "skip" document
        if (i && 0 == i % skip) {
          writer.skip(i);
        }
      }

      auto out = dir.create(file);
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }
    
    // check levels data
    {
      size_t step = skip;
      for (auto& level : levels) {
        int expected_doc = 0;

        for (auto doc : level) {
          expected_doc += irs::doc_id_t(2*step);
          ASSERT_EQ(expected_doc, doc);
        }
        step *= skip;
      }
    }

    // check data in stream
    // we write levels in reverse order into a stream (from n downto 0)
    {
      auto in = dir.open(file, irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);

      // skip number of levels
      in->read_vint();

      // check levels from n downto 1
      for (size_t i = writer.num_levels(); i > 1; --i) {
        // skip level size
        in->read_vlong();
        auto& level = levels[i-1];
        for (auto expected_doc : level) {
          auto doc = in->read_vint();
          ASSERT_EQ(expected_doc, doc);
          // skip child pointer
          in->read_vlong();
        }
      }

      // check level 0
      {
        // skip level size
        in->read_vlong();
        auto& level = levels[0];
        for (auto expected_doc : level) {
          auto doc = in->read_vint();
          ASSERT_EQ(expected_doc, doc);
        }
      }
    }
  }
}

TEST_F(skip_reader_test, prepare) {
  // prepare empty
  {
    size_t count = 1000;
    size_t max_levels = 5;
    size_t skip = 8;

    irs::skip_writer writer(skip, skip);
    ASSERT_FALSE(writer);
    writer.prepare(max_levels, count);
    ASSERT_TRUE(static_cast<bool>(writer));
    irs::memory_directory dir;
    {
      auto out = dir.create("docs");
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }

    irs::skip_reader reader(skip, skip);
    ASSERT_FALSE(reader);
    {
      auto in = dir.open("docs", irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);
      reader.prepare(std::move(in));
    }
    ASSERT_TRUE(static_cast<bool>(reader));
    ASSERT_EQ(0, reader.num_levels());
    ASSERT_EQ(skip, reader.skip_0());
    ASSERT_EQ(skip, reader.skip_n());
  }
 
  // prepare not empty
  {
    size_t count = 1932;
    size_t max_levels = 5;
    size_t skip = 8;

    irs::skip_writer writer(skip, skip);
    ASSERT_FALSE(writer);
    irs::memory_directory dir;

    // write data
    {
      writer.prepare(
        max_levels, count,
        [](size_t, irs::index_output& out) {
          out.write_vint(0);
      });
      ASSERT_TRUE(static_cast<bool>(writer));

      for (size_t i = 0; i <= count; ++i) {
        if (i && 0 == i % skip) {
          writer.skip(i);
        }
      }

      auto out = dir.create("docs");
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }

    irs::skip_reader reader(skip, skip);
    ASSERT_FALSE(reader);
    auto in = dir.open("docs", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!in);
    reader.prepare(std::move(in));
    ASSERT_TRUE(static_cast<bool>(reader));
    ASSERT_EQ(writer.num_levels(), reader.num_levels());
    ASSERT_EQ(skip, reader.skip_0());
    ASSERT_EQ(skip, reader.skip_n());
  }
}

TEST_F(skip_reader_test, seek) {
  {
    const size_t count = 1932;
    const size_t max_levels = 5;
    const size_t skip_0 = 8;
    const size_t skip_n = 8;
    const std::string file = "docs";

    irs::memory_directory dir;
    // write data
    {
      irs::skip_writer writer(skip_0, skip_n);
      ASSERT_FALSE(writer);

      // write data

      size_t low = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      size_t high = irs::type_limits<irs::type_t::doc_id_t>::invalid();

      writer.prepare(
        max_levels, count,
        [&low, &high, skip_0](size_t level, irs::index_output& out) {
          if (!level) {
            out.write_vlong(low);
          }
          out.write_vlong(high); // upper
      });
      ASSERT_TRUE(static_cast<bool>(writer));

      size_t size = 0;
      for (size_t doc = 0; doc <= count; ++doc, ++size) {
        // skip every "skip" document
        if (size == skip_0) {
          writer.skip(doc);
          size = 0;
          low = high;
        }

        high = doc;
      }

      auto out = dir.create(file);
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }

    // check written data
    {
      irs::doc_id_t lower = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      irs::doc_id_t upper = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      size_t calls_count = 0;

      irs::skip_reader reader(skip_0, skip_n);
      ASSERT_FALSE(reader);
      auto in = dir.open(file, irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);
      reader.prepare(
        std::move(in), [&lower, &upper, &calls_count](size_t level, irs::index_input& in) {
          ++calls_count;

          if (in.eof()) {
            lower = upper;
            upper = irs::doc_limits::eof();
          } else {
            if (!level) {
              lower = in.read_vlong();
            }

            upper = in.read_vlong();
          }

          return upper;
      });
      ASSERT_TRUE(static_cast<bool>(reader));
      ASSERT_EQ(skip_0, reader.skip_0());
      ASSERT_EQ(skip_n, reader.skip_n());

      // seek to 5
      {
        ASSERT_EQ(0, reader.seek(5));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_EQ(7, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(0, reader.seek(5)); 
        ASSERT_EQ(0, calls_count);
      }

      // seek to last document in a 1st block 
      {
        ASSERT_EQ(0, reader.seek(7));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_EQ(7, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(0, reader.seek(7)); 
        ASSERT_EQ(0, calls_count);
      }

      // seek to the first document in a 2nd block
      {
        ASSERT_EQ(8, reader.seek(8));
        ASSERT_EQ(7, lower);
        ASSERT_EQ(15, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(8, reader.seek(8));
        ASSERT_EQ(0, calls_count);
      }

      // seek to 63
      {
        ASSERT_EQ(56, reader.seek(63));
        ASSERT_EQ(55, lower);
        ASSERT_EQ(63, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(56, reader.seek(63));
        ASSERT_EQ(0, calls_count);
      }

      // seek to 64
      {
        ASSERT_EQ(64, reader.seek(64));
        ASSERT_EQ(63, lower);
        ASSERT_EQ(71, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(64, reader.seek(64));
        ASSERT_EQ(0, calls_count);
      }

      // seek to the 767 
      {
        ASSERT_EQ(760, reader.seek(767));
        ASSERT_EQ(759, lower);
        ASSERT_EQ(767, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(760, reader.seek(767));
        ASSERT_EQ(0, calls_count);
      }

      // seek to the 1023 
      {
        ASSERT_EQ(1016, reader.seek(1023));
        ASSERT_EQ(1015, lower);
        ASSERT_EQ(1023, upper);
      }

      // seek to the 1024 
      {
        ASSERT_EQ(1024, reader.seek(1024));
        ASSERT_EQ(1023, lower);
        ASSERT_EQ(1031, upper);
      }

      // seek to the 1512
      {
        ASSERT_EQ(1512, reader.seek(1512));
        ASSERT_EQ(1511, lower);
        ASSERT_EQ(1519, upper);
      }

      // seek to the 1701 
      {
        ASSERT_EQ(1696, reader.seek(1701));
        ASSERT_EQ(1695, lower);
        ASSERT_EQ(1703, upper);
      }

      // seek to 1920
      {
        ASSERT_EQ(1920, reader.seek(1920));
        ASSERT_EQ(1919, lower);
        ASSERT_EQ(1927, upper);
      }

      // seek to last doc in a skip-list
      {
        ASSERT_EQ(1928, reader.seek(1928));
        ASSERT_EQ(1927, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
      }

      // seek after the last doc in a skip-list
      {
        calls_count = 0;
        ASSERT_EQ(1928, reader.seek(1930));
        ASSERT_EQ(1927, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
        ASSERT_EQ(0, calls_count);
      }

      // seek after the last doc in a skip-list
      {
        calls_count = 0;
        ASSERT_EQ(1928, reader.seek(1935));
        ASSERT_EQ(1927, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
        ASSERT_EQ(0, calls_count);
      }

      // seek to NO_MORE_DOCS
      {
        calls_count = 0;
        ASSERT_EQ(1928, reader.seek(irs::doc_limits::eof()));
        ASSERT_EQ(1927, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
        ASSERT_EQ(0, calls_count);
      }

      // seek to lower document
      {
        calls_count = 0;
        ASSERT_EQ(1928, reader.seek(767));
        ASSERT_EQ(1927, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
        ASSERT_EQ(0, calls_count);
      }

      // reset && seek to document
      {
        reader.reset();
        ASSERT_EQ(760, reader.seek(767));
        ASSERT_EQ(759, lower);
        ASSERT_EQ(767, upper);
      }

      // reset && seek to type_limits<type_t::doc_id_t>::min()
      {
        reader.reset();
        ASSERT_EQ(0, reader.seek(irs::type_limits<irs::type_t::doc_id_t>::min()));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_EQ(7, upper);
      }

      // reset && seek to irs::INVALID_DOC
      {
        calls_count = 0;
        lower = upper = irs::type_limits<irs::type_t::doc_id_t>::invalid();
        reader.reset();
        ASSERT_EQ(0, reader.seek(irs::type_limits<irs::type_t::doc_id_t>::invalid()));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(upper));
        ASSERT_EQ(0, calls_count);
      }

      // reset && seek to irs::NO_MORE_DOCS
      {
        reader.reset();
        ASSERT_EQ(1928, reader.seek(irs::doc_limits::eof()));
        ASSERT_EQ(1927, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
      }

      // reset && seek to 1928 
      {
        reader.reset();
        ASSERT_EQ(1928, reader.seek(1928));
        ASSERT_EQ(1927, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
      }

      // reset && seek to 1927 
      {
        reader.reset();
        ASSERT_EQ(1920, reader.seek(1927));
        ASSERT_EQ(1919, lower);
        ASSERT_EQ(1927, upper);
      }

      // reset && seek to 1511 
      {
        reader.reset();
        ASSERT_EQ(1504, reader.seek(1511));
        ASSERT_EQ(1503, lower);
        ASSERT_EQ(1511, upper);
      }

      // reset && seek to 1512 
      {
        reader.reset();
        ASSERT_EQ(1512, reader.seek(1512));
        ASSERT_EQ(1511, lower);
        ASSERT_EQ(1519, upper);
      }
    }
  } 

  {
    const size_t count = 7192;
    const size_t max_levels = 5;
    const size_t skip_0 = 8;
    const size_t skip_n = 16;
    const std::string file = "docs";

    irs::memory_directory dir;
    irs::skip_writer writer(skip_0, skip_n);

    // write data
    {
      ASSERT_FALSE(writer);

      // write data

      size_t low = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      size_t high = irs::type_limits<irs::type_t::doc_id_t>::invalid();

      writer.prepare(
        max_levels, count,
        [&low, &high, skip_0](size_t level, irs::index_output& out) {
          if (!level) {
            out.write_vlong(low);
          }
          out.write_vlong(high); // upper
      });
      ASSERT_TRUE(static_cast<bool>(writer));

      size_t size = 0;
      for (size_t doc = 0; doc <= count; ++doc, ++size) {
        // skip every "skip" document
        if (size == skip_0) {
          writer.skip(doc);
          size = 0;
          low = high;
        }

        high = doc;
      }

      auto out = dir.create(file);
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }

    // check written data
    {
      irs::doc_id_t lower = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      irs::doc_id_t upper = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      size_t calls_count = 0;

      irs::skip_reader reader(skip_0, skip_n);
      ASSERT_FALSE(reader);
      auto in = dir.open(file, irs::IOAdvice::RANDOM);
      ASSERT_FALSE(!in);
      reader.prepare(
        std::move(in), [&lower, &upper, &calls_count](size_t level, irs::index_input& in) {
          ++calls_count;

          if (in.eof()) {
            lower = upper;
            upper = irs::doc_limits::eof();
          } else {
            if (!level) {
              lower = in.read_vlong();
            }
            upper = in.read_vlong();
          }

          return upper;
      });
      ASSERT_TRUE(static_cast<bool>(reader));
      ASSERT_EQ(writer.num_levels(), reader.num_levels());
      ASSERT_EQ(skip_0, reader.skip_0());
      ASSERT_EQ(skip_n, reader.skip_n());

      // seek to 5
      {
        ASSERT_EQ(0, reader.seek(5));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_EQ(7, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(0, reader.seek(5)); 
        ASSERT_EQ(0, calls_count);
      }

      // seek to last document in a 1st block 
      {
        ASSERT_EQ(0, reader.seek(7));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_EQ(7, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(0, reader.seek(7)); 
        ASSERT_EQ(0, calls_count);
      }

      // seek to the first document in a 2nd block
      {
        ASSERT_EQ(8, reader.seek(8));
        ASSERT_EQ(7, lower);
        ASSERT_EQ(15, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(8, reader.seek(8));
        ASSERT_EQ(0, calls_count);
      }

      // seek to 63
      {
        ASSERT_EQ(56, reader.seek(63));
        ASSERT_EQ(55, lower);
        ASSERT_EQ(63, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(56, reader.seek(63));
        ASSERT_EQ(0, calls_count);
      }

      // seek to 64
      {
        ASSERT_EQ(64, reader.seek(64));
        ASSERT_EQ(63, lower);
        ASSERT_EQ(71, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(64, reader.seek(64));
        ASSERT_EQ(0, calls_count);
      }

      // seek to the 767 
      {
        ASSERT_EQ(760, reader.seek(767));
        ASSERT_EQ(759, lower);
        ASSERT_EQ(767, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(760, reader.seek(767));
        ASSERT_EQ(0, calls_count);
      }

      // seek to the 1023 
      {
        ASSERT_EQ(1016, reader.seek(1023));
        ASSERT_EQ(1015, lower);
        ASSERT_EQ(1023, upper);
      }

      // seek to the 1024 
      {
        ASSERT_EQ(1024, reader.seek(1024));
        ASSERT_EQ(1023, lower);
        ASSERT_EQ(1031, upper);
      }

      // seek to the 1512
      {
        ASSERT_EQ(1512, reader.seek(1512));
        ASSERT_EQ(1511, lower);
        ASSERT_EQ(1519, upper);
      }

      // seek to the 1701 
      {
        ASSERT_EQ(1696, reader.seek(1701));
        ASSERT_EQ(1695, lower);
        ASSERT_EQ(1703, upper);
      }

      // seek to 1920
      {
        ASSERT_EQ(1920, reader.seek(1920));
        ASSERT_EQ(1919, lower);
        ASSERT_EQ(1927, upper);
      }

      // seek to one doc before the last in a skip-list
      {
        ASSERT_EQ(7184, reader.seek(7191));
        ASSERT_EQ(7183, lower);
        ASSERT_EQ(7191, upper);
      }

      // seek to last doc in a skip-list
      {
        ASSERT_EQ(7192, reader.seek(7192));
        ASSERT_EQ(7191, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
      }

      // seek to after the last doc in a skip-list
      {
        ASSERT_EQ(7192, reader.seek(7193));
        ASSERT_EQ(7191, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
      }
    }
  }

  {
    const size_t count = 14721;
    const size_t max_levels = 5;
    const size_t skip_0 = 16;
    const size_t skip_n = 8;
    const std::string file = "docs";

    irs::memory_directory dir;
    irs::skip_writer writer(skip_0, skip_n);

    // write data
    {
      ASSERT_FALSE(writer);

      // write data

      size_t low = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      size_t high = irs::type_limits<irs::type_t::doc_id_t>::invalid();

      writer.prepare(
        max_levels, count,
        [&low, &high, skip_0](size_t level, irs::index_output& out) {
          out.write_vlong(high); // upper
      });
      ASSERT_TRUE(static_cast<bool>(writer));

      size_t size = 0;
      for (size_t doc = 0; doc <= count; ++doc, ++size) {
        // skip every "skip" document
        if (size == skip_0) {
          writer.skip(doc);
          size = 0;
          low = high;
        }

        high = doc;
      }

      auto out = dir.create(file);
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }

    // check written data
    {
      irs::doc_id_t lower = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      irs::doc_id_t upper = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      size_t calls_count = 0;
      size_t last_level = max_levels;

      irs::skip_reader reader(skip_0, skip_n);
      ASSERT_FALSE(reader);
      auto in = dir.open(file, irs::IOAdvice::RANDOM);
      ASSERT_FALSE(!in);
      reader.prepare(
        std::move(in), [&lower, &last_level, &upper, &calls_count](size_t level, irs::index_input& in) {
          ++calls_count;

          if (last_level > level) {
            upper = lower;
          } else {
            lower = upper;
          }
          last_level = level;

          if (in.eof()) {
            upper = irs::doc_limits::eof();
          } else {
            upper = in.read_vlong();
          }

          return upper;
      });
      ASSERT_TRUE(static_cast<bool>(reader));
      ASSERT_EQ(writer.num_levels(), reader.num_levels());
      ASSERT_EQ(skip_0, reader.skip_0());
      ASSERT_EQ(skip_n, reader.skip_n());

      // seek to 5
      {
        ASSERT_EQ(0, reader.seek(5));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_EQ(15, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(0, reader.seek(5)); 
        ASSERT_EQ(0, calls_count);
      }

      // seek to last document in a 1st block 
      {
        ASSERT_EQ(0, reader.seek(15));
        ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(lower));
        ASSERT_EQ(15, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(0, reader.seek(7)); 
        ASSERT_EQ(0, calls_count);
      }

      // seek to the first document in a 2nd block
      {
        ASSERT_EQ(16, reader.seek(16));
        ASSERT_EQ(15, lower);
        ASSERT_EQ(31, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(16, reader.seek(16));
        ASSERT_EQ(0, calls_count);
      }

      // seek to 127 
      {
        ASSERT_EQ(112, reader.seek(127));
        ASSERT_EQ(111, lower);
        ASSERT_EQ(127, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(112, reader.seek(127));
        ASSERT_EQ(0, calls_count);
      }

      // seek to 128 
      {
        ASSERT_EQ(128, reader.seek(128));
        ASSERT_EQ(127, lower);
        ASSERT_EQ(143, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(128, reader.seek(128));
        ASSERT_EQ(0, calls_count);
      }

      // seek to the 1767 
      {
        ASSERT_EQ(1760, reader.seek(1767));
        ASSERT_EQ(1759, lower);
        ASSERT_EQ(1775, upper);

        // seek to same document
        calls_count = 0;
        ASSERT_EQ(1760, reader.seek(767));
        ASSERT_EQ(0, calls_count);
      }

      // seek to the 3999 
      {
        ASSERT_EQ(3984, reader.seek(3999));
        ASSERT_EQ(3983, lower);
        ASSERT_EQ(3999, upper);
      }

      // seek to the 4000 
      {
        ASSERT_EQ(4000, reader.seek(4000));
        ASSERT_EQ(3999, lower);
        ASSERT_EQ(4015, upper);
      }

      // seek to 7193 
      {
        ASSERT_EQ(7184, reader.seek(7193));
        ASSERT_EQ(7183, lower);
        ASSERT_EQ(7199, upper);
      }

      // seek to last doc in a skip-list
      {
        ASSERT_EQ(14720, reader.seek(14721));
        ASSERT_EQ(14719, lower);
        ASSERT_TRUE(irs::doc_limits::eof(upper));
      }
    }
  }

  // case with empty 4th skip-level
  {
    const size_t count = 32768;
    const size_t max_levels = 8;
    const size_t skip_0 = 128;
    const size_t skip_n = 8;
    const std::string file = "docs";

    irs::memory_directory dir;
    irs::skip_writer writer(skip_0, skip_n);

    // write data
    {
      ASSERT_FALSE(writer);

      // write data
      size_t high = irs::type_limits<irs::type_t::doc_id_t>::invalid();

      writer.prepare(
        max_levels, count,
        [&high, skip_0](size_t level, irs::index_output& out) {
          out.write_vlong(high); // upper
      });
      ASSERT_TRUE(static_cast<bool>(writer));

      irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
      for (size_t size = 0; size <= count; doc += 2, ++size) {
        // skip every "skip" document
        if (size && 0 == size % skip_0) {
          writer.skip(size);
        }

        high = doc;
      }

      auto out = dir.create(file);
      ASSERT_FALSE(!out);
      writer.flush(*out);
    }

    // check written data
    {
      irs::doc_id_t lower = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      irs::doc_id_t upper = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      size_t calls_count = 0;
      size_t last_level = max_levels;

      irs::skip_reader reader(skip_0, skip_n);
      ASSERT_FALSE(reader);
      auto in = dir.open(file, irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);
      reader.prepare(
        std::move(in), [&lower, &last_level, &upper, &calls_count](size_t level, irs::index_input &in) {
          ++calls_count;

          if (last_level > level) {
            upper = lower;
          } else {
            lower = upper;
          }
          last_level = level;

          if (in.eof()) {
            upper = irs::doc_limits::eof();
          } else {
            upper = in.read_vlong();
          }

          return upper;
      });

      // seek for every document
      {
        irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
        for (size_t i = 0; i <= count; ++i, doc += 2) {
          const size_t skipped = (i/skip_0) * skip_0;
          ASSERT_EQ(skipped, reader.seek(doc));
          ASSERT_TRUE(lower < doc);
          ASSERT_TRUE(doc <= upper);
        }
      }

      // seek backwards
      irs::doc_id_t doc = count*2 + irs::type_limits<irs::type_t::doc_id_t>::min();
      for (size_t i = count; i <= count; --i, doc -= 2) {
        lower = irs::type_limits<irs::type_t::doc_id_t>::invalid();
        upper = irs::type_limits<irs::type_t::doc_id_t>::invalid();
        reader.reset();
        size_t skipped = (i/skip_0)*skip_0;
        ASSERT_EQ(skipped, reader.seek(doc));
        ASSERT_TRUE(lower < doc);
        ASSERT_TRUE(doc <= upper);
      }
    }
  }
}
