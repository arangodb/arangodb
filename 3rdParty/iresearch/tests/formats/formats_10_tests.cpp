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
#include "store/memory_directory.hpp"
#include "store/fs_directory.hpp"
#include "utils/bit_packing.hpp"
#include "utils/type_limits.hpp"
#include "formats/formats_10.hpp"
#include "formats/formats_10_attributes.hpp"
#include "formats_test_case_base.hpp"
#include "formats/format_utils.hpp"

class format_10_test_case : public tests::format_test_case_base {
 protected:
  const size_t VERSION10_POSTINGS_WRITER_BLOCK_SIZE = 128;

  irs::format::ptr get_codec() {
    return irs::formats::get("1_0");
  }

  void postings_read_write_single_doc() {
    irs::field_meta field;

    // docs & attributes for term0
    std::vector<irs::doc_id_t> docs0{ 3 };

    // docs & attributes for term0
    std::vector<irs::doc_id_t> docs1{ 6 };

    auto codec = std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
    ASSERT_NE(nullptr, codec);
    auto writer = codec->get_postings_writer(false);
    irs::postings_writer::state meta0, meta1;

    // write postings
    {
      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 100;
      state.name = "segment_name";
      state.features = &field.features;

      auto out = dir().create("attributes");
      ASSERT_FALSE(!out);

      // prepare writer
      writer->prepare(*out, state);

      // begin field
      writer->begin_field(field.features);

      // write postings for term0
      {
        postings docs(docs0.begin(), docs0.end());
        meta0 = writer->write(docs);

        // check term_meta
        {
          auto& meta = dynamic_cast<irs::version10::term_meta&>(*meta0);
          ASSERT_EQ(1, meta.docs_count);
          ASSERT_EQ(2, meta.e_single_doc);
        }

        // write term0 attributes to out
        writer->encode(*out, *meta0);
      }

      // write postings for term0
      {
        postings docs(docs1.begin(), docs1.end());
        meta1 = writer->write(docs);

        // check term_meta
        {
          auto& meta = dynamic_cast<irs::version10::term_meta&>(*meta1);
          ASSERT_EQ(1, meta.docs_count);
          ASSERT_EQ(5, meta.e_single_doc);
        }

        // write term0 attributes to out
        writer->encode(*out, *meta1);
      }

      // check doc positions for term0 & term1
      {
        auto& typed_meta0 = dynamic_cast<irs::version10::term_meta&>(*meta0);
        auto& typed_meta1 = dynamic_cast<irs::version10::term_meta&>(*meta1);
        ASSERT_EQ(typed_meta0.docs_count, typed_meta1.docs_count);
        ASSERT_EQ(typed_meta0.doc_start,  typed_meta1.doc_start);
        ASSERT_EQ(typed_meta0.pos_start,  typed_meta1.pos_start);
        ASSERT_EQ(typed_meta0.pos_end,    typed_meta1.pos_end);
        ASSERT_EQ(typed_meta0.pay_start,  typed_meta1.pay_start);
      }

      // finish writing
      writer->end();
    }

    // read postings
    {
      irs::segment_meta meta;
      meta.name = "segment_name";

      irs::reader_state state;
      state.dir = &dir();
      state.meta = &meta;

      auto in = dir().open("attributes", irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);

      // prepare reader
      auto reader = codec->get_postings_reader();
      ASSERT_NE(nullptr, reader);
      reader->prepare(*in, state, field.features);

      // read term0 attributes & postings
      {
        irs::version10::term_meta read_meta;
        irs::attribute_view read_attrs;
        read_attrs.emplace(read_meta);
        reader->decode(*in, field.features, read_attrs, read_meta);

        // check term_meta for term0
        {
          auto& typed_meta0 = dynamic_cast<const irs::version10::term_meta&>(*meta0);
          ASSERT_EQ(typed_meta0.docs_count, read_meta.docs_count);
          ASSERT_EQ(typed_meta0.doc_start, read_meta.doc_start);
          ASSERT_EQ(typed_meta0.pos_start, read_meta.pos_start);
          ASSERT_EQ(typed_meta0.pos_end, read_meta.pos_end);
          ASSERT_EQ(typed_meta0.pay_start, read_meta.pay_start);
          ASSERT_EQ(typed_meta0.e_single_doc, read_meta.e_single_doc);
          ASSERT_EQ(typed_meta0.e_skip_start, read_meta.e_skip_start);
        }

        // read documents
        auto it = reader->iterator(field.features, read_attrs, irs::flags::empty_instance());
        for (size_t i = 0; it->next();) {
          ASSERT_EQ(docs0[i++], it->value());
        }
      }

      // check term_meta for term1
      {
        irs::version10::term_meta read_meta;
        irs::attribute_view read_attrs;
        read_attrs.emplace(read_meta);
        reader->decode(*in, field.features, read_attrs, read_meta);

        {
          auto& typed_meta1 = dynamic_cast<const irs::version10::term_meta&>(*meta1);
          auto& read_meta = **read_attrs.get<irs::version10::term_meta>();
          ASSERT_EQ(typed_meta1.docs_count, read_meta.docs_count);
          ASSERT_EQ(0, read_meta.doc_start); /* we don't read doc start in case of singleton */
          ASSERT_EQ(typed_meta1.pos_start, read_meta.pos_start);
          ASSERT_EQ(typed_meta1.pos_end, read_meta.pos_end);
          ASSERT_EQ(typed_meta1.pay_start, read_meta.pay_start);
          ASSERT_EQ(typed_meta1.e_single_doc, read_meta.e_single_doc);
          ASSERT_EQ(typed_meta1.e_skip_start, read_meta.e_skip_start);
        }

        // read documents
        auto it = reader->iterator(field.features, read_attrs, irs::flags::empty_instance());
        for (size_t i = 0; it->next();) {
          ASSERT_EQ(docs1[i++], it->value());
        }
      }
    }
  }

  void postings_read_write() {
    irs::field_meta field;

    // docs & attributes for term0
    std::vector<irs::doc_id_t> docs0{ 1, 3, 5, 7, 79, 101, 124 };

    // docs & attributes for term1
    std::vector<irs::doc_id_t> docs1{ 2, 7, 9, 19 };

    auto codec = std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
    ASSERT_NE(nullptr, codec);
    auto writer = codec->get_postings_writer(false);
    ASSERT_NE(nullptr, writer);
    irs::postings_writer::state meta0, meta1; // must be destroyed before writer

    // write postings
    {
      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 150;
      state.name = "segment_name";
      state.features = &field.features;

      auto out = dir().create("attributes");
      ASSERT_FALSE(!out);

      // prepare writer
      writer->prepare(*out, state);

      // begin field
      writer->begin_field(field.features);

      // write postings for term0
      {
        postings docs(docs0.begin(), docs0.end());
        meta0 = writer->write(docs);

        // write attributes to out
        writer->encode(*out, *meta0);
      }
      // write postings for term1
      {
        postings docs(docs1.begin(), docs1.end());
        meta1 = writer->write(docs);

        // write attributes to out
        writer->encode(*out, *meta1);
      }

      // check doc positions for term0 & term1
      {
        auto& typed_meta0 = dynamic_cast<irs::version10::term_meta&>(*meta0);
        auto& typed_meta1 = dynamic_cast<irs::version10::term_meta&>(*meta1);
        ASSERT_GT(typed_meta1.doc_start, typed_meta0.doc_start);
      }

      // finish writing
      writer->end();
    }

    // read postings
    {
      irs::segment_meta meta;
      meta.name = "segment_name";

      irs::reader_state state;
      state.dir = &dir();
      state.meta = &meta;

      auto in = dir().open("attributes", irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);

      // prepare reader
      auto reader = codec->get_postings_reader();
      ASSERT_NE(nullptr, reader);
      reader->prepare(*in, state, field.features);

      // cumulative attribute
      irs::version10::term_meta read_meta;
      irs::attribute_view read_attrs;
      read_attrs.emplace(read_meta);

      // read term0 attributes
      {
        reader->decode(*in, field.features, read_attrs, read_meta);

        // check term_meta
        {
          auto& meta = dynamic_cast<irs::version10::term_meta&>(*meta0);
          ASSERT_EQ(meta.docs_count, read_meta.docs_count);
          ASSERT_EQ(meta.doc_start, read_meta.doc_start);
          ASSERT_EQ(meta.pos_start, read_meta.pos_start);
          ASSERT_EQ(meta.pos_end, read_meta.pos_end);
          ASSERT_EQ(meta.pay_start, read_meta.pay_start);
          ASSERT_EQ(meta.e_single_doc, read_meta.e_single_doc);
          ASSERT_EQ(meta.e_skip_start, read_meta.e_skip_start);
        }

        // read documents
        auto it = reader->iterator(field.features, read_attrs, irs::flags::empty_instance());
        for (size_t i = 0; it->next();) {
          ASSERT_EQ(docs0[i++], it->value());
        }
      }

      // read term1 attributes
      {
        reader->decode(*in, field.features, read_attrs, read_meta);

        // check term_meta
        {
          auto& meta = dynamic_cast<irs::version10::term_meta&>(*meta1);
          ASSERT_EQ(meta.docs_count, read_meta.docs_count);
          ASSERT_EQ(meta.doc_start, read_meta.doc_start);
          ASSERT_EQ(meta.pos_start, read_meta.pos_start);
          ASSERT_EQ(meta.pos_end, read_meta.pos_end);
          ASSERT_EQ(meta.pay_start, read_meta.pay_start);
          ASSERT_EQ(meta.e_single_doc, read_meta.e_single_doc);
          ASSERT_EQ(meta.e_skip_start, read_meta.e_skip_start);
        }

        /* read documents */
        auto it = reader->iterator(field.features, read_attrs, irs::flags::empty_instance());
        for (size_t i = 0; it->next();) {
          ASSERT_EQ(docs1[i++], it->value());
        }
      }
    }
  }

  void postings_writer_reuse() {
    auto codec = std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
    ASSERT_NE(nullptr, codec);
    auto writer = codec->get_postings_writer(false);
    ASSERT_NE(nullptr, writer);

    std::vector<irs::doc_id_t> docs0;
    irs::doc_id_t i = (irs::type_limits<irs::type_t::doc_id_t>::min)();
    for (; i < 1000; ++i) {
      docs0.push_back(i);
    }

    // gap

    for (i += 1000; i < 10000; ++i) {
      docs0.push_back(i);
    }

    // write docs 'segment0' with all possible streams
    {
      const irs::field_meta field(
        "field", irs::flags{ irs::frequency::type(), irs::position::type(), irs::offset::type(), irs::payload::type() }
      );

      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 10000;
      state.name = "0";
      state.features = &field.features; // all possible features in segment

      auto out = dir().create(std::string("postings") + state.name.c_str());
      ASSERT_FALSE(!out);

      postings docs(docs0.begin(), docs0.end());

      writer->prepare(*out, state);
      writer->begin_field(*state.features);
      writer->write(docs);
      writer->end();
    }

    // write docs 'segment1' with position & offset
    {
      const irs::field_meta field(
        "field", irs::flags{ irs::frequency::type(), irs::position::type(), irs::offset::type() }
      );

      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 10000;
      state.name = "1";
      state.features = &field.features; // all possible features in segment

      auto out = dir().create(std::string("postings") + state.name.c_str());
      ASSERT_FALSE(!out);

      postings docs(docs0.begin(), docs0.end());

      writer->prepare(*out, state);
      writer->begin_field(*state.features);
      writer->write(docs);
      writer->end();
    }

    // write docs 'segment2' with position & payload 
    {
      const irs::field_meta field(
        "field", irs::flags{ irs::frequency::type(), irs::position::type(), irs::payload::type() }
      );

      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 10000;
      state.name = "2";
      state.features = &field.features; // all possible features in segment

      auto out = dir().create(std::string("postings") + state.name.c_str());
      ASSERT_FALSE(!out);

      postings docs(docs0.begin(), docs0.end());

      writer->prepare(*out, state);
      writer->begin_field(*state.features);
      writer->write(docs);
      writer->end();
    }

    // write docs 'segment3' with position
    {
      const irs::field_meta field(
        "field", irs::flags{ irs::frequency::type(), irs::position::type() }
      );

      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 10000;
      state.name = "3";
      state.features = &field.features; // all possible features in segment

      auto out = dir().create(std::string("postings") + state.name.c_str());
      ASSERT_FALSE(!out);

      postings docs(docs0.begin(), docs0.end());

      writer->prepare(*out, state);
      writer->begin_field(*state.features);
      writer->write(docs);
      writer->end();
    }

    // write docs 'segment3' with frequency
    {
      const irs::field_meta field(
        "field", irs::flags{ irs::frequency::type() }
      );

      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 10000;
      state.name = "4";
      state.features = &field.features; // all possible features in segment

      auto out = dir().create(std::string("postings") + state.name.c_str());
      ASSERT_FALSE(!out);

      postings docs(docs0.begin(), docs0.end());

      writer->prepare(*out, state);
      writer->begin_field(*state.features);
      writer->write(docs);
      writer->end();
    }


    // writer segment without any attributes
    {
      const irs::field_meta field_no_features(
        "field", irs::flags{}
      );

      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 10000;
      state.name = "5";
      state.features = &field_no_features.features; // all possible features in segment

      auto out = dir().create(std::string("postings") + state.name.c_str());
      ASSERT_FALSE(!out);

      postings docs(docs0.begin(), docs0.end());

      writer->prepare(*out, state);
      writer->begin_field(*state.features);
      writer->write(docs);
      writer->end();
    }
  }

  void assert_positions(const irs::doc_iterator& expected, const irs::doc_iterator& actual) {
    auto& expected_pos = expected.attributes().get<irs::position>();
    auto& actual_pos = actual.attributes().get<irs::position>();
    ASSERT_EQ(!expected_pos, !actual_pos);

    if (!expected_pos) {
      return;
    }

    auto& expected_offset = expected_pos->attributes().get<irs::offset>();
    auto& actual_offset = actual_pos->attributes().get<irs::offset>();
    ASSERT_EQ(!expected_offset, !actual_offset);

    auto& expected_payload = expected_pos->attributes().get<irs::payload>();
    auto& actual_payload = actual_pos->attributes().get<irs::payload>();
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

  void postings_seek(const std::vector<irs::doc_id_t>& docs, const irs::flags& features) {
    irs::field_meta field;
    field.features = features;

    // attributes for term
    irs::attribute_store attrs;
    auto codec = std::dynamic_pointer_cast<const irs::version10::format>(get_codec());
    ASSERT_NE(nullptr, codec);
    auto writer = codec->get_postings_writer(false);
    ASSERT_NE(nullptr, writer);
    irs::postings_writer::state term_meta; // must be destroyed before the writer

    // write postings for field
    {
      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = docs.back()+1;
      state.name = "segment_name";
      state.features = &field.features;

      auto out = dir().create("attributes");
      ASSERT_FALSE(!out);
      irs::write_string(*out, irs::string_ref("file_header"));

      // prepare writer
      writer->prepare(*out, state);

      // begin field
      writer->begin_field(field.features);
      // write postings for term
      {
        postings it(docs.begin(), docs.end(), field.features);
        term_meta = writer->write(it);

        /* write attributes to out */
//      writer.encode(*out, attrs);
      }

      attrs.clear();

      // begin field
      writer->begin_field(field.features);
      // write postings for term
      {
        postings it(docs.begin(), docs.end(), field.features);
        term_meta = writer->write(it);

        // write attributes to out
        writer->encode(*out, *term_meta);
      }

      // finish writing
      writer->end();
    }

    // read postings
    {
      irs::segment_meta meta;
      meta.name = "segment_name";

      irs::reader_state state;
      state.dir = &dir();
      state.meta = &meta;

      auto in = dir().open("attributes", irs::IOAdvice::NORMAL);
      ASSERT_FALSE(!in);
      irs::read_string<std::string>(*in);

      // prepare reader
      auto reader = codec->get_postings_reader();
      ASSERT_NE(nullptr, reader);
      reader->prepare(*in, state, field.features);

      // cumulative attributes
      irs::frequency freq;
      freq.value = 10;

      irs::attribute_view read_attrs;
      if (field.features.check<irs::frequency>()) {
        read_attrs.emplace(freq);
      }

      // read term attributes
      {
        irs::version10::term_meta read_meta;
        read_attrs.emplace(read_meta);
        reader->decode(*in, field.features, read_attrs, read_meta);

        // check term_meta
        {
          auto& typed_meta = dynamic_cast<irs::version10::term_meta&>(*term_meta);
          ASSERT_EQ(typed_meta.docs_count, read_meta.docs_count);
          ASSERT_EQ(typed_meta.doc_start, read_meta.doc_start);
          ASSERT_EQ(typed_meta.pos_start, read_meta.pos_start);
          ASSERT_EQ(typed_meta.pos_end, read_meta.pos_end);
          ASSERT_EQ(typed_meta.pay_start, read_meta.pay_start);
          ASSERT_EQ(typed_meta.e_single_doc, read_meta.e_single_doc);
          ASSERT_EQ(typed_meta.e_skip_start, read_meta.e_skip_start);
        }

        // seek for every document 127th document in a block
        {
          const size_t inc = VERSION10_POSTINGS_WRITER_BLOCK_SIZE;
          const size_t seed = VERSION10_POSTINGS_WRITER_BLOCK_SIZE-1;
          auto it = reader->iterator(field.features, read_attrs, field.features);
          ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->value()));

          postings expected(docs.begin(), docs.end(), field.features);
          for (size_t i = seed, size = docs.size(); i < size; i += inc) {
            auto doc = docs[i];
            ASSERT_EQ(doc, it->seek(doc));
            ASSERT_EQ(doc, it->seek(doc)); // seek to the same doc
            ASSERT_EQ(doc, it->seek(irs::type_limits<irs::type_t::doc_id_t>::invalid())); // seek to the smaller doc

            ASSERT_EQ(doc, expected.seek(doc));
            assert_positions(expected, *it);
          }
        }

        // seek for every 128th document in a block
        {
          const size_t inc = VERSION10_POSTINGS_WRITER_BLOCK_SIZE;
          const size_t seed = VERSION10_POSTINGS_WRITER_BLOCK_SIZE;
          auto it = reader->iterator(field.features, read_attrs, field.features);
          ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->value()));

          postings expected(docs.begin(), docs.end(), field.features);
          for (size_t i = seed, size = docs.size(); i < size; i += inc) {
            auto doc = docs[i];
            ASSERT_EQ(doc, it->seek(doc));
            ASSERT_EQ(doc, it->seek(doc)); // seek to the same doc
            ASSERT_EQ(doc, it->seek(irs::type_limits<irs::type_t::doc_id_t>::invalid())); // seek to the smaller doc

            ASSERT_EQ(doc, expected.seek(doc));
            assert_positions(expected, *it);
          }
        }

        // seek for every document
        {
          auto it = reader->iterator(field.features, read_attrs, field.features);
          ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->value()));

          postings expected(docs.begin(), docs.end(), field.features);
          for (auto doc : docs) {
            ASSERT_EQ(doc, it->seek(doc));
            ASSERT_EQ(doc, it->seek(doc)); // seek to the same doc
            ASSERT_EQ(doc, it->seek(irs::type_limits<irs::type_t::doc_id_t>::invalid())); // seek to the smaller doc

            ASSERT_EQ(doc, expected.seek(doc));
            assert_positions(expected, *it);
          }
          ASSERT_FALSE(it->next());
          ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it->value()));

          // seek after the existing documents
          ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it->seek(docs.back() + 10)));
        }

        // seek for backwards && next
        {
          for (auto doc = docs.rbegin(), end = docs.rend(); doc != end; ++doc) {
            postings expected(docs.begin(), docs.end(), field.features);
            auto it = reader->iterator(field.features, read_attrs, field.features);
            ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->value()));
            ASSERT_EQ(*doc, it->seek(*doc));

            ASSERT_EQ(*doc, expected.seek(*doc));
            assert_positions(expected, *it);
            if (doc != docs.rbegin()) {
              ASSERT_TRUE(it->next());
              ASSERT_EQ(*(doc-1), it->value());

              ASSERT_TRUE(expected.next());
              ASSERT_EQ(*(doc - 1), expected.value());
              assert_positions(expected, *it);
            }
          }
        }

        // seek for every 5th document
        {
          const size_t inc = 5;
          const size_t seed = 0;
          auto it = reader->iterator(field.features, read_attrs, field.features);
          ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->value()));

          postings expected(docs.begin(), docs.end(), field.features);
          for (size_t i = seed, size = docs.size(); i < size; i += inc) {
            auto doc = docs[i];
            ASSERT_EQ(doc, it->seek(doc));
            ASSERT_EQ(doc, it->seek(doc)); // seek to the same doc
            ASSERT_EQ(doc, it->seek(irs::type_limits<irs::type_t::doc_id_t>::invalid())); // seek to the smaller doc

            ASSERT_EQ(doc, expected.seek(doc));
            assert_positions(expected, *it);
          }
        }

        // seek for INVALID_DOC
        {
          auto it = reader->iterator(field.features, read_attrs, irs::flags::empty_instance());
          ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->value()));
          ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->seek(irs::type_limits<irs::type_t::doc_id_t>::invalid())));
          ASSERT_TRUE(it->next());
          ASSERT_EQ(docs.front(), it->value());
        }

        // seek for NO_MORE_DOCS
        {
          auto it = reader->iterator(field.features, read_attrs, irs::flags::empty_instance());
          ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(it->value()));
          ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
          ASSERT_FALSE(it->next());
          ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(it->value()));
        }
      }
    }
  }

  void postings_seek() {
    // bug: ires336
    {
      irs::directory::ptr dir(get_directory());
      const irs::string_ref segment_name = "bug";
      const irs::string_ref field = "sbiotype";
      const irs::bytes_ref term = irs::ref_cast<irs::byte_type>(irs::string_ref("protein_coding"));

      std::vector<irs::doc_id_t> docs;
      {
        std::string buf;
        std::ifstream in(resource("postings.txt"));
        char* pend;
        while (std::getline(in, buf)) {
          docs.push_back(strtol(buf.c_str(), &pend, 10));
        }
      }
      std::vector<irs::bytes_ref> terms{ term };
      tests::format_test_case_base::terms<decltype(terms.begin())> trms(terms.begin(), terms.end(), docs.begin(), docs.end());

      iresearch::flush_state flush_state;
      flush_state.dir = dir.get();
      flush_state.doc_count = 10000;
      flush_state.features = &irs::flags::empty_instance();
      flush_state.name = segment_name;

      irs::field_meta field_meta;
      field_meta.name = field;
      {
        auto fw = get_codec()->get_field_writer(true);
        fw->prepare(flush_state);
        fw->write(field_meta.name, field_meta.norm, field_meta.features, trms);
        fw->end();
      }

      irs::segment_meta meta;
      meta.name = segment_name;

      irs::document_mask docs_mask;
      auto fr = get_codec()->get_field_reader();
      fr->prepare(*dir, meta, docs_mask);

      auto it = fr->field(field_meta.name)->iterator();
      ASSERT_TRUE(it->seek(term));

      // ires-336 sequence
      {
        auto docs = it->postings(irs::flags::empty_instance());
        ASSERT_EQ(4048, docs->seek(4048));
        ASSERT_EQ(6830, docs->seek(6829));
      }

      // ires-336 extended sequence
      {
        auto docs = it->postings(irs::flags::empty_instance());
        ASSERT_EQ(1068, docs->seek(1068));
        ASSERT_EQ(1875, docs->seek(1873));
        ASSERT_EQ(4048, docs->seek(4048));
        ASSERT_EQ(6830, docs->seek(6829));
      }

      // extended sequence
      {
        auto docs = it->postings(irs::flags::empty_instance());
        ASSERT_EQ(4048, docs->seek(4048));
        ASSERT_EQ(4400, docs->seek(4400));
        ASSERT_EQ(6830, docs->seek(6829));
      }
      
      // ires-336 full sequence
      {
        auto docs = it->postings(irs::flags::empty_instance());
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

    // short list (< postings_writer::BLOCK_SIZE)
    {
      std::vector<irs::doc_id_t> docs;
      {
        const size_t count = 117;
        docs.reserve(count);
        auto i = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        std::generate_n(std::back_inserter(docs), count,[&i]() {return i++;});
      }
      postings_seek(docs, { irs::frequency::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::payload::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type(), irs::payload::type() });
    }

    // equals to postings_writer::BLOCK_SIZE
    {
      std::vector<irs::doc_id_t> docs;
      {
        const size_t count = VERSION10_POSTINGS_WRITER_BLOCK_SIZE;
        docs.reserve(count);
        auto i = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        std::generate_n(std::back_inserter(docs), count,[&i]() {return i++;});
      }
      postings_seek(docs, {});
      postings_seek(docs, { irs::frequency::type(), irs::position::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::payload::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type(), irs::payload::type() });
    }

    // long list
    {
      std::vector<irs::doc_id_t> docs;
      {
        const size_t count = 10000;
        docs.reserve(count);
        auto i = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        std::generate_n(std::back_inserter(docs), count,[&i]() {return i++;});
      }
      postings_seek(docs, {});
      postings_seek(docs, { irs::frequency::type(), irs::position::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::payload::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type(), irs::payload::type() });
    }

    // 2^15 
    {
      std::vector<irs::doc_id_t> docs;
      {
        const size_t count = 32768;
        docs.reserve(count);
        auto i = (irs::type_limits<irs::type_t::doc_id_t>::min)();
        std::generate_n(std::back_inserter(docs), count,[&i]() {return i+=2;});
      }
      postings_seek(docs, {});
      postings_seek(docs, { irs::frequency::type(), irs::position::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::payload::type() });
      postings_seek(docs, { irs::frequency::type(), irs::position::type(), irs::offset::type(), irs::payload::type() });
    }
  }
}; // format_10_test_case

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_format_10_test_case : public format_10_test_case {
 protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }
};

TEST_F(memory_format_10_test_case, directory_cleaner) {
  directory_artifact_cleaner();
}

TEST_F(memory_format_10_test_case, fields_rw) {
  fields_read_write();
}

TEST_F(memory_format_10_test_case, postings_rw) {
  postings_read_write_single_doc();
  postings_read_write();
}

TEST_F(memory_format_10_test_case, postings_seek) {
  postings_seek();
}

TEST_F(memory_format_10_test_case, segment_meta_rw) {
  segment_meta_read_write();
}

TEST_F(memory_format_10_test_case, columns_rw_sparse_column_dense_block) {
  sparse_column_dense_block();
}

TEST_F(memory_format_10_test_case, columns_rw_dense_mask) {
  columns_dense_mask();
}

TEST_F(memory_format_10_test_case, columns_rw_bit_mask) {
  columns_bit_mask();
}

TEST_F(memory_format_10_test_case, columns_rw_empty) {
  columns_read_write_empty();
}

TEST_F(memory_format_10_test_case, columns_rw_same_col_empty_repeat) {
  columns_read_write_same_col_empty_repeat();
}

TEST_F(memory_format_10_test_case, columns_rw_big_document) {
  columns_big_document_read_write();
}

TEST_F(memory_format_10_test_case, columns_rw_writer_reuse) {
  columns_read_write_writer_reuse();
}

TEST_F(memory_format_10_test_case, columns_rw_typed) {
  columns_read_write_typed();
}

TEST_F(memory_format_10_test_case, columns_rw_sparse_dense_offset_column_border_case) {
  dense_or_sparse_fixed_offset_border_case();
}

TEST_F(memory_format_10_test_case, columns_rw) {
  columns_read_write();
}

TEST_F(memory_format_10_test_case, columns_meta_rw) {
  columns_meta_read_write();
}

TEST_F(memory_format_10_test_case, document_mask_rw) {
  document_mask_read_write();
}

TEST_F(memory_format_10_test_case, reuse_postings_writer) {
  postings_writer_reuse();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_format_10_test_case : public format_10_test_case {
protected:
  virtual irs::directory* get_directory() override {
    auto dir = test_dir();

    dir /= "index";
    dir.mkdir();

    return new irs::fs_directory(dir.utf8());
  }
};

TEST_F(fs_format_10_test_case, test_load) {
  auto format = iresearch::formats::get("1_0");

  ASSERT_NE(nullptr, format);
}

TEST_F(fs_format_10_test_case, directory_cleaner) {
  directory_artifact_cleaner();
}

TEST_F(fs_format_10_test_case, fields_rw) {
  fields_read_write();
}

TEST_F(fs_format_10_test_case, postings_seek) {
  postings_seek();
}

TEST_F(fs_format_10_test_case, postings_rw) {
  postings_read_write();
  postings_read_write_single_doc();
}

TEST_F(fs_format_10_test_case, segment_meta_rw) {
  segment_meta_read_write();
}

TEST_F(fs_format_10_test_case, columns_rw_empty) {
  columns_read_write_empty();
}

TEST_F(fs_format_10_test_case, columns_rw_dense_mask) {
  columns_dense_mask();
}

TEST_F(fs_format_10_test_case, columns_rw_bit_mask) {
  columns_bit_mask();
}

TEST_F(fs_format_10_test_case, columns_rw_sparse_dense_offset_column_border_case) {
  dense_or_sparse_fixed_offset_border_case();
}

TEST_F(fs_format_10_test_case, columns_rw) {
  columns_read_write();
}

TEST_F(fs_format_10_test_case, columns_rw_same_col_empty_repeatcolumns_rw) {
  columns_read_write_same_col_empty_repeat();
}

TEST_F(fs_format_10_test_case, columns_rw_big_document) {
  columns_big_document_read_write();
}

TEST_F(fs_format_10_test_case, columns_rw_writer_reuse) {
  columns_read_write_writer_reuse();
}

TEST_F(fs_format_10_test_case, columns_rw_typed) {
  columns_read_write_typed();
}

TEST_F(fs_format_10_test_case, columns_meta_rw) {
  columns_meta_read_write();
}

TEST_F(fs_format_10_test_case, document_mask_rw) {
  document_mask_read_write();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
