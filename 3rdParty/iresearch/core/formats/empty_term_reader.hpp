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

#ifndef IRESEARCH_EMPTY_TERM_READER_H
#define IRESEARCH_EMPTY_TERM_READER_H

#include "formats.hpp"
#include "index/field_meta.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @brief a term_reader implementation with docs_count but without terms
////////////////////////////////////////////////////////////////////////////////
class empty_term_reader: public irs::term_reader {
 public:
  empty_term_reader(uint64_t docs_count): docs_count_(docs_count) {
  }

  virtual irs::seek_term_iterator::ptr iterator() const NOEXCEPT override {
    return nullptr; // no terms in reader
  }

  virtual const irs::field_meta& meta() const NOEXCEPT override {
    return irs::field_meta::EMPTY;
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return irs::attribute_view::empty_instance();
  }

  // total number of terms
  virtual size_t size() const NOEXCEPT override {
    return 0; // no terms in reader
  }

  // total number of documents
  virtual uint64_t docs_count() const NOEXCEPT override {
    return docs_count_;
  }

  // least significant term
  virtual const irs::bytes_ref& (min)() const NOEXCEPT override {
    return irs::bytes_ref::NIL;
  }

  // most significant term
  virtual const irs::bytes_ref& (max)() const NOEXCEPT override {
    return irs::bytes_ref::NIL;
  }

 private:
  uint64_t docs_count_;
};

NS_END // ROOT

#endif