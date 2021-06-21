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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_COMPOSITE_READER_H
#define IRESEARCH_COMPOSITE_READER_H

#include "shared.hpp"
#include "index_reader.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief common implementation for readers composied of multiple other readers
///        for use/inclusion into cpp files
////////////////////////////////////////////////////////////////////////////////
template<typename ReaderType>
class composite_reader : public index_reader {
 public:
  typedef typename std::enable_if<
    std::is_base_of<index_reader, ReaderType>::value,
    ReaderType
  >::type reader_type;

  typedef std::vector<reader_type> readers_t;

  composite_reader(
      readers_t&& readers,
      uint64_t docs_count,
      uint64_t docs_max
  ) noexcept
    : readers_(std::move(readers)),
      docs_count_(docs_count),
      docs_max_(docs_max) {
  }

  // returns corresponding sub-reader
  virtual const reader_type& operator[](size_t i) const noexcept override {
    assert(i < readers_.size());
    return *(readers_[i]);
  }

  // maximum number of documents
  virtual uint64_t docs_count() const noexcept override {
    return docs_max_;
  }

  // number of live documents
  virtual uint64_t live_docs_count() const noexcept override {
    return docs_count_;
  }

  // returns total number of opened writers
  virtual size_t size() const noexcept override {
    return readers_.size();
  }

 private:
  readers_t readers_;
  uint64_t docs_count_;
  uint64_t docs_max_;
}; // composite_reader

}

#endif
