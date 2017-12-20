////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8CLIENT_CLIENT_HELPER_H
#define ARANGODB_V8CLIENT_CLIENT_HELPER_H 1

#include <memory>
#include <queue>
#include <vector>

#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"

namespace arangodb {
class Result;
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

template <typename JobData>
class ClientWorker;

template <typename JobData>
class ClientHelper {
 public:
  ClientHelper();
  virtual ~ClientHelper();

 protected:
  static std::string rewriteLocation(void* data, std::string const& location) noexcept;
  static std::string getHttpErrorMessage(httpclient::SimpleHttpResult*,
                                         int* err) noexcept;

 protected:
  // client creation and related methods

  std::unique_ptr<httpclient::SimpleHttpClient> getConnectedClient(
      bool force = false, bool verbose = false) noexcept;
  bool getArangoIsCluster(int* err) noexcept;
  bool getArangoIsUsingEngine(int* err, std::string const& name) noexcept;

  // queue management methods
  bool spawnWorkers(size_t const&) noexcept;
  bool isQueueEmpty() const noexcept;
  bool allWorkersBusy() const noexcept;
  bool queueJob(std::unique_ptr<JobData>&&) noexcept;
  std::unique_ptr<JobData> fetchJob() noexcept;
  void waitForWork() noexcept;

  virtual Result processJob(httpclient::SimpleHttpClient&,
                            JobData&) noexcept = 0;
  virtual void handleJobResult(std::unique_ptr<JobData>&&,
                               Result&) noexcept = 0;

 private:
  mutable Mutex _jobsLock;
  basics::ConditionVariable _jobsCondition;
  std::queue<std::unique_ptr<JobData>> _jobs;
  std::vector<std::unique_ptr<ClientWorker<JobData>>> _workers;

  friend class ClientWorker<JobData>;
};
}  // namespace arangodb

#endif
