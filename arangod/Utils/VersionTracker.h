////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_UTILS_VERSION_TRACKER_H
#define ARANGOD_UTILS_VERSION_TRACKER_H 1

#include "Basics/Common.h"

namespace arangodb {

/// @brief a class for tracking a global version number
/// this version number is increased on every DDL operation, and may be sent
/// to the agency in order to notify other listeners about DDL changes
class VersionTracker {
 public:
  VersionTracker() : _value(0) {}

  void track(char const*) {
    ++_value;
    // can enable this for tracking things later
    // LOG_TOPIC("08a33", TRACE, Logger::FIXME) << "version updated by " << msg;
  }

  uint64_t current() const { return _value; }

 private:
  std::atomic<uint64_t> _value;
};
}  // namespace arangodb

#endif
