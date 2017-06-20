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

#include "cpuinfo.hpp"

#if defined(_MSC_VER)

#include "bit_utils.hpp"

NS_ROOT

const cpuinfo cpuinfo::instance_;

/*static*/ bool cpuinfo::support_popcnt() {
  // according to https://msdn.microsoft.com/en-us/library/bb385231.aspx
  return check_bit<23>(instance_.f1_cpuinfo_[2]);
}

NS_END

#endif
