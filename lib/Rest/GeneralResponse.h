////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "GeneralRequest.h"
#include "lib/Endpoint/Endpoint.h"

#include "CommonDefines.h"
namespace arangodb {
namespace velocypack {
struct Options;
class Slice;
}

using rest::ContentType;
using rest::ConnectionType;
using rest::ResponseCode;

class GeneralRequest;

class GeneralResponse {
  GeneralResponse() = delete;
  GeneralResponse(GeneralResponse const&) = delete;
  GeneralResponse& operator=(GeneralResponse const&) = delete;

 public:
  // converts the response code to a string for delivering to a http client.
  static std::string responseString(ResponseCode);

  // converts the response code string to the internal code
  static ResponseCode responseCode(std::string const& str);

  // response code from integer error code
  static ResponseCode responseCode(int);

  /// @brief set content-type this sets the contnt type like you expect it
  void setContentType(ContentType type) { _contentType = type; }

  /// @brief set content-type from a string. this should only be used in
  /// cases when the content-type is user-defined
  /// this is a functionality so that user can set a type like application/zip
  /// from java script code the ContentType will be CUSTOM!!
  void setContentType(std::string const& contentType) {
    _headers[arangodb::StaticStrings::ContentTypeHeader] = contentType;
    _contentType = ContentType::CUSTOM;
  }

  void setContentType(std::string&& contentType) {
    _headers[arangodb::StaticStrings::ContentTypeHeader] =
        std::move(contentType);
    _contentType = ContentType::CUSTOM;
  }

  void setConnectionType(ConnectionType type) { _connectionType = type; }

  virtual arangodb::Endpoint::TransportType transportType() = 0;

 protected:
  explicit GeneralResponse(ResponseCode);

 public:
  virtual ~GeneralResponse() {}

 public:
  // response codes are http response codes, but they are used in other
  // protocols as well
  ResponseCode responseCode() const { return _responseCode; }
  void setResponseCode(ResponseCode responseCode) {
    _responseCode = responseCode;
  }

  std::unordered_map<std::string, std::string> headers() const {
    return _headers;
  }

  // adds a header. the header field name will be lower-cased
  void setHeader(std::string const& key, std::string const& value) {
    _headers[basics::StringUtils::tolower(key)] = value;
  }

  // adds a header. the header field name must be lower-cased
  void setHeaderNC(std::string const& key, std::string const& value) {
    _headers[key] = value;
  }

  // adds a header. the header field name must be lower-cased
  void setHeaderNC(std::string const& key, std::string&& value) {
    _headers[key] = std::move(value);
  }

 public:
  virtual uint64_t messageId() const { return 1; }

  virtual void reset(ResponseCode) = 0;

  // generates the response body, sets the content type; this might
  // throw an error
  virtual void setPayload(arangodb::velocypack::Slice const&,
                          bool generateBody = true,
                          arangodb::velocypack::Options const& =
                              arangodb::velocypack::Options::Defaults) = 0;

  virtual void addPayload(VPackSlice const& slice) {
    // this is slower as it has an extra copy!!
    _vpackPayloads.emplace_back(slice.byteSize());
    std::memcpy(&_vpackPayloads.back(), slice.start(), slice.byteSize());
  };

  virtual void addPayload(VPackBuffer<uint8_t>&& buffer) {
    _vpackPayloads.push_back(std::move(buffer));
  };

  void setOptions(VPackOptions options) { _options = std::move(options); };

 protected:
  ResponseCode _responseCode;  // http response code
  std::unordered_map<std::string, std::string>
      _headers;  // headers/metadata map

  std::vector<VPackBuffer<uint8_t>> _vpackPayloads;
  ContentType _contentType;
  ConnectionType _connectionType;
  velocypack::Options _options;
};
}

#endif
