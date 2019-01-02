////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BASICS_OPEN_FILES_TRACKER_H
#define ARANGODB_BASICS_OPEN_FILES_TRACKER_H 1

#include "Basics/Common.h"

#define TRI_TRACKED_CREATE_FILE(a, b, c) \
  arangodb::OpenFilesTracker::instance()->create((a), (b), (c))
#define TRI_TRACKED_OPEN_FILE(a, b) \
  arangodb::OpenFilesTracker::instance()->open((a), (b))
#define TRI_TRACKED_CLOSE_FILE arangodb::OpenFilesTracker::instance()->close

namespace arangodb {

class OpenFilesTracker {
 public:
  OpenFilesTracker();
  ~OpenFilesTracker();

  int create(char const* path, int oflag, int mode);
  int open(char const* path, int oflag);
  int close(int fd) noexcept;

  uint64_t numOpenFiles() const { return _numOpenFiles.load(); }

  void warnThreshold(uint64_t threshold) {
    _warnThreshold = threshold;
    _lastWarning = 0.0;
  }

  static OpenFilesTracker* instance();

 private:
  void increase();
  void decrease() noexcept;

 private:
  std::atomic<uint64_t> _numOpenFiles;
  uint64_t _warnThreshold;  // configured threshold
  double _lastWarning;      // timestamp of last warning
};

}  // namespace arangodb

#endif
