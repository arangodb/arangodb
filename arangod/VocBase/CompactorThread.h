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

#ifndef ARANGOD_VOC_BASE_COMPACTOR_THREAD_H
#define ARANGOD_VOC_BASE_COMPACTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {

class CompactorThread : public Thread {
 public:
  explicit CompactorThread(TRI_vocbase_t* vocbase);
  ~CompactorThread();

  void signal() { _condition.signal(); }

 protected:
  void run() override;

 private:
  TRI_vocbase_t* _vocbase;

  arangodb::basics::ConditionVariable _condition;
};

}

/// @brief remove data of expired compaction blockers
bool TRI_CleanupCompactorVocBase(TRI_vocbase_t*);

/// @brief insert a compaction blocker
int TRI_InsertBlockerCompactorVocBase(TRI_vocbase_t*, double, TRI_voc_tick_t*);

/// @brief touch an existing compaction blocker
int TRI_TouchBlockerCompactorVocBase(TRI_vocbase_t*, TRI_voc_tick_t, double);

/// @brief remove an existing compaction blocker
int TRI_RemoveBlockerCompactorVocBase(TRI_vocbase_t*, TRI_voc_tick_t);

/// @brief compactor event loop
void TRI_CompactorVocBase(void*);

#endif
