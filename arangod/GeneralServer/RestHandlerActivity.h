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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Activities/GuardedActivity.h"
#include "Endpoint/ConnectionInfo.h"

#include <string>

namespace arangodb::rest {

struct RestHandlerActivityData {
  std::string handler;
  std::string url;
  std::string method;
  std::unordered_map<std::string, std::string> headers;
  ConnectionInfo connectionInfo;

  template<typename Inspector>
  inline friend auto inspect(Inspector& f, RestHandlerActivityData& d) {
    return f.object(d).fields(
        f.field("handler", d.handler),                             //
        f.field("url", d.url),                                     //
        f.field("method", d.method),                               //
        f.field("headers", d.headers),                             //
        f.field("serverAddress", d.connectionInfo.serverAddress),  //
        f.field("serverPort", d.connectionInfo.serverPort),        //
        f.field("clientAddress", d.connectionInfo.clientAddress),  //
        f.field("clientPort", d.connectionInfo.clientPort)         //
    );                                                             //
  }
};

struct RestHandlerActivity
    : activities::GuardedActivity<RestHandlerActivity,
                                  RestHandlerActivityData> {
  RestHandlerActivity(activities::ActivityId id,
                      activities::ActivityHandle parent,
                      RestHandlerActivityData data)
      : activities::GuardedActivity<RestHandlerActivity,
                                    RestHandlerActivityData>(
            id, parent, "RestHandler", std::move(data)) {}
  using Data = RestHandlerActivityData;
};
}  // namespace arangodb::rest
