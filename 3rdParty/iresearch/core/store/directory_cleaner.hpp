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

#ifndef IRESEARCH_DIRECTORY_CLEANER_H
#define IRESEARCH_DIRECTORY_CLEANER_H

#include <functional>

#include "shared.hpp"
#include "store/directory_attributes.hpp"
#include "utils/string.hpp"

NS_ROOT

struct directory;

//////////////////////////////////////////////////////////////////////////////
/// @class directory_cleaner
/// @brief represents a cleaner for directory files without any references
//////////////////////////////////////////////////////////////////////////////

class IRESEARCH_API directory_cleaner {
 public:
  typedef std::function<bool(const std::string& filename)> removal_acceptor_t;

  // @param acceptor returns if removal candidates should actually be removed
  // @return count of files removed
  static size_t clean(
    directory& dir,
    const removal_acceptor_t& acceptor =
      [](const std::string&)->bool { return true; }
  );
  static iresearch::index_file_refs::counter_t& init(directory& dir);
};

NS_END

#endif
