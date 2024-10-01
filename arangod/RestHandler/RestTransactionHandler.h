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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ReadWriteLock.h"
#include "Cluster/ServerState.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "Transaction/Status.h"

namespace arangodb {

class V8Executor;

class RestTransactionHandler : public arangodb::RestVocbaseBaseHandler {
  V8Executor* _v8Context;
  basics::ReadWriteLock _lock;

 public:
  RestTransactionHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

  char const* name() const override final { return "RestTransactionHandler"; }
  RequestLane lane() const override final;

  RestStatus execute() override;
  void cancel() override final;

 protected:
  virtual ResultT<std::pair<std::string, bool>> forwardingTarget() override;

 private:
  void executeGetState();
  [[nodiscard]] futures::Future<futures::Unit> executeBegin();
  [[nodiscard]] futures::Future<futures::Unit> executeCommit();
  [[nodiscard]] futures::Future<futures::Unit> executeAbort();
  void generateTransactionResult(rest::ResponseCode code, TransactionId tid,
                                 transaction::Status status);

  /// start a legacy JS transaction
  void executeJSTransaction();
};
}  // namespace arangodb
