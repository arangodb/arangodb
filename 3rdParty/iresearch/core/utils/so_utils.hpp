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

#ifndef IRESEARCH_SO_UTILS_H
#define IRESEARCH_SO_UTILS_H

#include <functional>

NS_ROOT

// #define RTLD_LAZY   1
// #define RTLD_NOW    2
// #define RTLD_GLOBAL 4

void* load_library(const char* soname, int mode = 2);
void* get_function(void* handle, const char* fname);
bool free_library(void* handle);
void load_libraries(
  const std::string& path,
  const std::string& prefix,
  const std::string& suffix
);

NS_END

#endif // IRESEARCH_SO_UTILS_H
