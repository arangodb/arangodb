////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_KERBEROS_CLIENT_H
#define ARANGOD_KERBEROS_CLIENT_H 1

#ifndef _WIN32
#include <netinet/in.h>

#define TO_GSS_BUFFER(stdstring) {                                      \
  stdstring.length(),                                                   \
    const_cast<void*>(static_cast<const void*>(stdstring.c_str()))      \
  }

namespace arangodb {

std::string gssApiError(uint32_t maj, uint32_t min);
std::string getKerberosBase64Token(std::string const& service, std::string& error, sockaddr_in* source, sockaddr_in* dest);
}
#endif
#endif
