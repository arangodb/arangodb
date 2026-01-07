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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>

#include "Basics/asio_ns.h"
#include "Ssl/ssl-helper.h"

namespace arangodb {

/// @brief Pure value struct holding all configurable options for SslServer
/// No behavior - just data fields with default values
struct SslServerOptions {
  std::string cafile;
  std::string keyfile;
  std::string cipherList = "HIGH:!EXPORT:!aNULL@STRENGTH";
  uint64_t sslProtocol = TLS_GENERIC;
  uint64_t sslOptions = asio_ns::ssl::context::default_workarounds |
                        asio_ns::ssl::context::single_dh_use;
  std::string ecdhCurve = "prime256v1";
  bool sessionCache = false;
  bool preferHttp11InAlpn = false;
};

}  // namespace arangodb
