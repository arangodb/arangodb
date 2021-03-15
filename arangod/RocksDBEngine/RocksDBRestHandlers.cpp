////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBRestHandlers.h"

#include "Aql/QueryRegistry.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RocksDBEngine/RocksDBRestCollectionHandler.h"
#include "RocksDBEngine/RocksDBRestExportHandler.h"
#include "RocksDBEngine/RocksDBRestReplicationHandler.h"
#include "RocksDBEngine/RocksDBRestWalHandler.h"

using namespace arangodb;

void RocksDBRestHandlers::registerResources(rest::RestHandlerFactory* handlerFactory) {
  handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::COLLECTION_PATH,
                                   RestHandlerCreator<RocksDBRestCollectionHandler>::createNoData);
  auto queryRegistry = QueryRegistryFeature::registry();
  handlerFactory->addPrefixHandler(
      "/_api/export",
      RestHandlerCreator<RocksDBRestExportHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);
  handlerFactory->addPrefixHandler("/_api/replication",
                                   RestHandlerCreator<RocksDBRestReplicationHandler>::createNoData);
  handlerFactory->addPrefixHandler("/_admin/wal",
                                   RestHandlerCreator<RocksDBRestWalHandler>::createNoData);
}
