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

#ifndef IRESEARCH_FORMAT_TEST_CASE_BASE
#define IRESEARCH_FORMAT_TEST_CASE_BASE

#include "tests_shared.hpp"

#include "search/cost.hpp"

#include "index/doc_generator.hpp"
#include "index/index_tests.hpp"
#include "iql/query_builder.hpp"

#include "analysis/token_attributes.hpp"
#include "store/memory_directory.hpp"
#include "utils/version_utils.hpp"

// ----------------------------------------------------------------------------
// --SECTION--                                                   Base test case
// ----------------------------------------------------------------------------

namespace tests {

class format_test_case_base : public index_test_base {
 public:  
  class postings;

  class position: public irs::position {
   public:
    position(const irs::flags& features): irs::position(2) {
      if (features.check<irs::offset>()) {
        attrs_.emplace(offs_);
      }

      if (features.check<irs::payload>()) {
        attrs_.emplace(pay_);
      }
    }

    uint32_t value() const override {
      return begin_;
    }

    bool next() override {
      if (begin_ == end_) {
        begin_ = irs::type_limits<irs::type_t::pos_t>::eof();

        return false;
      }

      ++begin_;

      const auto written = sprintf(pay_data_, "%d", begin_);
      pay_.value = irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(pay_data_), written);

      offs_.start = begin_;
      offs_.end = offs_.start + written;
      return true;
    }

    void clear() override {
      pay_.clear();
      offs_.clear();
    }

   private:
    friend class postings;

    uint32_t begin_{ irs::type_limits<irs::type_t::pos_t>::invalid() };
    uint32_t end_;
    irs::offset offs_{};
    irs::payload pay_{};
    char pay_data_[21]; // enough to hold numbers up to max of uint64_t
  };

  class postings: public irs::doc_iterator {
   public:
    typedef std::vector<irs::doc_id_t> docs_t;
    typedef std::vector<irs::cost::cost_t> costs_t;

    postings(
        const docs_t::const_iterator& begin,
        const docs_t::const_iterator& end,
        const irs::flags& features = irs::flags::empty_instance()
    )
      : next_(begin), end_(end), pos_(features) {
      if (features.check<irs::frequency>()) {
        freq_.value = 10;
        attrs_.emplace<irs::frequency>(freq_);

        if (features.check<irs::position>()) {
          attrs_.emplace(pos_);
        }
      }
    }

    bool next() override {
      if (next_ == end_) {
        doc_ = irs::type_limits<irs::type_t::doc_id_t>::eof();
        return false;
      }

      doc_ = *next_;
      pos_.begin_ = doc_;
      pos_.end_ = pos_.begin_ + 10;
      pos_.clear();
      ++next_;

      return true;
    }

    irs::doc_id_t value() const override {
      return doc_;
    }

    irs::doc_id_t seek(irs::doc_id_t target) override {
      irs::seek(*this, target);
      return value();
    }

    const irs::attribute_view& attributes() const NOEXCEPT override {
      return attrs_;
    }

   private:
    irs::attribute_view attrs_;
    docs_t::const_iterator next_;
    docs_t::const_iterator end_;
    irs::frequency freq_;
    tests::format_test_case_base::position pos_;
    irs::doc_id_t doc_{ irs::type_limits<irs::type_t::doc_id_t>::invalid() };
  }; // postings 

  template<typename Iterator>
  class terms: public irs::term_iterator {
  public:
    terms(const Iterator& begin, const Iterator& end)
      : next_(begin), end_(end) {
      docs_.push_back((irs::type_limits<irs::type_t::doc_id_t>::min)());
    }

    terms(const Iterator& begin, const Iterator& end,
        std::vector<irs::doc_id_t>::const_iterator doc_begin,
        std::vector<irs::doc_id_t>::const_iterator doc_end
    )
      : docs_(doc_begin, doc_end), next_(begin), end_(end) {
    }

    bool next() {
      if (next_ == end_) {
        return false;
      }

      val_ = irs::ref_cast<irs::byte_type>(*next_);
      ++next_;
      return true;
    }

    const irs::bytes_ref& value() const {
      return val_;
    }

    irs::doc_iterator::ptr postings(const irs::flags& features) const {
      return irs::doc_iterator::make<format_test_case_base::postings>(
        docs_.begin(), docs_.end()
      );
    }

    void read() { }

    const irs::attribute_view& attributes() const NOEXCEPT {
      return irs::attribute_view::empty_instance();
    }

  private:
    irs::bytes_ref val_;
    std::vector<irs::doc_id_t> docs_;
    Iterator next_;
    Iterator end_;
  }; // terms

  void assert_no_directory_artifacts(
    const iresearch::directory& dir,
    const iresearch::format& codec,
    const std::unordered_set<std::string>& expect_additional = std::unordered_set<std::string> ()
  ) {
    std::vector<std::string> dir_files;
    auto visitor = [&dir_files] (std::string& file) {
      // ignore lock file present in fs_directory
      if (iresearch::index_writer::WRITE_LOCK_NAME != file) {
        dir_files.emplace_back(std::move(file));
      }      
      return true;
    };
    ASSERT_TRUE(dir.visit(visitor));

    iresearch::index_meta index_meta;
    std::string segment_file;

    auto reader = codec.get_index_meta_reader();
    std::unordered_set<std::string> index_files(expect_additional.begin(), expect_additional.end());
    const bool exists = reader->last_segments_file(dir, segment_file);

    if (exists) {
      reader->read(dir, index_meta, segment_file);

      index_meta.visit_files([&index_files] (const std::string& file) {
        index_files.emplace(file);
        return true;
      });

      index_files.insert(segment_file);
    }

    for (auto& file: dir_files) {
      ASSERT_TRUE(index_files.erase(file) == 1);
    }

    ASSERT_TRUE(index_files.empty());
  }

  void directory_artifact_cleaner() {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory);
    tests::document const* doc1 = gen.next();
    tests::document const* doc2 = gen.next();
    tests::document const* doc3 = gen.next();
    tests::document const* doc4 = gen.next();
    auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
    auto query_doc2 = iresearch::iql::query_builder().build("name==B", std::locale::classic());
    auto query_doc3 = iresearch::iql::query_builder().build("name==C", std::locale::classic());
    auto query_doc4 = iresearch::iql::query_builder().build("name==D", std::locale::classic());

    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };

    iresearch::directory::ptr dir(get_directory());
    files.clear();
    ASSERT_TRUE(dir->visit(list_files));
    ASSERT_TRUE(files.empty());

    // register ref counter
    iresearch::directory_cleaner::init(*dir);

    // cleanup on refcount decrement (old files not in use)
    {
      // create writer to directory
      auto writer = iresearch::index_writer::make(*dir, codec(), iresearch::OM_CREATE);

      // initialize directory
      {
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // add first segment
      {
        ASSERT_TRUE(insert(*writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        ));
        ASSERT_TRUE(insert(*writer,
          doc2->indexed.begin(), doc2->indexed.end(),
          doc2->stored.begin(), doc2->stored.end()
        ));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // add second segment (creating new index_meta file, remove old)
      {
        ASSERT_TRUE(insert(*writer,
          doc3->indexed.begin(), doc3->indexed.end(),
          doc3->stored.begin(), doc3->stored.end()
        ));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // delete record from first segment (creating new index_meta file + doc_mask file, remove old)
      {
        writer->documents().remove(*(query_doc1.filter));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // delete all record from first segment (creating new index_meta file, remove old meta + unused segment)
      {
        writer->documents().remove(*(query_doc2.filter));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // delete all records from second segment (creating new index_meta file, remove old meta + unused segment)
      {
        writer->documents().remove(*(query_doc2.filter));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }
    }

    files.clear();
    ASSERT_TRUE(dir->visit(list_files));

    // reset directory
    for (auto& file: files) {
      ASSERT_TRUE(dir->remove(file));
    }

    files.clear();
    ASSERT_TRUE(dir->visit(list_files));
    ASSERT_TRUE(files.empty());

    // cleanup on refcount decrement (old files still in use)
    {
      // create writer to directory
      auto writer = iresearch::index_writer::make(*dir, codec(), iresearch::OM_CREATE);

      // initialize directory
      {
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // add first segment
      {
        ASSERT_TRUE(insert(*writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        ));
        ASSERT_TRUE(insert(*writer,
          doc2->indexed.begin(), doc2->indexed.end(),
          doc2->stored.begin(), doc2->stored.end()
        ));
        ASSERT_TRUE(insert(*writer,
          doc3->indexed.begin(), doc3->indexed.end(),
          doc3->stored.begin(), doc3->stored.end()
        ));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // delete record from first segment (creating new doc_mask file)
      {
        writer->documents().remove(*(query_doc1.filter));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }

      // create reader to directory
      auto reader = iresearch::directory_reader::open(*dir, codec());
      std::unordered_set<std::string> reader_files;
      {
        iresearch::index_meta index_meta;
        std::string segments_file;
        auto meta_reader = codec()->get_index_meta_reader();
        const bool exists = meta_reader->last_segments_file(*dir, segments_file);
        ASSERT_TRUE(exists);

        meta_reader->read(*dir, index_meta, segments_file);

        index_meta.visit_files([&reader_files] (std::string& file) {
          reader_files.emplace(std::move(file));
          return true;
        });

        reader_files.emplace(segments_file);
      }

      // add second segment (creating new index_meta file, not-removing old)
      {
        ASSERT_TRUE(insert(*writer,
          doc4->indexed.begin(), doc4->indexed.end(),
          doc4->stored.begin(), doc4->stored.end()
        ));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec(), reader_files);
      }

      // delete record from first segment (creating new doc_mask file, not-remove old)
      {
        writer->documents().remove(*(query_doc2.filter));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec(), reader_files);
      }

      // delete all record from first segment (creating new index_meta file, remove old meta but leave first segment)
      {
        writer->documents().remove(*(query_doc3.filter));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec(), reader_files);
      }

      // delete all records from second segment (creating new index_meta file, remove old meta + unused segment)
      {
        writer->documents().remove(*(query_doc4.filter));
        writer->commit();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec(), reader_files);
      }

      // close reader (remove old meta + old doc_mask + first segment)
      {
        reader.reset();
        iresearch::directory_cleaner::clean(*dir); // clean unused files
        assert_no_directory_artifacts(*dir, *codec());
      }
    }

    files.clear();
    ASSERT_TRUE(dir->visit(list_files));

    // reset directory
    for (auto& file: files) {
      ASSERT_TRUE(dir->remove(file));
    }

    files.clear();
    ASSERT_TRUE(dir->visit(list_files));
    ASSERT_TRUE(files.empty());

    // cleanup on writer startup
    {
      // fill directory
      {
        auto writer = iresearch::index_writer::make(*dir, codec(), iresearch::OM_CREATE);

        writer->commit(); // initialize directory
        ASSERT_TRUE(insert(*writer,
          doc1->indexed.begin(), doc1->indexed.end(),
          doc1->stored.begin(), doc1->stored.end()
        ));
        writer->commit(); // add first segment
        ASSERT_TRUE(insert(*writer,
          doc2->indexed.begin(), doc2->indexed.end(),
          doc2->stored.begin(), doc2->stored.end()
        ));
        ASSERT_TRUE(insert(*writer,
          doc3->indexed.begin(), doc3->indexed.end(),
          doc3->stored.begin(), doc3->stored.end()
        ));
        writer->commit(); // add second segment
        writer->documents().remove(*(query_doc1.filter));
        writer->commit(); // remove first segment
      }

      // add invalid files
      {
        iresearch::index_output::ptr tmp;
        tmp = dir->create("dummy.file.1");
        ASSERT_FALSE(!tmp);
        tmp = dir->create("dummy.file.2");
        ASSERT_FALSE(!tmp);
      }

      bool exists;
      ASSERT_TRUE(dir->exists(exists, "dummy.file.1") && exists);
      ASSERT_TRUE(dir->exists(exists, "dummy.file.2") && exists);

      // open writer
      auto writer = iresearch::index_writer::make(*dir, codec(), iresearch::OM_CREATE);

      // if directory has files (for fs directory) then ensure only valid meta+segments loaded
      ASSERT_TRUE(dir->exists(exists, "dummy.file.1") && !exists);
      ASSERT_TRUE(dir->exists(exists, "dummy.file.2") && !exists);
      assert_no_directory_artifacts(*dir, *codec());
    }
  }

  void fields_read_write() {
    // create sorted && unsorted terms
    typedef std::set<irs::bytes_ref> sorted_terms_t;
    typedef std::vector<irs::bytes_ref> unsorted_terms_t;
    sorted_terms_t sorted_terms;
    unsorted_terms_t unsorted_terms;

    tests::json_doc_generator gen(
      resource("fst_prefixes.json"),
      [&sorted_terms, &unsorted_terms] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
        doc.insert(std::make_shared<tests::templates::string_field>(
          irs::string_ref(name),
          data.str
        ));

        auto ref = irs::ref_cast<irs::byte_type>((doc.indexed.end() - 1).as<tests::templates::string_field>().value());
        sorted_terms.emplace(ref);
        unsorted_terms.emplace_back(ref);
    });

    // define field
    irs::field_meta field;
    field.name = "field";
    field.norm = 5;

    // write fields
    {
      irs::flush_state state;
      state.dir = &dir();
      state.doc_count = 100;
      state.name = "segment_name";
      state.features = &field.features;

      // should use sorted terms on write
      terms<sorted_terms_t::iterator> terms(sorted_terms.begin(), sorted_terms.end());

      auto writer = codec()->get_field_writer(false);
      writer->prepare(state);
      writer->write(field.name, field.norm, field.features, terms);
      writer->end();
    }

    // read field
    {
      irs::segment_meta meta;
      meta.name = "segment_name";

      irs::document_mask docs_mask;
      auto reader = codec()->get_field_reader();
      reader->prepare(dir(), meta, docs_mask);
      ASSERT_EQ(1, reader->size());

      // check terms
      ASSERT_EQ(nullptr, reader->field("invalid_field"));
      auto term_reader = reader->field(field.name);
      ASSERT_NE(nullptr, term_reader);
      ASSERT_EQ(field.name, term_reader->meta().name);
      ASSERT_EQ(field.norm, term_reader->meta().norm);
      ASSERT_EQ(field.features, term_reader->meta().features);

      ASSERT_EQ(sorted_terms.size(), term_reader->size());
      ASSERT_EQ(*sorted_terms.begin(), (term_reader->min)());
      ASSERT_EQ(*sorted_terms.rbegin(), (term_reader->max)());

      // check terms using "next"
      {
        auto expected_term = sorted_terms.begin();
        auto term = term_reader->iterator();
        for (; term->next(); ++expected_term) {
          ASSERT_EQ(*expected_term, term->value());
        }
        ASSERT_EQ(sorted_terms.end(), expected_term);
        ASSERT_FALSE(term->next());
      }

      // check terms using single "seek"
       {
         auto expected_sorted_term = sorted_terms.begin();
         for (auto end = sorted_terms.end(); expected_sorted_term != end; ++expected_sorted_term) {
           auto term = term_reader->iterator();
           ASSERT_TRUE(term->seek(*expected_sorted_term));
           ASSERT_EQ(*expected_sorted_term, term->value());
         }
       }

       // check sorted terms using "seek to cookie"
       {
         auto expected_sorted_term = sorted_terms.begin();
         auto term = term_reader->iterator();
         for (auto end = sorted_terms.end(); term->next(); ++expected_sorted_term) {
           ASSERT_EQ(*expected_sorted_term, term->value());

           // get cookie
           auto cookie = term->cookie();
           ASSERT_NE(nullptr, cookie);
           {
             auto seeked_term = term_reader->iterator();
             ASSERT_TRUE(seeked_term->seek(*expected_sorted_term, *cookie));
             ASSERT_EQ(*expected_sorted_term, seeked_term->value());

             // iterate to the end with seeked_term
             auto copy_expected_sorted_term = expected_sorted_term;
             for (++copy_expected_sorted_term; seeked_term->next(); ++copy_expected_sorted_term) {
               ASSERT_EQ(*copy_expected_sorted_term, seeked_term->value());
             }
             ASSERT_EQ(sorted_terms.end(), copy_expected_sorted_term);
             ASSERT_FALSE(seeked_term->next());
           }
         }
         ASSERT_EQ(sorted_terms.end(), expected_sorted_term);
         ASSERT_FALSE(term->next());
       }

       // check unsorted terms using "seek to cookie"
       {
         auto expected_term = unsorted_terms.begin();
         auto term = term_reader->iterator();
         for (auto end = unsorted_terms.end(); term->next(); ++expected_term) {
           auto sorted_term = sorted_terms.find(*expected_term);
           ASSERT_NE(sorted_terms.end(), sorted_term);

           // get cookie
           auto cookie = term->cookie();
           ASSERT_NE(nullptr, cookie);
           {
             auto seeked_term = term_reader->iterator();
             ASSERT_TRUE(seeked_term->seek(*sorted_term, *cookie));
             ASSERT_EQ(*sorted_term, seeked_term->value());

             // iterate to the end with seeked_term
             auto copy_sorted_term = sorted_term;
             for (++copy_sorted_term; seeked_term->next(); ++copy_sorted_term) {
               ASSERT_EQ(*copy_sorted_term, seeked_term->value());
             }
             ASSERT_EQ(sorted_terms.end(), copy_sorted_term);
             ASSERT_FALSE(seeked_term->next());
           }
         }
         ASSERT_EQ(unsorted_terms.end(), expected_term);
         ASSERT_FALSE(term->next());
       }

       // check sorted terms using multiple "seek"s on single iterator
       {
         auto expected_term = sorted_terms.begin();
         auto term = term_reader->iterator();
         for (auto end = sorted_terms.end(); expected_term != end; ++expected_term) {
           ASSERT_TRUE(term->seek(*expected_term));

           /* seek to the same term */
           ASSERT_TRUE(term->seek(*expected_term));
           ASSERT_EQ(*expected_term, term->value());
         }
       }
       
       /* check sorted terms in reverse order using multiple "seek"s on single iterator */
       {
         auto expected_term = sorted_terms.rbegin();
         auto term = term_reader->iterator();
         for (auto end = sorted_terms.rend(); expected_term != end; ++expected_term) {
           ASSERT_TRUE(term->seek(*expected_term));

           /* seek to the same term */
           ASSERT_TRUE(term->seek(*expected_term));
           ASSERT_EQ(*expected_term, term->value());
         }
       }
       
       /* check unsorted terms using multiple "seek"s on single iterator */
       {
         auto expected_term = unsorted_terms.begin();
         auto term = term_reader->iterator();
         for (auto end = unsorted_terms.end(); expected_term != end; ++expected_term) {
           ASSERT_TRUE(term->seek(*expected_term));

           /* seek to the same term */
           ASSERT_TRUE(term->seek(*expected_term));
           ASSERT_EQ(*expected_term, term->value());
         }
       }

       // seek to nil (the smallest possible term)
       {
         // with state
         {
           auto term = term_reader->iterator();
           ASSERT_FALSE(term->seek(irs::bytes_ref::NIL));
           ASSERT_EQ((term_reader->min)(), term->value());
           ASSERT_EQ(irs::SeekResult::NOT_FOUND, term->seek_ge(irs::bytes_ref::NIL));
           ASSERT_EQ((term_reader->min)(), term->value());
         }

         // without state
         {
           auto term = term_reader->iterator();
           ASSERT_FALSE(term->seek(irs::bytes_ref::NIL));
           ASSERT_EQ((term_reader->min)(), term->value());
         }

         {
           auto term = term_reader->iterator();
           ASSERT_EQ(irs::SeekResult::NOT_FOUND, term->seek_ge(irs::bytes_ref::NIL));
           ASSERT_EQ((term_reader->min)(), term->value());
         }
       }

       /* Here is the structure of blocks:
        *   TERM aaLorem
        *   TERM abaLorem
        *   BLOCK abab ------> Integer
        *                      ...
        *                      ...
        *   TERM abcaLorem
        *   ...
        *
        * Here we seek to "abaN" and since first entry that 
        * is greater than "abaN" is BLOCK entry "abab".
        *   
        * In case of "seek" we end our scan on BLOCK entry "abab",
        * and further "next" cause the skipping of the BLOCK "abab".
        *
        * In case of "seek_next" we also end our scan on BLOCK entry "abab"
        * but furher "next" get us to the TERM "ababInteger" */
       {
         auto seek_term = irs::ref_cast<irs::byte_type>(irs::string_ref("abaN"));
         auto seek_result = irs::ref_cast<irs::byte_type>(irs::string_ref("ababInteger"));

         /* seek exactly to term */
         {
           auto term = term_reader->iterator();
           ASSERT_FALSE(term->seek(seek_term));
           /* we on the BLOCK "abab" */
           ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abab")), term->value()); 
           ASSERT_TRUE(term->next());
           ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abcaLorem")), term->value());
         }

         /* seek to term which is equal or greater than current */
         {
           auto term = term_reader->iterator();
           ASSERT_EQ(irs::SeekResult::NOT_FOUND, term->seek_ge(seek_term));
           ASSERT_EQ(seek_result, term->value());

           /* iterate over the rest of the terms */
           auto expected_sorted_term = sorted_terms.find(seek_result);
           ASSERT_NE(sorted_terms.end(), expected_sorted_term);
           for (++expected_sorted_term; term->next(); ++expected_sorted_term) {
             ASSERT_EQ(*expected_sorted_term, term->value());
           }
           ASSERT_FALSE(term->next());
           ASSERT_EQ(sorted_terms.end(), expected_sorted_term);
         }
       }
    }
  }

  void segment_meta_read_write() {
    // read valid meta
    {
      iresearch::segment_meta meta;
      meta.name = "meta_name";
      meta.docs_count = 453;
      meta.live_docs_count = 345;
      meta.size = 666;
      meta.version = 100;
      meta.column_store = true;

      meta.files.emplace("file1");
      meta.files.emplace("index_file2");
      meta.files.emplace("file3");
      meta.files.emplace("stored_file4");

      std::string filename;

      // write segment meta
      {
        auto writer = codec()->get_segment_meta_writer();
        writer->write(dir(), filename, meta);
      }

      // read segment meta
      {
        irs::segment_meta read_meta;
        read_meta.name = meta.name;
        read_meta.version = 100;

        auto reader = codec()->get_segment_meta_reader();
        reader->read(dir(), read_meta);
        ASSERT_EQ(meta.codec, read_meta.codec); // codec stays nullptr
        ASSERT_EQ(meta.name, read_meta.name);
        ASSERT_EQ(meta.docs_count, read_meta.docs_count);
        ASSERT_EQ(meta.live_docs_count, read_meta.live_docs_count);
        ASSERT_EQ(meta.version, read_meta.version);
        ASSERT_EQ(meta.size, read_meta.size);
        ASSERT_EQ(meta.files, read_meta.files);
        ASSERT_EQ(meta.column_store, read_meta.column_store);
      }
    }

    // read broken meta (live_docs_count > docs_count)
    {
      iresearch::segment_meta meta;
      meta.name = "broken_meta_name";
      meta.docs_count = 453;
      meta.live_docs_count = 1345;
      meta.size = 666;
      meta.version = 100;

      meta.files.emplace("file1");
      meta.files.emplace("index_file2");
      meta.files.emplace("file3");
      meta.files.emplace("stored_file4");

      std::string filename;

      // write segment meta
      {
        auto writer = codec()->get_segment_meta_writer();
        writer->write(dir(), filename, meta);
      }

      // read segment meta
      {
        irs::segment_meta read_meta;
        read_meta.name = meta.name;
        read_meta.version = 100;

        auto reader = codec()->get_segment_meta_reader();
        ASSERT_THROW(reader->read(dir(), read_meta), irs::index_error);
      }
    }
  }

  void document_mask_read_write() {
    const std::unordered_set<iresearch::doc_id_t> mask_set = { 1, 4, 5, 7, 10, 12 };
    iresearch::segment_meta meta("_1", nullptr);
    meta.version = 42;

    // write document_mask
    {
      auto writer = codec()->get_document_mask_writer();

      writer->write(dir(), meta, mask_set);
    }

    // read document_mask
    {
      auto reader = codec()->get_document_mask_reader();
      irs::document_mask expected;
      EXPECT_TRUE(reader->read(dir(), meta, expected));
      for (auto id : mask_set) {
        EXPECT_EQ(1, expected.erase(id));
      }
      EXPECT_TRUE(expected.empty());
    }
  }

  void sparse_column_dense_block() {
    iresearch::segment_meta seg("_1", codec());

    size_t column_id;
    const irs::bytes_ref payload(irs::ref_cast<irs::byte_type>(irs::string_ref("abcd")));

    // write docs
    {

      auto writer = codec()->get_columnstore_writer();
      writer->prepare(dir(), seg);
      auto column = writer->push_column();
      column_id = column.first;
      auto& column_handler = column.second;

      auto id = irs::type_limits<irs::type_t::doc_id_t>::min();

      for (; id <= 1024; ++id, ++seg.docs_count) {
        auto& stream = column_handler(id);
        stream.write_bytes(payload.c_str(), payload.size());
      }

      ++id; // gap

      for (; id <= 2037; ++id, ++seg.docs_count) {
        auto& stream = column_handler(id);
        stream.write_bytes(payload.c_str(), payload.size());
      }

      ASSERT_TRUE(writer->commit());
    }

    // read documents
    {
      irs::bytes_ref actual_value;

      // check 1st segment
      {
        auto reader = codec()->get_columnstore_reader();
        ASSERT_TRUE(reader->prepare(dir(), seg));

        auto column = reader->column(column_id);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        irs::doc_id_t id = 0;
        ASSERT_FALSE(values(0, actual_value));

        for (++id; id < seg.docs_count; ++id) {
          if (id == 1025) {
            // gap
            ASSERT_FALSE(values(id, actual_value));
          } else {
            ASSERT_TRUE(values(id, actual_value));
            ASSERT_EQ(payload, actual_value);
          }
        }
      }
    }
  }

  void columns_dense_mask() {
    iresearch::segment_meta seg("_1", codec());
    const irs::doc_id_t MAX_DOC = 1026;

    size_t column_id;

    // write docs
    {
      auto writer = codec()->get_columnstore_writer();
      writer->prepare(dir(), seg);
      auto column = writer->push_column();
      column_id = column.first;
      auto& column_handler = column.second;

      for (auto id = irs::type_limits<irs::type_t::doc_id_t>::min(); id <= MAX_DOC; ++id, ++seg.docs_count) {
        column_handler(id);
      }

      ASSERT_TRUE(writer->commit());
    }

    // read documents
    {
      irs::bytes_ref actual_value;

      // check 1st segment
      {
        auto reader_1 = codec()->get_columnstore_reader();
        ASSERT_TRUE(reader_1->prepare(dir(), seg));

        auto column = reader_1->column(column_id);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        for (irs::doc_id_t id = 0; id < seg.docs_count; ) {
          ASSERT_TRUE(values(++id, actual_value));
          ASSERT_TRUE(actual_value.null());
        }
      }
    }
  }

  void columns_read_write_same_col_empty_repeat() {
    struct csv_doc_template: public csv_doc_generator::doc_template {
      virtual void init() {
        clear();
        reserve(3);
        insert(std::make_shared<tests::templates::string_field>("id"));
        insert(std::make_shared<tests::templates::string_field>("name"));
      }

      virtual void value(size_t idx, const irs::string_ref& value) {
        auto& field = indexed.get<tests::templates::string_field>(idx);

        // amount of data written per doc_id is < sizeof(doc_id)
        field.value(irs::string_ref("x", idx)); // length 0 or 1
      }
      virtual void end() {}
      virtual void reset() {}
    } doc_template; // two_columns_doc_template

    tests::csv_doc_generator gen(resource("simple_two_column.csv"), doc_template);
    iresearch::segment_meta seg("_1", nullptr);

    seg.codec = codec();

    std::unordered_map<std::string, iresearch::columnstore_writer::column_t> columns;

    // write documents
    {
      auto writer = codec()->get_columnstore_writer();
      iresearch::doc_id_t id = 0;
      writer->prepare(dir(), seg);

      for (const document* doc; seg.docs_count < 30000 && (doc = gen.next());) {
        ++id;

        for (auto& field : doc->stored) {
          const auto res = columns.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::string(field.name())),
            std::forward_as_tuple()
          );

          if (res.second) {
            res.first->second = writer->push_column();
          }

          auto& column = res.first->second.second;
          auto& stream = column(id);

          field.write(stream);

          // repeat requesting the same column without writing anything
          for (size_t i = 10; i; --i) {
            column(id);
          }
        }

        ++seg.docs_count;
      }

      ASSERT_TRUE(writer->commit());

      gen.reset();
    }

    // read documents
    {
      irs::bytes_ref actual_value;

      // check 1st segment
      {
        auto reader_1 = codec()->get_columnstore_reader();
        ASSERT_TRUE(reader_1->prepare(dir(), seg));

        auto id_column = reader_1->column(columns["id"].first);
        ASSERT_NE(nullptr, id_column);
        auto id_values = id_column->values();

        auto name_column = reader_1->column(columns["name"].first);
        ASSERT_NE(nullptr, name_column);
        auto name_values = name_column->values();

        gen.reset();
        irs::doc_id_t i = 0;
        for (const document* doc; i < seg.docs_count && (doc = gen.next());) {
          ++i;
          ASSERT_TRUE(id_values(i, actual_value));
          ASSERT_EQ(doc->stored.get<tests::templates::string_field>(0).value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
          ASSERT_TRUE(name_values(i, actual_value));
          ASSERT_EQ(doc->stored.get<tests::templates::string_field>(1).value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
        }
      }
    }
  }

  void columns_read_write_writer_reuse() {
    struct csv_doc_template: public csv_doc_generator::doc_template {
      virtual void init() {
        clear();
        reserve(2);
        insert(std::make_shared<tests::templates::string_field>("id"));
        insert(std::make_shared<tests::templates::string_field>("name"));
      }

      virtual void value(size_t idx, const irs::string_ref& value) {
        auto& field = indexed.get<tests::templates::string_field>(idx);
        field.value(value);
      }
      virtual void end() {}
      virtual void reset() {}
    } doc_template; // two_columns_doc_template 

    tests::csv_doc_generator gen(resource("simple_two_column.csv"), doc_template);

    iresearch::segment_meta seg_1("_1", nullptr);
    iresearch::segment_meta seg_2("_2", nullptr);
    iresearch::segment_meta seg_3("_3", nullptr);

    seg_1.codec = codec();
    seg_2.codec = codec();
    seg_3.codec = codec();

    std::unordered_map<std::string, iresearch::columnstore_writer::column_t> columns_1;
    std::unordered_map<std::string, iresearch::columnstore_writer::column_t> columns_2;
    std::unordered_map<std::string, iresearch::columnstore_writer::column_t> columns_3;

    // write documents 
    {
      auto writer = codec()->get_columnstore_writer();

      // write 1st segment 
      iresearch::doc_id_t id = 0;
      writer->prepare(dir(), seg_1);

      for (const document* doc; seg_1.docs_count < 30000 && (doc = gen.next());) {
        ++id;
        for (auto& field : doc->stored) {
          const auto res = columns_1.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::string(field.name())),
            std::forward_as_tuple()
          );

          if (res.second) {
            res.first->second = writer->push_column();
          }

          auto& column = res.first->second.second;
          auto& stream = column(id);

          field.write(stream);
        }
        ++seg_1.docs_count;
      }

      ASSERT_TRUE(writer->commit());

      gen.reset();

      // write 2nd segment 
      id = 0;
      writer->prepare(dir(), seg_2);

      for (const document* doc; seg_2.docs_count < 30000 && (doc = gen.next());) {
        ++id;
        for (auto& field : doc->stored) {
          const auto res = columns_2.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::string(field.name())),
            std::forward_as_tuple()
          );

          if (res.second) {
            res.first->second = writer->push_column();
          }
          
          auto& column = res.first->second.second;
          auto& stream = column(id);

          field.write(stream);
        }
        ++seg_2.docs_count;
      }

      ASSERT_TRUE(writer->commit());

      // write 3rd segment
      id = 0;
      writer->prepare(dir(), seg_3);

      for (const document* doc; seg_3.docs_count < 70000 && (doc = gen.next());) {
        ++id;
        for (auto& field : doc->stored) {
          const auto res = columns_3.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::string(field.name())),
            std::forward_as_tuple()
          );

          if (res.second) {
            res.first->second = writer->push_column();
          }

          auto& column = res.first->second.second;
          auto& stream = column(id);

          field.write(stream);
        }
        ++seg_3.docs_count;
      }

      ASSERT_TRUE(writer->commit());
    }

    // read documents
    {
      irs::bytes_ref actual_value;

      // check 1st segment
      {
        auto reader_1 = codec()->get_columnstore_reader();
        ASSERT_TRUE(reader_1->prepare(dir(), seg_1));

        auto id_column = reader_1->column(columns_1["id"].first);
        ASSERT_NE(nullptr, id_column);
        auto id_values = id_column->values();

        auto name_column = reader_1->column(columns_1["name"].first);
        ASSERT_NE(nullptr, name_column);
        auto name_values = name_column->values();

        gen.reset();
        irs::doc_id_t i = 0;
        for (const document* doc; i < seg_1.docs_count && (doc = gen.next());) {
          ++i;
          ASSERT_TRUE(id_values(i, actual_value));
          ASSERT_EQ(doc->stored.get<tests::templates::string_field>(0).value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
          ASSERT_TRUE(name_values(i, actual_value));
          ASSERT_EQ(doc->stored.get<tests::templates::string_field>(1).value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
        }

        // check 2nd segment (same as 1st)
        auto reader_2 = codec()->get_columnstore_reader();
        ASSERT_TRUE(reader_2->prepare(dir(), seg_2));

        auto id_column_2 = reader_2->column(columns_2["id"].first);
        ASSERT_NE(nullptr, id_column_2);
        auto id_values_2 = id_column_2->values();

        auto name_column_2 = reader_2->column(columns_2["name"].first);
        ASSERT_NE(nullptr, name_column_2);
        auto name_values_2 = name_column_2->values();

        // check for equality
        irs::bytes_ref value;
        for (irs::doc_id_t i = 0, count = seg_2.docs_count; i < count;) {
          ++i;
          ASSERT_TRUE(id_values(i, value));
          ASSERT_TRUE(id_values_2(i, actual_value));
          ASSERT_EQ(value, actual_value);
          ASSERT_TRUE(name_values(i, value));
          ASSERT_TRUE(name_values_2(i, actual_value));
          ASSERT_EQ(value, actual_value);
        }
      }

      // check 3rd segment
      {
        auto reader = codec()->get_columnstore_reader();
        ASSERT_TRUE(reader->prepare(dir(), seg_3));

        auto id_column = reader->column(columns_3["id"].first);
        ASSERT_NE(nullptr, id_column);
        auto id_values = id_column->values();

        auto name_column = reader->column(columns_3["name"].first);
        ASSERT_NE(nullptr, name_column);
        auto name_values = name_column->values();

        irs::doc_id_t i = 0;
        for (const document* doc; i < seg_3.docs_count && (doc = gen.next());) {
          ++i;
          ASSERT_TRUE(id_values(i, actual_value));
          ASSERT_EQ(doc->stored.get<tests::templates::string_field>(0).value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
          ASSERT_TRUE(name_values(i, actual_value));
          ASSERT_EQ(doc->stored.get<tests::templates::string_field>(1).value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
        }
      }
    }
  }

  void columns_meta_read_write() {
    // write meta
    {
      auto writer = codec()->get_column_meta_writer();
      irs::segment_meta meta0;
      irs::segment_meta meta1;

      meta0.name = "_1";
      meta1.name = "_2";

      // write segment _1
      writer->prepare(dir(), meta0);
      writer->write("_1_column1", 1);
      writer->write("_1_column2", 2);
      writer->write("_1_column0", 0);
      writer->flush();

      // write segment _2
      writer->prepare(dir(), meta1);
      writer->write("_2_column2", 2);
      writer->write("_2_column1", 1);
      writer->write("_2_column0", 0);
      writer->flush();
    }

    // read meta from segment _1
    {
      auto reader = codec()->get_column_meta_reader();
      size_t actual_count = 0;
      irs::field_id actual_max_id = 0;
      irs::segment_meta seg_meta;

      seg_meta.name = "_1";

      ASSERT_TRUE(reader->prepare(dir(), seg_meta, actual_count, actual_max_id));
      ASSERT_EQ(3, actual_count);
      ASSERT_EQ(2, actual_max_id);

      iresearch::column_meta meta;
      ASSERT_TRUE(reader->read(meta));
      ASSERT_EQ("_1_column1", meta.name);
      ASSERT_EQ(1, meta.id);
      ASSERT_TRUE(reader->read(meta));
      ASSERT_EQ("_1_column2", meta.name);
      ASSERT_EQ(2, meta.id);
      ASSERT_TRUE(reader->read(meta));
      ASSERT_EQ("_1_column0", meta.name);
      ASSERT_EQ(0, meta.id);
      ASSERT_FALSE(reader->read(meta));
    }

    // read meta from segment _2
    {
      auto reader = codec()->get_column_meta_reader();
      size_t actual_count = 0;
      irs::field_id actual_max_id = 0;
      irs::segment_meta seg_meta;

      seg_meta.name = "_2";

      ASSERT_TRUE(reader->prepare(dir(), seg_meta, actual_count, actual_max_id));
      ASSERT_EQ(3, actual_count);
      ASSERT_EQ(2, actual_max_id);

      iresearch::column_meta meta;
      ASSERT_TRUE(reader->read(meta));
      ASSERT_EQ("_2_column2", meta.name);
      ASSERT_EQ(2, meta.id);
      ASSERT_TRUE(reader->read(meta));
      ASSERT_EQ("_2_column1", meta.name);
      ASSERT_EQ(1, meta.id);
      ASSERT_TRUE(reader->read(meta));
      ASSERT_EQ("_2_column0", meta.name);
      ASSERT_EQ(0, meta.id);
      ASSERT_FALSE(reader->read(meta));
    }
  }

  void columns_read_write_empty() {
    iresearch::segment_meta meta0("_1", nullptr);
    meta0.version = 42;
    meta0.docs_count = 89;
    meta0.live_docs_count = 67;
    meta0.codec = codec();

    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_TRUE(dir().visit(list_files));
    ASSERT_TRUE(files.empty());

    iresearch::field_id column0_id;
    iresearch::field_id column1_id;

    // add columns
    {
      auto writer = codec()->get_columnstore_writer();
      writer->prepare(dir(), meta0);
      column0_id = writer->push_column().first;
      ASSERT_EQ(0, column0_id);
      column1_id = writer->push_column().first;
      ASSERT_EQ(1, column1_id);
      ASSERT_FALSE(writer->commit()); // flush empty columns
    }

    files.clear();
    ASSERT_TRUE(dir().visit(list_files));
    ASSERT_TRUE(files.empty()); // must be empty after flush

    {
      auto reader = codec()->get_columnstore_reader();
      ASSERT_FALSE(reader->prepare(dir(), meta0)); // no columns found

      irs::bytes_ref actual_value;

      // check empty column 0
      ASSERT_EQ(nullptr, reader->column(column0_id));
      // check empty column 1
      ASSERT_EQ(nullptr, reader->column(column1_id));
    }
  }

  void dense_or_sparse_fixed_offset_border_case() {
    // border case for dense/sparse fixed offset columns, e.g.
    // |-----|------------|  |-----|------------|
    // |doc  | value_size |  |doc  | value_size |
    // |-----|------------|  |-----|------------|
    // | 1   | 0          |  | 1   | 0          |
    // | 2   | 16         |  | 4   | 16         |
    // |-----|------------|  |-----|------------|

    irs::segment_meta meta0("_fixed_offset_columns", nullptr);
    meta0.version = 0;
    meta0.docs_count = 2;
    meta0.live_docs_count = 2;
    meta0.codec = codec();

    const uint64_t keys[] = { 42, 42 };
    const irs::bytes_ref keys_ref (
      reinterpret_cast<const irs::byte_type*>(&keys),
      sizeof keys
    );

    irs::columnstore_writer::column_t dense_fixed_offset_column;
    irs::columnstore_writer::column_t sparse_fixed_offset_column;

    {
      // write columns values
      auto writer = codec()->get_columnstore_writer();
      writer->prepare(dir(), meta0);

      dense_fixed_offset_column = writer->push_column();
      sparse_fixed_offset_column = writer->push_column();

      irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();

      // write first document
      {
        dense_fixed_offset_column.second(doc);
        sparse_fixed_offset_column.second(doc);
      }

      // write second document
      {
        {
          auto& stream = dense_fixed_offset_column.second(doc+1);

          stream.write_bytes(
            reinterpret_cast<const irs::byte_type*>(&keys),
            sizeof keys
          );
        }

        {
          auto& stream = sparse_fixed_offset_column.second(doc+3);

          stream.write_bytes(
            reinterpret_cast<const irs::byte_type*>(&keys),
            sizeof keys
          );
        }
      }

      ASSERT_TRUE(writer->commit());
    }

    // dense fixed offset column
    {
      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), meta0));

      auto column = reader->column(dense_fixed_offset_column.first);
      ASSERT_NE(nullptr, column);

      std::vector<std::pair<irs::doc_id_t, irs::bytes_ref>> expected_values {
        { irs::type_limits<irs::type_t::doc_id_t>::min(), irs::bytes_ref::NIL },
        { irs::type_limits<irs::type_t::doc_id_t>::min()+1, keys_ref },
      };

      // check iterator
      {
        auto it = column->iterator();
        auto payload = it->attributes().get<irs::payload_iterator>();

        for (auto& expected_value : expected_values) {
          ASSERT_TRUE(it->next());
          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_value.second, payload->value());
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
      }

      // check visit
      {
        auto expected_value = expected_values.begin();

        const auto res = column->visit([&expected_value](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_value) {
          if (expected_value->first != actual_doc) {
            return false;
          }

          if (expected_value->second != actual_value) {
            return false;
          }

          ++expected_value;

          return true;
        });

        ASSERT_TRUE(res);

        ASSERT_EQ(expected_values.end(), expected_value);
      }

      // random read
      {
        irs::bytes_ref actual_value;
        auto values = column->values();

        for (auto& expected_value : expected_values) {
          ASSERT_TRUE(values(expected_value.first, actual_value));
          ASSERT_EQ(expected_value.second, actual_value);
        }
      }
    }

    // sparse fixed offset column
    {
      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), meta0));

      auto column = reader->column(sparse_fixed_offset_column.first);
      ASSERT_NE(nullptr, column);

      std::vector<std::pair<irs::doc_id_t, irs::bytes_ref>> expected_values {
        { irs::type_limits<irs::type_t::doc_id_t>::min(), irs::bytes_ref::NIL },
        { irs::type_limits<irs::type_t::doc_id_t>::min()+3, keys_ref },
      };

      // check iterator
      {
        auto it = column->iterator();
        auto payload = it->attributes().get<irs::payload_iterator>();

        for (auto& expected_value : expected_values) {
          ASSERT_TRUE(it->next());
          ASSERT_EQ(expected_value.first, it->value());
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_value.second, payload->value());
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
      }

      // check visit
      {
        auto expected_value = expected_values.begin();

        ASSERT_TRUE(column->visit([&expected_value](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_value) {
          if (expected_value->first != actual_doc) {
            return false;
          }

          if (expected_value->second != actual_value) {
            return false;
          }

          ++expected_value;

          return true;
        }));

        ASSERT_EQ(expected_values.end(), expected_value);
      }

      // random read
      {
        irs::bytes_ref actual_value;
        auto values = column->values();

        for (auto& expected_value : expected_values) {
          ASSERT_TRUE(values(expected_value.first, actual_value));
          ASSERT_EQ(expected_value.second, actual_value);
        }
      }
    }
  }

  void columns_read_write() {
    iresearch::field_id segment0_field0_id;
    iresearch::field_id segment0_field1_id;
    iresearch::field_id segment0_empty_column_id;
    iresearch::field_id segment0_field2_id;
    iresearch::field_id segment0_field3_id;
    iresearch::field_id segment0_field4_id;
    iresearch::field_id segment1_field0_id;
    iresearch::field_id segment1_field1_id;
    iresearch::field_id segment1_field2_id;

    iresearch::segment_meta meta0("_1", nullptr);
    meta0.version = 42;
    meta0.docs_count = 89;
    meta0.live_docs_count = 67;
    meta0.codec = codec();

    iresearch::segment_meta meta1("_2", nullptr);
    meta1.version = 23;
    meta1.docs_count = 115;
    meta1.live_docs_count = 111;
    meta1.codec = codec();

    // read attributes from empty directory
    {
      auto reader = codec()->get_columnstore_reader();
      ASSERT_FALSE(reader->prepare(dir(), meta1)); // no attributes found

      // try to get invalild column
      ASSERT_EQ(nullptr, reader->column(iresearch::type_limits<iresearch::type_t::field_id_t>::invalid()));
    }

    // write columns values
    auto writer = codec()->get_columnstore_writer();

    // write _1 segment
    {
      writer->prepare(dir(), meta0);

      auto field0 = writer->push_column();
      segment0_field0_id = field0.first;
      auto& field0_writer = field0.second;
      ASSERT_EQ(0, segment0_field0_id);
      auto field1 = writer->push_column();
      segment0_field1_id = field1.first;
      auto& field1_writer = field1.second;
      ASSERT_EQ(1, segment0_field1_id);
      auto empty_field = writer->push_column(); // gap between filled columns
      segment0_empty_column_id = empty_field.first;
      ASSERT_EQ(2, segment0_empty_column_id);
      auto field2 = writer->push_column();
      segment0_field2_id = field2.first;
      auto& field2_writer = field2.second;
      ASSERT_EQ(3, segment0_field2_id);
      auto field3 = writer->push_column();
      segment0_field3_id = field3.first;
      auto& field3_writer = field3.second;
      ASSERT_EQ(4, segment0_field3_id);
      auto field4 = writer->push_column();
      segment0_field4_id = field4.first;
      auto& field4_writer = field4.second;
      ASSERT_EQ(5, segment0_field4_id);

      // column==field0
      {
        auto& stream = field0_writer(1);
        irs::write_string(stream, irs::string_ref("field0_doc0")); // doc==1
      }

      // column==field4
      {
        auto& stream = field4_writer(1); // doc==1
        irs::write_string(stream, irs::string_ref("field4_doc_min"));
      }

      // column==field1, multivalued attribute
      {
        auto& stream = field1_writer(1); // doc==1
        irs::write_string(stream, irs::string_ref("field1_doc0"));
        irs::write_string(stream, irs::string_ref("field1_doc0_1"));
      }

      // column==field2
      {
        // rollback
        {
          auto& stream = field2_writer(1); // doc==1
          irs::write_string(stream, irs::string_ref("invalid_string"));
          stream.reset(); // rollback changes
          stream.reset(); // rollback changes
        }
        {
          auto& stream = field2_writer(1); // doc==1
          irs::write_string(stream, irs::string_ref("field2_doc1"));
        }
      }

      // column==field0, rollback
      {
        auto& stream = field0_writer(2); // doc==2
        irs::write_string(stream, irs::string_ref("field0_doc1"));
        stream.reset();
      }

      // column==field0
      {
        auto& stream = field0_writer(2); // doc==2
        irs::write_string(stream, irs::string_ref("field0_doc2"));
      }

      // column==field0
      {
        auto& stream = field0_writer(33); // doc==33
        irs::write_string(stream, irs::string_ref("field0_doc33"));
      }

      // column==field1, multivalued attribute
      {
        // Get stream by the same key. In this case written values
        // will be concatenated and accessible via the specified key 
        // (e.g. 'field1_doc12_1', 'field1_doc12_2' in this case)
        {
          auto& stream = field1_writer(12); // doc==12
          irs::write_string(stream, irs::string_ref("field1_doc12_1"));
        }
        {
          auto& stream = field1_writer(12); // doc==12
          irs::write_string(stream, irs::string_ref("field1_doc12_2"));
        }
      }

      ASSERT_TRUE(writer->commit());
    }

    // write _2 segment, reuse writer
    {
      writer->prepare(dir(), meta1);

      auto field0 = writer->push_column();
      segment1_field0_id = field0.first;
      auto& field0_writer = field0.second;
      ASSERT_EQ(0, segment1_field0_id);
      auto field1 = writer->push_column();
      segment1_field1_id = field1.first;
      auto& field1_writer = field1.second;
      ASSERT_EQ(1, segment1_field1_id);
      auto field2 = writer->push_column();
      segment1_field2_id = field2.first;
      auto& field2_writer = field2.second;
      ASSERT_EQ(2, segment1_field2_id);

      // column==field3
      {
        auto& stream = field2_writer(1); // doc==1
        irs::write_string(stream, irs::string_ref("segment_2_field3_doc0"));
      }

      // column==field1, multivalued attribute
      {
        auto& stream = field0_writer(1); // doc==1
        irs::write_string(stream, irs::string_ref("segment_2_field1_doc0"));
      }

      // column==field2, rollback
      {
        auto& stream = field1_writer(1);
        irs::write_string(stream, irs::string_ref("segment_2_field2_doc0"));
        stream.reset(); // rollback
      }

      // column==field3, rollback
      {
        auto& stream = field2_writer(2); // doc==2
        irs::write_string(stream, irs::string_ref("segment_2_field0_doc1"));
        stream.reset(); // rollback
      }

      // colum==field1
      {
        auto& stream = field0_writer(12); // doc==12
        irs::write_string(stream, irs::string_ref("segment_2_field1_doc12"));
      }

      // colum==field3
      {
        auto& stream = field2_writer(23); // doc==23
        irs::write_string(stream, irs::string_ref("segment_2_field3_doc23"));
        stream.reset(); // rollback
      }

      ASSERT_TRUE(writer->commit());
    }

    // read columns values from segment _1
    {
      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), meta0));

      // try to get invalild column
      ASSERT_EQ(nullptr, reader->column(irs::type_limits<irs::type_t::field_id_t>::invalid()));

      // check field4
      {
        irs::bytes_ref actual_value;
        auto column_reader = reader->column(segment0_field4_id);
        ASSERT_NE(nullptr, column_reader);
        auto column = column_reader->values();

        ASSERT_FALSE(column((std::numeric_limits<irs::doc_id_t>::min)(), actual_value)); // check doc==min, column==field4
        ASSERT_TRUE(column(1, actual_value)); // check doc==1, column==field4
        ASSERT_EQ("field4_doc_min", irs::to_string<irs::string_ref>(actual_value.c_str()));
      }

      // visit field0 values (not cached)
      {
        std::unordered_map<irs::string_ref, iresearch::doc_id_t> expected_values = {
          {"field0_doc0", 1},
          {"field0_doc2", 2},
          {"field0_doc33", 33}
        };

        auto visitor = [&expected_values] (iresearch::doc_id_t doc, const irs::bytes_ref& value) {
          const auto actual_value = irs::to_string<irs::string_ref>(value.c_str());

          auto it = expected_values.find(actual_value);
          if (it == expected_values.end()) {
            // can't find value
            return false;
          }

          if (it->second != doc) {
            // wrong document
            return false;
          }

          expected_values.erase(it);
          return true;
        };

        auto column = reader->column(segment0_field0_id);
        ASSERT_NE(nullptr, column);
        ASSERT_TRUE(column->visit(visitor));
        ASSERT_TRUE(expected_values.empty());
      }

      // partailly visit field0 values (not cached)
      {
        std::unordered_map<irs::string_ref, iresearch::doc_id_t> expected_values = {
          {"field0_doc0", 1},
          {"field0_doc2", 2},
          {"field0_doc33", 33}
        };

        size_t calls_count = 0;
        auto visitor = [&expected_values, &calls_count] (iresearch::doc_id_t doc, const irs::bytes_ref& in) {
          ++calls_count;

          if (calls_count > 2) {
            // break the loop
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());

          auto it = expected_values.find(actual_value);
          if (it == expected_values.end()) {
            // can't find value
            return false;
          }

          if (it->second != doc) {
            // wrong document
            return false;
          }

          expected_values.erase(it);
          return true;
        };

        auto column = reader->column(segment0_field0_id);
        ASSERT_NE(nullptr, column);
        ASSERT_FALSE(column->visit(visitor));
        ASSERT_FALSE(expected_values.empty());
        ASSERT_EQ(1, expected_values.size());
        ASSERT_NE(expected_values.end(), expected_values.find("field0_doc33"));
      }

      // check field0
      {
        irs::bytes_ref actual_value;
        auto column_reader = reader->column(segment0_field0_id);
        ASSERT_NE(nullptr, column_reader);
        auto column = column_reader->values();

        // read (not cached)
        {
          ASSERT_TRUE(column(1, actual_value)); // check doc==1, column==field0
          ASSERT_EQ("field0_doc0", irs::to_string<irs::string_ref>(actual_value.c_str()));
          ASSERT_FALSE(column(5, actual_value)); // doc without value in field0
          ASSERT_EQ("field0_doc0", irs::to_string<irs::string_ref>(actual_value.c_str()));
          ASSERT_TRUE(column(33, actual_value)); // check doc==33, column==field0
          ASSERT_EQ("field0_doc33", irs::to_string<irs::string_ref>(actual_value.c_str()));
        }

        // read (cached)
        {
          ASSERT_TRUE(column(1, actual_value)); // check doc==0, column==field0
          ASSERT_EQ("field0_doc0", irs::to_string<irs::string_ref>(actual_value.c_str()));
          ASSERT_FALSE(column(5, actual_value)); // doc without value in field0
          ASSERT_EQ("field0_doc0", irs::to_string<irs::string_ref>(actual_value.c_str()));
          ASSERT_TRUE(column(33, actual_value)); // check doc==33, column==field0
          ASSERT_EQ("field0_doc33", irs::to_string<irs::string_ref>(actual_value.c_str()));
        }
      }

      // visit field0 values (cached)
      {
        std::unordered_map<irs::string_ref, iresearch::doc_id_t> expected_values = {
          {"field0_doc0", 1},
          {"field0_doc2", 2},
          {"field0_doc33", 33}
        };

        auto visitor = [&expected_values] (iresearch::doc_id_t doc, const irs::bytes_ref& in) {
          const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());

          auto it = expected_values.find(actual_value);
          if (it == expected_values.end()) {
            // can't find value
            return false;
          }

          if (it->second != doc) {
            // wrong document
            return false;
          }

          expected_values.erase(it);
          return true;
        };

        auto column_reader = reader->column(segment0_field0_id);
        ASSERT_NE(nullptr, column_reader);
        ASSERT_TRUE(column_reader->visit(visitor));
        ASSERT_TRUE(expected_values.empty());
      }

      // iterate over field0 values (cached)
      {
        auto column_reader = reader->column(segment0_field0_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::string_ref, irs::doc_id_t>> expected_values = {
          {"field0_doc0", 1},
          {"field0_doc2", 2},
          {"field0_doc33", 33}
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.second, it->value());
          ASSERT_EQ(expected_value.first, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek over field0 values (cached)
      {
        auto column_reader = reader->column(segment0_field0_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::string_ref, std::pair<irs::doc_id_t, irs::doc_id_t>>> expected_values = {
          {"field0_doc0", { 0, 1 } },
          {"field0_doc2", { 2, 2 } },
          {"field0_doc33",{ 22, 33 }}, {"field0_doc33", { 33, 33 } }
        };

        for (auto& expected : expected_values) {
          const auto target = expected.second.first;
          const auto expected_doc = expected.second.second;
          const auto expected_value = expected.first;

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_value, irs::to_string<irs::string_ref>(payload->value().c_str()));
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // iterate over field1 values (not cached)
      {
        auto column_reader = reader->column(segment0_field1_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<std::vector<irs::string_ref>, irs::doc_id_t>> expected_values = {
          { { "field1_doc0", "field1_doc0_1" }, 1},
          { { "field1_doc12_1", "field1_doc12_2" }, 12 }
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];

          std::vector<irs::string_ref> actual_str_values;
          actual_str_values.push_back(irs::to_string<irs::string_ref>(payload->value().c_str()));
          actual_str_values.push_back(irs::to_string<irs::string_ref>(
            reinterpret_cast<const irs::byte_type*>(actual_str_values.back().c_str() + actual_str_values.back().size())
          ));

          ASSERT_EQ(expected_value.second, it->value());
          ASSERT_EQ(expected_value.first, actual_str_values);
        }
        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek over field1 values (cached)
      {
        auto column_reader = reader->column(segment0_field1_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<std::vector<irs::string_ref>, std::pair<irs::doc_id_t, irs::doc_id_t>>> expected_values = {
          { { "field1_doc0", "field1_doc0_1" }, { 0, 1 } },
          { { "field1_doc12_1", "field1_doc12_2" }, { 1, 12 } }, { { "field1_doc12_1", "field1_doc12_2" }, { 3, 12 } }, { { "field1_doc12_1", "field1_doc12_2" }, { 12, 12 } }
        };

        for (auto& expected : expected_values) {
          const auto expected_doc = expected.second.second;
          const auto& expected_value = expected.first;

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());

          std::vector<irs::string_ref> actual_str_values;
          actual_str_values.push_back(irs::to_string<irs::string_ref>(payload->value().c_str()));
          actual_str_values.push_back(irs::to_string<irs::string_ref>(
            reinterpret_cast<const irs::byte_type*>(actual_str_values.back().c_str() + actual_str_values.back().size())
          ));

          ASSERT_EQ(expected_value, actual_str_values);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // check field1 (multiple values per document - cached)
      {
        irs::bytes_ref actual_value;
        irs::bytes_ref_input in;
        auto column_reader = reader->column(segment0_field1_id);
        ASSERT_NE(nullptr, column_reader);
        auto column = column_reader->values();

        // read compound column value
        // check doc==0, column==field1
        ASSERT_TRUE(column(1, actual_value)); in.reset(actual_value);
        ASSERT_EQ("field1_doc0", irs::read_string<std::string>(in));
        ASSERT_EQ("field1_doc0_1", irs::read_string<std::string>(in));

        ASSERT_FALSE(column(2, actual_value));

        // read overwritten compund value
        // check doc==12, column==field1
        ASSERT_TRUE(column(12, actual_value)); in.reset(actual_value);
        ASSERT_EQ("field1_doc12_1", irs::read_string<std::string>(in));
        ASSERT_EQ("field1_doc12_2", irs::read_string<std::string>(in));

        ASSERT_FALSE(column(13, actual_value));

        // read by invalid key
        ASSERT_FALSE(column(iresearch::type_limits<iresearch::type_t::doc_id_t>::eof(), actual_value));
      }

      // visit empty column
      {
        size_t calls_count = 0;
        auto visitor = [&calls_count] (iresearch::doc_id_t doc, const irs::bytes_ref& in) {
          ++calls_count;
          return true;
        };

        auto column_reader = reader->column(segment0_empty_column_id);
        ASSERT_NE(nullptr, column_reader);
        ASSERT_TRUE(column_reader->visit(visitor));
        ASSERT_EQ(0, calls_count);
      }

      // iterate over empty column
      {
        auto column_reader = reader->column(segment0_empty_column_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!it->attributes().get<irs::payload_iterator>());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
      }

      // visit field2 values (field after an the field)
      {
        std::unordered_map<irs::string_ref, iresearch::doc_id_t> expected_values = {
          {"field2_doc1", 1},
        };

        auto visitor = [&expected_values] (iresearch::doc_id_t doc, const irs::bytes_ref& in) {
          const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());

          auto it = expected_values.find(actual_value);
          if (it == expected_values.end()) {
            // can't find value
            return false;
          }

          if (it->second != doc) {
            // wrong document
            return false;
          }

          expected_values.erase(it);
          return true;
        };

        auto column_reader = reader->column(segment0_field2_id);
        ASSERT_NE(nullptr, column_reader);
        ASSERT_TRUE(column_reader->visit(visitor));
        ASSERT_TRUE(expected_values.empty());
      }

      // iterate over field2 values (not cached)
      {
        auto column_reader = reader->column(segment0_field2_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::string_ref, irs::doc_id_t>> expected_values = {
          {"field2_doc1", 1}
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.second, it->value());
          ASSERT_EQ(expected_value.first, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek over field2 values (cached)
      {
        auto column_reader = reader->column(segment0_field2_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::string_ref, std::pair<irs::doc_id_t, irs::doc_id_t>>> expected_values = {
          {"field2_doc1", { 1, 1 } }
        };

        for (auto& expected : expected_values) {
          const auto expected_doc = expected.second.second;
          const auto expected_value = expected.first;

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }
    }

    // read columns values from segment _2
    {
      irs::bytes_ref actual_value;
      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), meta1));

      // try to read invalild column
      {
        ASSERT_EQ(nullptr, reader->column(irs::type_limits<irs::type_t::field_id_t>::invalid()));
      }

      // iterate over field0 values (not cached)
      {
        auto column_reader = reader->column(segment1_field0_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::string_ref, irs::doc_id_t>> expected_values = {
          {"segment_2_field1_doc0", 1},
          {"segment_2_field1_doc12", 12}
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.second, it->value());
          ASSERT_EQ(expected_value.first, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // seek over field0 values (cached)
      {
        auto column_reader = reader->column(segment1_field0_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::string_ref, std::pair<irs::doc_id_t, irs::doc_id_t>>> expected_values = {
          {"segment_2_field1_doc0", { 0, 1 } },
          {"segment_2_field1_doc12", { 12, 12} }
        };

        for (auto& expected : expected_values) {
          const auto target = expected.second.first;
          const auto expected_doc = expected.second.second;
          const auto expected_value = expected.first;

          ASSERT_EQ(expected_doc, it->seek(expected_doc));
          ASSERT_TRUE(payload->next());
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());
          ASSERT_EQ(expected_value, actual_str_value);
        }

        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->seek(13));
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // check field0 (cached)
      {
        auto column_reader = reader->column(segment1_field0_id);
        ASSERT_NE(nullptr, column_reader);
        auto column = column_reader->values();
        ASSERT_TRUE(column(1, actual_value)); // check doc==1, column==field0
        ASSERT_EQ("segment_2_field1_doc0", irs::to_string<irs::string_ref>(actual_value.c_str()));
        ASSERT_TRUE(column(12, actual_value)); // check doc==12, column==field1
        ASSERT_EQ("segment_2_field1_doc12", irs::to_string<irs::string_ref>(actual_value.c_str()));
        ASSERT_FALSE(column(5, actual_value)); // doc without value in field0
        ASSERT_EQ("segment_2_field1_doc12", irs::to_string<irs::string_ref>(actual_value.c_str()));
      }

      // iterate over field0 values (cached)
      {
        auto column_reader = reader->column(segment1_field0_id);
        ASSERT_NE(nullptr, column_reader);
        auto it = column_reader->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::string_ref, irs::doc_id_t>> expected_values = {
          {"segment_2_field1_doc0", 1},
          {"segment_2_field1_doc12", 12}
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          const auto& expected_value = expected_values[i];
          const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value().c_str());

          ASSERT_EQ(expected_value.second, it->value());
          ASSERT_EQ(expected_value.first, actual_str_value);
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }
    }
  }

  void columns_bit_mask() {
    iresearch::segment_meta segment("bit_mask", nullptr);
    iresearch::field_id id;

    segment.codec = codec();

    // write bit mask into the column without actual data
    {
      auto writer = codec()->get_columnstore_writer();
      writer->prepare(dir(), segment);

      auto column = writer->push_column();

      id = column.first; 
      auto& handle = column.second;
      // we don't support irs::type_limits<<irs::type_t::doc_id_t>::invalid() key value
      handle(2); ++segment.docs_count;
      handle(4); ++segment.docs_count;
      handle(8); ++segment.docs_count;
      handle(9); ++segment.docs_count;
      // we don't support irs::type_limits<<irs::type_t::doc_id_t>::eof() key value

      ASSERT_TRUE(writer->commit());
    }

    // check previously written mask
    // - random read (not cached)
    // - iterate (cached)
    {
      irs::bytes_ref actual_value;
      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), segment));

      // read field values (not cached)
      {
        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto mask = column->values();
        ASSERT_FALSE(mask(0, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_FALSE(mask(1, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_FALSE(mask(1, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(4, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_FALSE(mask(6, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(8, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(9, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_FALSE(mask(irs::type_limits<irs::type_t::doc_id_t>::eof(), actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(2, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }

      // seek over field values (cached)
      {
        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<std::pair<irs::doc_id_t, irs::doc_id_t>> expected_values = {
          { 0, 2 }, { 2, 2 },
          { 3, 4 }, { 4, 4 },
          { 5, 8 },
          { 9, 9 },
          { 10, irs::type_limits<irs::type_t::doc_id_t>::eof() },
          { 6, irs::type_limits<irs::type_t::doc_id_t>::eof() },
          { 10, irs::type_limits<irs::type_t::doc_id_t>::eof() }
        };

        for (auto& pair : expected_values) {
          const auto value_to_find = pair.first;
          const auto expected_value = pair.second;

          ASSERT_EQ(expected_value, it->seek(value_to_find));
          ASSERT_EQ(!irs::type_limits<irs::type_t::doc_id_t>::eof(expected_value), payload->next());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // iterate over field values (cached)
      {
        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<irs::doc_id_t> expected_values = {
          2, 4, 8, 9
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_values[i], it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }
    }

    // check previously written mask
    // - iterate (not cached)
    // - random read (cached)
    // - iterate (cached)
    {
      irs::bytes_ref actual_value;
      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), segment));

      {
        // iterate over field values (not cached)
        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<irs::doc_id_t> expected_values = {
          2, 4, 8, 9
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_values[i], it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }

      // read field values (cached)
      {
        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto mask = column->values();
        ASSERT_FALSE(mask(0, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_FALSE(mask(1, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_FALSE(mask(1, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(4, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_FALSE(mask(6, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(8, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(2, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        ASSERT_TRUE(mask(9, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }

      // iterate over field values (cached)
      {
        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        std::vector<irs::doc_id_t> expected_values = {
          2, 4, 8, 9
        };

        size_t i = 0;
        for (; it->next(); ++i) {
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(expected_values[i], it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
        }

        ASSERT_FALSE(it->next());
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(i, expected_values.size());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }
    }
  }

  void columns_big_document_read_write() {
    struct big_stored_field {
      bool write(iresearch::data_output& out) const {
        out.write_bytes(reinterpret_cast<const iresearch::byte_type*>(buf), sizeof buf);
        return true;
      }

      char buf[65536];
    } field;

    std::fstream stream(resource("simple_two_column.csv"));
    ASSERT_FALSE(!stream);

    irs::field_id id;

    iresearch::segment_meta segment("big_docs", nullptr);

    segment.codec = codec();

    // write big document 
    {
      auto writer = codec()->get_columnstore_writer();
      writer->prepare(dir(), segment);

      auto column = writer->push_column();
      id = column.first;

      {
        auto& out = column.second(1);
        stream.read(field.buf, sizeof field.buf);
        ASSERT_FALSE(!stream); // ensure that all requested data has been read
        ASSERT_TRUE(field.write(out)); // must be written
        ++segment.docs_count;
      }

      {
        auto& out = column.second(2);
        stream.read(field.buf, sizeof field.buf);
        ASSERT_FALSE(!stream); // ensure that all requested data has been read
        ASSERT_TRUE(field.write(out)); // must be written
        ++segment.docs_count;
      }

      ASSERT_TRUE(writer->commit());
    }

    // read big document
    {
      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), segment));

      // random access
      {
        irs::bytes_ref actual_value;

        stream.clear(); // clear eof flag if set
        stream.seekg(0, stream.beg); // seek to the beginning of the file

        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto values = column->values();

        std::memset(field.buf, 0, sizeof field.buf); // clear buffer
        stream.read(field.buf, sizeof field.buf);
        ASSERT_TRUE(bool(stream));
        ASSERT_TRUE(values(1, actual_value));
        ASSERT_EQ(irs::string_ref(field.buf, sizeof field.buf), irs::ref_cast<char>(actual_value));

        std::memset(field.buf, 0, sizeof field.buf); // clear buffer
        stream.read(field.buf, sizeof field.buf);
        ASSERT_TRUE(bool(stream));
        ASSERT_TRUE(values(2, actual_value));
        ASSERT_EQ(irs::string_ref(field.buf, sizeof field.buf), irs::ref_cast<char>(actual_value));
      }

      // iterator "next"
      {
        stream.clear(); // clear eof flag if set
        stream.seekg(0, stream.beg); // seek to the beginning of the file

        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_TRUE(it->next());
        ASSERT_TRUE(payload->next());
        std::memset(field.buf, 0, sizeof field.buf); // clear buffer
        stream.read(field.buf, sizeof field.buf);
        ASSERT_TRUE(bool(stream));
        ASSERT_EQ(1, it->value());
        ASSERT_EQ(irs::string_ref(field.buf, sizeof field.buf), irs::ref_cast<char>(payload->value()));

        ASSERT_TRUE(it->next());
        ASSERT_TRUE(payload->next());
        std::memset(field.buf, 0, sizeof field.buf); // clear buffer
        stream.read(field.buf, sizeof field.buf);
        ASSERT_TRUE(bool(stream));
        ASSERT_EQ(2, it->value());
        ASSERT_EQ(irs::string_ref(field.buf, sizeof field.buf), irs::ref_cast<char>(payload->value()));

        ASSERT_FALSE(it->next());
      }

      // iterator "seek"
      {
        stream.clear(); // clear eof flag if set
        stream.seekg(0, stream.beg); // seek to the beginning of the file

        auto column = reader->column(id);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());

        ASSERT_EQ(1, it->seek(0));
        ASSERT_TRUE(payload->next());
        std::memset(field.buf, 0, sizeof field.buf); // clear buffer
        stream.read(field.buf, sizeof field.buf);
        ASSERT_TRUE(bool(stream));
        ASSERT_EQ(irs::string_ref(field.buf, sizeof field.buf), irs::ref_cast<char>(payload->value()));

        ASSERT_EQ(2, it->seek(2));
        ASSERT_TRUE(payload->next());
        std::memset(field.buf, 0, sizeof field.buf); // clear buffer
        stream.read(field.buf, sizeof field.buf);
        ASSERT_TRUE(bool(stream));
        ASSERT_EQ(irs::string_ref(field.buf, sizeof field.buf), irs::ref_cast<char>(payload->value()));

        ASSERT_FALSE(it->next());
      }
    }
  }

  void columns_read_write_typed() {
    struct Value {
      enum class Type {
        String, Binary, Double
      };

      Value(const irs::string_ref& name, const irs::string_ref& value)
        : name(name), value(value), type(Type::String) {
      }

      Value(const irs::string_ref& name, const irs::bytes_ref& value)
        : name(name), value(value), type(Type::Binary) {
      }

      Value(const irs::string_ref& name, double_t value)
        : name(name), value(value), type(Type::Double) {
      }

      irs::string_ref name;
      struct Rep {
        Rep(const irs::string_ref& value): sValue(value) {}
        Rep(const irs::bytes_ref& value): binValue(value) {}
        Rep(double_t value) : dblValue(value) {}
        ~Rep() { }

        irs::string_ref sValue;
        irs::bytes_ref binValue;
        double_t dblValue;
      } value;
      Type type;
    };

    std::deque<Value> values;
    tests::json_doc_generator gen(
      resource("simple_sequential_33.json"),
      [&values](tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<templates::string_field>(
          irs::string_ref(name),
          data.str
        ));

        auto& field = (doc.indexed.end() - 1).as<templates::string_field>();
        values.emplace_back(field.name(), field.value());
      } else if (data.is_null()) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(iresearch::string_ref(name));
        field.value(irs::null_token_stream::value_null());
        values.emplace_back(field.name(), field.value());
      } else if (data.is_bool() && data.b) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(iresearch::string_ref(name));
        field.value(irs::boolean_token_stream::value_true());
        values.emplace_back(field.name(), field.value());
      } else if (data.is_bool() && !data.b) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(iresearch::string_ref(name));
        field.value(irs::boolean_token_stream::value_true());
        values.emplace_back(field.name(), field.value());
      } else if (data.is_number()) {
        const double dValue = data.as_number<double_t>();

        // 'value' can be interpreted as a double
        doc.insert(std::make_shared<tests::double_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
        field.name(iresearch::string_ref(name));
        field.value(dValue);
        values.emplace_back(field.name(), field.value());
      }
    });

    iresearch::segment_meta meta("_1", nullptr);
    meta.version = 42;
    meta.codec = codec();

    std::unordered_map<std::string, irs::columnstore_writer::column_t> columns;

    // write stored documents
    {
      auto writer = codec()->get_columnstore_writer();
      writer->prepare(dir(), meta);

      irs::doc_id_t id = 0;

      for (const document* doc; (doc = gen.next());) {
        ++id;

        for (const auto& field : doc->stored) {
          const auto res = columns.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(std::string(field.name())),
            std::forward_as_tuple()
          );

          if (res.second) {
            res.first->second = writer->push_column();
          }

          auto& column = res.first->second.second;
          auto& stream = column(id);

          ASSERT_TRUE(field.write(stream));
        }
        ++meta.docs_count;
      }

      ASSERT_TRUE(writer->commit());
    }

    // read stored documents
    {
      gen.reset();

      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), meta));

      std::unordered_map<std::string, iresearch::columnstore_reader::values_reader_f> readers;

      irs::bytes_ref actual_value;
      irs::bytes_ref_input in;
      iresearch::doc_id_t i = 0;
      size_t value_id = 0;
      for (const document* doc; (doc = gen.next());) {
        ++i;

        for (size_t size = doc->stored.size(); size; --size) {
          auto& expected_field = values[value_id++];
          const std::string name(expected_field.name);
          const auto res = readers.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(name),
            std::forward_as_tuple()
          );

          if (res.second) {
            auto column = reader->column(columns[name].first);
            ASSERT_NE(nullptr, column);
            res.first->second = column->values();
          }

          auto& column_reader = res.first->second;
          ASSERT_TRUE(column_reader(i, actual_value)); in.reset(actual_value);

          switch (expected_field.type) {
            case Value::Type::String: {
              ASSERT_EQ(expected_field.value.sValue, irs::read_string<std::string>(in));
            } break;
            case Value::Type::Binary: {
              ASSERT_EQ(expected_field.value.binValue, irs::read_string<irs::bstring>(in));
            } break;
            case Value::Type::Double: {
              ASSERT_EQ(expected_field.value.dblValue, irs::read_zvdouble(in));
            } break;
            default:
              ASSERT_TRUE(false);
              break;
          };
        }
      }
      ASSERT_EQ(meta.docs_count, i);
    }

    // iterate over stored columns
    {
      gen.reset();

      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), meta));

      std::unordered_map<std::string, irs::doc_iterator::ptr> readers;

      irs::bytes_ref actual_value;
      irs::bytes_ref_input in;
      iresearch::doc_id_t i = 0;
      size_t value_id = 0;
      for (const document* doc; (doc = gen.next());) {
        ++i;

        for (size_t size = doc->stored.size(); size; --size) {
          auto& expected_field = values[value_id++];
          const std::string name(expected_field.name);
          const auto res = readers.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(name),
            std::forward_as_tuple()
          );

          if (res.second) {
            auto column = reader->column(columns[name].first);
            ASSERT_NE(nullptr, column);

            auto& it = res.first->second;
            it = column->iterator();

            auto& payload = it->attributes().get<irs::payload_iterator>();
            ASSERT_FALSE(!payload);
            ASSERT_FALSE(payload->next());
            ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
            ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          }

          auto& it = res.first->second;
          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_EQ(!res.second, payload->next());

          it->next();
          ASSERT_TRUE(payload->next());
          ASSERT_EQ(i, it->value());
          in.reset(payload->value());

          switch (expected_field.type) {
            case Value::Type::String: {
              ASSERT_EQ(expected_field.value.sValue, irs::read_string<std::string>(in));
            } break;
            case Value::Type::Binary: {
              ASSERT_EQ(expected_field.value.binValue, irs::read_string<irs::bstring>(in));
            } break;
            case Value::Type::Double: {
              ASSERT_EQ(expected_field.value.dblValue, irs::read_zvdouble(in));
            } break;
            default:
              ASSERT_TRUE(false);
              break;
          };
        }
      }
      ASSERT_EQ(meta.docs_count, i);

      // ensure that all iterators are in the end
      for (auto& entry : readers) {
        auto& it = entry.second;
        ASSERT_FALSE(it->next());
        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }
    }

    // seek over stored columns
    {
      gen.reset();

      auto reader = codec()->get_columnstore_reader();
      ASSERT_TRUE(reader->prepare(dir(), meta));

      std::unordered_map<std::string, irs::doc_iterator::ptr> readers;

      irs::bytes_ref actual_value;
      irs::bytes_ref_input in;
      iresearch::doc_id_t i = 0;
      size_t value_id = 0;
      for (const document* doc; (doc = gen.next());) {
        ++i;

        for (size_t size = doc->stored.size(); size; --size) {
          auto& expected_field = values[value_id++];
          const std::string name(expected_field.name);
          const auto res = readers.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(name),
            std::forward_as_tuple()
          );

          if (res.second) {
            auto column = reader->column(columns[name].first);
            ASSERT_NE(nullptr, column);

            auto& it = res.first->second;
            it = column->iterator();
            auto& payload = it->attributes().get<irs::payload_iterator>();
            ASSERT_FALSE(!payload);
            ASSERT_FALSE(payload->next());
            ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
            ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
          }

          auto& it = res.first->second;
          auto& payload = it->attributes().get<irs::payload_iterator>();
          ASSERT_FALSE(!payload);
          ASSERT_EQ(!res.second, payload->next());

          ASSERT_EQ(i, it->seek(i));
          ASSERT_TRUE(payload->next());

          in.reset(payload->value());

          switch (expected_field.type) {
            case Value::Type::String: {
              ASSERT_EQ(expected_field.value.sValue, irs::read_string<std::string>(in));
            } break;
            case Value::Type::Binary: {
              ASSERT_EQ(expected_field.value.binValue, irs::read_string<irs::bstring>(in));
            } break;
            case Value::Type::Double: {
              ASSERT_EQ(expected_field.value.dblValue, irs::read_zvdouble(in));
            } break;
            default:
              ASSERT_TRUE(false);
              break;
          };
        }
      }
      ASSERT_EQ(meta.docs_count, i);

      // ensure that all iterators are in the end
      for (auto& entry : readers) {
        auto& it = entry.second;
        ASSERT_FALSE(it->next());
        auto& payload = it->attributes().get<irs::payload_iterator>();
        ASSERT_FALSE(!payload);
        ASSERT_FALSE(payload->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value());
      }
    }
  }
}; // format_test_case_base

} // tests

#endif // IRESEARCH_FORMAT_TEST_CASE_BASE
