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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOSH_UTILS_PROGRESS_TRACKER_H
#define ARANGOSH_UTILS_PROGRESS_TRACKER_H 1

#include "ManagedDirectory.h"
#include "Basics/FileUtils.h"

namespace arangodb {
template <typename T>
struct ProgressTracker {
  ProgressTracker(ManagedDirectory& directory, bool ignoreExisting);

  ProgressTracker(ProgressTracker const&) = delete;
  ProgressTracker(ProgressTracker&&) noexcept = delete;
  ProgressTracker& operator=(ProgressTracker const&) = delete;
  ProgressTracker& operator=(ProgressTracker&&) noexcept = delete;

  T getStatus(std::string const& collectionName);
  /// @brief returns true if the progress was synced to disc
  bool updateStatus(std::string const& collectionName, T const& status);
  std::string filename() const;

  ManagedDirectory& directory;
  std::shared_mutex _collectionStatesMutex;
  std::mutex _writeFileMutex;
  std::unordered_map<std::string, T> _collectionStates;
  std::atomic<bool> _writeQueued{false};
};

template <typename T>
bool ProgressTracker<T>::updateStatus(std::string const& collectionName,
                                      T const& status) {
  {
    std::unique_lock guard(_collectionStatesMutex);
    _collectionStates[collectionName] = status;

    if (_writeQueued) {
      return false;
    }

    _writeQueued = true;
  }

  {
    std::unique_lock guard(_writeFileMutex);
    VPackBufferUInt8 buffer;

    {
      std::unique_lock guardState(_collectionStatesMutex);
      _writeQueued = false;

      VPackBuilder builder{buffer};
      VPackObjectBuilder object(&builder);
      for (auto const& [filename, state] : _collectionStates) {
        builder.add(VPackValue(filename));
        state.toVelocyPack(builder);
      }
    }

    arangodb::basics::VelocyPackHelper::velocyPackToFile(directory.pathToFile("continue.json"),
                                                         VPackSlice(buffer.data()), true);
  }
  return true;
}

template <typename T>
T ProgressTracker<T>::getStatus(const std::string& collectionName) {
  std::shared_lock guard(_collectionStatesMutex);
  return _collectionStates[collectionName];  // intentionally default construct
}

template <typename T>
ProgressTracker<T>::ProgressTracker(ManagedDirectory& directory, bool ignoreExisting)
    : directory(directory) {
  if (ignoreExisting) {
    return;
  }

  VPackBuilder progressBuilder = directory.vpackFromJsonFile("continue.json");
  VPackSlice progress = progressBuilder.slice();

  if (progress.isNone()) {
    return;
  }

  std::unique_lock guardState(_collectionStatesMutex);
  for (auto&& [key, value] : VPackObjectIterator(progress)) {
    _collectionStates[key.copyString()] = T(value);
  }
}

template <typename T>
std::string ProgressTracker<T>::filename() const {
  return directory.pathToFile("continue.json");
}

}

#endif
