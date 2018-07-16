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

#include "shared.hpp"
#include "composite_reader_impl.hpp"
#include "utils/directory_utils.hpp"
#include "utils/type_limits.hpp"
#include "field_meta.hpp"
#include "index_reader.hpp"
#include "segment_reader.hpp"
#include "index_meta.hpp"

NS_ROOT

// -------------------------------------------------------------------
// index_reader
// -------------------------------------------------------------------

index_reader::~index_reader() { }

// -------------------------------------------------------------------
// sub_reader
// -------------------------------------------------------------------

const columnstore_reader::column_reader* sub_reader::column_reader(
    const string_ref& field) const {
  const auto* meta = column(field);
  return meta ? column_reader(meta->id) : nullptr;
}

// -------------------------------------------------------------------
// context specialization for sub_reader
// -------------------------------------------------------------------

template<>
struct context<sub_reader> {
  sub_reader::ptr reader;
  doc_id_t base = 0; // min document id
  doc_id_t max = 0; // max document id

  operator doc_id_t() const { return max; }
}; // reader_context

NS_END
