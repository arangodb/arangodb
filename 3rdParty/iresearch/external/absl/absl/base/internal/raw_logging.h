// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Thread-safe logging routines that do not allocate any memory or
// acquire any locks, and can therefore be used by low-level memory
// allocation, synchronization, and signal-handling code.

#ifndef IRESEARCH_ABSL_BASE_INTERNAL_RAW_LOGGING_H_
#define IRESEARCH_ABSL_BASE_INTERNAL_RAW_LOGGING_H_

#include <string>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/atomic_hook.h"
#include "absl/base/log_severity.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/base/port.h"

// This is similar to LOG(severity) << format..., but
// * it is to be used ONLY by low-level modules that can't use normal LOG()
// * it is designed to be a low-level logger that does not allocate any
//   memory and does not need any locks, hence:
// * it logs straight and ONLY to STDERR w/o buffering
// * it uses an explicit printf-format and arguments list
// * it will silently chop off really long message strings
// Usage example:
//   IRESEARCH_ABSL_RAW_LOG(ERROR, "Failed foo with %i: %s", status, error);
// This will print an almost standard log line like this to stderr only:
//   E0821 211317 file.cc:123] RAW: Failed foo with 22: bad_file

#define IRESEARCH_ABSL_RAW_LOG(severity, ...)                                            \
  do {                                                                         \
    constexpr const char* absl_raw_logging_internal_basename =                 \
        ::iresearch_absl::raw_logging_internal::Basename(__FILE__,                       \
                                               sizeof(__FILE__) - 1);          \
    ::iresearch_absl::raw_logging_internal::RawLog(IRESEARCH_ABSL_RAW_LOGGING_INTERNAL_##severity, \
                                         absl_raw_logging_internal_basename,   \
                                         __LINE__, __VA_ARGS__);               \
  } while (0)

// Similar to CHECK(condition) << message, but for low-level modules:
// we use only IRESEARCH_ABSL_RAW_LOG that does not allocate memory.
// We do not want to provide args list here to encourage this usage:
//   if (!cond)  IRESEARCH_ABSL_RAW_LOG(FATAL, "foo ...", hard_to_compute_args);
// so that the args are not computed when not needed.
#define IRESEARCH_ABSL_RAW_CHECK(condition, message)                             \
  do {                                                                 \
    if (IRESEARCH_ABSL_PREDICT_FALSE(!(condition))) {                            \
      IRESEARCH_ABSL_RAW_LOG(FATAL, "Check %s failed: %s", #condition, message); \
    }                                                                  \
  } while (0)

// IRESEARCH_ABSL_INTERNAL_LOG and IRESEARCH_ABSL_INTERNAL_CHECK work like the RAW variants above,
// except that if the richer log library is linked into the binary, we dispatch
// to that instead.  This is potentially useful for internal logging and
// assertions, where we are using RAW_LOG neither for its async-signal-safety
// nor for its non-allocating nature, but rather because raw logging has very
// few other dependencies.
//
// The API is a subset of the above: each macro only takes two arguments.  Use
// StrCat if you need to build a richer message.
#define IRESEARCH_ABSL_INTERNAL_LOG(severity, message)                             \
  do {                                                                   \
    constexpr const char* absl_raw_logging_internal_filename = __FILE__; \
    ::iresearch_absl::raw_logging_internal::internal_log_function(                 \
        IRESEARCH_ABSL_RAW_LOGGING_INTERNAL_##severity,                            \
        absl_raw_logging_internal_filename, __LINE__, message);          \
  } while (0)

#define IRESEARCH_ABSL_INTERNAL_CHECK(condition, message)                    \
  do {                                                             \
    if (IRESEARCH_ABSL_PREDICT_FALSE(!(condition))) {                        \
      std::string death_message = "Check " #condition " failed: "; \
      death_message += std::string(message);                       \
      IRESEARCH_ABSL_INTERNAL_LOG(FATAL, death_message);                     \
    }                                                              \
  } while (0)

#define IRESEARCH_ABSL_RAW_LOGGING_INTERNAL_INFO ::iresearch_absl::LogSeverity::kInfo
#define IRESEARCH_ABSL_RAW_LOGGING_INTERNAL_WARNING ::iresearch_absl::LogSeverity::kWarning
#define IRESEARCH_ABSL_RAW_LOGGING_INTERNAL_ERROR ::iresearch_absl::LogSeverity::kError
#define IRESEARCH_ABSL_RAW_LOGGING_INTERNAL_FATAL ::iresearch_absl::LogSeverity::kFatal
#define IRESEARCH_ABSL_RAW_LOGGING_INTERNAL_LEVEL(severity) \
  ::iresearch_absl::NormalizeLogSeverity(severity)

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace raw_logging_internal {

// Helper function to implement IRESEARCH_ABSL_RAW_LOG
// Logs format... at "severity" level, reporting it
// as called from file:line.
// This does not allocate memory or acquire locks.
void RawLog(iresearch_absl::LogSeverity severity, const char* file, int line,
            const char* format, ...) IRESEARCH_ABSL_PRINTF_ATTRIBUTE(4, 5);

// Writes the provided buffer directly to stderr, in a safe, low-level manner.
//
// In POSIX this means calling write(), which is async-signal safe and does
// not malloc.  If the platform supports the SYS_write syscall, we invoke that
// directly to side-step any libc interception.
void SafeWriteToStderr(const char *s, size_t len);

// compile-time function to get the "base" filename, that is, the part of
// a filename after the last "/" or "\" path separator.  The search starts at
// the end of the string; the second parameter is the length of the string.
constexpr const char* Basename(const char* fname, int offset) {
  return offset == 0 || fname[offset - 1] == '/' || fname[offset - 1] == '\\'
             ? fname + offset
             : Basename(fname, offset - 1);
}

// For testing only.
// Returns true if raw logging is fully supported. When it is not
// fully supported, no messages will be emitted, but a log at FATAL
// severity will cause an abort.
//
// TODO(gfalcon): Come up with a better name for this method.
bool RawLoggingFullySupported();

// Function type for a raw_logging customization hook for suppressing messages
// by severity, and for writing custom prefixes on non-suppressed messages.
//
// The installed hook is called for every raw log invocation.  The message will
// be logged to stderr only if the hook returns true.  FATAL errors will cause
// the process to abort, even if writing to stderr is suppressed.  The hook is
// also provided with an output buffer, where it can write a custom log message
// prefix.
//
// The raw_logging system does not allocate memory or grab locks.  User-provided
// hooks must avoid these operations, and must not throw exceptions.
//
// 'severity' is the severity level of the message being written.
// 'file' and 'line' are the file and line number where the IRESEARCH_ABSL_RAW_LOG macro
// was located.
// 'buffer' and 'buf_size' are pointers to the buffer and buffer size.  If the
// hook writes a prefix, it must increment *buffer and decrement *buf_size
// accordingly.
using LogPrefixHook = bool (*)(iresearch_absl::LogSeverity severity, const char* file,
                               int line, char** buffer, int* buf_size);

// Function type for a raw_logging customization hook called to abort a process
// when a FATAL message is logged.  If the provided AbortHook() returns, the
// logging system will call abort().
//
// 'file' and 'line' are the file and line number where the IRESEARCH_ABSL_RAW_LOG macro
// was located.
// The NUL-terminated logged message lives in the buffer between 'buf_start'
// and 'buf_end'.  'prefix_end' points to the first non-prefix character of the
// buffer (as written by the LogPrefixHook.)
using AbortHook = void (*)(const char* file, int line, const char* buf_start,
                           const char* prefix_end, const char* buf_end);

// Internal logging function for IRESEARCH_ABSL_INTERNAL_LOG to dispatch to.
//
// TODO(gfalcon): When string_view no longer depends on base, change this
// interface to take its message as a string_view instead.
using InternalLogFunction = void (*)(iresearch_absl::LogSeverity severity,
                                     const char* file, int line,
                                     const std::string& message);

IRESEARCH_ABSL_INTERNAL_ATOMIC_HOOK_ATTRIBUTES IRESEARCH_ABSL_DLL extern base_internal::AtomicHook<
    InternalLogFunction>
    internal_log_function;

void RegisterInternalLogFunction(InternalLogFunction func);

}  // namespace raw_logging_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_BASE_INTERNAL_RAW_LOGGING_H_
