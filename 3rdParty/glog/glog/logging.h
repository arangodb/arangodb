////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GLOG_SHAM_LOGGING_H
#define ARANGOD_GLOG_SHAM_LOGGING_H

#include "Basics/Common.h"

#include <iostream>

namespace google {
class S2NullStream : public std::ostream {
  class S2NullBuffer : public std::streambuf {
  public:
    int overflow( int c ) { return c; }
  } m_nb;
public:
  S2NullStream() : std::ostream( &m_nb ) {}
};
  
#ifdef NDEBUG
  enum { DEBUG_MODE = 0 };
#define IF_DEBUG_MODE(x)
#else
  enum { DEBUG_MODE = 1 };
#define IF_DEBUG_MODE(x) x
#endif
  
};

// Variables of type LogSeverity are widely taken to lie in the range
// [0, NUM_SEVERITIES-1].  Be careful to preserve this assumption if
// you ever need to change their values or add a new severity.
typedef int LogSeverity;

const int GLOG_INFO = 0, GLOG_WARNING = 1, GLOG_ERROR = 2, GLOG_FATAL = 3,
NUM_SEVERITIES = 4;
#ifndef GLOG_NO_ABBREVIATED_SEVERITIES
# ifdef ERROR
#  error ERROR macro is defined. Define GLOG_NO_ABBREVIATED_SEVERITIES before including logging.h. See the document for detail.
# endif
const int INFO = GLOG_INFO, WARNING = GLOG_WARNING,
ERROR = GLOG_ERROR, FATAL = GLOG_FATAL ;
#endif

// DFATAL is FATAL in debug mode, ERROR in normal mode
#ifdef NDEBUG
#define DFATAL GLOG_ERROR
#else
#define DFATAL GLOG_FATAL
#endif

#ifndef NDEBUG
#define VLOG(a) google::S2NullStream()
#define DLOG(a) ((a) >= GLOG_INFO ? std::cout : std::cerr)
#define LOG(a) ((a) > GLOG_WARNING ? std::cerr : std::cout)
#define LOG_CHECK(a, cond) std::cout
//          (!(cond) ? ((a) >= GLOG_INFO ? std::cerr : std::cout) : google::S2NullStream())
#else
#define VLOG(a) google::S2NullStream()
#define DLOG(a) google::S2NullStream()
#define LOG(a) (a > GLOG_WARNING) ? std::cerr : google::S2NullStream();
#define LOG_CHECK(a, cond)                                       \
          !(TRI_LIKELY(cond)) ? ((a > GLOG_INFO) ? \
                std::cout : google::S2NullStream()) \
            : google::S2NullStream()
#endif

#define LOG_IF(a, cond) std::cout
//((cond) ? ((a) > GLOG_INFO ? std::cerr : std::cout) : google::S2NullStream())
#define DLOG_IF(severity, condition) LOG_IF(GLOG_INFO, condition)

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by DCHECK_IS_ON(), so the check will be executed regardless of
// compilation mode.  Therefore, it is safe to do things like:
// CHECK(fp->Write(x) == 4)
// FIXMe these won't crash the server
#define CHECK(expr) LOG_CHECK(GLOG_FATAL, expr)
#define CHECK_EQ(val1, val2) LOG_CHECK(GLOG_FATAL, val1 == val2)
#define CHECK_NE(val1, val2) LOG_CHECK(GLOG_FATAL, val1 != val2)
#define CHECK_LE(val1, val2) LOG_CHECK(GLOG_FATAL, val1 <= val2)
#define CHECK_LT(val1, val2) LOG_CHECK(GLOG_FATAL, val1 < val2)
#define CHECK_GE(val1, val2) LOG_CHECK(GLOG_FATAL, val1 >= val2)
#define CHECK_GT(val1, val2) LOG_CHECK(GLOG_FATAL, val1 > val2)

#define DVLOG(verboselevel) VLOG(verboselevel)
//#define DLOG_ASSERT(condition) LOG_ASSERT(condition)

// debug-only checking.  executed if NDEBUG is undefined
#define DCHECK(expr) LOG_CHECK(GLOG_INFO, expr)
#define DCHECK_EQ(val1, val2) LOG_CHECK(GLOG_INFO, (val1) == (val2))
#define DCHECK_NE(val1, val2) LOG_CHECK(GLOG_INFO, (val1) != (val2))
#define DCHECK_LE(val1, val2) LOG_CHECK(GLOG_INFO, (val1) <= (val2))
#define DCHECK_LT(val1, val2) LOG_CHECK(GLOG_INFO, (val1) < (val2))
#define DCHECK_GE(val1, val2) LOG_CHECK(GLOG_INFO, (val1) >= (val2))
#define DCHECK_GT(val1, val2) LOG_CHECK(GLOG_INFO, (val1) > (val2))
#define DCHECK_NOTNULL(val) LOG_CHECK(GLOG_INFO, (val) != 0)

/*#else

#define DCHECK(condition) google::S2NullStream()
#define DCHECK_EQ(val1, val2) google::S2NullStream()
#define DCHECK_NE(val1, val2) google::S2NullStream()
#define DCHECK_LE(val1, val2) google::S2NullStream()
#define DCHECK_LT(val1, val2) google::S2NullStream()
#define DCHECK_GE(val1, val2) google::S2NullStream()
#define DCHECK_GT(val1, val2) google::S2NullStream()
#define DCHECK_NOTNULL(val) google::S2NullStream()

#endif*/

#endif /* ARANGOD_GLOG_SHAM_LOGGING_H */
