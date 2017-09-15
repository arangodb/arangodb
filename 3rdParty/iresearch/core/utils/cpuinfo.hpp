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

#ifndef IRESEARCH_CPUID_ID
#define IRESEARCH_CPUID_ID

#if defined(_MSC_VER)

#include "shared.hpp"
#include <intrin.h>

NS_ROOT

class IRESEARCH_API cpuinfo {
 public:
  static bool support_popcnt();
 private:
  static const cpuinfo instance_;

  cpuinfo() {
    __cpuid( f1_cpuinfo_, 1 );
  }

  int f1_cpuinfo_[4];
};

NS_END

#endif

#endif
