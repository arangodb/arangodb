////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for user transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_USER_TRANSACTION_H
#define TRIAGENS_UTILS_USER_TRANSACTION_H 1

#include "Utils/Transaction.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                             class UserTransaction
// -----------------------------------------------------------------------------

    class UserTransaction : public Transaction {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction
////////////////////////////////////////////////////////////////////////////////

        UserTransaction (const TRI_vocbase_col_t* vocbase, const vector<string>& readCollections, const vector<string>& writeCollections) : 
          Transaction(vocbase), _readCollections(readCollections), _writeCollections(writeCollections) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~UserTransaction () {
          if (_trx != 0) {
            if (_trx->_status == TRI_TRANSACTION_RUNNING) {
              // auto abort
              abort();
            }
          }
        }

      public:

        int begin () {
          if (_trx != 0) {
            // already started
            return TRI_ERROR_TRANSACTION_INVALD_STATE;
          }

          _trx = TRI_CreateTransaction(_vocbase->_transactionContext, TRI_TRANSACTION_READ_REPEATABLE, 0);
          if (_trx == 0) {
            return TRI_ERROR_OUT_OF_MEMORY;
          }
 
          for (size_t i = 0; i < _readCollections.size(); ++i) { 
            if (! TRI_AddCollectionTransaction(_trx, _readCollections[i].c_str(), TRI_TRANSACTION_READ)) {
              return TRI_ERROR_INTERNAL;
            }
          }
          
          for (size_t i = 0; i < _writeCollections.size(); ++i) { 
            if (! TRI_AddCollectionTransaction(_trx, _readCollections[i].c_str(), TRI_TRANSACTION_WRITE)) {
              return TRI_ERROR_INTERNAL;
            }
          }

          if (_trx->_status != TRI_TRANSACTION_CREATED) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_StartTransaction(_trx);
          return res;
        }

        int commit () {
          if (_trx == 0 || _trx->_status != TRI_TRANSACTION_RUNNING) {
            // not created or not running
            return TRI_ERROR_TRANSACTION_INVALD_STATE;
          }

          int res;

          if (_trx->_type == TRI_TRANSACTION_READ) {
            res = TRI_FinishTransaction(_trx);
          }
          else {
            res = TRI_Commitransaction(_trx);
          }
          
          return res;
        }

        int abort () {
          if (_trx == 0) {
            // transaction already ended or not created
            return TRI_ERROR_NO_ERROR;
          }

          if (_trx->_status != TRI_TRANSACTION_RUNNING) {
            return TRI_ERROR_TRANSACTION_INVALID_STATE;
          }

          int res = TRI_AbortTransaction(_trx);

          return res;
        }

      private:

        vector<string> _readCollections;

        vector<string> _writeCollections;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
