////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, collections
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_COLLECTIONS_H
#define ARANGODB_AQL_COLLECTIONS_H 1

#include "Basics/Common.h"
#include "Aql/Collection.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                 class Collections
// -----------------------------------------------------------------------------

    class Collections {
      public:

        Collections& operator= (Collections const& other) = delete;
      
        Collections () {
        }
      
        ~Collections () {
          for (auto it = _collections.begin(); it != _collections.end(); ++it) {
            delete (*it).second;
          }
        }

      public:

        void add (std::string const& name,
                  TRI_transaction_type_e accessType) {
          // check if collection already is in our map
          auto it = _collections.find(name);

          if (it == _collections.end()) {
            auto collection = new Collection(name, accessType);
            try {
              _collections.insert(std::make_pair(name, collection));
            }
            catch (...) {
              delete collection;
              throw;
            }
          }
          else {
            // change access type from read to write
            if (accessType == TRI_TRANSACTION_WRITE &&
                (*it).second->accessType == TRI_TRANSACTION_READ) {
              (*it).second->accessType = TRI_TRANSACTION_WRITE;
            }
          }
        }

        std::vector<std::string> collectionNames () const {
          std::vector<std::string> result;

          for (auto it = _collections.begin(); it != _collections.end(); ++it) {
            result.push_back((*it).first);
          }
          return result;
        }

        std::map<std::string, Collection*>* collections () {
          return &_collections;
        }

      private:

        std::map<std::string, Collection*> _collections;
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
