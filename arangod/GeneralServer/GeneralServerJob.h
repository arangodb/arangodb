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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_HTTP_SERVER_JOB_H
#define ARANGOD_HTTP_SERVER_HTTP_SERVER_JOB_H 1

#include "Dispatcher/Job.h"

#include "Basics/Exceptions.h"
#include "Basics/WorkMonitor.h"

namespace arangodb {
namespace rest {
class RestHandler;
class GeneralServer;

class GeneralServerJob : public Job {
  GeneralServerJob(GeneralServerJob const&) = delete;
  GeneralServerJob& operator=(GeneralServerJob const&) = delete;

 public:
  GeneralServerJob(GeneralServer*, arangodb::WorkItem::uptr<RestHandler>,
                   bool isAsync = false);

  ~GeneralServerJob();

 public:
  RestHandler* handler() const { return _handler.get(); }

 public:
  size_t queue() const override;
  void work() override;
  bool cancel() override;
  void cleanup(DispatcherQueue*) override;
  void handleError(basics::Exception const&) override;

 protected:
  GeneralServer* _server;
  arangodb::WorkItem::uptr<RestHandler> _handler;
  arangodb::WorkDescription* _workDesc;
  bool _isAsync;
};
}
}

#endif
