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

#ifndef IRESEARCH_COMPOSITE_READER_H
#define IRESEARCH_COMPOSITE_READER_H

#include "shared.hpp"
#include "index_reader.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @brief composite reader per-sub_reader storage context
////////////////////////////////////////////////////////////////////////////////
template<typename ReaderType>
struct context;

////////////////////////////////////////////////////////////////////////////////
/// @brief common implementation for readers composied of multiple other readers
///        for use/inclusion into cpp files
////////////////////////////////////////////////////////////////////////////////
template<typename ReaderType>
class composite_reader_impl: public index_reader {
 public:
  typedef typename std::enable_if<
    std::is_base_of<index_reader, ReaderType>::value,
    ReaderType
  >::type reader_type;

  typedef context<reader_type> reader_context;
  typedef std::vector<reader_context> ctxs_t;

  composite_reader_impl(
    index_meta&& meta,
    ctxs_t&& ctxs,
    uint64_t docs_count,
    uint64_t docs_max
  )
  : ctxs_(std::move(ctxs)),
    docs_count_(docs_count),
    docs_max_(docs_max),
    meta_(std::move(meta)) {
  }

  // returns corresponded reader
  const reader_type& operator[](size_t i) const override {
    assert(i < ctxs_.size());
    return *(ctxs_[i].reader);
  }

  // maximum number of documents
  virtual uint64_t docs_count() const override {
    return docs_max_;
  }

  // number of live documents
  virtual uint64_t live_docs_count() const override {
    return docs_count_;
  }

  // returns total number of opened writers
  virtual size_t size() const override {
    return ctxs_.size();
  }

 protected:
  const index_meta& meta() const {
    return meta_;
  }

  // returns corresponding reader's context by the specified document id
  const reader_context& sub_ctx(doc_id_t doc) const {
    auto it = std::lower_bound(ctxs_.begin(), ctxs_.end(), doc);
    assert(ctxs_.end() != it);
    return *it;
  }

 private:
  ctxs_t ctxs_;
  uint64_t docs_count_;
  uint64_t docs_max_;
  index_meta meta_;
};

NS_END

#endif
