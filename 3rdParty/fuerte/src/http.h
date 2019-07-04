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
/// @author Jan Christoph Uhde
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_HTTP
#define ARANGO_CXX_DRIVER_HTTP

#include <fuerte/message.h>
#include <fuerte/types.h>
#include <string>

namespace arangodb { namespace fuerte { inline namespace v1 { namespace http {

// in-flight request data
struct RequestItem {
  /// ID of this message
  MessageID _messageID;
  /// Reference to the request we're processing
  std::unique_ptr<arangodb::fuerte::v1::Request> _request;
  /// response data, may be null before response header is received
  std::unique_ptr<arangodb::fuerte::v1::Response> _response;
  /// Callback for when request is done (in error or succeeded)
  RequestCallback _callback;

  // buffer for the request header, reset after request was send
  std::string _requestHeader;
  /// response buffer, moved after writing
  velocypack::Buffer<uint8_t> _responseBuffer;

  // parser state
  std::string lastHeaderField;
  std::string lastHeaderValue;
  bool last_header_was_a_value = false;
  bool should_keep_alive = false;
  bool message_complete = false;

  inline MessageID messageID() { return _messageID; }
  inline void invokeOnError(Error e) {
    _callback(e, std::move(_request), nullptr);
  }
};

std::string urlDecode(std::string const& str);
std::string urlEncode(char const* src, size_t const len);
std::string urlEncode(char const* src);

inline std::string urlEncode(std::string const& str) {
  return urlEncode(str.c_str(), str.size());
}
}}}}  // namespace arangodb::fuerte::v1::http
#endif
