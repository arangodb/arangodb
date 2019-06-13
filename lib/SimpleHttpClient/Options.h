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
/// @author Andreas Streichardt
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_OPTIONS_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_OPTIONS_H 1

#include <functional>
#include <memory>
#include "curl/curl.h"

namespace arangodb {
namespace communicator {
class Options {
 public:
  double requestTimeout = 120.0;
  double connectionTimeout = 2.0;
#ifdef ARANGODB_USE_GOOGLE_TESTS
  std::shared_ptr<std::function<void(CURLcode)>> _curlRcFn;
#endif
};
}  // namespace communicator
}  // namespace arangodb

#endif
