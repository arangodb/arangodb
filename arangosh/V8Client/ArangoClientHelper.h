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

#ifndef ARANGODB_V8CLIENT_ARANGO_CLIENT_HELPER_H
#define ARANGODB_V8CLIENT_ARANGO_CLIENT_HELPER_H 1

#include <memory>
#include <string>

#include "Basics/Common.h"
#include "SimpleHttpClient/SimpleHttpClient.h"

namespace arangodb {
namespace httpclient {
class GeneralClientConnection;
class SimpleHttpResult;
}  // namespace httpclient

class ArangoClientHelper {
 public:
  ArangoClientHelper();

 public:
  static std::string rewriteLocation(void* data, std::string const& location);

 public:
  std::string getHttpErrorMessage(httpclient::SimpleHttpResult*, int* err);
  bool getArangoIsCluster(int* err);

 protected:
  std::unique_ptr<httpclient::SimpleHttpClient> _httpClient;
};
}  // namespace arangodb

#endif
