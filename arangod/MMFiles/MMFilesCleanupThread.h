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

#ifndef ARANGOD_MMFILES_MMFILES_CLEANUP_THREAD_H
#define ARANGOD_MMFILES_MMFILES_CLEANUP_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;

class MMFilesCleanupThread final : public Thread {
 public:
  explicit MMFilesCleanupThread(TRI_vocbase_t* vocbase);
  ~MMFilesCleanupThread();

  void signal();

 protected:
  void run() override;

 private:
  /// @brief cleanup interval in microseconds
  static constexpr unsigned cleanupInterval() { return (1 * 1000 * 1000); }

  /// @brief how many cleanup iterations until query cursors are cleaned
  static constexpr uint64_t cleanupCursorIterations() { return 3; }

  /// @brief clean up cursors
  void cleanupCursors(bool force);

  /// @brief checks all datafiles of a collection
  void cleanupCollection(arangodb::LogicalCollection* collection);

 private:
  TRI_vocbase_t* _vocbase;

  arangodb::basics::ConditionVariable _condition;
};

}

#endif
