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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ACTIONS_REST_ACTION_HANDLER_H
#define ARANGOD_ACTIONS_REST_ACTION_HANDLER_H 1

#include "RestHandler/RestVocbaseBaseHandler.h"

#include "Actions/actions.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
class RestActionHandler : public RestVocbaseBaseHandler {
 public:
  RestActionHandler(application_features::ApplicationServer&, GeneralRequest*,
                    GeneralResponse*);

 public:
  char const* name() const override final { return "RestActionHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_V8; }
  RestStatus execute() override;
  void cancel() override;

 private:
  // executes an action
  void executeAction();

 protected:
  // action
  std::shared_ptr<TRI_action_t> _action;

  // data lock
  Mutex _dataLock;

  // data for cancelation
  void* _data;
};
}  // namespace arangodb

#endif
