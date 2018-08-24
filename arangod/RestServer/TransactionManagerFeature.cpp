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

#include "TransactionManagerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionManager.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

std::unique_ptr<TransactionManager> TransactionManagerFeature::MANAGER;

TransactionManagerFeature::TransactionManagerFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "TransactionManager") {
  setOptional(false);
  startsAfter("BasicsPhase");

  startsAfter("EngineSelector");
}

void TransactionManagerFeature::prepare() {
  TRI_ASSERT(MANAGER == nullptr);
  TRI_ASSERT(EngineSelectorFeature::ENGINE != nullptr);
  MANAGER = EngineSelectorFeature::ENGINE->createTransactionManager();
}

void TransactionManagerFeature::unprepare() {
  MANAGER.reset();
}

} // arangodb
