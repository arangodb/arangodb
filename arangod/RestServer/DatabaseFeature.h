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
struct TRI_server_t;

namespace arangodb {
namespace basics {
class ThreadPool;
}

namespace aql {
class QueryRegistry;
}

class DatabaseFeature final : public application_features::ApplicationFeature {
 public:
  explicit DatabaseFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void start() override;
  void stop() override;

 public:
  TRI_vocbase_t* vocbase() const { return _vocbase; }

 private:
  std::string _directory;
  uint64_t _maximalJournalSize;
  bool _queryTracking;
  std::string _queryCacheMode;
  uint64_t _queryCacheEntries;
  bool _checkVersion;
  bool _upgrade;
  bool _skipUpgrade;
  uint64_t _indexThreads;
  bool _defaultWaitForSync;
  bool _forceSyncProperties;

 private:
  void checkVersion();
  void openDatabases();
  void closeDatabases();
  void upgradeDatabase();
  void updateContexts();
  void shutdownCompactor();

 private:
  TRI_vocbase_t* _vocbase;
  std::unique_ptr<TRI_server_t> _server;
  std::unique_ptr<aql::QueryRegistry> _queryRegistry;
  std::string _databasePath;
  std::unique_ptr<basics::ThreadPool> _indexPool;
  bool _replicationApplier;
};
}

#endif
