////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ARANGODB_MOCK_TRX_MANAGER
#define ARANGODB_MOCK_TRX_MANAGER

#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/ManagerFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Mocks/Servers.h"

namespace arangodb {
namespace tests {
namespace mocks {
  
  // -----------------------------------------------------------------------------
  // --SECTION--                                                 setup / tear-down
  // -----------------------------------------------------------------------------
  
  struct TransactionManagerSetup {
    arangodb::tests::mocks::MockAqlServer server;

    TransactionManagerSetup() : server(false) {
      server.addFeature<transaction::ManagerFeature>(true);
      server.startFeatures();
    }

    ~TransactionManagerSetup() = default;
  };

} // namespace tests
} // namespace mocks
} // namespace arangodb
#endif
