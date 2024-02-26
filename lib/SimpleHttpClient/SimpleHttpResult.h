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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/StringBuffer.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>

#include <memory>
#include <string>
#include <unordered_map>

/// @brief class for storing a request result
namespace arangodb::httpclient {

class SimpleHttpResult final {
  SimpleHttpResult(SimpleHttpResult const&) = delete;
  SimpleHttpResult& operator=(SimpleHttpResult const&) = delete;

 public:
  /// @brief result types
  enum class ResultType : uint8_t {
    COMPLETE = 0,
    COULD_NOT_CONNECT,
    WRITE_ERROR,
    READ_ERROR,
    UNKNOWN
  };

  SimpleHttpResult();

  ~SimpleHttpResult();

  /// @brief clear result values
  void clear();

  /// @brief returns whether the response contains an HTTP error
  bool wasHttpError() const noexcept { return (_returnCode >= 400); }

  /// @brief returns the http return code
  int getHttpReturnCode() const noexcept { return _returnCode; }

  /// @brief sets the http return code
  void setHttpReturnCode(int returnCode) noexcept { _returnCode = returnCode; }

  /// @brief returns the http return message
  std::string getHttpReturnMessage() const { return _returnMessage; }

  /// @brief sets the http return message
  void setHttpReturnMessage(std::string const& message) {
    _returnMessage = message;
  }

  void setHttpReturnMessage(std::string&& message) {
    _returnMessage = std::move(message);
  }

  /// @brief whether or not the response contained a content length header
  bool hasContentLength() const noexcept { return _hasContentLength; }

  /// @brief returns the content length
  size_t getContentLength() const noexcept { return _contentLength; }

  /// @brief sets the content length
  void setContentLength(size_t len) noexcept {
    _contentLength = len;
    _hasContentLength = true;
  }

  /// @brief returns the http body
  arangodb::basics::StringBuffer& getBody();

  /// @brief returns the http body
  arangodb::basics::StringBuffer const& getBody() const;

  /// @brief returns the http body as velocypack
  std::shared_ptr<VPackBuilder> getBodyVelocyPack() const;

  rest::EncodingType getEncodingType() const noexcept { return _encodingType; }

  /// @brief returns the request result type
  ResultType getResultType() const noexcept { return _requestResultType; }

  /// @brief returns true if result type == OK
  bool isComplete() const noexcept {
    return _requestResultType == ResultType::COMPLETE;
  }

  /// @brief returns true if "transfer-encoding: chunked"
  bool isChunked() const noexcept { return _chunked; }

  /// @brief sets the request result type
  void setResultType(ResultType requestResultType) noexcept {
    _requestResultType = requestResultType;
  }

  /// @brief add header field
  void addHeaderField(char const* line, size_t length);

  /// @brief return the value of a single header
  std::string getHeaderField(std::string const& name, bool& found) const;

  /// @brief check if a header is present
  bool hasHeaderField(std::string const& name) const;

  /// @brief get all header fields
  std::unordered_map<std::string, std::string> const& getHeaderFields() const {
    return _headerFields;
  }

 private:
  /// @brief add header field
  void addHeaderField(char const* key, size_t keyLength, char const* value,
                      size_t valueLength);

  // header information
  std::string _returnMessage;
  size_t _contentLength;
  int _returnCode;
  rest::EncodingType _encodingType;
  bool _foundHeader;
  bool _hasContentLength;
  bool _chunked;

  // request result type
  ResultType _requestResultType;

  // body content
  arangodb::basics::StringBuffer _resultBody;

  // header fields
  std::unordered_map<std::string, std::string> _headerFields;
};
}  // namespace arangodb::httpclient
