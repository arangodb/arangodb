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

#include "directory.hpp"
#include "directory_attributes.hpp"
#include "directory_cleaner.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                 directory_cleaner
// -----------------------------------------------------------------------------

/*static*/ index_file_refs::counter_t& directory_cleaner::init(
  directory& dir
) {
  return dir.attributes().emplace<index_file_refs>()->refs();
}

/*static*/ size_t directory_cleaner::clean(
  directory& dir, const removal_acceptor_t& acceptor
) {
  auto& ref_attr = const_cast<const attribute_store&>(dir.attributes()).get<index_file_refs>();

  if (!ref_attr) {
    return 0; // nothing to do
  }

  size_t remove_count = 0;
  auto& refs = ref_attr->refs();
  index_file_refs::ref_t tmp_ref;
  auto visitor = [&dir, &refs, &acceptor, &remove_count, &tmp_ref](
    const std::string& filename, size_t count
  )->bool {
    // for retained files add a temporary reference to avoid removal
    if (!acceptor(filename)) {
      tmp_ref = refs.add(std::string(filename));
    } else if (!count && dir.remove(filename)) {
      ++remove_count;
    }

    return true;
  };

  ref_attr->refs().visit(visitor, true);

  return remove_count;
}

NS_END
