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

#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace arango {

    template<typename T>
    class UserTransaction : public Transaction<T> {

// -----------------------------------------------------------------------------
// --SECTION--                                             class UserTransaction
// -----------------------------------------------------------------------------

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

        UserTransaction (TRI_vocbase_t* const vocbase, 
                         const vector<string>& readCollections, 
                         const vector<string>& writeCollections) : 
          Transaction<T>(vocbase, "UserTransaction"), 
          _readCollections(readCollections), 
          _writeCollections(writeCollections) { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~UserTransaction () {
          if (this->_trx != 0) {
            if (this->status() == TRI_TRANSACTION_RUNNING) {
              // auto abort
              this->abort();
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       virtual protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief use all collections 
/// this is a no op, as using is done when trx is started
////////////////////////////////////////////////////////////////////////////////

        int useCollections () {
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief release all collections in use
/// this is a no op, as releasing is done when trx is finished
////////////////////////////////////////////////////////////////////////////////

        int releaseCollections () {
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add all collections to the transaction
////////////////////////////////////////////////////////////////////////////////

        int addCollections () {
          int res;
          size_t i;

          for (i = 0; i < _readCollections.size(); ++i) {
            res = TRI_AddCollectionTransaction(this->_trx, _readCollections[i].c_str(), TRI_TRANSACTION_READ, 0);
            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }    

          for (i = 0; i < _writeCollections.size(); ++i) {
            res = TRI_AddCollectionTransaction(this->_trx, _writeCollections[i].c_str(), TRI_TRANSACTION_WRITE, 0);
            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collections that are opened in read mode
////////////////////////////////////////////////////////////////////////////////

        vector<string> _readCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief collections that are opened in write mode
////////////////////////////////////////////////////////////////////////////////

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
