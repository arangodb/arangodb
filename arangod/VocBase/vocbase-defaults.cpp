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

#include "vocbase-defaults.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief apply default settings
////////////////////////////////////////////////////////////////////////////////

void TRI_vocbase_defaults_t::applyToVocBase(TRI_vocbase_t* vocbase) const {
  vocbase->_settings.defaultMaximalSize = defaultMaximalSize;
  vocbase->_settings.defaultWaitForSync = defaultWaitForSync;
  vocbase->_settings.requireAuthentication = requireAuthentication;
  vocbase->_settings.requireAuthenticationUnixSockets =
      requireAuthenticationUnixSockets;
  vocbase->_settings.authenticateSystemOnly = authenticateSystemOnly;
  vocbase->_settings.forceSyncProperties = forceSyncProperties;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert defaults into a VelocyPack array
////////////////////////////////////////////////////////////////////////////////

void TRI_vocbase_defaults_t::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(!builder.isClosed());

  builder.add("waitForSync", VPackValue(defaultWaitForSync));
  builder.add("requireAuthentication", VPackValue(requireAuthentication));
  builder.add("requireAuthenticationUnixSockets",
              VPackValue(requireAuthenticationUnixSockets));
  builder.add("authenticateSystemOnly", VPackValue(authenticateSystemOnly));
  builder.add("forceSyncProperties", VPackValue(forceSyncProperties));
  builder.add("defaultMaximalSize", VPackValue(defaultMaximalSize));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert defaults into a VelocyPack array
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> TRI_vocbase_defaults_t::toVelocyPack() const {
  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();
  toVelocyPack(*builder);
  builder->close();
  return builder;
}
