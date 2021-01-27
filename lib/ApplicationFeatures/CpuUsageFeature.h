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

#ifndef ARANGODB_APPLICATION_FEATURES_CPU_USAGE_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_CPU_USAGE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/CpuUsageSnapshot.h"
#include "Basics/Mutex.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class CpuUsageFeature final : public application_features::ApplicationFeature {
 public:
  explicit CpuUsageFeature(application_features::ApplicationServer& server);
  ~CpuUsageFeature();

  void prepare() override final;

  /// @brief returns a snapshot containing CPU usage statistics.
  CpuUsageSnapshot snapshot();

 private:
  struct SnapshotProvider;

  /// @brief the provider that is used to obtain a CpuUsageSnapshot.
  /// The actual provider implementation is OS dependent.
  std::unique_ptr<SnapshotProvider> _snapshotProvider;

  /// @brief a mutex protecting concurrent reads and writes of the snapshot
  Mutex _snapshotMutex;

  /// @brief last snapshot taken
  /// protected by _snapshotMutex
  CpuUsageSnapshot _snapshot;

  /// @brief the delta of the last snapshot taken to its predecessor
  /// protected by _snapshotMutex
  CpuUsageSnapshot _snapshotDelta;

  /// @brief whether or not a stats update in currently in progress.
  /// protected by _snapshotMutex
  bool _updateInProgress;
};

}  // namespace arangodb

#endif
