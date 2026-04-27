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
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"

namespace arangodb {
class ClusterEngine;
class RocksDBEngine;
class StorageEngine;

// a stub class that other features can use to check whether a storage
// engine (no matter what type) is ready
class StorageEngineFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "StorageEngine"; }

  explicit StorageEngineFeature(application_features::ApplicationServer& server);

  StorageEngine& engine();
  template<typename As,
           typename std::enable_if<std::is_base_of<StorageEngine, As>::value,
                                   int>::type = 0>
  As& engine();

  std::string_view engineName() const;
  static std::string_view defaultEngine();
  bool isRocksDB();
  bool selected() const { return _selected.load(); }
  void prepare() override final;
  void unprepare() override final;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setEngineTesting(StorageEngine*);
#endif

 private:
  StorageEngine* _engine{nullptr};
  std::atomic<bool> _selected{false};
};

}  // namespace arangodb
