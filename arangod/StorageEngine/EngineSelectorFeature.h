////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_STORAGE_ENGINE_ENGINE_SELECTOR_FEATURE_H
#define ARANGODB_STORAGE_ENGINE_ENGINE_SELECTOR_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

class StorageEngine;

class EngineSelectorFeature final : public application_features::ApplicationFeature {
 public:
  explicit EngineSelectorFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  // return the names of all available storage engines
  static std::unordered_set<std::string> availableEngineNames();

  // whether the engine has been selected yet
  bool selected() const { return _selected.load(); }

  StorageEngine& engine();

  template <typename As, typename std::enable_if<std::is_base_of<StorageEngine, As>::value, int>::type = 0>
  As& engine();

  std::string const& engineName();

  static std::string const& defaultEngine();

  /// @brief note that this will return true for the ClusterEngine too, in case
  /// the underlying engine is the RocksDB engine.
  bool isRocksDB();

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setEngineTesting(StorageEngine*);
#endif

 private:
  StorageEngine* _engine;
  std::string _engineName;
  std::string _engineFilePath;
  std::atomic<bool> _selected;
  bool _allowDeprecatedDeployments;
};

}  // namespace arangodb

#endif
