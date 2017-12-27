////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

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
