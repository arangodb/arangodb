////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Utils/RequestCoding.h"

#include "Basics/EncodingUtils.h"
#include "Basics/FileUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"

#include <fuerte/message.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <string>

#include <unistd.h>

using namespace arangodb;
using namespace arangodb::request_coding;
namespace fu = arangodb::fuerte;

namespace {

std::string payloadOf(fu::Message const& message) {
  auto const payload = message.payload();
  return {static_cast<char const*>(payload.data()), payload.size()};
}

std::string inflated(std::string const& deflated) {
  velocypack::Buffer<uint8_t> out;
  EXPECT_EQ(
      TRI_ERROR_NO_ERROR,
      encoding::zlibInflate(reinterpret_cast<uint8_t const*>(deflated.data()),
                            deflated.size(), out));
  return {reinterpret_cast<char const*>(out.data()), out.size()};
}

std::string const kCompressible(1000, 'a');

std::string incompressible(std::size_t size) {
  std::mt19937 random(42);
  std::string data(size, '\0');
  for (auto& c : data) {
    c = static_cast<char>(random());
  }
  return data;
}

void translate(fu::Request& request, std::string_view location,
               bool forceJson = false,
               std::unordered_map<std::string, std::string> const& headers = {},
               bool requestCompression = false) {
  translateHeaders(request, fu::RestVerb::Post, location, "db", forceJson,
                   std::chrono::milliseconds(1234), headers,
                   requestCompression);
}

}  // namespace

TEST(RequestCodingHeadersTest, addresses_the_database_and_path) {
  fu::Request request;
  translate(request, "/_api/document/c?waitForSync=true");

  EXPECT_EQ(fu::RestVerb::Post, request.header.restVerb);
  EXPECT_EQ("db", request.header.database);
  EXPECT_EQ("/_api/document/c", request.header.path);
  EXPECT_EQ("true", request.header.parameters.at("waitForSync"));
  EXPECT_EQ(std::chrono::milliseconds(1234), request.timeout());
}

TEST(RequestCodingHeadersTest, accepts_velocypack_by_default) {
  fu::Request request;
  translate(request, "/_api/version");

  EXPECT_EQ(fu::ContentType::VPack, request.header.acceptType());
  EXPECT_EQ(fu::ContentType::Unset, request.header.contentType());
}

TEST(RequestCodingHeadersTest, forced_json_is_overridden_by_header_fields) {
  fu::Request request;
  translate(request, "/_api/version", /*forceJson*/ true,
            {{"Content-Type", "text/plain"}});

  EXPECT_EQ(fu::ContentType::Text, request.header.contentType());
  EXPECT_EQ(fu::ContentType::Json, request.header.acceptType());
}

TEST(RequestCodingHeadersTest, lowercases_header_fields) {
  fu::Request request;
  translate(request, "/_api/version", false, {{"X-Arango-Async", "store"}});

  EXPECT_EQ("store", request.header.metaByKey("x-arango-async"));
}

TEST(RequestCodingHeadersTest, asks_for_deflate_unless_an_encoding_is_given) {
  fu::Request compressed;
  translate(compressed, "/_api/version", false, {}, /*compression*/ true);
  EXPECT_EQ(StaticStrings::EncodingDeflate,
            compressed.header.metaByKey(StaticStrings::AcceptEncoding));

  fu::Request given;
  translate(given, "/_api/version", false,
            {{StaticStrings::AcceptEncoding, "gzip"}}, /*compression*/ true);
  EXPECT_EQ("gzip", given.header.metaByKey(StaticStrings::AcceptEncoding));

  fu::Request uncompressed;
  translate(uncompressed, "/_api/version");
  EXPECT_FALSE(
      uncompressed.header.meta().contains(StaticStrings::AcceptEncoding));
}

TEST(RequestCodingBodyTest, no_body_defaults_to_json) {
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, NoBody{}, false, 0).ok());

  EXPECT_EQ(0u, request.payloadSize());
  EXPECT_EQ(fu::ContentType::Json, request.header.contentType());
}

TEST(RequestCodingBodyTest, no_body_keeps_a_given_content_type) {
  fu::Request request;
  request.header.contentType(fu::ContentType::Text);
  ASSERT_TRUE(addRequestBody(request, NoBody{}, false, 0).ok());

  EXPECT_EQ(fu::ContentType::Text, request.header.contentType());
}

TEST(RequestCodingBodyTest, json_is_sent_as_is) {
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, JsonBody{R"({"a":1})"}, false, 0).ok());

  EXPECT_EQ(R"({"a":1})", payloadOf(request));
  EXPECT_EQ(fu::ContentType::Json, request.header.contentType());
  EXPECT_FALSE(request.header.meta().contains(StaticStrings::ContentEncoding));
}

TEST(RequestCodingBodyTest, binary_sets_no_content_type) {
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, BinaryBody{"\x01\x02"}, false, 0).ok());

  EXPECT_EQ("\x01\x02", payloadOf(request));
  EXPECT_EQ(fu::ContentType::Unset, request.header.contentType());
}

TEST(RequestCodingBodyTest, value_is_sent_as_velocypack) {
  auto const value = velocypack::Parser::fromJson(R"({"a":1})");
  fu::Request request;
  ASSERT_TRUE(
      addRequestBody(request, ValueBody{value->slice()}, false, 0).ok());

  EXPECT_EQ(fu::ContentType::VPack, request.header.contentType());
  ASSERT_EQ(value->slice().byteSize(), request.payloadSize());
  EXPECT_TRUE(request.slices().front().binaryEquals(value->slice()));
}

TEST(RequestCodingBodyTest, value_is_sent_as_json_if_forced) {
  auto const value = velocypack::Parser::fromJson(R"({"a":1})");
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, ValueBody{value->slice()},
                             /*forceJson*/ true, 0)
                  .ok());

  EXPECT_EQ(fu::ContentType::Json, request.header.contentType());
  EXPECT_EQ(R"({"a":1})", payloadOf(request));
}

TEST(RequestCodingBodyTest, deflates_a_body_from_the_threshold_on) {
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, JsonBody{kCompressible}, false,
                             /*threshold*/ kCompressible.size())
                  .ok());

  EXPECT_EQ(StaticStrings::EncodingDeflate,
            request.header.metaByKey(StaticStrings::ContentEncoding));
  EXPECT_LT(request.payloadSize(), kCompressible.size());
  EXPECT_EQ(kCompressible, inflated(payloadOf(request)));
}

TEST(RequestCodingBodyTest, deflates_values) {
  auto const value =
      velocypack::Parser::fromJson(R"({"text":")" + kCompressible + R"("})");
  fu::Request request;
  ASSERT_TRUE(
      addRequestBody(request, ValueBody{value->slice()}, false, 1).ok());

  EXPECT_EQ(StaticStrings::EncodingDeflate,
            request.header.metaByKey(StaticStrings::ContentEncoding));
  EXPECT_EQ(fu::ContentType::VPack, request.header.contentType());
  auto const vpack = inflated(payloadOf(request));
  EXPECT_TRUE(velocypack::Slice(reinterpret_cast<uint8_t const*>(vpack.data()))
                  .binaryEquals(value->slice()));
}

TEST(RequestCodingBodyTest, keeps_a_body_below_the_threshold) {
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, JsonBody{kCompressible}, false,
                             kCompressible.size() + 1)
                  .ok());

  EXPECT_EQ(kCompressible, payloadOf(request));
  EXPECT_FALSE(request.header.meta().contains(StaticStrings::ContentEncoding));
}

TEST(RequestCodingBodyTest, threshold_zero_disables_compression) {
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, JsonBody{kCompressible}, false, 0).ok());

  EXPECT_EQ(kCompressible, payloadOf(request));
}

TEST(RequestCodingBodyTest, keeps_a_body_that_has_an_encoding) {
  fu::Request request;
  request.header.addMeta(StaticStrings::ContentEncoding, "gzip");
  ASSERT_TRUE(addRequestBody(request, JsonBody{kCompressible}, false, 1).ok());

  EXPECT_EQ(kCompressible, payloadOf(request));
  EXPECT_EQ("gzip", request.header.metaByKey(StaticStrings::ContentEncoding));
}

TEST(RequestCodingBodyTest, keeps_a_body_that_does_not_shrink) {
  auto const data = incompressible(1000);
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, BinaryBody{data}, false, 1).ok());

  EXPECT_EQ(data, payloadOf(request));
  EXPECT_FALSE(request.header.meta().contains(StaticStrings::ContentEncoding));
}

class RequestCodingFileBodyTest : public ::testing::Test {
 protected:
  void TearDown() override { std::filesystem::remove(path); }

  std::string const path = (std::filesystem::temp_directory_path() /
                            ("RequestCodingTest-" + std::to_string(::getpid())))
                               .string();
};

TEST_F(RequestCodingFileBodyTest, sends_the_file_uncompressed) {
  basics::FileUtils::spit(path, kCompressible);
  fu::Request request;
  ASSERT_TRUE(addRequestBody(request, FileBody{path}, false, 1).ok());

  EXPECT_EQ(kCompressible, payloadOf(request));
  EXPECT_EQ(fu::ContentType::Custom, request.header.contentType());
  EXPECT_FALSE(request.header.meta().contains(StaticStrings::ContentEncoding));
}

TEST_F(RequestCodingFileBodyTest, fails_for_a_missing_file) {
  fu::Request request;
  auto const result = addRequestBody(request, FileBody{path}, false, 0);

  EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, result.errorNumber());
  EXPECT_EQ(0u, request.payloadSize());
}

namespace {

std::unique_ptr<fu::Response> responseWith(std::string const& payload,
                                           fu::ContentEncoding encoding,
                                           std::string const& encodingHeader) {
  fu::ResponseHeader header;
  header.contentEncoding(encoding);
  header.addMeta(StaticStrings::ContentEncoding, encodingHeader);
  auto response = std::make_unique<fu::Response>(std::move(header));
  velocypack::Buffer<uint8_t> buffer;
  buffer.append(payload.data(), payload.size());
  response->setPayload(std::move(buffer), 0);
  return response;
}

}  // namespace

TEST(RequestCodingResponseTest, inflates_deflate) {
  velocypack::Buffer<uint8_t> deflated;
  ASSERT_EQ(TRI_ERROR_NO_ERROR,
            encoding::zlibDeflate(
                reinterpret_cast<uint8_t const*>(kCompressible.data()),
                kCompressible.size(), deflated));
  auto response = responseWith(
      std::string(reinterpret_cast<char const*>(deflated.data()),
                  deflated.size()),
      fu::ContentEncoding::Deflate, StaticStrings::EncodingDeflate);

  ASSERT_EQ(TRI_ERROR_NO_ERROR, uncompressResponse(*response));
  EXPECT_EQ(kCompressible, payloadOf(*response));
  EXPECT_EQ(fu::ContentEncoding::Identity, response->contentEncoding());
  EXPECT_FALSE(
      response->header.meta().contains(StaticStrings::ContentEncoding));
}

TEST(RequestCodingResponseTest, inflates_gzip) {
  velocypack::Buffer<uint8_t> gzipped;
  ASSERT_EQ(TRI_ERROR_NO_ERROR,
            encoding::gzipCompress(
                reinterpret_cast<uint8_t const*>(kCompressible.data()),
                kCompressible.size(), gzipped));
  auto response =
      responseWith(std::string(reinterpret_cast<char const*>(gzipped.data()),
                               gzipped.size()),
                   fu::ContentEncoding::Gzip, StaticStrings::EncodingGzip);

  ASSERT_EQ(TRI_ERROR_NO_ERROR, uncompressResponse(*response));
  EXPECT_EQ(kCompressible, payloadOf(*response));
  EXPECT_EQ(fu::ContentEncoding::Identity, response->contentEncoding());
}

TEST(RequestCodingResponseTest, leaves_an_identity_payload_alone) {
  auto response =
      responseWith("plain", fu::ContentEncoding::Identity, "identity");

  ASSERT_EQ(TRI_ERROR_NO_ERROR, uncompressResponse(*response));
  EXPECT_EQ("plain", payloadOf(*response));
}

TEST(RequestCodingResponseTest, fails_for_a_corrupt_payload) {
  auto response = responseWith("not deflated", fu::ContentEncoding::Deflate,
                               StaticStrings::EncodingDeflate);

  EXPECT_NE(TRI_ERROR_NO_ERROR, uncompressResponse(*response));
}

TEST(RequestCodingErrorTest, maps_fuerte_errors) {
  EXPECT_EQ(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
            fuerteToArangoErrorCode(fu::Error::CouldNotConnect));
  EXPECT_EQ(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
            fuerteToArangoErrorCode(fu::Error::ConnectionClosed));
  EXPECT_EQ(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ,
            fuerteToArangoErrorCode(fu::Error::ReadError));
  EXPECT_EQ(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE,
            fuerteToArangoErrorCode(fu::Error::WriteError));
  EXPECT_EQ(TRI_ERROR_HTTP_REQUEST_TIMEOUT,
            fuerteToArangoErrorCode(fu::Error::RequestTimeout));
  EXPECT_EQ(TRI_ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR,
            fuerteToArangoErrorCode(fu::Error::ProtocolError));
}
