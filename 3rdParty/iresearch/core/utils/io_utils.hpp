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

#ifndef IRESEARCH_IO_UTILS_H
#define IRESEARCH_IO_UTILS_H

#include "memory.hpp"
#include "utils/string.hpp"

#define MAKE_DELETER(method) \
template< typename _T > \
struct auto_##method : std::default_delete< _T > \
{ \
    typedef _T type; \
    typedef std::default_delete<type> base; \
    typedef std::unique_ptr< type, auto_##method<type> > ptr; \
    void operator()(type* p) const NOEXCEPT \
    { \
      if (p) { \
        try { \
            p->method(); \
        } catch(...) { } \
      } \
      base::operator()(p); \
    } \
} 

NS_ROOT
NS_BEGIN(io_utils)

MAKE_DELETER(close);
MAKE_DELETER(unlock);

NS_END
NS_END

#define DECLARE_IO_PTR(class_name, method) \
  typedef iresearch::io_utils::auto_##method< class_name >::ptr ptr

#endif
