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

/// url-decodes [src, src+len) into out
void urlDecode(std::string& out, char const* src, size_t len);

/// url-decodes str into out - convenience function
inline void urlDecode(std::string& out, std::string const& str) {
  return urlDecode(out, str.data(), str.size());
}

/// url-decodes str and returns it - convenience function
inline std::string urlDecode(std::string const& str) {
  std::string result;
  urlDecode(result, str.c_str(), str.size());
  return result;
}

/// url-encodes [src, src+len) into out
void urlEncode(std::string& out, char const* src, size_t len);

/// url-encodes str into out - convenience function
inline void urlEncode(std::string& out, std::string const& str) {
  return urlEncode(out, str.data(), str.size());
}

/// url-encodes str and returns it - convenience function
inline std::string urlEncode(std::string const& str) {
  std::string result;
  urlEncode(result, str.c_str(), str.size());
  return result;
}

void appendPath(Request const& req, std::string& target);
}}}}  // namespace arangodb::fuerte::v1::http
#endif
