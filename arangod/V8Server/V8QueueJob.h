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

#ifndef ARANGOD_V8_SERVER_V8_QUEUE_JOB_H
#define ARANGOD_V8_SERVER_V8_QUEUE_JOB_H 1

#include "Dispatcher/Job.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Builder;
}

class V8QueueJob : public rest::Job {
  V8QueueJob(V8QueueJob const&) = delete;
  V8QueueJob& operator=(V8QueueJob const&) = delete;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new V8 queue job
  //////////////////////////////////////////////////////////////////////////////

  V8QueueJob(size_t queue, TRI_vocbase_t*,
             std::shared_ptr<arangodb::velocypack::Builder>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys a V8 job
  //////////////////////////////////////////////////////////////////////////////

  ~V8QueueJob();

 public:
  size_t queue() const override;

  void work() override;

  bool cancel() override;

  void cleanup(rest::DispatcherQueue*) override;

  void handleError(basics::Exception const& ex) override;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief queue name
  //////////////////////////////////////////////////////////////////////////////

  size_t const _queue;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief vocbase
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_t* _vocbase;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief paramaters
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::velocypack::Builder> _parameters;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cancel flag
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<bool> _canceled;
};
}

#endif
