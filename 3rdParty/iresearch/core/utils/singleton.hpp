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

#ifndef IRESEARCH_SINGLETONE_H
#define IRESEARCH_SINGLETONE_H

// internal iResearch functionality not to be exported outside main library (so/dll)
#ifdef IRESEARCH_DLL_PLUGIN
  static_assert(false, "Singleton should not be visible in plugins")
#else

#include "shared.hpp"
#include "utils/noncopyable.hpp"

NS_ROOT

template< typename T >
class singleton: util::noncopyable {
 public:
  static T& instance() {
    static T inst;
    return inst;
  }

 protected:
  singleton() = default;
};

NS_END // NS_ROOT

#endif // #ifdef IRESEARCH_DLL_PLUGIN
#endif
