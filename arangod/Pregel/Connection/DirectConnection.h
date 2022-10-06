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

#include "Pregel/Connection/Connection.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel {

struct DirectConnection : Connection {
  DirectConnection(PregelFeature& feature, TRI_vocbase_t& vocbase)
      : _feature{feature}, _vocbaseGuard{vocbase} {}
  auto post(Destination const& destination, ModernMessage&& message) const
      -> futures::Future<Result> override;
  [[deprecated("Use new post function")]] auto send(
      Destination const& destination, ModernMessage&& message) const
      -> futures::Future<ResultT<ModernMessage>> override;
  ~DirectConnection() = default;

 private:
  PregelFeature& _feature;
  const DatabaseGuard _vocbaseGuard;
};

}  // namespace arangodb::pregel
