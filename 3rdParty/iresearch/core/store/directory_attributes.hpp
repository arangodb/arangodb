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

#ifndef IRESEARCH_DIRECTORY_ATTRIBUTES_H
#define IRESEARCH_DIRECTORY_ATTRIBUTES_H

#include "shared.hpp"
#include "utils/attributes.hpp"
#include "utils/ref_counter.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class fd_pool_size
/// @brief the size of file descriptor pools
///        where applicable, e.g. fs_directory
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API fd_pool_size: public attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();
  size_t size;

  fd_pool_size();
  void clear();
};

//////////////////////////////////////////////////////////////////////////////
/// @class index_file_refs
/// @brief represents a ref_counter for index related files
//////////////////////////////////////////////////////////////////////////////

class IRESEARCH_API index_file_refs: public attribute {
 public:
  typedef attribute_store::ref<index_file_refs> attribute_t;
  typedef ref_counter<std::string> counter_t;
  typedef counter_t::ref_t ref_t;
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();
  index_file_refs();
  ref_t add(const std::string& key);
  ref_t add(std::string&& key);
  void clear();
  counter_t& refs();

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  counter_t refs_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif
