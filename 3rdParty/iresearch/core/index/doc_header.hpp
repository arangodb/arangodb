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

#ifndef IRESEARCH_DOC_HEADER_H
#define IRESEARCH_DOC_HEADER_H

#include "shared.hpp"
#include "store/store_utils.hpp"

NS_ROOT
NS_BEGIN(stored)

////////////////////////////////////////////////////////////////////////////////
/// @brief writes stored document header 
////////////////////////////////////////////////////////////////////////////////
template<typename Iterator>
bool write_header(data_output& out, Iterator begin, Iterator end) {
  if (begin == end) {
    return false;
  }

  for (--end; begin != end; ++begin) {
    out.write_vint(shift_pack_32(*begin, true));
  }
  out.write_vint(shift_pack_32(*begin, false));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visits stored document header elements
/// Visitor(field_id id, bool next_element_exists);
////////////////////////////////////////////////////////////////////////////////
template<typename Visitor>
bool visit_header(iresearch::data_input& in, const Visitor& visitor) {
  field_id id;
  bool next;

  do {
    next = shift_unpack_32(in.read_vint(), id);
    if (!visitor(id, next)) {
      return false;
    }
  } while (next);

  return true;
}

NS_END // stored
NS_END

#endif
