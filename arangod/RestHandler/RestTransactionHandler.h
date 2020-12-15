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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_TRANSACTION_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_TRANSACTION_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "RestHandler/RestVocbaseBaseHandler.h"
#include "Transaction/Status.h"

namespace arangodb {

class V8Context;

class RestTransactionHandler : public arangodb::RestVocbaseBaseHandler {
  V8Context* _v8Context;
  basics::ReadWriteLock _lock;

 public:
  RestTransactionHandler(application_features::ApplicationServer&,
                         GeneralRequest*, GeneralResponse*);

 public:
  char const* name() const override final { return "RestTransactionHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_V8; }
  RestStatus execute() override;
  void cancel() override final;

 protected:
  virtual ResultT<std::pair<std::string, bool>> forwardingTarget() override;

 private:
  void executeGetState();
  void executeBegin();
  void executeCommit();
  void executeAbort();
  void generateTransactionResult(rest::ResponseCode code, TRI_voc_tid_t tid,
                                 transaction::Status status);

  /// start a legacy JS transaction
  void executeJSTransaction();
};
}  // namespace arangodb

#endif
