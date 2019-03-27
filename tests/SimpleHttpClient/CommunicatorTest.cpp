////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for CuckooFilter based index selectivity estimator
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
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/Communicator.h"
#include "SimpleHttpClient/Callbacks.h"

#include <thread>
#include <chrono>

using namespace arangodb;
using namespace arangodb::communicator;

TEST_CASE("requests are properly aborted", "[communicator]" ) {
  Communicator communicator;

  bool callbacksCalled = false;

  communicator::Callbacks callbacks([&callbacksCalled](std::unique_ptr<GeneralResponse> response) {
    WARN("RESULT: " << GeneralResponse::responseString(response->responseCode()));
    REQUIRE(false); // it should be aborted?!
    callbacksCalled = true;
  }, [&callbacksCalled](int errorCode, std::unique_ptr<GeneralResponse> response) {
    REQUIRE(!response);
    REQUIRE(errorCode == TRI_COMMUNICATOR_REQUEST_ABORTED);
    callbacksCalled = true;
  });
  auto request = std::unique_ptr<HttpRequest>(HttpRequest::createHttpRequest(rest::ContentType::TEXT, "", 0, {}));
  request->setRequestType(RequestType::GET);
  communicator::Options opt;
  auto destination = std::string("http://www.example.com");
  auto newRequest = std::make_unique<communicator::NewRequest>(
    std::move(destination), 
    std::move(request), 
    callbacks, opt);

  communicator.addRequest(std::move(newRequest));
  communicator.work_once();
  communicator.abortRequests();
  while (communicator.work_once() > 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
  REQUIRE(callbacksCalled);
}

TEST_CASE("requests will call the progress callback", "[communicator]") {
  Communicator communicator;

  communicator::Callbacks callbacks([](std::unique_ptr<GeneralResponse> response) {
  }, [](int errorCode, std::unique_ptr<GeneralResponse> response) {
  });

  auto request = std::unique_ptr<HttpRequest>(HttpRequest::createHttpRequest(rest::ContentType::TEXT, "", 0, {}));
  request->setRequestType(RequestType::GET);

  // CURL_LAST /* never use! */ HA! FOOL!
  CURLcode curlRc = CURL_LAST;
  communicator::Options opt;
  opt._curlRcFn = std::make_shared<std::function<void(CURLcode)>>([&curlRc](CURLcode rc) {
    curlRc = rc;
  });

  auto destination = std::string("http://www.example.com");
  auto newRequest = std::make_unique<communicator::NewRequest>(
    std::move(destination), 
    std::move(request), 
    callbacks, opt);

  communicator.addRequest(std::move(newRequest));
  communicator.work_once();
  communicator.abortRequests();
  while (communicator.work_once() > 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
  REQUIRE(curlRc == CURLE_ABORTED_BY_CALLBACK); // curlRcFn was called
}


class ConnectionCountTester : public ConnectionCount {
public:

  /// this allows testing of time bucket rotation
    void moveCursor() {advanceCursor();}


};// class ConnectionCountTester


TEST_CASE("ConnectionCount:", "[communicator]" ) {
  ConnectionCountTester tester;
  int loop;

  // loop through the coverage minutes, see if minimum is consistent
  for (loop=0; loop<=ConnectionCount::eMinutesTracked; ++loop) {
    REQUIRE(ConnectionCount::eMinOpenConnects == tester.newMaxConnections(0));
    tester.moveCursor();
  } // for

  // parameter to newMaxConnections() does NOT change history
  REQUIRE(ConnectionCount::eMinOpenConnects+10 == tester.newMaxConnections(10));
  REQUIRE(ConnectionCount::eMinOpenConnects == tester.newMaxConnections(0));
  REQUIRE(ConnectionCount::eMinOpenConnects+2 == tester.newMaxConnections(2));
  REQUIRE(ConnectionCount::eMinOpenConnects == tester.newMaxConnections(0));

  // parameter to updateMaxConnections() DOES change history if bigger
  tester.updateMaxConnections(10);
  REQUIRE(10 == tester.newMaxConnections(0));
  REQUIRE(16 == tester.newMaxConnections(6));
  tester.updateMaxConnections(7);
  REQUIRE(10 == tester.newMaxConnections(0));
  REQUIRE(13 == tester.newMaxConnections(3));

  // simulate time passing and returned max changing ... assumes 6 min history
  //  "10" is still in current minute
  REQUIRE(6 == ConnectionCount::eMinutesTracked);
  REQUIRE(10 == tester.newMaxConnections(0));
  tester.updateMaxConnections(17);
  REQUIRE(17 == tester.newMaxConnections(0));
  tester.moveCursor();
  tester.updateMaxConnections(13);
  REQUIRE(17 == tester.newMaxConnections(0));
  tester.moveCursor();
  tester.updateMaxConnections(11);
  REQUIRE(17 == tester.newMaxConnections(0));
  tester.moveCursor();
  tester.updateMaxConnections(9);
  REQUIRE(17 == tester.newMaxConnections(0));
  tester.moveCursor();
  tester.updateMaxConnections(10);
  REQUIRE(17 == tester.newMaxConnections(0));
  tester.moveCursor();
  tester.updateMaxConnections(7);
  REQUIRE(17 == tester.newMaxConnections(0));
  tester.moveCursor();

  // minute history now full ... should see sliding window now
  REQUIRE(13 == tester.newMaxConnections(0));
  tester.moveCursor();
  REQUIRE(11 == tester.newMaxConnections(0));
  tester.moveCursor();
  REQUIRE(10 == tester.newMaxConnections(0)); // 9 smaller than 10
  tester.moveCursor();
  REQUIRE(10 == tester.newMaxConnections(0));
  tester.moveCursor();
  REQUIRE(7 == tester.newMaxConnections(0));
  tester.moveCursor();
  REQUIRE(ConnectionCount::eMinOpenConnects == tester.newMaxConnections(0));
  tester.moveCursor();
  REQUIRE(ConnectionCount::eMinOpenConnects == tester.newMaxConnections(0));

}
