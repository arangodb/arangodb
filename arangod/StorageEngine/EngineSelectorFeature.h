////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
  explicit EngineSelectorFeature(
    application_features::ApplicationServer& server
  );

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  // return the names of all available storage engines
  static std::unordered_set<std::string> availableEngineNames();

  // return all available storage engines
  static std::unordered_map<std::string, std::string> availableEngines();

  // whether the engine has been started yet
  bool hasStarted() const { return _hasStarted.load(); }

  static std::string const& engineName();

  static std::string const& defaultEngine();

  static bool isMMFiles();
  static bool isRocksDB();

  // selected storage engine. this will contain a pointer to the storage engine after
  // prepare() and before unprepare()
  static StorageEngine* ENGINE;

 private:
  std::string _engine;
  std::string _engineFilePath;
  std::atomic<bool> _hasStarted;
};

}

#endif
