// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "s2/third_party/absl/base/internal/raw_logging.h"

#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "s2/third_party/absl/base/attributes.h"
#include "s2/third_party/absl/base/config.h"
#include "s2/third_party/absl/base/internal/atomic_hook.h"
#include "s2/third_party/absl/base/log_severity.h"

// We know how to perform low-level writes to stderr in POSIX and Windows.  For
// these platforms, we define the token ABSL_LOW_LEVEL_WRITE_SUPPORTED.
// Much of raw_logging.cc becomes a no-op when we can't output messages,
// although a FATAL ABSL_RAW_LOG message will still abort the process.

// ABSL_HAVE_POSIX_WRITE is defined when the platform provides posix write()
// (as from unistd.h)
//
// This preprocessor token is also defined in raw_io.cc.  If you need to copy
// this, consider moving both to config.h instead.
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__Fuchsia__) || defined(__native_client__) || defined(__ASYLO__)
#include <unistd.h>


#define ABSL_HAVE_POSIX_WRITE 1
#define ABSL_LOW_LEVEL_WRITE_SUPPORTED 1
#else
#undef ABSL_HAVE_POSIX_WRITE
#endif

// ABSL_HAVE_SYSCALL_WRITE is defined when the platform provides the syscall
//   syscall(SYS_write, /*int*/ fd, /*char* */ buf, /*size_t*/ len);
// for low level operations that want to avoid libc.
#if (defined(__linux__) || defined(__FreeBSD__)) && !defined(__ANDROID__)
#include <sys/syscall.h>
#define ABSL_HAVE_SYSCALL_WRITE 1
#define ABSL_LOW_LEVEL_WRITE_SUPPORTED 1
#else
#undef ABSL_HAVE_SYSCALL_WRITE
#endif

#ifdef _WIN32
#include <io.h>

#define ABSL_HAVE_RAW_IO 1
#define ABSL_LOW_LEVEL_WRITE_SUPPORTED 1
#else
#undef ABSL_HAVE_RAW_IO
#endif

// TODO(user): We want raw-logging to work on as many platforms as possible.
// Explicitly #error out when not ABSL_LOW_LEVEL_WRITE_SUPPORTED, except for a
// whitelisted set of platforms for which we expect not to be able to raw log.

ABSL_CONST_INIT static absl::base_internal::AtomicHook<
    absl::raw_logging_internal::LogPrefixHook> log_prefix_hook;
ABSL_CONST_INIT static absl::base_internal::AtomicHook<
    absl::raw_logging_internal::AbortHook> abort_hook;

#ifdef ABSL_LOW_LEVEL_WRITE_SUPPORTED
static const char kTruncated[] = " ... (message truncated)\n";

// sprintf the format to the buffer, adjusting *buf and *size to reflect the
// consumed bytes, and return whether the message fit without truncation.  If
// truncation occurred, if possible leave room in the buffer for the message
// kTruncated[].
inline static bool VADoRawLog(char** buf, int* size, const char* format,
                              va_list ap) ABSL_PRINTF_ATTRIBUTE(3, 0);
inline static bool VADoRawLog(char** buf, int* size,
                              const char* format, va_list ap) {
  int n = vsnprintf(*buf, *size, format, ap);
  bool result = true;
  if (n < 0 || n > *size) {
    result = false;
    if (static_cast<size_t>(*size) > sizeof(kTruncated)) {
      n = *size - sizeof(kTruncated);  // room for truncation message
    } else {
      n = 0;                           // no room for truncation message
    }
  }
  *size -= n;
  *buf += n;
  return result;
}
#endif  // ABSL_LOW_LEVEL_WRITE_SUPPORTED

static constexpr int kLogBufSize = 3000;

namespace {

// CAVEAT: vsnprintf called from *DoRawLog below has some (exotic) code paths
// that invoke malloc() and getenv() that might acquire some locks.
// If this becomes a problem we should reimplement a subset of vsnprintf
// that does not need locks and malloc.
// E.g. //third_party/clearsilver/core/util/snprintf.c
// looks like such a reimplementation.

// Helper for RawLog below.
// *DoRawLog writes to *buf of *size and move them past the written portion.
// It returns true iff there was no overflow or error.
bool DoRawLog(char** buf, int* size, const char* format, ...)
    ABSL_PRINTF_ATTRIBUTE(3, 4);
bool DoRawLog(char** buf, int* size, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(*buf, *size, format, ap);
  va_end(ap);
  if (n < 0 || n > *size) return false;
  *size -= n;
  *buf += n;
  return true;
}

void RawLogVA(absl::LogSeverity severity, const char* file, int line,
              const char* format, va_list ap) ABSL_PRINTF_ATTRIBUTE(4, 0);
void RawLogVA(absl::LogSeverity severity, const char* file, int line,
              const char* format, va_list ap) {
  char buffer[kLogBufSize];
  char* buf = buffer;
  int size = sizeof(buffer);
#ifdef ABSL_LOW_LEVEL_WRITE_SUPPORTED
  bool enabled = true;
#else
  bool enabled = false;
#endif

#ifdef ABSL_MIN_LOG_LEVEL
  if (static_cast<int>(severity) < ABSL_MIN_LOG_LEVEL &&
      severity < absl::LogSeverity::kFatal) {
    enabled = false;
  }
#endif

  auto log_prefix_hook_ptr = log_prefix_hook.Load();
  if (log_prefix_hook_ptr) {
    enabled = log_prefix_hook_ptr(severity, file, line, &buf, &size);
  } else {
    if (enabled) {
      DoRawLog(&buf, &size, "[%s : %d] RAW: ", file, line);
    }
  }
  const char* const prefix_end = buf;

#ifdef ABSL_LOW_LEVEL_WRITE_SUPPORTED
  if (enabled) {
    bool no_chop = VADoRawLog(&buf, &size, format, ap);
    if (no_chop) {
      DoRawLog(&buf, &size, "\n");
    } else {
      DoRawLog(&buf, &size, "%s", kTruncated);
    }
    absl::raw_logging_internal::SafeWriteToStderr(buffer, strlen(buffer));
  }
#else
  static_cast<void>(format);
  static_cast<void>(ap);
#endif

  // Abort the process after logging a FATAL message, even if the output itself
  // was suppressed.
  if (severity == absl::LogSeverity::kFatal) {
    abort_hook(file, line, buffer, prefix_end, buffer + kLogBufSize);
    abort();
  }
}

}  // namespace

namespace absl {
namespace raw_logging_internal {
void SafeWriteToStderr(const char *s, size_t len) {
#if defined(ABSL_HAVE_SYSCALL_WRITE)
  syscall(SYS_write, STDERR_FILENO, s, len);
#elif defined(ABSL_HAVE_POSIX_WRITE)
  write(STDERR_FILENO, s, len);
#elif defined(ABSL_HAVE_RAW_IO)
  _write(/* stderr */ 2, s, len);
#else
  // stderr logging unsupported on this platform
  (void) s;
  (void) len;
#endif
}

void RawLog(absl::LogSeverity severity, const char* file, int line,
            const char* format, ...) ABSL_PRINTF_ATTRIBUTE(4, 5);
void RawLog(absl::LogSeverity severity, const char* file, int line,
            const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  RawLogVA(severity, file, line, format, ap);
  va_end(ap);
}
void RawLog(int severity, const char* file, int line, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  RawLogVA(absl::NormalizeLogSeverity(severity), file, line, format, ap);
  va_end(ap);
}

bool RawLoggingFullySupported() {
#ifdef ABSL_LOW_LEVEL_WRITE_SUPPORTED
  return true;
#else  // !ABSL_LOW_LEVEL_WRITE_SUPPORTED
  return false;
#endif  // !ABSL_LOW_LEVEL_WRITE_SUPPORTED
}

}  // namespace raw_logging_internal
}  // namespace absl

namespace base_raw_logging {

void RawLog(absl::LogSeverity severity, const char* file, int line,
            const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  RawLogVA(severity, file, line, format, ap);
  va_end(ap);
}
void RawLog(int severity, const char* file, int line, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  RawLogVA(absl::NormalizeLogSeverity(severity), file, line, format, ap);
  va_end(ap);
}

void RegisterLogPrefixHook(LogPrefixHook fn) {
  log_prefix_hook.Store(fn);
}

void RegisterAbortHook(AbortHook fn) {
  abort_hook.Store(fn);
}
}  // namespace base_raw_logging

