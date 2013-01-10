////////////////////////////////////////////////////////////////////////////////
/// @brief transaction collections list
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

#ifndef TRIAGENS_UTILS_TRANSACTION_COLLECTION_LIST_H
#define TRIAGENS_UTILS_TRANSACTION_COLLECTION_LIST_H 1

#include "Basics/Common.h"

#include "VocBase/collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#include "Utils/TransactionCollection.h"


namespace triagens {
  namespace arango {

    class TransactionCollectionsList {
      typedef map<string, TransactionCollection*> ListType;

      private:
        TransactionCollectionsList& operator= (const TransactionCollectionsList&);
        TransactionCollectionsList (const TransactionCollectionsList&);
      
      public:
        TransactionCollectionsList () : 
          _collections() {
        }

        TransactionCollectionsList (TRI_vocbase_t* const vocbase, 
                                    const string& name, 
                                    TRI_transaction_type_e accessType) : 
          _vocbase(vocbase),
          _collections(),
          _error(TRI_ERROR_NO_ERROR) {
          addCollection(name, accessType, false);
        }
        
        TransactionCollectionsList (TRI_vocbase_t* const vocbase, 
                                    const string& name, 
                                    TRI_transaction_type_e accessType, 
                                    const TRI_col_type_e createType) : 
          _vocbase(vocbase),
          _collections(),
          _error(TRI_ERROR_NO_ERROR) {
          addCollection(name, accessType, true, createType);
        }
        
        TransactionCollectionsList (TRI_vocbase_t* const vocbase, 
                                    const string& name, 
                                    TRI_transaction_type_e accessType, 
                                    const bool create,
                                    const TRI_col_type_e createType) : 
          _vocbase(vocbase),
          _collections(),
          _error(TRI_ERROR_NO_ERROR) {
          addCollection(name, accessType, create, createType);
        }
        
        TransactionCollectionsList (TRI_vocbase_t* const vocbase,
                                    const vector<string>& readCollections,
                                    const vector<string>& writeCollections) : 
          _vocbase(vocbase),
          _collections() {
          for (size_t i = 0; i < readCollections.size(); ++i) {
            addCollection(readCollections[i], TRI_TRANSACTION_READ, false);
          }

          for (size_t i = 0; i < writeCollections.size(); ++i) {
            addCollection(writeCollections[i], TRI_TRANSACTION_WRITE, false);
          }
        }

        ~TransactionCollectionsList () {
          ListType::iterator it;

          for (it = _collections.begin(); it != _collections.end(); ++it) {
            delete (*it).second;
          }

          _collections.clear();
        }

        inline int getError () const {
          return _error;
        }

        const vector<TransactionCollection*> getCollections () {
          ListType::iterator it;
          vector<TransactionCollection*> collections;

          for (it = _collections.begin(); it != _collections.end(); ++it) {
            collections.push_back((*it).second);
          }
    
          return collections; 
        }

        void addCollection (const string& name, 
                            TRI_transaction_type_e type, 
                            bool create, 
                            TRI_col_type_e createType = TRI_COL_TYPE_DOCUMENT) {
          ListType::iterator it;
          string realName;
          
          if (name.empty()) {
            _error = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
            return;
          }
          
          if (isdigit(name[0])) {
            // name is passed as a string with the collection id

            // look up the collection name for the id
            TRI_voc_cid_t id = triagens::basics::StringUtils::uint64(name);

            char* n = TRI_GetCollectionNameByIdVocBase(_vocbase, id);
            if (n == 0) {
              _error = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
              return;
            }

            realName = string(n);
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, n);
          }
          else {
            // name is passed as a "real" name
            realName = name;
          }

          it = _collections.find(realName);
          if (it != _collections.end()) {
            TransactionCollection* c = (*it).second;

            if (type == TRI_TRANSACTION_WRITE && type != c->getAccessType()) {
              // upgrade the type
              c->setAccessType(type);
            }
          }
          else {

            // check whether the collection exists. if not, create it            
            if ((  create && TRI_FindCollectionByNameOrBearVocBase(_vocbase, realName.c_str(), createType) == NULL) ||
                (! create && TRI_LookupCollectionByNameVocBase(_vocbase, realName.c_str()) == NULL)) {
              _error = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
              return;
            }

            _collections[realName] = new TransactionCollection(realName, type);
          }
        }

      private:
        TRI_vocbase_t* _vocbase;

        ListType _collections;

        int _error;

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
