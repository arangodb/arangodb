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

NS_LOCAL
irs::columnstore_reader::values_reader_f NOOP_VISITOR =
  [] (iresearch::doc_id_t, irs::bytes_ref&) { return false; };

NS_END

NS_ROOT

/* -------------------------------------------------------------------
* index_reader
* ------------------------------------------------------------------*/

index_reader::~index_reader() { }

// -------------------------------------------------------------------
// sub_reader
// -------------------------------------------------------------------

columnstore_reader::values_reader_f sub_reader::values(
    const string_ref& field) const {
  auto* meta = column(field);

  if (!meta) {
    return NOOP_VISITOR;
  }

  return values(meta->id);
}

bool sub_reader::visit(
    const string_ref& field,
    const columnstore_reader::values_visitor_f& visitor) const {
  auto* meta = column(field);

  if (!meta) {
    return false;
  }

  return visit(meta->id, visitor);
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
