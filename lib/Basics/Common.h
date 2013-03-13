////////////////////////////////////////////////////////////////////////////////
/// @brief High-Performance Database Framework made by triagens
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_COMMON_H
#define TRIAGENS_BASICS_COMMON_H 1

#include "BasicsC/common.h"

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Configuration Configuration
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--           C++ header files that are always present on all systems
// -----------------------------------------------------------------------------

#include <algorithm>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// --SECTIONS--                                               triagens namespace
// -----------------------------------------------------------------------------

namespace triagens {
  using namespace std;

  typedef TRI_blob_t blob_t;
  typedef TRI_datetime_t datetime_t;
  typedef TRI_date_t date_t;
  typedef TRI_seconds_t seconds_t;
  typedef TRI_msec_t msec_t;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 noncopyable class
// -----------------------------------------------------------------------------

namespace triagens {
  class noncopyable {
   protected:
      noncopyable () {
      }

      ~noncopyable () {
      }

   private:
      noncopyable (const noncopyable&);
      noncopyable& operator= (const noncopyable&);
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
