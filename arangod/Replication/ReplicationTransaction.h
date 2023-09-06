////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/AccessMode.h"
#include "VocBase/vocbase.h"

namespace arangodb {

class ReplicationTransaction : public transaction::Methods {
 public:
  /// @brief create the transaction
  explicit ReplicationTransaction(TRI_vocbase_t& vocbase,
                                  transaction::OperationOrigin operationOrigin)
      : transaction::Methods(
            transaction::StandaloneContext::create(vocbase, operationOrigin),
            transaction::Options::replicationDefaults()),
        _guard(vocbase) {
    TRI_ASSERT(state() != nullptr);
    state()->setExclusiveAccessType();
  }

 private:
  DatabaseGuard _guard;
};

}  // namespace arangodb
