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

#include <sstream>

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

namespace google {
class LogStream {
 public:
  LogStream(LogSeverity severity) {
    _severity = severity;
  }
  ~LogStream();
  
  template <typename T>
  LogStream& operator<<(T const& obj) {
    try {
      _out << obj;
    } catch (...) {
      // ignore any errors here. logging should not have side effects
    }
    return *this;
  }
 private:
  std::stringstream _out;
  LogSeverity _severity;
};
  
class LogVoidify {
public:
  LogVoidify() {}
  void operator&(LogStream const&) {}
};
  
#ifdef NDEBUG
  enum { DEBUG_MODE = 0 };
#define IF_DEBUG_MODE(x)
#else
  enum { DEBUG_MODE = 1 };
#define IF_DEBUG_MODE(x) x
#endif
  
};

#ifndef NDEBUG
#define VLOG(a) ((a) < GLOG_FATAL) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#define DLOG(a) ((a) < GLOG_ERROR) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#define LOG(a) ((a) < GLOG_WARNING) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#define LOG_IF(a, cond)                \
          !(cond) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#define DLOG_IF(a, cond)                \
        !(cond) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#else
#define VLOG(a) ((a) < GLOG_FATAL) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#define DLOG(a) ((a) < GLOG_ERROR) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#define LOG(a) ((a) < GLOG_WARNING) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#define LOG_IF(a, cond)                \
          (!cond) ? (void)0  : google::LogVoidify() & (google::LogStream(a))
#define DLOG_IF(a, cond)                \
        (!cond && a < GLOG_WARNING) ? (void)0 : google::LogVoidify() & (google::LogStream(a))
#endif

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by DCHECK_IS_ON(), so the check will be executed regardless of
// compilation mode.  Therefore, it is safe to do things like:
// CHECK(fp->Write(x) == 4)
// FIXMe these won't crash the server
#ifdef CHECK
#undef CHECK // Collision with CATCH define
#endif

#define CHECK(expr) LOG_IF(GLOG_FATAL, !(expr))
#define CHECK_EQ(val1, val2) CHECK((val1) == (val2))
#define CHECK_NE(val1, val2) CHECK((val1) != (val2))
#define CHECK_LE(val1, val2) CHECK((val1) <= (val2))
#define CHECK_LT(val1, val2) CHECK((val1) < (val2))
#define CHECK_GE(val1, val2) CHECK((val1) >= (val2))
#define CHECK_GT(val1, val2) CHECK((val1) > (val2))

#define DVLOG(verboselevel) VLOG(verboselevel)
//#define DLOG_ASSERT(condition) LOG_ASSERT(condition)

// debug-only checking.  executed if NDEBUG is undefined
#define DCHECK(expr) DLOG_IF(GLOG_INFO, !(expr))
#define DCHECK_EQ(val1, val2) DCHECK((val1) == (val2))
#define DCHECK_NE(val1, val2) DCHECK((val1) != (val2))
#define DCHECK_LE(val1, val2) DCHECK((val1) <= (val2))
#define DCHECK_LT(val1, val2) DCHECK((val1) < (val2))
#define DCHECK_GE(val1, val2) DCHECK((val1) >= (val2))
#define DCHECK_GT(val1, val2) DCHECK((val1) > (val2))
#define DCHECK_NOTNULL(val) DCHECK((val) != 0)

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
