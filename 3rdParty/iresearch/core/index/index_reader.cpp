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
