// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef S2_BASE_LOGGING_H_
#define S2_BASE_LOGGING_H_

#ifdef S2_USE_GLOG

#include <glog/logging.h>

// The names CHECK, etc. are too common and may conflict with other
// packages.  We use S2_CHECK to make it easier to switch to
// something other than GLOG for logging.

#define S2_LOG LOG
#define S2_LOG_IF LOG_IF
#define S2_DLOG_IF DLOG_IF

#define S2_CHECK CHECK
#define S2_CHECK_EQ CHECK_EQ
#define S2_CHECK_NE CHECK_NE
#define S2_CHECK_LT CHECK_LT
#define S2_CHECK_LE CHECK_LE
#define S2_CHECK_GT CHECK_GT
#define S2_CHECK_GE CHECK_GE

#define S2_DCHECK DCHECK
#define S2_DCHECK_EQ DCHECK_EQ
#define S2_DCHECK_NE DCHECK_NE
#define S2_DCHECK_LT DCHECK_LT
#define S2_DCHECK_LE DCHECK_LE
#define S2_DCHECK_GT DCHECK_GT
#define S2_DCHECK_GE DCHECK_GE

#define S2_VLOG VLOG
#define S2_VLOG_IS_ON VLOG_IS_ON

#else  // !defined(S2_USE_GLOG)

#include <iostream>

#include "s2/base/log_severity.h"
#include "s2/third_party/absl/base/attributes.h"
#include "s2/third_party/absl/base/log_severity.h"

class S2LogMessage {
 public:
  S2LogMessage(const char* file, int line,
               absl::LogSeverity severity, std::ostream& stream)
    : severity_(severity), stream_(stream) {
    if (enabled()) {
      stream_ << file << ":" << line << " "
              << absl::LogSeverityName(severity) << " ";
    }
  }
  ~S2LogMessage() { if (enabled()) stream_ << std::endl; }

  std::ostream& stream() { return stream_; }

 private:
  bool enabled() const {
#ifdef ABSL_MIN_LOG_LEVEL
    return (static_cast<int>(severity_) >= ABSL_MIN_LOG_LEVEL ||
            severity_ >= absl::LogSeverity::kFatal);
#else
    return true;
#endif
  }

  absl::LogSeverity severity_;
  std::ostream& stream_;
};

// Same as S2LogMessage, but destructor is marked no-return to avoid
// "no return value warnings" in functions that return non-void.
class S2FatalLogMessage : public S2LogMessage {
 public:
  S2FatalLogMessage(const char* file, int line,
                    absl::LogSeverity severity, std::ostream& stream)
      ABSL_ATTRIBUTE_COLD
    : S2LogMessage(file, line, severity, stream) {}
  ABSL_ATTRIBUTE_NORETURN ~S2FatalLogMessage() { abort(); }
};

// Logging stream that does nothing.
struct S2NullStream {
  template <typename T>
  S2NullStream& operator<<(const T& v) { return *this; }
};

// Used to suppress "unused value" warnings.
struct S2LogMessageVoidify {
  // Must have precedence lower than << but higher than ?:.
  void operator&(std::ostream&) {}
};

#define S2_LOG_MESSAGE_(LogMessageClass, log_severity) \
    LogMessageClass(__FILE__, __LINE__, log_severity, std::cerr)
#define S2_LOG_INFO \
    S2_LOG_MESSAGE_(S2LogMessage, absl::LogSeverity::kInfo)
#define S2_LOG_WARNING \
    S2_LOG_MESSAGE_(S2LogMessage, absl::LogSeverity::kWarning)
#define S2_LOG_ERROR \
    S2_LOG_MESSAGE_(S2LogMessage, absl::LogSeverity::kError)
#define S2_LOG_FATAL \
    S2_LOG_MESSAGE_(S2FatalLogMessage, absl::LogSeverity::kFatal)
#ifndef NDEBUG
#define S2_LOG_DFATAL S2_LOG_FATAL
#else
#define S2_LOG_DFATAL S2_LOG_ERROR
#endif

#define S2_LOG(severity) S2_LOG_##severity.stream()

// Implementing this as if (...) {} else S2_LOG(...) will cause dangling else
// warnings when someone does if (...) S2_LOG_IF(...), so do this tricky
// thing instead.
#define S2_LOG_IF(severity, condition) \
    !(condition) ? (void)0 : S2LogMessageVoidify() & S2_LOG(severity)

#define S2_CHECK(condition) \
    S2_LOG_IF(FATAL, ABSL_PREDICT_FALSE(!(condition))) \
        << ("Check failed: " #condition " ")

#ifndef NDEBUG

#define S2_DLOG_IF S2_LOG_IF
#define S2_DCHECK S2_CHECK

#else  // defined(NDEBUG)

#define S2_DLOG_IF(severity, condition) \
    while (false && (condition)) S2NullStream()
#define S2_DCHECK(condition) \
    while (false && (condition)) S2NullStream()

#endif  // defined(NDEBUG)

#define S2_CHECK_OP(op, val1, val2) S2_CHECK((val1) op (val2))
#define S2_CHECK_EQ(val1, val2) S2_CHECK_OP(==, val1, val2)
#define S2_CHECK_NE(val1, val2) S2_CHECK_OP(!=, val1, val2)
#define S2_CHECK_LT(val1, val2) S2_CHECK_OP(<, val1, val2)
#define S2_CHECK_LE(val1, val2) S2_CHECK_OP(<=, val1, val2)
#define S2_CHECK_GT(val1, val2) S2_CHECK_OP(>, val1, val2)
#define S2_CHECK_GE(val1, val2) S2_CHECK_OP(>=, val1, val2)

#define S2_DCHECK_OP(op, val1, val2) S2_DCHECK((val1) op (val2))
#define S2_DCHECK_EQ(val1, val2) S2_DCHECK_OP(==, val1, val2)
#define S2_DCHECK_NE(val1, val2) S2_DCHECK_OP(!=, val1, val2)
#define S2_DCHECK_LT(val1, val2) S2_DCHECK_OP(<, val1, val2)
#define S2_DCHECK_LE(val1, val2) S2_DCHECK_OP(<=, val1, val2)
#define S2_DCHECK_GT(val1, val2) S2_DCHECK_OP(>, val1, val2)
#define S2_DCHECK_GE(val1, val2) S2_DCHECK_OP(>=, val1, val2)

// We don't support VLOG.
#define S2_VLOG(verbose_level) S2NullStream()
#define S2_VLOG_IS_ON(verbose_level) (false)

#endif  // !defined(S2_USE_GLOG)

#endif  // S2_BASE_LOGGING_H_
