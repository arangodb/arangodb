////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB3_RESTREPAIRHANDLER_H
#define ARANGODB3_RESTREPAIRHANDLER_H

#include "GeneralServer/AsyncJobManager.h"
#include "RestHandler/RestBaseHandler.h"
#include "Cluster/ClusterRepairs.h"
#include "Agency/AgencyComm.h"

namespace arangodb {
namespace rest {
class AsyncJobManager;
}

namespace rest_repair {
using VPackBufferPtr = std::shared_ptr<VPackBuffer<uint8_t>>;

enum class JobStatus {
  todo,
  finished,
  pending,
  failed,
  missing
};

template<typename T>
class TResult : public Result {
 public:

  TResult static success(T val) {
    return TResult(val, 0);
  }

  TResult static error(int errorNumber) {
    return TResult(boost::none, errorNumber);
  }

  TResult static error(int errorNumber, std::string const &errorMessage) {
    return TResult(boost::none, errorNumber, errorMessage);
  }

  explicit TResult(Result const &other)
    : Result(other) {}

  T get() {
    return _val.get();
  }

 protected:
  boost::optional<T> _val;

  TResult(boost::optional<T> val_, int errorNumber)
    : Result(errorNumber), _val(val_) {}

  TResult(boost::optional<T> val_, int errorNumber, std::string const &errorMessage)
    : Result(errorNumber, errorMessage), _val(val_) {}
};
}

class RestRepairHandler : public arangodb::RestBaseHandler {
 public:
  RestRepairHandler(
    GeneralRequest *request,
    GeneralResponse *response
  );

  char const *name() const final {
    return "RestDemoHandler";
  }

  bool isDirect() const override {
    // TODO [tobias] does this do what I think it does?
    return false;
  }

  RestStatus execute() override;

  // TODO [tobias] maybe implement cancel()

 private:
  void handlePost();

  RestStatus repairDistributeShardsLike();

  void executeRepairOperations(
    std::list<cluster_repairs::RepairOperation> list
  );

  template <std::size_t N>
  rest_repair::TResult<std::array<rest_repair::VPackBufferPtr, N>>
    getFromAgency(std::array<std::string const, N> const& agencyKeyArray);

  rest_repair::TResult<rest_repair::VPackBufferPtr>
    getFromAgency(std::string const& agencyKey);

  rest_repair::TResult<rest_repair::JobStatus>
    getJobStatusFromAgency(std::string const &jobId);
};

}


#endif //ARANGODB3_RESTREPAIRHANDLER_H
