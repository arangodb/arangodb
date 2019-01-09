////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CRITICAL_THREAD_H
#define ARANGOD_CLUSTER_CRITICAL_THREAD_H 1

#include "Basics/Thread.h"
#include "Logger/Logger.h"

namespace arangodb {

class CriticalThread : public Thread {
 public:
  // copy constructor and assignment duplicate base class
  CriticalThread(CriticalThread const&) = delete;
  CriticalThread& operator=(CriticalThread const&) = delete;

  CriticalThread(std::string const& name, bool deleteOnExit = false)
      : Thread(name, deleteOnExit) {}

  virtual ~CriticalThread() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Notification of when thread crashes via uncaught throw ... log it
  ///        for hourly repeat log
  //////////////////////////////////////////////////////////////////////////////

  virtual void crashNotification(std::exception const& ex) override;

};  // class CriticalThread

}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_CRITICAL_THREAD_H
