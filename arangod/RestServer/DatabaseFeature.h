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

#ifndef APPLICATION_FEATURES_DATABASE_FEATURE_H
#define APPLICATION_FEATURES_DATABASE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

struct TRI_vocbase_t;

namespace arangodb {
class DatabaseFeature final : public application_features::ApplicationFeature {
 public:
  static DatabaseFeature* DATABASE;

 public:
  explicit DatabaseFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void unprepare() override final;

 public:
  TRI_vocbase_t* vocbase() const { return _vocbase; }

  bool ignoreDatafileErrors() const { return _ignoreDatafileErrors; }
  bool isInitiallyEmpty() const { return _isInitiallyEmpty; }

  void disableReplicationApplier() { _replicationApplier = false; }
  void disableCompactor() { _disableCompactor = true; }
  void enableCheckVersion() { _checkVersion = true; }
  void enableUpgrade() { _upgrade = true; }

  std::string const& directory() { return _directory; }

 private:
  std::string _directory;
  uint64_t _maximalJournalSize;
  bool _defaultWaitForSync;
  bool _forceSyncProperties;
  bool _ignoreDatafileErrors;
  bool _throwCollectionNotLoadedError;

 private:
  void openDatabases();
  void closeDatabases();
  void updateContexts();
  void shutdownCompactor();

 private:
  TRI_vocbase_t* _vocbase;
  std::string _databasePath;
  bool _isInitiallyEmpty;
  bool _replicationApplier;
  bool _disableCompactor;
  bool _checkVersion;
  bool _upgrade;
};
}

#endif
