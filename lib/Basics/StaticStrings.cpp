////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "StaticStrings.h"

using namespace arangodb;

// constants
std::string const StaticStrings::N1800("1800");

// system attribute names
std::string const StaticStrings::IdString("_id");
std::string const StaticStrings::KeyString("_key");
std::string const StaticStrings::RevString("_rev");
std::string const StaticStrings::FromString("_from");
std::string const StaticStrings::ToString("_to");

// HTTP headers
std::string const StaticStrings::Accept("accept");
std::string const StaticStrings::AccessControlAllowCredentials("access-control-allow-credentials");
std::string const StaticStrings::AccessControlAllowHeaders("access-control-allow-headers");
std::string const StaticStrings::AccessControlAllowMethods("access-control-allow-methods");
std::string const StaticStrings::AccessControlAllowOrigin("access-control-allow-origin");
std::string const StaticStrings::AccessControlExposeHeaders("access-control-expose-headers");
std::string const StaticStrings::AccessControlMaxAge("access-control-max-age");
std::string const StaticStrings::AccessControlRequestHeaders("access-control-request-headers");
std::string const StaticStrings::Allow("allow");
std::string const StaticStrings::Close("Close");
std::string const StaticStrings::Connection("connection");
std::string const StaticStrings::ContentTypeHeader("content-type");
std::string const StaticStrings::KeepAlive("Keep-Alive");
std::string const StaticStrings::Location("location");
std::string const StaticStrings::WwwAuthenticate("www-authenticate");

// mime types
std::string const StaticStrings::MimeTypeJson("application/json; charset=utf-8");
std::string const StaticStrings::MimeTypeVPack("application/x-velocypack");

