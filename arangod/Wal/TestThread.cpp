////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log garbage collection thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "TestThread.h"
#include "BasicsC/logging.h"
#include "Basics/ConditionLocker.h"
#include "VocBase/datafile.h"
#include "VocBase/marker.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "Wal/Logfile.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                             class TestThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the collector thread
////////////////////////////////////////////////////////////////////////////////

TestThread::TestThread (LogfileManager* logfileManager) 
  : Thread("WalCollector"),
    _logfileManager(logfileManager),
    _condition(),
    _stop(0) {
  
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the collector thread
////////////////////////////////////////////////////////////////////////////////

TestThread::~TestThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the collector thread
////////////////////////////////////////////////////////////////////////////////

void TestThread::stop () {
  if (_stop > 0) {
    return;
  }

  _stop = 1;
  _condition.signal();

  while (_stop != 2) {
    usleep(1000);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void TestThread::run () {
  uint64_t i = 0;
  while (_stop == 0) {
      size_t s;
      void* p;

      if (i % 1 == 0) {
        s = sizeof(TRI_doc_begin_transaction_marker_t);
        p = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, s, true);
        TRI_doc_begin_transaction_marker_t* marker = static_cast<TRI_doc_begin_transaction_marker_t*>(p);
      
        TRI_InitMarker((char*) p, TRI_DOC_MARKER_BEGIN_TRANSACTION, s);
        marker->_tid = i;
        marker->_numCollections = 0;
      }
      else {
        s = sizeof(TRI_doc_commit_transaction_marker_t);
        
        p = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, s, true);
        TRI_doc_commit_transaction_marker_t* marker = static_cast<TRI_doc_commit_transaction_marker_t*>(p);
      
        TRI_InitMarker((char*) p, TRI_DOC_MARKER_COMMIT_TRANSACTION, s);
        marker->_tid = i - 1;
      }


      if (i % 500000 == 0) {
        LOG_INFO("now at: %d", (int) i);
      }
      _logfileManager->allocateAndWrite(p, static_cast<uint32_t>(s), false);
      
      ++i;

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, p);
  }

  _stop = 2;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
