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

#ifndef IRESEARCH_RUNTIME_UTILS_H
#define IRESEARCH_RUNTIME_UTILS_H

NS_ROOT

inline const char* getenv(const char* name) {
  #ifdef _MSC_VER
    #pragma warning(disable : 4996)
  #endif

    return std::getenv(name);

  #ifdef _MSC_VER
    #pragma warning(default : 4996)
  #endif
}

inline bool localtime(struct tm& buf, const time_t& time) {
  // use a thread safe conversion function
  #ifdef _MSC_VER
    return 0 == ::localtime_s(&buf, &time);
  #else
    return nullptr != ::localtime_r(&time, &buf);
  #endif
}

inline int setenv(const char *name, const char *value, bool overwrite) {
  #ifdef _MSC_VER
    UNUSED(overwrite);
    return _putenv_s(name, value);  // OVERWRITE is always true for MSVC
  #else
    return ::setenv(name, value, overwrite);
  #endif
}

NS_END

#endif
