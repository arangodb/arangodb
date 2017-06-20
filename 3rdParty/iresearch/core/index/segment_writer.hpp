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

#ifndef IRESEARCH_TL_DOC_WRITER_H
#define IRESEARCH_TL_DOC_WRITER_H

#include "field_data.hpp"
#include "doc_header.hpp"
#include "analysis/token_stream.hpp"
#include "formats/formats.hpp"
#include "utils/directory_utils.hpp"
#include "utils/noncopyable.hpp"

NS_ROOT

struct segment_meta;

class IRESEARCH_API segment_writer: util::noncopyable {
 public:
  DECLARE_PTR(segment_writer);
  DECLARE_FACTORY_DEFAULT(directory& dir);

  struct update_context {
    size_t generation;
    size_t update_id;
  };

  typedef std::vector<update_context> update_contexts;

  // begin document-write transaction
  void begin(const update_context& ctx) {
    valid_ = true;
    norm_fields_.clear(); // clear norm fields
    docs_mask_.reserve(docs_mask_.size() + 1); // reserve space for potential rollback
    docs_context_.emplace_back(ctx);
  }

  // adds stored document field
  template<typename Field>
  bool store(Field& field) {
    return valid_ = valid_ && store_worker(field);
  }

  // adds indexed document field
  template<typename Field>
  bool index(Field& field) {
    return valid_ = valid_ && index_worker(field);
  }

  // adds indexed and stored document field
  template<typename Field>
  bool index_and_store(Field& field) {
    // FIXME optimize, do not evaluate field name hash twice
    return valid_ = valid_ && index_and_store_worker(field);
  }

  // commit document-write transaction
  void commit() {
    if (valid_) {
      finish();
    } else {
      rollback();
    }
  }

  // rollbacks document-write transaction,
  // implicitly NOEXCEPT since we reserve memory in 'begin'
  void rollback() {
    // mark as removed since not fully inserted
    remove(docs_cached() - (type_limits<type_t::doc_id_t>::min)());
    valid_ = false;
  }

  bool flush(std::string& filename, segment_meta& meta);

  const std::string& name() const NOEXCEPT { return seg_name_; }
  size_t docs_cached() const NOEXCEPT { return docs_context_.size(); }
  const update_contexts& docs_context() const NOEXCEPT { return docs_context_; }
  const update_context& doc_context() const { return docs_context_.back(); }
  const document_mask& docs_mask() NOEXCEPT { return docs_mask_; }
  bool initialized() const NOEXCEPT { return initialized_; }
  bool remove(doc_id_t doc_id); // expect 0-based doc_id
  bool valid() const NOEXCEPT { return valid_; }
  void reset();
  void reset(const segment_meta& meta);

 private:
  struct column : util::noncopyable {
    column(const string_ref& name, columnstore_writer& columnstore);

    column(column&& other) NOEXCEPT
      : name(std::move(other.name)),
        handle(std::move(other.handle)) {
    }

    std::string name;
    columnstore_writer::column_t handle;
  };

  segment_writer(directory& dir) NOEXCEPT;

  bool index(
    const hashed_string_ref& name,
    token_stream& tokens,
    const flags& features,
    float_t boost
  );

  template<typename Field>
  bool store_worker(Field& field) {
    REGISTER_TIMER_DETAILED();

    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<irs::string_ref>()
    );

    const doc_id_t doc = docs_cached();
    auto& stream = this->stream(doc, name);

    if (!field.write(stream)) {
      stream.reset();
      return false;
    }

    return true;
  }

  // adds document field
  template<typename Field>
  bool index_worker(Field& field) {
    REGISTER_TIMER_DETAILED();

    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<irs::string_ref>()
    );

    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const flags&>(field.features());
    const auto boost = static_cast<float_t>(field.boost());

    return index(name, tokens, features, boost);
  }

  template<typename Field>
  bool index_and_store_worker(Field& field) {
    REGISTER_TIMER_DETAILED();

    const auto name = make_hashed_ref(
      static_cast<const string_ref&>(field.name()),
      std::hash<irs::string_ref>()
    );

    // index field
    auto& tokens = static_cast<token_stream&>(field.get_tokens());
    const auto& features = static_cast<const flags&>(field.features());
    const auto boost = static_cast<float_t>(field.boost());

    const bool indexed = index(name, tokens, features, boost);

    // store field
    const doc_id_t doc = docs_cached();
    auto& stream = this->stream(doc, name);

    if (!field.write(stream)) {
      stream.reset();
      return indexed;
    }

    return true; // at least indexed
  }

  // returns stream for storing attributes
  columnstore_writer::column_output& stream(
    doc_id_t doc,
    const hashed_string_ref& name
  );

  void finish(); // finishes document

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  update_contexts docs_context_;
  document_mask docs_mask_; // invalid/removed doc_ids (e.g. partially indexed due to indexing failure)
  fields_data fields_;
  std::unordered_map<hashed_string_ref, column> columns_;
  std::unordered_set<field_data*> norm_fields_; // document fields for normalization
  std::string seg_name_;
  field_writer::ptr field_writer_;
  column_meta_writer::ptr col_meta_writer_;
  columnstore_writer::ptr col_writer_;
  tracking_directory dir_;
  bool initialized_;
  bool valid_{ true }; // current state
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // segment_writer

NS_END
#endif