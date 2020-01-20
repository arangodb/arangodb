////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for ClusterComm
///
/// @file
///
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
/// @author Matthew Von-Maszewski
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <future>
#include <thread>
#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/system-functions.h"
#include "Cluster/ClusterComm.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/SupervisedScheduler.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::rest;

class ClusterCommTester : public ClusterComm {
 public:
  ClusterCommTester(application_features::ApplicationServer& server)
      : ClusterComm(server, false),
        _oldSched(nullptr),
        _testerSched(server, 1, 2, 3, 4, 5) {
    // fake a scheduler object
    _oldSched = SchedulerFeature::SCHEDULER;
    SchedulerFeature::SCHEDULER = &_testerSched;
  }

  ~ClusterCommTester() {
    // restore previous scheduler
    SchedulerFeature::SCHEDULER = _oldSched;
  }

  OperationID addSimpleRequest(OperationID transId, ClusterCommOpStatus status) {
    OperationID id = TRI_NewTickServer();
    std::shared_ptr<ClusterCommResult> result(new ClusterCommResult);

    result->operationID = id;
    result->coordTransactionID = transId;
    result->status = status;

    responses.emplace(id, AsyncResponse{TRI_microtime(), result,
                                        nullptr /* communicator, we do not care for it*/});

    return id;
  }  // addSimpleRequest

  AsyncResponse& getResponse(size_t index) {
    auto it = responses.begin();
    for (; 0 < index; ++it, --index)
      ;
    return it->second;
  }  // getResponse

  void signalResponse() {
    CONDITION_LOCKER(locker, somethingReceived);
    somethingReceived.broadcast();
  }  // signalResponse

  decltype(SchedulerFeature::SCHEDULER) _oldSched;
  SupervisedScheduler _testerSched;

};  // class ClusterCommTester

TEST(ClusterCommTest, no_matching_response) {
  application_features::ApplicationServer server(nullptr, nullptr);
  ClusterCommTester testme(server);
  ClusterCommResult result;
  CoordTransactionID id = TRI_NewTickServer();

  result = testme.wait(id, 42, "", 100);
  ASSERT_EQ(CL_COMM_DROPPED, result.status);
  ASSERT_EQ(42, result.operationID);

}  // no matching responses

TEST(ClusterCommTest, single_response) {
  application_features::ApplicationServer server(nullptr, nullptr);
  ClusterCommTester testme(server);
  ClusterCommResult result;
  OperationID id;
  CoordTransactionID transId;

  // find by transId
  transId = TRI_NewTickServer();
  id = testme.addSimpleRequest(transId, CL_COMM_RECEIVED);

  result = testme.wait(transId, 0, "", 0.1);
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id, result.operationID);

  // find by ticketId
  transId = TRI_NewTickServer();
  id = testme.addSimpleRequest(transId, CL_COMM_RECEIVED);

  result = testme.wait(0, id, "", 0.1);
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id, result.operationID);

}  // single response

TEST(ClusterCommTest, out_of_order_response) {
  application_features::ApplicationServer server(nullptr, nullptr);
  ClusterCommTester testme(server);
  ClusterCommResult result;
  OperationID id_first, id_other;
  CoordTransactionID transId;

  // first response object is still waiting reply
  transId = TRI_NewTickServer();
  testme.addSimpleRequest(transId, CL_COMM_RECEIVED);

  // second response object is live and ready to return
  testme.addSimpleRequest(transId, CL_COMM_RECEIVED);

  // responses object is unordered map.  make "first" entry blocking
  id_first = testme.getResponse(0).result->operationID;
  testme.getResponse(0).result->status = CL_COMM_SUBMITTED;
  id_other = testme.getResponse(1).result->operationID;

  result = testme.wait(transId, 0, "", 0.1);
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id_other, result.operationID);
  ASSERT_NE(id_first, result.operationID);

}  // out of order response

TEST(ClusterCommTest, simple_function_timeout) {
  application_features::ApplicationServer server(nullptr, nullptr);
  ClusterCommTester testme(server);
  ClusterCommResult result;
  CoordTransactionID transId;
  ClusterCommTimeout startTime, endTime, diff;

  // insert a response that receives no answer
  transId = TRI_NewTickServer();
  testme.addSimpleRequest(transId, CL_COMM_SUBMITTED);
  startTime = TRI_microtime();
  result = testme.wait(transId, 0, "", 0.005);
  endTime = TRI_microtime();
  diff = endTime - startTime;
  ASSERT_TRUE(0.0049 < diff);  // must write range test in two parts for REQUIRE
  ASSERT_EQ(CL_COMM_TIMEOUT, result.status);
  ASSERT_EQ(0, result.operationID);

  // larger timeout
  startTime = TRI_microtime();
  result = testme.wait(transId, 0, "", 0.1);
  endTime = TRI_microtime();
  diff = endTime - startTime;
  ASSERT_TRUE(0.09 <= diff);  // must write range test in two parts for REQUIRE
  ASSERT_EQ(CL_COMM_TIMEOUT, result.status);
  ASSERT_EQ(0, result.operationID);
}  // simple function time out

TEST(ClusterCommTest, time_delayed_out_of_order_response) {
  application_features::ApplicationServer server(nullptr, nullptr);
  ClusterCommTester testme(server);
  ClusterCommResult result;
  OperationID id_first, id_other;
  CoordTransactionID transId;
  ClusterCommTimeout startTime, endTime, diff;

  // first response object is still waiting reply
  transId = TRI_NewTickServer();
  testme.addSimpleRequest(transId, CL_COMM_SUBMITTED);

  // second response object is still waiting reply
  testme.addSimpleRequest(transId, CL_COMM_SUBMITTED);

  // responses object is unordered map.  make "first" entry blocking
  id_first = testme.getResponse(0).result->operationID;
  id_other = testme.getResponse(1).result->operationID;

  startTime = TRI_microtime();
  std::future<void> f1(std::async(std::launch::async,
                                  [&] {
                                    timespec ts = {0, 15000000};
                                    std::this_thread::sleep_for(
                                        std::chrono::microseconds(ts.tv_nsec / 1000L));
                                    testme.getResponse(0).result->status = CL_COMM_RECEIVED;
                                    testme.signalResponse();
                                  }  // lambda
                                  ));

  result = testme.wait(transId, 0, "", 0.1);
  endTime = TRI_microtime();
  diff = endTime - startTime;
  ASSERT_TRUE(0.014 <= diff);  // must write range test in two parts for REQUIRE
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id_first, result.operationID);
  f1.get();

  // do second time to get other response
  startTime = TRI_microtime();
  std::future<void> f2(std::async(std::launch::async,
                                  [&] {
                                    timespec ts = {0, 30000000};
                                    std::this_thread::sleep_for(
                                        std::chrono::microseconds(ts.tv_nsec / 1000L));
                                    testme.getResponse(0).result->status = CL_COMM_RECEIVED;
                                    testme.signalResponse();
                                  }  // lambda
                                  ));

  result = testme.wait(transId, 0, "", 0.1);
  endTime = TRI_microtime();
  diff = endTime - startTime;
  ASSERT_TRUE(0.029 <= diff);  // must write range test in two parts for REQUIRE
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id_other, result.operationID);
  f2.get();

  //
  // do same test but retrieve second object first
  //
  // first response object is still waiting reply
  transId = TRI_NewTickServer();
  testme.addSimpleRequest(transId, CL_COMM_SUBMITTED);

  // second response object is still waiting reply
  testme.addSimpleRequest(transId, CL_COMM_SUBMITTED);

  // responses object is unordered map.  make "first" entry blocking
  id_first = testme.getResponse(0).result->operationID;
  id_other = testme.getResponse(1).result->operationID;

  startTime = TRI_microtime();
  std::future<void> f3(std::async(std::launch::async,
                                  [&] {
                                    timespec ts = {0, 15000000};
                                    std::this_thread::sleep_for(
                                        std::chrono::microseconds(ts.tv_nsec / 1000L));
                                    testme.getResponse(1).result->status = CL_COMM_RECEIVED;
                                    testme.signalResponse();
                                  }  // lambda
                                  ));

  result = testme.wait(transId, 0, "", 0.1);
  endTime = TRI_microtime();
  diff = endTime - startTime;
  ASSERT_TRUE(0.014 <= diff);  // must write range test in two parts for REQUIRE
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id_other, result.operationID);
  f3.get();

  // do second time to get other response
  startTime = TRI_microtime();
  std::future<void> f4(std::async(std::launch::async,
                                  [&] {
                                    timespec ts = {0, 30000000};
                                    std::this_thread::sleep_for(
                                        std::chrono::microseconds(ts.tv_nsec / 1000L));
                                    testme.getResponse(0).result->status = CL_COMM_RECEIVED;
                                    testme.signalResponse();
                                  }  // lambda
                                  ));

  result = testme.wait(transId, 0, "", 0.1);
  endTime = TRI_microtime();
  diff = endTime - startTime;
  ASSERT_TRUE(0.029 <= diff);  // must write range test in two parts for REQUIRE
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id_first, result.operationID);
  f4.get();

  // infinite wait
  id_first = testme.addSimpleRequest(transId, CL_COMM_SUBMITTED);
  startTime = TRI_microtime();
  std::future<void> f5(
      std::async(std::launch::async,
                 [&] {
                   timespec ts = {0, 500000000};  // 0.5 seconds
                   std::this_thread::sleep_for(std::chrono::microseconds(ts.tv_nsec / 1000L));
                   testme.getResponse(0).result->status = CL_COMM_RECEIVED;
                   testme.signalResponse();
                 }  // lambda
                 ));

  result = testme.wait(transId, 0, "", 0.0);
  endTime = TRI_microtime();
  diff = endTime - startTime;
  ASSERT_TRUE(0.499 <= diff);  // must write range test in two parts for REQUIRE
  ASSERT_EQ(CL_COMM_RECEIVED, result.status);
  ASSERT_EQ(id_first, result.operationID);
  f5.get();

}  // out of order response
