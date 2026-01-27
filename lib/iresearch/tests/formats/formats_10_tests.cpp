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

#include "formats/format_utils.hpp"
#include "formats/formats_10.hpp"
#include "formats/formats_10_attributes.hpp"
#include "formats_test_case_base.hpp"
#include "index/field_meta.hpp"
#include "store/mmap_directory.hpp"
#include "tests_shared.hpp"
#include "utils/bit_packing.hpp"
#include "utils/type_limits.hpp"

namespace {

using tests::format_test_case;

class format_10_test_case : public tests::format_test_case {
 protected:
  static constexpr size_t kVersion10PostingsWriterBlockSize = 128;

  struct basic_attribute_provider : irs::attribute_provider {
    irs::attribute* get_mutable(irs::type_info::type_id type) noexcept {
      if (type == irs::type<irs::frequency>::id()) {
        return freq;
      }
      if (type == irs::type<irs::term_meta>::id()) {
        return meta;
      }
      return nullptr;
    }

    irs::frequency* freq{};
    irs::term_meta* meta{};
  };

  void AssertFrequencyAndPositions(irs::doc_iterator& expected,
                                   irs::doc_iterator& actual) {
    auto* expected_freq = irs::get_mutable<irs::frequency>(&expected);
    auto* actual_freq = irs::get_mutable<irs::frequency>(&actual);
    ASSERT_EQ(!expected_freq, !actual_freq);

    if (!expected_freq) {
      return;
    }

    auto* expected_pos = irs::get_mutable<irs::position>(&expected);
    auto* actual_pos = irs::get_mutable<irs::position>(&actual);
    ASSERT_EQ(!expected_pos, !actual_pos);

    if (!expected_pos) {
      return;
    }

    auto* expected_offset = irs::get<irs::offset>(*expected_pos);
    auto* actual_offset = irs::get<irs::offset>(*actual_pos);
    ASSERT_EQ(!expected_offset, !actual_offset);

    auto* expected_payload = irs::get<irs::payload>(*expected_pos);
    auto* actual_payload = irs::get<irs::payload>(*actual_pos);
    ASSERT_EQ(!expected_payload, !actual_payload);

    for (; expected_pos->next();) {
      ASSERT_TRUE(actual_pos->next());
      ASSERT_EQ(expected_pos->value(), actual_pos->value());

      if (expected_offset) {
        ASSERT_EQ(expected_offset->start, actual_offset->start);
        ASSERT_EQ(expected_offset->end, actual_offset->end);
      }

      if (expected_payload) {
        ASSERT_EQ(expected_payload->value, actual_payload->value);
      }
    }
    ASSERT_FALSE(actual_pos->next());
  }

  void postings_seek(
    const std::vector<std::pair<irs::doc_id_t, uint32_t>>& docs,
    irs::IndexFeatures features) {
    irs::field_meta field;
    field.index_features = features;
    auto dir = get_directory(*this);

    // attributes for term
    auto codec =
      std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
    ASSERT_NE(nullptr, codec);
    auto writer =
      codec->get_postings_writer(false, irs::IResourceManager::kNoop);
    ASSERT_NE(nullptr, writer);
    irs::version10::term_meta term_meta;

    // write postings for field
    {
      irs::flush_state state{
        .dir = dir.get(),
        .name = "segment_name",
        .doc_count = docs.back().first + 1,
        .index_features = field.index_features,
      };

      auto out = dir->create("attributes");
      ASSERT_FALSE(!out);
      irs::write_string(*out, std::string_view("file_header"));

      // prepare writer
      writer->prepare(*out, state);

      writer->begin_field(features, field.features);

      // write postings for term
      {
        postings it(docs, field.index_features);
        writer->write(it, term_meta);

        // write attributes to out
        writer->encode(*out, term_meta);
      }

      auto stats = writer->end_field();
      ASSERT_EQ(0, stats.wand_mask);
      ASSERT_EQ(docs.size(), stats.docs_count);

      writer->end();
    }

    // read postings
    {
      irs::SegmentMeta meta;
      meta.name = "segment_name";

      irs::ReaderState state;
      state.dir = dir.get();
      state.meta = &meta;

      auto in = dir->open("attributes", irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);
      const auto tmp = irs::read_string<std::string>(*in);

      // prepare reader
      auto reader = codec->get_postings_reader();
      ASSERT_NE(nullptr, reader);
      reader->prepare(*in, state, field.index_features);

      irs::bstring in_data(in->length() - in->file_pointer(), 0);
      in->read_bytes(&in_data[0], in_data.size());
      const auto* begin = in_data.c_str();

      // read term attributes
      {
        irs::version10::term_meta read_meta;
        begin += reader->decode(begin, field.index_features, read_meta);

        // check term_meta
        {
          ASSERT_EQ(term_meta.docs_count, read_meta.docs_count);
          ASSERT_EQ(term_meta.doc_start, read_meta.doc_start);
          ASSERT_EQ(term_meta.pos_start, read_meta.pos_start);
          ASSERT_EQ(term_meta.pos_end, read_meta.pos_end);
          ASSERT_EQ(term_meta.pay_start, read_meta.pay_start);
          ASSERT_EQ(term_meta.e_single_doc, read_meta.e_single_doc);
          ASSERT_EQ(term_meta.e_skip_start, read_meta.e_skip_start);
        }

        auto assert_docs = [&](size_t seed, size_t inc) {
          postings expected_postings{docs, field.index_features};

          auto actual =
            reader->iterator(field.index_features, features, read_meta, 0);
          ASSERT_FALSE(irs::doc_limits::valid(actual->value()));

          postings expected(docs, field.index_features);
          for (size_t i = seed, size = docs.size(); i < size; i += inc) {
            auto& doc = docs[i];
            ASSERT_EQ(doc.first, actual->seek(doc.first));
            ASSERT_EQ(doc.first,
                      actual->seek(doc.first));  // seek to the same doc
            ASSERT_EQ(
              doc.first,
              actual->seek(
                irs::doc_limits::invalid()));  // seek to the smaller doc

            ASSERT_EQ(doc.first, expected.seek(doc.first));
            AssertFrequencyAndPositions(expected, *actual);
          }

          if (inc == 1) {
            ASSERT_FALSE(actual->next());
            ASSERT_TRUE(irs::doc_limits::eof(actual->value()));

            // seek after the existing documents
            ASSERT_TRUE(
              irs::doc_limits::eof(actual->seek(docs.back().first + 42)));
          }
        };

        // next + seek to eof
        {
          auto it = reader->iterator(field.index_features,
                                     irs::IndexFeatures::NONE, read_meta, 0);
          ASSERT_FALSE(irs::doc_limits::valid(it->value()));
          ASSERT_TRUE(it->next());
          ASSERT_EQ(docs.front().first, it->value());
          ASSERT_TRUE(irs::doc_limits::eof(it->seek(docs.back().first + 42)));
        }

        // seek to every document 127th document in a block
        assert_docs(kVersion10PostingsWriterBlockSize - 1,
                    kVersion10PostingsWriterBlockSize);

        // seek to every 128th document in a block
        assert_docs(kVersion10PostingsWriterBlockSize,
                    kVersion10PostingsWriterBlockSize);

        // seek to every document
        assert_docs(0, 1);

        // seek to every 5th document
        assert_docs(0, 5);

        // seek for backwards && next
        {
          for (auto doc = docs.rbegin(), end = docs.rend(); doc != end; ++doc) {
            postings expected(docs, field.index_features);
            auto it =
              reader->iterator(field.index_features, features, read_meta, 0);
            ASSERT_FALSE(irs::doc_limits::valid(it->value()));
            ASSERT_EQ(doc->first, it->seek(doc->first));

            ASSERT_EQ(doc->first, expected.seek(doc->first));
            AssertFrequencyAndPositions(expected, *it);
            if (doc != docs.rbegin()) {
              ASSERT_TRUE(it->next());
              const auto expected_doc = (doc - 1)->first;
              ASSERT_EQ(expected_doc, it->value());

              ASSERT_TRUE(expected.next());
              ASSERT_EQ(expected_doc, expected.value());
              AssertFrequencyAndPositions(expected, *it);
            }
          }
        }

        // seek to irs::doc_limits::invalid()
        {
          auto it = reader->iterator(field.index_features,
                                     irs::IndexFeatures::NONE, read_meta, 0);
          ASSERT_FALSE(irs::doc_limits::valid(it->value()));
          ASSERT_FALSE(
            irs::doc_limits::valid(it->seek(irs::doc_limits::invalid())));
          ASSERT_TRUE(it->next());
          ASSERT_EQ(docs.front().first, it->value());
        }

        // seek to irs::doc_limits::eof()
        {
          auto it = reader->iterator(field.index_features,
                                     irs::IndexFeatures::NONE, read_meta, 0);
          ASSERT_FALSE(irs::doc_limits::valid(it->value()));
          ASSERT_TRUE(irs::doc_limits::eof(it->seek(irs::doc_limits::eof())));
          ASSERT_FALSE(it->next());
          ASSERT_TRUE(irs::doc_limits::eof(it->value()));
        }
      }

      ASSERT_EQ(begin, in_data.data() + in_data.size());
    }
  }
};

TEST_P(format_10_test_case, postings_read_write_single_doc) {
  irs::field_meta field;

  // docs & attributes for term0
  const std::vector<std::pair<irs::doc_id_t, uint32_t>> docs0{{3, 10}};

  // docs & attributes for term0
  const std::vector<std::pair<irs::doc_id_t, uint32_t>> docs1{{6, 10}};

  auto codec =
    std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
  ASSERT_NE(nullptr, codec);
  auto writer = codec->get_postings_writer(false, irs::IResourceManager::kNoop);
  irs::version10::term_meta meta0, meta1;

  // write postings
  {
    irs::flush_state state{
      .dir = &dir(),
      .name = "segment_name",
      .doc_count = 100,
      .index_features = field.index_features,
    };

    auto out = dir().create("attributes");
    ASSERT_FALSE(!out);

    // prepare writer
    writer->prepare(*out, state);

    // begin field
    writer->begin_field(field.index_features, field.features);

    // write postings for term0
    {
      postings docs(docs0);
      writer->write(docs, meta0);

      // check term_meta
      {
        auto& meta = static_cast<irs::version10::term_meta&>(meta0);
        ASSERT_EQ(1, meta.docs_count);
        ASSERT_EQ(2, meta.e_single_doc);
      }

      // write term0 attributes to out
      writer->encode(*out, meta0);
    }

    // write postings for term0
    {
      postings docs(docs1);
      writer->write(docs, meta1);

      // check term_meta
      {
        auto& meta = static_cast<irs::version10::term_meta&>(meta1);
        ASSERT_EQ(1, meta.docs_count);
        ASSERT_EQ(5, meta.e_single_doc);
      }

      // write term0 attributes to out
      writer->encode(*out, meta1);
    }

    // check doc positions for term0 & term1
    {
      ASSERT_EQ(meta0.docs_count, meta1.docs_count);
      ASSERT_EQ(meta0.doc_start, meta1.doc_start);
      ASSERT_EQ(meta0.pos_start, meta1.pos_start);
      ASSERT_EQ(meta0.pos_end, meta1.pos_end);
      ASSERT_EQ(meta0.pay_start, meta1.pay_start);
    }

    // finish writing
    writer->end();
  }

  // read postings
  {
    irs::SegmentMeta meta;
    meta.name = "segment_name";

    irs::ReaderState state;
    state.dir = &dir();
    state.meta = &meta;

    auto in = dir().open("attributes", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!in);

    // prepare reader
    auto reader = codec->get_postings_reader();
    ASSERT_NE(nullptr, reader);
    reader->prepare(*in, state, field.index_features);

    irs::bstring in_data(in->length() - in->file_pointer(), 0);
    in->read_bytes(&in_data[0], in_data.size());
    const auto* begin = in_data.c_str();

    // read term0 attributes & postings
    {
      irs::version10::term_meta read_meta;

      begin += reader->decode(begin, field.index_features, read_meta);

      // check term_meta for term0
      {
        ASSERT_EQ(meta0.docs_count, read_meta.docs_count);
        ASSERT_EQ(meta0.doc_start, read_meta.doc_start);
        ASSERT_EQ(meta0.pos_start, read_meta.pos_start);
        ASSERT_EQ(meta0.pos_end, read_meta.pos_end);
        ASSERT_EQ(meta0.pay_start, read_meta.pay_start);
        ASSERT_EQ(meta0.e_single_doc, read_meta.e_single_doc);
        ASSERT_EQ(meta0.e_skip_start, read_meta.e_skip_start);
      }

      // read documents
      auto it = reader->iterator(field.index_features, irs::IndexFeatures::NONE,
                                 read_meta, 0);
      for (size_t i = 0; it->next();) {
        ASSERT_EQ(docs0[i++].first, it->value());
      }
    }

    // check term_meta for term1
    {
      irs::version10::term_meta read_meta;
      begin += reader->decode(begin, field.index_features, read_meta);

      {
        ASSERT_EQ(meta1.docs_count, read_meta.docs_count);
        ASSERT_EQ(0, read_meta.doc_start); /* we don't read doc start in case of
                                              singleton */
        ASSERT_EQ(meta1.pos_start, read_meta.pos_start);
        ASSERT_EQ(meta1.pos_end, read_meta.pos_end);
        ASSERT_EQ(meta1.pay_start, read_meta.pay_start);
        ASSERT_EQ(meta1.e_single_doc, read_meta.e_single_doc);
        ASSERT_EQ(meta1.e_skip_start, read_meta.e_skip_start);
      }

      // read documents
      auto it = reader->iterator(field.index_features, irs::IndexFeatures::NONE,
                                 read_meta, 0);
      for (size_t i = 0; it->next();) {
        ASSERT_EQ(docs1[i++].first, it->value());
      }
    }

    ASSERT_EQ(begin, in_data.data() + in_data.size());
  }
}

TEST_P(format_10_test_case, postings_read_write) {
  constexpr irs::IndexFeatures features = irs::IndexFeatures::NONE;

  irs::field_meta field;
  field.index_features = features;

  // docs & attributes for term0
  const std::vector<std::pair<irs::doc_id_t, uint32_t>> docs0{
    {1, 10}, {3, 10}, {5, 10}, {7, 10}, {79, 10}, {101, 10}, {124, 10}};

  // docs & attributes for term1
  const std::vector<std::pair<irs::doc_id_t, uint32_t>> docs1{
    {2, 10}, {7, 10}, {9, 10}, {19, 10}};

  auto codec =
    std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
  ASSERT_NE(nullptr, codec);
  auto writer = codec->get_postings_writer(false, irs::IResourceManager::kNoop);
  ASSERT_NE(nullptr, writer);
  irs::version10::term_meta meta0, meta1;  // must be destroyed before writer

  // write postings
  {
    irs::flush_state state{
      .dir = &dir(),
      .name = "segment_name",
      .doc_count = 150,
      .index_features = field.index_features,
    };

    auto out = dir().create("attributes");
    ASSERT_FALSE(!out);

    // prepare writer
    writer->prepare(*out, state);

    // begin field
    writer->begin_field(features, field.features);

    // write postings for term0
    {
      postings docs(docs0);
      writer->write(docs, meta0);

      // write attributes to out
      writer->encode(*out, meta0);
    }
    // write postings for term1
    {
      postings docs(docs1);
      writer->write(docs, meta1);

      // write attributes to out
      writer->encode(*out, meta1);
    }

    // check doc positions for term0 & term1
    ASSERT_LT(meta0.doc_start, meta1.doc_start);

    // finish writing
    writer->end();
  }

  // read postings
  {
    irs::SegmentMeta meta;
    meta.name = "segment_name";

    irs::ReaderState state;
    state.dir = &dir();
    state.meta = &meta;

    auto in = dir().open("attributes", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!in);

    // prepare reader
    auto reader = codec->get_postings_reader();
    ASSERT_NE(nullptr, reader);
    reader->prepare(*in, state, field.index_features);

    irs::bstring in_data(in->length() - in->file_pointer(), 0);
    in->read_bytes(&in_data[0], in_data.size());
    const auto* begin = in_data.c_str();

    // cumulative attribute
    irs::version10::term_meta read_meta;

    // read term0 attributes
    {
      begin += reader->decode(begin, field.index_features, read_meta);

      // check term_meta
      {
        ASSERT_EQ(meta0.docs_count, read_meta.docs_count);
        ASSERT_EQ(meta0.doc_start, read_meta.doc_start);
        ASSERT_EQ(meta0.pos_start, read_meta.pos_start);
        ASSERT_EQ(meta0.pos_end, read_meta.pos_end);
        ASSERT_EQ(meta0.pay_start, read_meta.pay_start);
        ASSERT_EQ(meta0.e_single_doc, read_meta.e_single_doc);
        ASSERT_EQ(meta0.e_skip_start, read_meta.e_skip_start);
      }

      // read documents
      auto it = reader->iterator(field.index_features, irs::IndexFeatures::NONE,
                                 read_meta, 0);
      for (size_t i = 0; it->next();) {
        ASSERT_EQ(docs0[i++].first, it->value());
      }
    }

    // read term1 attributes
    {
      begin += reader->decode(begin, field.index_features, read_meta);

      // check term_meta
      {
        ASSERT_EQ(meta1.docs_count, read_meta.docs_count);
        ASSERT_EQ(meta1.doc_start, read_meta.doc_start);
        ASSERT_EQ(meta1.pos_start, read_meta.pos_start);
        ASSERT_EQ(meta1.pos_end, read_meta.pos_end);
        ASSERT_EQ(meta1.pay_start, read_meta.pay_start);
        ASSERT_EQ(meta1.e_single_doc, read_meta.e_single_doc);
        ASSERT_EQ(meta1.e_skip_start, read_meta.e_skip_start);
      }

      // read documents
      auto it = reader->iterator(field.index_features, irs::IndexFeatures::NONE,
                                 read_meta, 0);
      for (size_t i = 0; it->next();) {
        ASSERT_EQ(docs1[i++].first, it->value());
      }
    }

    ASSERT_EQ(begin, in_data.data() + in_data.size());
  }
}

TEST_P(format_10_test_case, postings_writer_reuse) {
  auto codec =
    std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
  ASSERT_NE(nullptr, codec);
  auto writer = codec->get_postings_writer(false, irs::IResourceManager::kNoop);
  ASSERT_NE(nullptr, writer);

  std::vector<std::pair<irs::doc_id_t, uint32_t>> docs0;
  irs::doc_id_t i = (irs::doc_limits::min)();
  for (; i < 1000; ++i) {
    docs0.emplace_back(i, 10);
  }

  // gap

  for (i += 1000; i < 10000; ++i) {
    docs0.emplace_back(i, 10);
  }

  // write docs 'segment0' with all possible streams
  {
    constexpr irs::IndexFeatures features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::field_meta field;
    field.name = "field";
    field.index_features = features;

    irs::flush_state state{
      .dir = &dir(),
      .name = "0",
      .doc_count = 10000,
      // all possible features in segment
      .index_features = field.index_features,
    };

    auto out = dir().create(std::string("postings") + state.name.data());
    ASSERT_FALSE(!out);

    postings docs(docs0);

    writer->prepare(*out, state);
    writer->begin_field(features, field.features);
    irs::version10::term_meta meta;
    writer->write(docs, meta);
    writer->end();
  }

  // write docs 'segment1' with position & offset
  {
    constexpr irs::IndexFeatures features = irs::IndexFeatures::FREQ |
                                            irs::IndexFeatures::POS |
                                            irs::IndexFeatures::OFFS;

    irs::field_meta field;
    field.name = "field";
    field.index_features = features;

    irs::flush_state state{
      .dir = &dir(),
      .name = "1",
      .doc_count = 10000,
      // all possible features in segment
      .index_features = field.index_features,
    };

    auto out = dir().create(std::string("postings") + state.name.data());
    ASSERT_FALSE(!out);

    postings docs(docs0);

    writer->prepare(*out, state);
    writer->begin_field(features, field.features);
    irs::version10::term_meta meta;
    writer->write(docs, meta);
    writer->end();
  }

  // write docs 'segment2' with position & payload
  {
    constexpr irs::IndexFeatures features = irs::IndexFeatures::FREQ |
                                            irs::IndexFeatures::POS |
                                            irs::IndexFeatures::PAY;

    irs::field_meta field;
    field.name = "field";
    field.index_features = features;

    irs::flush_state state{
      .dir = &dir(),
      .name = "2",
      .doc_count = 10000,
      // all possible features in segment
      .index_features = field.index_features,
    };

    auto out = dir().create(std::string("postings") + state.name.data());
    ASSERT_FALSE(!out);

    postings docs(docs0);

    writer->prepare(*out, state);
    writer->begin_field(features, field.features);
    irs::version10::term_meta meta;
    writer->write(docs, meta);
    writer->end();
  }

  // write docs 'segment3' with position
  {
    constexpr irs::IndexFeatures features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS;

    irs::field_meta field;
    field.name = "field";
    field.index_features = features;

    irs::flush_state state{
      .dir = &dir(),
      .name = "3",
      .doc_count = 10000,
      // all possible features in segment
      .index_features = field.index_features,
    };

    auto out = dir().create(std::string("postings") + state.name.data());
    ASSERT_FALSE(!out);

    postings docs(docs0);

    writer->prepare(*out, state);
    writer->begin_field(features, field.features);
    irs::version10::term_meta meta;
    writer->write(docs, meta);
    writer->end();
  }

  // write docs 'segment3' with frequency
  {
    constexpr irs::IndexFeatures features = irs::IndexFeatures::FREQ;

    irs::field_meta field;
    field.name = "field";
    field.index_features = features;

    irs::flush_state state{
      .dir = &dir(),
      .name = "4",
      .doc_count = 10000,
      // all possible features in segment
      .index_features = field.index_features,
    };

    auto out = dir().create(std::string("postings") + state.name.data());
    ASSERT_FALSE(!out);

    postings docs(docs0);

    writer->prepare(*out, state);
    writer->begin_field(features, field.features);
    irs::version10::term_meta meta;
    writer->write(docs, meta);
    writer->end();
  }

  // writer segment without any attributes
  {
    constexpr irs::IndexFeatures features = irs::IndexFeatures::NONE;

    irs::field_meta field;
    field.name = "field";
    field.index_features = features;

    irs::flush_state state{
      .dir = &dir(),
      .name = "5",
      .doc_count = 10000,
    };

    auto out = dir().create(std::string("postings") + state.name.data());
    ASSERT_FALSE(!out);

    postings docs(docs0);

    writer->prepare(*out, state);
    writer->begin_field(features, field.features);
    irs::version10::term_meta meta;
    writer->write(docs, meta);
    writer->end();
  }
}

TEST_P(format_10_test_case, ires336) {
  // bug: ires336
  auto dir = get_directory(*this);
  const std::string_view segment_name = "bug";
  const std::string_view field = "sbiotype";
  const irs::bytes_view term =
    irs::ViewCast<irs::byte_type>(std::string_view("protein_coding"));

  std::vector<std::pair<irs::doc_id_t, uint32_t>> docs;
  {
    std::string buf;
    std::ifstream in(resource("postings.txt").c_str());
    char* pend;
    while (std::getline(in, buf)) {
      docs.emplace_back(strtol(buf.c_str(), &pend, 10), 10);
    }
  }
  std::vector<irs::bytes_view> terms{term};
  tests::format_test_case::terms<decltype(terms.begin())> trms(
    terms.begin(), terms.end(), docs.begin(), docs.end());

  irs::flush_state flush_state{
    .dir = dir.get(),
    .name = segment_name,
    .doc_count = 10000,
  };

  irs::field_meta field_meta;
  field_meta.name = field;
  {
    tests::MockTermReader term_reader{
      trms, field_meta, (terms.empty() ? irs::bytes_view{} : *terms.begin()),
      (terms.empty() ? irs::bytes_view{} : *terms.rbegin())};
    auto fw = get_codec()->get_field_writer(true, irs::IResourceManager::kNoop);
    fw->prepare(flush_state);
    fw->write(term_reader, field_meta.features);
    fw->end();
  }

  irs::SegmentMeta meta;
  meta.name = segment_name;

  irs::DocumentMask docs_mask;
  auto fr = get_codec()->get_field_reader(irs::IResourceManager::kNoop);
  fr->prepare(irs::ReaderState{.dir = dir.get(), .meta = &meta});

  auto it = fr->field(field_meta.name)->iterator(irs::SeekMode::NORMAL);
  ASSERT_TRUE(it->seek(term));

  // ires-336 sequence
  {
    auto docs = it->postings(irs::IndexFeatures::NONE);
    ASSERT_EQ(4048, docs->seek(4048));
    ASSERT_EQ(6830, docs->seek(6829));
  }

  // ires-336 extended sequence
  {
    auto docs = it->postings(irs::IndexFeatures::NONE);
    ASSERT_EQ(1068, docs->seek(1068));
    ASSERT_EQ(1875, docs->seek(1873));
    ASSERT_EQ(4048, docs->seek(4048));
    ASSERT_EQ(6830, docs->seek(6829));
  }

  // extended sequence
  {
    auto docs = it->postings(irs::IndexFeatures::NONE);
    ASSERT_EQ(4048, docs->seek(4048));
    ASSERT_EQ(4400, docs->seek(4400));
    ASSERT_EQ(6830, docs->seek(6829));
  }

  // ires-336 full sequence
  {
    auto docs = it->postings(irs::IndexFeatures::NONE);
    ASSERT_EQ(334, docs->seek(334));
    ASSERT_EQ(1046, docs->seek(1046));
    ASSERT_EQ(1068, docs->seek(1068));
    ASSERT_EQ(2307, docs->seek(2307));
    ASSERT_EQ(2843, docs->seek(2843));
    ASSERT_EQ(3059, docs->seek(3059));
    ASSERT_EQ(3564, docs->seek(3564));
    ASSERT_EQ(4048, docs->seek(4048));
    ASSERT_EQ(7773, docs->seek(7773));
    ASSERT_EQ(8204, docs->seek(8204));
    ASSERT_EQ(9353, docs->seek(9353));
    ASSERT_EQ(9366, docs->seek(9366));
  }
}

TEST_P(format_10_test_case, postings_seek) {
  auto generate_docs = [](size_t count, size_t step) {
    std::vector<std::pair<irs::doc_id_t, uint32_t>> docs;
    docs.reserve(count);
    irs::doc_id_t i = (irs::doc_limits::min)();
    std::generate_n(std::back_inserter(docs), count, [&i, step]() {
      const irs::doc_id_t doc = i;
      const uint32_t freq = std::max(1U, doc % 7);
      i += step;

      return std::make_pair(doc, freq);
    });
    return docs;
  };

  constexpr auto kNone = irs::IndexFeatures::NONE;
  constexpr auto kFreq = irs::IndexFeatures::FREQ;
  constexpr auto kPos = irs::IndexFeatures::FREQ | irs::IndexFeatures::POS;
  constexpr auto kOffs = irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
                         irs::IndexFeatures::OFFS;
  constexpr auto kPay = irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
                        irs::IndexFeatures::PAY;
  constexpr auto kAll = irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
                        irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  // singleton doc
  {
    constexpr size_t kCount = 1;
    static_assert(kCount < kVersion10PostingsWriterBlockSize);

    const auto docs = generate_docs(kCount, 1);

    postings_seek(docs, kNone);
    postings_seek(docs, kFreq);
    postings_seek(docs, kPos);
    postings_seek(docs, kOffs);
    postings_seek(docs, kPay);
    postings_seek(docs, kAll);
  }

  // short list (< postings_writer::BLOCK_SIZE)
  {
    constexpr size_t kCount = 117;
    static_assert(kCount < kVersion10PostingsWriterBlockSize);

    const auto docs = generate_docs(kCount, 1);

    postings_seek(docs, kNone);
    postings_seek(docs, kFreq);
    postings_seek(docs, kPos);
    postings_seek(docs, kOffs);
    postings_seek(docs, kPay);
    postings_seek(docs, kAll);
  }

  // equals to postings_writer::BLOCK_SIZE
  {
    const auto docs = generate_docs(kVersion10PostingsWriterBlockSize, 1);

    postings_seek(docs, kNone);
    postings_seek(docs, kFreq);
    postings_seek(docs, kPos);
    postings_seek(docs, kOffs);
    postings_seek(docs, kPay);
    postings_seek(docs, kAll);
  }

  // long list
  {
    constexpr size_t kCount = 10000;
    const auto docs = generate_docs(kCount, 1);

    postings_seek(docs, kNone);
    postings_seek(docs, kFreq);
    postings_seek(docs, kPos);
    postings_seek(docs, kOffs);
    postings_seek(docs, kPay);
    postings_seek(docs, kAll);
  }

  // 2^15
  {
    constexpr size_t kCount = 32768;
    const auto docs = generate_docs(kCount, 2);

    postings_seek(docs, kNone);
    postings_seek(docs, kFreq);
    postings_seek(docs, kPos);
    postings_seek(docs, kOffs);
    postings_seek(docs, kPay);
    postings_seek(docs, kAll);
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();
static const auto kTestValues = ::testing::Combine(
  ::testing::ValuesIn(kTestDirs), ::testing::Values(tests::format_info{"1_0"}));

// 1.0 specific tests
INSTANTIATE_TEST_SUITE_P(format_10_test, format_10_test_case, kTestValues,
                         format_10_test_case::to_string);

// Generic tests
INSTANTIATE_TEST_SUITE_P(format_10_test, format_test_case, kTestValues,
                         format_test_case::to_string);

}  // namespace
