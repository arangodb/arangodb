////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Basics/Common.h"
#include "Endpoint/Endpoint.h"

namespace arangodb {
class EndpointList {
 public:
  EndpointList(EndpointList const&) = delete;
  EndpointList& operator=(EndpointList const&) = delete;

  EndpointList();
  ~EndpointList();

  static std::string_view encryptionName(Endpoint::EncryptionType);

  bool empty() const { return _endpoints.empty(); }
  bool add(std::string const& specification, int backlogSize,
           bool reuseAddress);
  std::vector<std::string> all() const;
  std::vector<std::string> all(Endpoint::TransportType transport) const;
  void apply(std::function<void(std::string const&, Endpoint&)> const& cb);
  bool hasSsl() const;
  void dump() const;

 private:
  std::map<std::string, std::unique_ptr<Endpoint>> _endpoints;
};
}  // namespace arangodb
