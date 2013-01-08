////////////////////////////////////////////////////////////////////////////////
/// @brief transaction collection wrapper
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

#ifndef TRIAGENS_UTILS_TRANSACTION_COLLECTION_H
#define TRIAGENS_UTILS_TRANSACTION_COLLECTION_H 1

#include "Basics/Common.h"

#include "VocBase/collection.h"
#include "VocBase/transaction.h"

namespace triagens {
  namespace arango {

    class TransactionCollection {

      private:
        TransactionCollection (const TransactionCollection&);
        TransactionCollection& operator= (const TransactionCollection&);
      
      public:
        TransactionCollection (const string& name, TRI_transaction_type_e accessType) : 
          _name(name), 
          _accessType(accessType),
          _createType(TRI_COL_TYPE_DOCUMENT),
          _create(false) {
        }

        TransactionCollection (const string& name, TRI_col_type_e createType) :
          _name(name),
          _accessType(TRI_TRANSACTION_WRITE),
          _createType(TRI_COL_TYPE_DOCUMENT),
          _create(true) {
        }

        ~TransactionCollection () {
        }

        inline const string getName () const {
          return _name;
        }
        
        inline TRI_transaction_type_e getAccessType () const {
          return _accessType;
        }
        
        inline void setAccessType (TRI_transaction_type_e accessType) {
          _accessType = accessType;
        }
        
        inline bool getCreateFlag () const {
          return _create;
        }
        
        inline TRI_col_type_e getCreateType () const {
          return _createType;
        }

      private:
        string _name;

        TRI_transaction_type_e _accessType;

        TRI_col_type_e _createType;

        bool _create;
    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
