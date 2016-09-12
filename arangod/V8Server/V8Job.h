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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_V8_SERVER_V8_JOB_H
#define ARANGOD_V8_SERVER_V8_JOB_H 1

#include "Basics/Common.h"
#include "Dispatcher/Job.h"
#include "VocBase/vocbase.h"

namespace arangodb {
namespace rest {
class Task;
}

class V8Job : public rest::Job {
  V8Job(V8Job const&) = delete;
  V8Job& operator=(V8Job const&) = delete;

 public:
  V8Job(TRI_vocbase_t*, std::string const&,
        std::shared_ptr<arangodb::velocypack::Builder> const, bool, rest::Task*);
  ~V8Job();

 public:
  void work() override;
  bool cancel() override;
  void cleanup(rest::DispatcherQueue*) override;
  void handleError(basics::Exception const& ex) override;

 public:
  virtual std::string const& getName() const override { return _command; }

 private:
  /// @brief guard to make sure the database is not dropped while used by us
  VocbaseGuard _vocbaseGuard;
  std::string const _command;
  std::shared_ptr<arangodb::velocypack::Builder> _parameters;
  std::atomic<bool> _canceled;
  bool const _allowUseDatabase;
  rest::Task* _task;
};
}

#endif
