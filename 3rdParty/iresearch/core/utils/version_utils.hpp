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

#ifndef IRESEARCH_VERSION_UTILS_H
#define IRESEARCH_VERSION_UTILS_H

#include "shared.hpp"
#include "utils/string.hpp"

#include "utils/version_defines.hpp"

#ifdef IResearch_int_version
  #define IRESEARCH_VERSION IResearch_int_version
#else
  #define IRESEARCH_VERSION 0
#endif

NS_ROOT
NS_BEGIN(version_utils)

const string_ref build_date();
const string_ref build_id();
const string_ref build_time();
const string_ref build_version();

NS_END
NS_END

#endif
