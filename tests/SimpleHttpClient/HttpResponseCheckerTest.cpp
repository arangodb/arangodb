////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/ErrorCode.h"
#include "Logger/LogMacros.h"
#include "SimpleHttpClient/HttpResponseChecker.h"
#include "SimpleHttpClient/SimpleHttpResult.h"


using namespace arangodb;
using namespace arangodb::httpclient;
using namespace arangodb::velocypack;

TEST(HttpResponseCheckerTest, testEmpty) {
  auto check = HttpResponseChecker::check("", nullptr);
  EXPECT_EQ(check.errorNumber(), TRI_ERROR_INTERNAL);
}

TEST(HttpResponseCheckerTest, testEmptyWithClientErrorMsg) {
  auto check = HttpResponseChecker::check("Http request", nullptr);
  EXPECT_EQ(check.errorNumber(), TRI_ERROR_INTERNAL);
  EXPECT_NE(check.errorMessage().find("Http request"), std::string::npos);
}

TEST(HttpResponseCheckerTest, testErrorResponse) {
  std::unique_ptr<httpclient::SimpleHttpResult> response = std::make_unique<httpclient::SimpleHttpResult>();
  response->setResultType(httpclient::SimpleHttpResult::resultTypes{arangodb::httpclient::SimpleHttpResult::COULD_NOT_CONNECT});
  response->setHttpReturnMessage("NOT FOUND");
  auto check = HttpResponseChecker::check("", response.get(), "Http request");
  EXPECT_EQ(check.errorNumber(), TRI_ERROR_INTERNAL);
  EXPECT_NE(check.errorMessage().find("Http request"), std::string::npos);
}

TEST(HttpResponseCheckerTest, testErrorResponse2) {
  std::unique_ptr<httpclient::SimpleHttpResult> response = std::make_unique<httpclient::SimpleHttpResult>();
  response->setResultType(httpclient::SimpleHttpResult::resultTypes{arangodb::httpclient::SimpleHttpResult::COMPLETE});
  response->setHttpReturnMessage("NOT FOUND");
  response->setHttpReturnCode(404);
  auto check = HttpResponseChecker::check("Http request", response.get());
  EXPECT_EQ(check.errorNumber(), ErrorCode{response->getHttpReturnCode()});
  LOG_DEVEL << check.errorMessage();
  EXPECT_NE(check.errorMessage().find(response->getHttpReturnMessage()), std::string::npos);
  EXPECT_NE(check.errorMessage().find(std::to_string(response->getHttpReturnCode())), std::string::npos);
}

TEST(HttpResponseCheckerTest, testValidResponse) {
  std::unique_ptr<httpclient::SimpleHttpResult> response = std::make_unique<httpclient::SimpleHttpResult>();
  response->setResultType(httpclient::SimpleHttpResult::resultTypes{arangodb::httpclient::SimpleHttpResult::COMPLETE});
  response->setHttpReturnMessage("COMPLETE");
  response->setHttpReturnCode(200);
  auto check = HttpResponseChecker::check("Http request", response.get());
  EXPECT_EQ(check.errorNumber(), TRI_ERROR_NO_ERROR);
}

TEST(HttpResponseCheckerTest, testValidResponseHtml) {
  std::unique_ptr<httpclient::SimpleHttpResult> response = std::make_unique<httpclient::SimpleHttpResult>();
  response->addHeaderField("content-type: text/html", 23);
  response->getBody().appendText("foo bar");
  response->setContentLength(7);
  response->setResultType(httpclient::SimpleHttpResult::resultTypes{arangodb::httpclient::SimpleHttpResult::COMPLETE});
  response->setHttpReturnMessage("COMPLETE");
  response->setHttpReturnCode(200);
  auto check = HttpResponseChecker::check("Http request", response.get());
  EXPECT_EQ(check.errorNumber(), TRI_ERROR_NO_ERROR);
}

TEST(HttpResponseCheckerTest, testValidResponseHtml2) {
  std::unique_ptr<httpclient::SimpleHttpResult> response = std::make_unique<httpclient::SimpleHttpResult>();
  response->addHeaderField("content-type: application/json", 28); //application/json
  response->getBody().appendText("{\"errorNum\": 3, \"errorMessage\": \"foo bar\"}");
  response->setContentLength(response->getBody().length());
  response->setResultType(httpclient::SimpleHttpResult::resultTypes{arangodb::httpclient::SimpleHttpResult::COMPLETE});
  response->setHttpReturnMessage("COMPLETE");
  response->setHttpReturnCode(403);
  auto check = HttpResponseChecker::check("Http request", response.get());
  EXPECT_EQ(check.errorNumber(), ErrorCode{3});
  EXPECT_NE(check.errorMessage().find("foo bar"), std::string::npos);

}
