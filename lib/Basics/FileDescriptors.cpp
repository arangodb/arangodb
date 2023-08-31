////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "FileDescriptors.h"
#include "Logger/LogMacros.h"

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <absl/strings/str_cat.h>

#include <cstdlib>
#include <cstring>
#include <string>

#ifdef TRI_HAVE_GETRLIMIT
namespace arangodb {

Result FileDescriptors::load(FileDescriptors& descriptors) {
  struct rlimit rlim;
  int res = getrlimit(RLIMIT_NOFILE, &rlim);

  if (res != 0) {
    return {TRI_ERROR_FAILED,
            absl::StrCat("cannot get the file descriptors limit value: ",
                         strerror(errno))};
  }

  descriptors.hard = rlim.rlim_max;
  descriptors.soft = rlim.rlim_cur;

  return {};
}

Result FileDescriptors::store(FileDescriptors const& descriptors) {
  struct rlimit rlim;
  rlim.rlim_max = descriptors.hard;
  rlim.rlim_cur = descriptors.soft;

  if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    return {TRI_ERROR_FAILED,
            absl::StrCat("cannot raise the file descriptors limit: ",
                         strerror(errno))};
  }

  return {};
}

Result FileDescriptors::adjustTo(FileDescriptors::ValueType value) {
  auto doAdjust = [](FileDescriptors::ValueType recommended) -> Result {
    FileDescriptors current;
    if (Result res = FileDescriptors::load(current); res.fail()) {
      return res;
    }

    LOG_TOPIC("6762c", DEBUG, Logger::SYSCALL)
        << "file-descriptors (nofiles) hard limit is "
        << FileDescriptors::stringify(current.hard) << ", soft limit is "
        << FileDescriptors::stringify(current.soft);

    if (recommended > 0) {
      if (current.hard < recommended) {
        LOG_TOPIC("0835c", DEBUG, Logger::SYSCALL)
            << "hard limit " << current.hard
            << " is too small, trying to raise";

        FileDescriptors copy = current;
        copy.hard = recommended;
        if (FileDescriptors::store(copy).ok()) {
          // value adjusted successfully
          current.hard = recommended;
        }
      } else {
        TRI_ASSERT(current.hard >= recommended);
        recommended = current.hard;
      }

#ifdef __APPLE__
      // For MacOs there is an upper bound on open file-handles
      // in addition to the user defined hard limit.
      // The bad news is that the user-defined hard limit can be larger
      // than the upper bound, in this case the below setting of soft limit
      // to hard limit will always fail. With the below line we only set
      // to at most the upper bound given by the System (OPEN_MAX).
      recommended = std::min(static_cast<FileDescriptors::ValueType>(OPEN_MAX),
                             recommended);
#endif
      if (current.soft < recommended) {
        LOG_TOPIC("2940e", DEBUG, Logger::SYSCALL)
            << "soft limit " << current.soft
            << " is too small, trying to raise";

        FileDescriptors copy = current;
        copy.soft = recommended;
        if (Result res = FileDescriptors::store(copy); res.fail()) {
          LOG_TOPIC("ba733", WARN, Logger::SYSCALL)
              << "cannot raise the file descriptors limit to " << recommended
              << ": " << res;
          return res;
        }
      }
    }

    return {};
  };

  // first try to raise file descriptors to at least the recommended minimum
  // value. as the recommended minimum value is pretty low, there is a high
  // chance that this actually succeeds and does not violate any hard limits
  if (Result res = doAdjust(std::max(value, recommendedMinimum()));
      res.fail()) {
    return res;
  }

  // still, we are not satisfied and will now try to raise the file descriptors
  // limit even further. if that fails, then it is at least likely that the
  // small raise in step 1 has worked.
  return doAdjust(65535);
}

FileDescriptors::ValueType FileDescriptors::recommendedMinimum() {
  // check if we are running under Valgrind...
  char const* v = getenv("LD_PRELOAD");
  if (v != nullptr && (strstr(v, "/valgrind/") != nullptr ||
                       strstr(v, "/vgpreload") != nullptr)) {
    // valgrind will somehow reset the ulimit values to some very low values.
    return requiredMinimum;
  }

#ifdef __APPLE__
  // some MacOS versions disallow raising file descriptor limits higher than
  // this 🙄
  return 8192;
#else
  // on Linux, we will also use 8192 for now. this should be low enough so
  // that it doesn't cause too much trouble when upgrading. however, this is
  // not a high enough value to operate with larger amounts of data! it is a
  // MINIMUM!
  return 8192;
#endif
}

bool FileDescriptors::isUnlimited(FileDescriptors::ValueType value) {
  auto max = std::numeric_limits<decltype(value)>::max();
  return (value == max || value == max / 2);
}

std::string FileDescriptors::stringify(FileDescriptors::ValueType value) {
  if (isUnlimited(value)) {
    return "unlimited";
  }
  return std::to_string(value);
}

}  // namespace arangodb
#endif
