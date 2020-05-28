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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ENDPOINT_ENDPOINT_LIST_H
#define ARANGODB_ENDPOINT_ENDPOINT_LIST_H 1

#include <string>
#include <vector>

#include "Basics/Common.h"

#include "Endpoint/Endpoint.h"
#include <map>

namespace arangodb {
class EndpointList {
 public:
  EndpointList();
  ~EndpointList();

 public:
  static std::string encryptionName(Endpoint::EncryptionType);

 public:
  bool empty() const { return _endpoints.empty(); }
  bool add(std::string const&, int, bool);
  bool remove(std::string const&, Endpoint**);
  std::vector<std::string> all() const;
  std::vector<std::string> all(Endpoint::TransportType transport) const;
  std::map<std::string, Endpoint*> allEndpoints() const { return _endpoints; }
  bool hasSsl() const;
  void dump() const;

 private:
  std::map<std::string, Endpoint*> _endpoints;
};
}  // namespace arangodb

#endif
