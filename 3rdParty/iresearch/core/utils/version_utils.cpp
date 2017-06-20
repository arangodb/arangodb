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

#include "version_utils.hpp"

NS_ROOT
NS_BEGIN(version_utils)

const string_ref build_date() {
  static const std::string value = __DATE__;

  return value;
}

const string_ref build_id() {
  static const std::string value = {
    #include "utils/build_identifier.csx"
  };

  return value;
}

const string_ref build_time() {
  static const std::string value = __TIME__;

  return value;
}

const string_ref build_version() {
  static const std::string value = {
    #include "utils/build_version.csx"
  };

  return value;
}

NS_END
NS_END
