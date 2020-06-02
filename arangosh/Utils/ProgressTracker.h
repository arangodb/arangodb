////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
namespace arangodb {
template <typename T>
struct ProgressTracker {
  ProgressTracker(ManagedDirectory& directory, bool ignoreExisting);

  ProgressTracker(ProgressTracker const&) = delete;
  ProgressTracker(ProgressTracker&&) noexcept = delete;
  ProgressTracker& operator=(ProgressTracker const&) = delete;
  ProgressTracker& operator=(ProgressTracker&&) noexcept = delete;

  T getStatus(std::string const& collectionName);
  void updateStatus(std::string const& collectionName, T const& status);

  ManagedDirectory& directory;
  std::shared_mutex _collectionStatesMutex;
  std::mutex _writeFileMutex;
  std::unordered_map<std::string, T> _collectionStates;
  std::atomic<bool> _writeQueued{false};
};

template <typename T>
void ProgressTracker<T>::updateStatus(std::string const& collectionName,
                                                      T const& status) {
  {
    std::unique_lock guard(_collectionStatesMutex);
    _collectionStates[collectionName] = status;

    if (_writeQueued) {
      return;
    }

    _writeQueued = true;
  }

  {
    std::unique_lock guard(_writeFileMutex);
    VPackBuilder builder;
    {
      std::unique_lock guardState(_collectionStatesMutex);
      _writeQueued = false;
      VPackObjectBuilder object(&builder);

      for (auto const& [filename, state] : _collectionStates) {
        builder.add(VPackValue(filename));
        state.toVelocyPack(builder);
      }
    }

    LOG_DEVEL << "Continuation file = " << builder.toJson();
    arangodb::basics::VelocyPackHelper::velocyPackToFile(directory.pathToFile("continue.json"),
                                                         builder.slice(), true);
  }
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
    return ;
  }

  VPackBuilder progressBuilder = directory.vpackFromJsonFile("continue.json");
  VPackSlice progress = progressBuilder.slice();

  if (progress.isNone()) {
    return;
  }

  // TODO add error checking
  std::unique_lock guardState(_collectionStatesMutex);
  for (auto&& [key, value] : VPackObjectIterator(progress)) {
    _collectionStates[key.copyString()] = T(value);
  }
}

}

#endif  // ARANGODB3_PROGRESSTRACKER_H
