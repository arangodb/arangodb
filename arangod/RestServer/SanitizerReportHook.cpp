////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

// Override the weak default provided by the sanitizer runtime.
// Called exactly once per sanitizer report, after the report text has been
// written, under the sanitizer's internal report mutex.  The string is always
// of the form "SUMMARY: <Sanitizer>: <kind> <location>".
//
// When ARANGODB_SANITIZER_REPORT_LOG is set as a path prefix, each call appends
// a timestamped line to "<prefix>.<pid>".  The PID suffix ensures multiple
// servers in the same directory don't overwrite each other — mirroring the
// convention used by the sanitizer runtimes themselves (e.g. tsan.log.<pid>).
//
// These timestamps allow us to attribute  individual reports to specific test
// cases — even when multiple reports end up in the same sanitizer log file.
//
// We deliberately avoid the server's logging infrastructure here — LOG_TOPIC
// and friends involve locks and memory operations that trigger re-entrant
// sanitizer checks, deadlocking under the report mutex.
extern "C" void __sanitizer_report_error_summary(const char* summary) {
  const char* prefix = std::getenv("ARANGODB_SANITIZER_REPORT_LOG");
  if (prefix == nullptr) {
    return;
  }

  char path[4096];
  std::snprintf(path, sizeof(path), "%s.%d", prefix,
                static_cast<int>(getpid()));

  FILE* f = std::fopen(path, "a");
  if (f == nullptr) {
    return;
  }

  auto now = std::chrono::system_clock::now();
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch())
                .count();

  std::fprintf(f, "%lld\t%s\n", static_cast<long long>(us), summary);
  std::fclose(f);
}
