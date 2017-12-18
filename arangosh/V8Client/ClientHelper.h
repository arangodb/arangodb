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
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8CLIENT_CLIENT_HELPER_H
#define ARANGODB_V8CLIENT_CLIENT_HELPER_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

class ClientHelper {
 public:
  typedef std::unique_ptr<httpclient::SimpleHttpClient> ClientPtr;

 public:
  ClientHelper();

 public:
  static std::string rewriteLocation(void* data, std::string const& location);
  static std::string getHttpErrorMessage(httpclient::SimpleHttpResult*,
                                         int* err);

 public:
  std::unique_ptr<httpclient::SimpleHttpClient> getConnectedClient(
      bool force = false, bool verbose = false);
  bool getArangoIsCluster(int* err);
  bool getArangoIsUsingEngine(int* err, std::string const& name);
};
}  // namespace arangodb

#endif
