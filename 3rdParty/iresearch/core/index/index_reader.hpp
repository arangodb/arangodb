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

#ifndef IRESEARCH_INDEX_READER_H
#define IRESEARCH_INDEX_READER_H

#include "store/directory.hpp"
#include "store/directory_attributes.hpp"
#include "utils/string.hpp"
#include "formats/formats.hpp"
#include "utils/memory.hpp"
#include "utils/iterator.hpp"

#include <vector>
#include <numeric>
#include <functional>

NS_ROOT

/* -------------------------------------------------------------------
* index_reader
* ------------------------------------------------------------------*/

struct sub_reader;

struct IRESEARCH_API index_reader {
  DECLARE_SHARED_PTR(const index_reader);
  DEFINE_FACTORY_INLINE(index_reader);

  typedef std::function<bool(const field_meta&, data_input&)> document_visitor_f;
  typedef forward_iterator_impl<sub_reader> reader_iterator_impl;
  typedef forward_iterator<reader_iterator_impl> reader_iterator;

  virtual ~index_reader();

  // number of live documents
  virtual uint64_t live_docs_count() const = 0;

  // total number of documents including deleted
  virtual uint64_t docs_count() const = 0;

  // first sub-segment
  virtual reader_iterator begin() const = 0;

  // after the last sub-segment
  virtual reader_iterator end() const = 0;

  // returns number of sub-segments in current reader
  virtual size_t size() const = 0;
}; // index_reader

/* -------------------------------------------------------------------
* sub_reader
* ------------------------------------------------------------------*/

struct IRESEARCH_API sub_reader : index_reader {
  DECLARE_SHARED_PTR(const sub_reader);
  DEFINE_FACTORY_INLINE(sub_reader);

  using index_reader::docs_count;

  // returns iterator over the live documents in current segment
  virtual doc_iterator::ptr docs_iterator() const = 0;

  virtual field_iterator::ptr fields() const = 0;

  virtual doc_iterator::ptr mask(doc_iterator::ptr&& it) const {
    return std::move(it);
  }

  // returns corresponding term_reader by the specified field
  virtual const term_reader* field(
    const string_ref& field
  ) const = 0;

  virtual column_iterator::ptr columns() const = 0;

  virtual const column_meta* column(const string_ref& name) const = 0;

  virtual const columnstore_reader::column_reader* column_reader(field_id field) const = 0;

  const columnstore_reader::column_reader* column_reader(const string_ref& field) const;
}; // sub_reader

NS_END

MSVC_ONLY(template class IRESEARCH_API std::function<bool(iresearch::doc_id_t)>); // sub_reader::value_visitor_f

NS_ROOT

// -------------------------------------------------------------------
// composite_reader
// -------------------------------------------------------------------

class IRESEARCH_API composite_reader: public index_reader {
 public:
  DECLARE_SHARED_PTR(const composite_reader);

  // return the i'th sub_reader
  virtual const sub_reader& operator[](size_t i) const = 0;
};

NS_END

#endif
