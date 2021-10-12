////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ITransactionable.h"

#include "Logger/LogMacros.h"

using namespace arangodb;

void Transactionable::updateStatus(transaction::Status status) noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_status != transaction::Status::CREATED && _status != transaction::Status::RUNNING) {
    LOG_TOPIC("257ea", ERR, Logger::FIXME)
        << "trying to update transaction status with "
           "an invalid state. current: "
        << _status << ", future: " << status;
  }
#endif

  TRI_ASSERT(_status == transaction::Status::CREATED ||
             _status == transaction::Status::RUNNING);

  if (_status == transaction::Status::CREATED) {
    TRI_ASSERT(status == transaction::Status::RUNNING ||
               status == transaction::Status::ABORTED);
  } else if (_status == transaction::Status::RUNNING) {
    TRI_ASSERT(status == transaction::Status::COMMITTED ||
               status == transaction::Status::ABORTED);
  }

  _status = status;
}

Transactionable::~Transactionable() {
  TRI_ASSERT(_status != transaction::Status::RUNNING);
}
