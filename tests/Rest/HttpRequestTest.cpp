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
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/Exceptions.h"
#include "Endpoint/ConnectionInfo.h"
#include "Rest/HttpRequest.h"

#include <string_view>

using namespace arangodb;

TEST(HttpRequestTest, testMessageId) {
  ConnectionInfo ci;
  HttpRequest request(ci, 43);

  EXPECT_EQ(43, request.messageId());
}

TEST(HttpRequestTest, testEmptyUrl) {
  ConnectionInfo ci;
  HttpRequest request(ci, 1);

  std::string_view url("/");

  request.parseUrl(url.data(), url.size());
  EXPECT_EQ("/", request.fullUrl());
  EXPECT_EQ("/", request.requestUrl());
  EXPECT_TRUE(request.values().empty());
}

TEST(HttpRequestTest, testPathOnlyUrl) {
  ConnectionInfo ci;
  HttpRequest request(ci, 1);

  std::string_view url("/a/foo/bar");

  request.parseUrl(url.data(), url.size());
  EXPECT_EQ("/a/foo/bar", request.fullUrl());
  EXPECT_EQ("/a/foo/bar", request.requestUrl());
  EXPECT_TRUE(request.values().empty());
}

TEST(HttpRequestTest, testDuplicateForwardSlashesInUrl) {
  ConnectionInfo ci;
  HttpRequest request(ci, 1);

  std::string_view url("//a//foo//bar");

  request.parseUrl(url.data(), url.size());
  EXPECT_EQ("/a/foo/bar", request.fullUrl());
  EXPECT_EQ("/a/foo/bar", request.requestUrl());
  EXPECT_TRUE(request.values().empty());
}

TEST(HttpRequestTest, testUrlParameters) {
  ConnectionInfo ci;
  HttpRequest request(ci, 1);

  std::string_view url("/foo/bar/baz?a=1&b=23&c=d&e=foobar&f=baz&bark=quxxxx");

  request.parseUrl(url.data(), url.size());

  EXPECT_EQ("1", request.value("a"));
  EXPECT_EQ("23", request.value("b"));
  EXPECT_EQ("d", request.value("c"));
  EXPECT_EQ("foobar", request.value("e"));
  EXPECT_EQ("baz", request.value("f"));
  EXPECT_EQ("quxxxx", request.value("bark"));
}

TEST(HttpRequestTest, testEmptyUrlParameters) {
  ConnectionInfo ci;
  HttpRequest request(ci, 1);

  std::string_view url("/?a=&b=&c=d&e=foobar&f=");

  request.parseUrl(url.data(), url.size());

  EXPECT_EQ("", request.value("a"));
  EXPECT_EQ("", request.value("b"));
  EXPECT_EQ("d", request.value("c"));
  EXPECT_EQ("foobar", request.value("e"));
  EXPECT_EQ("", request.value("f"));
}

TEST(HttpRequestTest, testUrlEncoding) {
  ConnectionInfo ci;
  HttpRequest request(ci, 1);

  std::string_view url(
      "/foo/bar/"
      "baz?a=%2fa%2eb%2ec&1=abc&foo=foo+bar&bark=%2Fa%5B%5D%3D%3F%26&uff=%09");

  request.parseUrl(url.data(), url.size());

  EXPECT_EQ("/a.b.c", request.value("a"));
  EXPECT_EQ("abc", request.value("1"));
  EXPECT_EQ("foo bar", request.value("foo"));
  EXPECT_EQ("/a[]=?&", request.value("bark"));
  EXPECT_EQ("\x09", request.value("uff"));
}

TEST(HttpRequestTest, testWrongUrlEncoding) {
  ConnectionInfo ci;
  HttpRequest request(ci, 1);

  std::string_view url("/foo/?a=%fg");

  EXPECT_THROW(request.parseUrl(url.data(), url.size()),
               arangodb::basics::Exception);
}
