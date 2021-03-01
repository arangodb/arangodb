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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_GENERAL_RESPONSE_H
#define ARANGODB_REST_GENERAL_RESPONSE_H 1

#include "Basics/Common.h"

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/voc-errors.h"
#include "Endpoint/Endpoint.h"
#include "GeneralRequest.h"
#include "Logger/Logger.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Options.h>

namespace arangodb {
namespace velocypack {
class Slice;
}  // namespace velocypack

using rest::ContentType;
using rest::EncodingType;
using rest::ResponseCode;

class GeneralRequest;

class GeneralResponse {
  GeneralResponse() = delete;
  GeneralResponse(GeneralResponse const&) = delete;
  GeneralResponse& operator=(GeneralResponse const&) = delete;

 public:
  // converts the response code to a string for delivering to a http client.
  static std::string responseString(ResponseCode);

  // returns true if code will become a valid http response.
  static bool isValidResponseCode(uint64_t code);

  // converts the response code string to the internal code
  static ResponseCode responseCode(std::string const& str);

  // response code from integer error code
  static ResponseCode responseCode(ErrorCode code);

  /// @brief set content-type this sets the contnt type like you expect it
  void setContentType(ContentType type) { _contentType = type; }
  ContentType contentType() const { return _contentType; }

  /// @brief set content-type from a string. this should only be used in
  /// cases when the content-type is user-defined
  /// this is a functionality so that user can set a type like application/zip
  /// from java script code the ContentType will be CUSTOM!!
  void setContentType(std::string const& contentType) {
    _headers[arangodb::StaticStrings::ContentTypeHeader] = contentType;
    _contentType = ContentType::CUSTOM;
  }

  void setContentType(std::string&& contentType) {
    _headers[arangodb::StaticStrings::ContentTypeHeader] = std::move(contentType);
    _contentType = ContentType::CUSTOM;
  }

  void setAllowCompression(bool allowed) { _allowCompression = allowed; }
  virtual bool isCompressionAllowed() { return _allowCompression; }

  void setContentTypeRequested(ContentType type) {
    _contentTypeRequested = type;
  }
  ContentType contentTypeRequested() const { return _contentTypeRequested; }

  virtual arangodb::Endpoint::TransportType transportType() = 0;

 protected:
  explicit GeneralResponse(ResponseCode, uint64_t mid);

 public:
  virtual ~GeneralResponse() = default;

 public:
  // response codes are http response codes, but they are used in other
  // protocols as well
  ResponseCode responseCode() const { return _responseCode; }
  void setResponseCode(ResponseCode responseCode) {
    _responseCode = responseCode;
  }

  void setHeaders(std::unordered_map<std::string, std::string> headers) {
    _headers = std::move(headers);
  }

  std::unordered_map<std::string, std::string> const& headers() const {
    return _headers;
  }

  // adds a header. the header field name will be lower-cased
  void setHeader(std::string const& key, std::string const& value) {
    _headers.insert_or_assign(basics::StringUtils::tolower(key), value);
  }

  // adds a header. the header field name must be lower-cased
  void setHeaderNC(std::string const& key, std::string value) {
    _headers.insert_or_assign(key, std::move(value));
  }

  // adds a header if not set. the header field name must be lower-cased
  void setHeaderNCIfNotSet(std::string const& key, std::string const& value) {
    _headers.emplace(key, value);
  }

  virtual bool isResponseEmpty() const = 0;

 public:
  uint64_t messageId() const { return _messageId; }
  void setMessageId(uint64_t msgId) { _messageId = msgId; }

  virtual void reset(ResponseCode) = 0;

  // Payload needs to be of type: VPackSlice const&
  // or VPackBuffer<uint8_t>&&
  template <typename Payload>
  void setPayload(Payload&& payload,
                  velocypack::Options const& options = velocypack::Options::Defaults,
                  bool resolveExternals = true) {
    TRI_ASSERT(isResponseEmpty());
    addPayload(std::forward<Payload>(payload), &options, resolveExternals);
  }

  virtual void addPayload(velocypack::Slice const&, arangodb::velocypack::Options const* = nullptr,
                          bool resolveExternals = true) = 0;
  virtual void addPayload(velocypack::Buffer<uint8_t>&&,
                          arangodb::velocypack::Options const* = nullptr,
                          bool resolveExternals = true) = 0;
  virtual void addRawPayload(velocypack::StringRef payload) = 0;
  virtual ErrorCode reservePayload(std::size_t size) { return TRI_ERROR_NO_ERROR; }

  /// used for head
  bool generateBody() const { return _generateBody; }

  /// used for head-responses
  bool setGenerateBody(bool generateBody) {
    return _generateBody = generateBody;
  }
  
  virtual ErrorCode deflate(size_t size = 16384) = 0;

 protected:
  std::unordered_map<std::string, std::string> _headers;  // headers/metadata map
  uint64_t _messageId;                                    // message ID
  ResponseCode _responseCode;                             // http response code
  ContentType _contentType;
  ContentType _contentTypeRequested;
  bool _generateBody;
  bool _allowCompression;
};
}  // namespace arangodb

#endif
