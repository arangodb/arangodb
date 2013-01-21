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

// -----------------------------------------------------------------------------
// --SECTION--                                  class TransactionCollectionsList
// -----------------------------------------------------------------------------

    class TransactionCollectionsList {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for contained collections list
////////////////////////////////////////////////////////////////////////////////

      typedef map<string, TransactionCollection*> ListType;

////////////////////////////////////////////////////////////////////////////////
/// @brief TransactionCollectionsList copy ctor
////////////////////////////////////////////////////////////////////////////////

      private:
        TransactionCollectionsList& operator= (const TransactionCollectionsList&);
        TransactionCollectionsList (const TransactionCollectionsList&);

////////////////////////////////////////////////////////////////////////////////
/// @}
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
/// @brief create an empty list
////////////////////////////////////////////////////////////////////////////////

        TransactionCollectionsList () : 
          _collections() {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with a single collection
////////////////////////////////////////////////////////////////////////////////

        TransactionCollectionsList (TRI_vocbase_t* const vocbase, 
                                    const string& name, 
                                    TRI_transaction_type_e accessType) : 
          _vocbase(vocbase),
          _collections(),
          _error(TRI_ERROR_NO_ERROR) {

          addCollection(name, accessType, false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with a single collection, with implicit create flag
////////////////////////////////////////////////////////////////////////////////
        
        TransactionCollectionsList (TRI_vocbase_t* const vocbase, 
                                    const string& name, 
                                    TRI_transaction_type_e accessType, 
                                    const TRI_col_type_e createType) : 
          _vocbase(vocbase),
          _collections(),
          _error(TRI_ERROR_NO_ERROR) {

          addCollection(name, accessType, true, createType);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with a single collection, with explicit create flag
////////////////////////////////////////////////////////////////////////////////
        
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

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with multiple collections
////////////////////////////////////////////////////////////////////////////////
        
        TransactionCollectionsList (TRI_vocbase_t* const vocbase,
                                    const vector<string>& readCollections,
                                    const vector<string>& writeCollections) : 
          _vocbase(vocbase),
          _collections(),
          _error(TRI_ERROR_NO_ERROR) {

          for (size_t i = 0; i < readCollections.size(); ++i) {
            addCollection(readCollections[i], TRI_TRANSACTION_READ, false);
          }

          for (size_t i = 0; i < writeCollections.size(); ++i) {
            addCollection(writeCollections[i], TRI_TRANSACTION_WRITE, false);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief dtor
////////////////////////////////////////////////////////////////////////////////

        ~TransactionCollectionsList () {
          ListType::iterator it;

          for (it = _collections.begin(); it != _collections.end(); ++it) {
            delete (*it).second;
          }

          _collections.clear();
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get potential raised during list setup
////////////////////////////////////////////////////////////////////////////////

        inline int getError () const {
          return _error;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
////////////////////////////////////////////////////////////////////////////////

        const vector<TransactionCollection*> getCollections () {
          ListType::iterator it;
          vector<TransactionCollection*> collections;

          for (it = _collections.begin(); it != _collections.end(); ++it) {
            collections.push_back((*it).second);
          }
    
          return collections; 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the list
////////////////////////////////////////////////////////////////////////////////

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

          // check if we already have the collection in our list
          it = _collections.find(realName);
          if (it != _collections.end()) {
            // yes, now update the collection in the list
            TransactionCollection* c = (*it).second;

            if (type == TRI_TRANSACTION_WRITE && type != c->getAccessType()) {
              // upgrade the type
              c->setAccessType(type);
            }
          }
          else {
            // collection not yet contained in our list

            // check whether the collection exists. if not, create it            
            if ((  create && TRI_FindCollectionByNameOrBearVocBase(_vocbase, realName.c_str(), createType) == NULL) ||
                (! create && TRI_LookupCollectionByNameVocBase(_vocbase, realName.c_str()) == NULL)) {
              _error = TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
              return;
            }

            _collections[realName] = new TransactionCollection(realName, type);
          }
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
/// @brief vocbase
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief the list of collections
////////////////////////////////////////////////////////////////////////////////

        ListType _collections;

////////////////////////////////////////////////////////////////////////////////
/// @brief error number
////////////////////////////////////////////////////////////////////////////////

        int _error;

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
