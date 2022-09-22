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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/ResultT.h"
#include "Cluster/ClusterTypes.h"
#include "Futures/Future.h"
#include "Pregel/WorkerConductorMessages.h"

namespace arangodb::pregel {

// This struct wraps the destination ServerID and the type of destination
// both properties are needed in a REST call
struct Destination {
  enum Type { server, shard };

 private:
  Type _type;
  const char* typeString[2] = {"server", "shard"};

 public:
  ServerID _id;
  Destination(Type type, ServerID id) : _type{type}, _id{std::move(id)} {}
  auto toString() const -> std::string;
};

struct Connection {
  virtual auto send(Destination const& destination,
                    ModernMessage&& message) const
      -> futures::Future<ResultT<ModernMessage>> = 0;
  virtual ~Connection() = default;
};
}  // namespace arangodb::pregel
