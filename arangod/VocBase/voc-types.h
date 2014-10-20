////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase types
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_VOC__TYPES_H
#define ARANGODB_VOC_BASE_VOC__TYPES_H 1

#include "Basics/Common.h"
#include "Cluster/ServerState.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief collection meta info filename
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_PARAMETER_FILE  "parameter.json"

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief tick type (48bit)
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_voc_tick_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection identifier type
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_voc_cid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile identifier type
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_voc_fid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document key identifier type
////////////////////////////////////////////////////////////////////////////////

typedef char* TRI_voc_key_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief revision identifier type
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_voc_rid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction identifier type
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_voc_tid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief size type
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_voc_size_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief signed size type
////////////////////////////////////////////////////////////////////////////////

typedef int32_t TRI_voc_ssize_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief index identifier
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t TRI_idx_iid_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief crc type
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_voc_crc_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection storage type
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_col_type_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief server id type
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_server_id_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief enum for write operations
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_VOC_DOCUMENT_OPERATION_UNKNOWN = 0,
  TRI_VOC_DOCUMENT_OPERATION_INSERT,
  TRI_VOC_DOCUMENT_OPERATION_UPDATE,
  TRI_VOC_DOCUMENT_OPERATION_REMOVE
}
TRI_voc_document_operation_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief server operation modes
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_VOCBASE_MODE_NORMAL    = 1,    // CRUD is allowed
  TRI_VOCBASE_MODE_NO_CREATE = 2,  // C & U not allowed RD allowed
}
TRI_vocbase_operationmode_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge from and to
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_document_edge_s {
  TRI_voc_cid_t _fromCid;
  TRI_voc_key_t _fromKey;

  TRI_voc_cid_t _toCid;
  TRI_voc_key_t _toKey;
}
TRI_document_edge_t;

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                             class TransactionBase
// -----------------------------------------------------------------------------
//
    class TransactionBase {

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction base, every transaction class has to inherit from here
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        TransactionBase () {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(_numberTrxInScope >= 0);
          TRI_ASSERT(_numberTrxInScope == _numberTrxActive);
          _numberTrxInScope++;
#endif
        }

        explicit TransactionBase (bool standalone) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(_numberTrxInScope >= 0);
          TRI_ASSERT(_numberTrxInScope == _numberTrxActive);
          _numberTrxInScope++;
          if (standalone) {
            _numberTrxActive++;
          }
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~TransactionBase () {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(_numberTrxInScope > 0);
          _numberTrxInScope--;
          // Note that embedded transactions might have seen a begin()
          // but no abort() or commit(), so _numberTrxActive might
          // be one too big. We simply fix it here:
          _numberTrxActive = _numberTrxInScope;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set counters, used in replication client to transfer transactions
/// between threads.
////////////////////////////////////////////////////////////////////////////////

        static void setNumbers (int numberInScope, int numberActive) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          _numberTrxInScope = numberInScope;
          _numberTrxActive = numberActive;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief increase counters, used in replication client in shutdown to
/// kill transactions of other threads.
////////////////////////////////////////////////////////////////////////////////

        static void increaseNumbers (int numberInScope, int numberActive) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(_numberTrxInScope + numberInScope >= 0);
          TRI_ASSERT(_numberTrxActive + numberActive >= 0);
          _numberTrxInScope += numberInScope;
          _numberTrxActive += numberActive;
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that a transaction object is in scope in the current thread
////////////////////////////////////////////////////////////////////////////////

        static void assertSomeTrxInScope () {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(_numberTrxInScope > 0);
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that the innermost transaction object in scope in the
/// current thread is actually active (between begin() and commit()/abort().
////////////////////////////////////////////////////////////////////////////////

        static void assertCurrentTrxActive () {
#ifdef TRI_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(_numberTrxInScope > 0 &&
                     _numberTrxInScope == _numberTrxActive);
#endif
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the following is for the runtime protection check, number of
/// transaction objects in scope in the current thread
////////////////////////////////////////////////////////////////////////////////

      protected:

#ifdef TRI_ENABLE_MAINTAINER_MODE
        static thread_local int _numberTrxInScope;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief the following is for the runtime protection check, number of
/// transaction objects in the current thread that are active (between
/// begin and commit()/abort().
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
        static thread_local int _numberTrxActive;
#endif

      };
  }
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
