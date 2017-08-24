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

#include "error/error.hpp"
#include "directory_attributes.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                      fd_pool_size
// -----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(fd_pool_size);
DEFINE_FACTORY_DEFAULT(fd_pool_size);

const size_t FD_POOL_DEFAULT_SIZE = 8;

fd_pool_size::fd_pool_size()
  : size(FD_POOL_DEFAULT_SIZE) { // arbitary size
}

void fd_pool_size::clear() {
  size = FD_POOL_DEFAULT_SIZE;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   index_file_refs
// -----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(index_file_refs);
DEFINE_FACTORY_DEFAULT(index_file_refs);

index_file_refs::ref_t index_file_refs::add(const std::string& key) {
  return refs_.add(std::string(key));
}

index_file_refs::ref_t index_file_refs::add(std::string&& key) {
  return refs_.add(std::move(key));
}

void index_file_refs::clear() {
  static auto cleaner = [](const std::string&, size_t)->bool { return true; };

  refs_.visit(cleaner, true);

  if (!refs_.empty()) {
    throw illegal_state(); // cannot clear ref_counter due to live refs
  }
}

index_file_refs::counter_t& index_file_refs::refs() {
  return refs_;
}

NS_END
