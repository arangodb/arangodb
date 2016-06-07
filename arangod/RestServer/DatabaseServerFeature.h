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

#ifndef APPLICATION_FEATURES_DATABASE_SERVER_FEATURE_H
#define APPLICATION_FEATURES_DATABASE_SERVER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

struct TRI_server_t;

namespace arangodb {
namespace basics {
class ThreadPool;
}

class DatabaseServerFeature final
    : public application_features::ApplicationFeature {
 public:
  static TRI_server_t* SERVER;
  static basics::ThreadPool* INDEX_POOL;

 public:
  explicit DatabaseServerFeature(
      application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

 private:
  uint64_t _indexThreads = 2;

 public:
  TRI_server_t* server() const { return _server.get(); }

 private:
  std::unique_ptr<TRI_server_t> _server;
  std::unique_ptr<basics::ThreadPool> _indexPool;
};
}

#endif
