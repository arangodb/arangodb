////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, indexes
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
/// @author not James
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_Indexes_H
#define ARANGODB_AQL_Indexes_H 1

#include "Basics/Common.h"
#include "Aql/Index.h"

struct TRI_vocbase_s;

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                 class Indexes
// -----------------------------------------------------------------------------

    class Indexes {
      public:

        Indexes& operator= (Indexes const& other) = delete;
      
        Indexes (struct TRI_vocbase_s* vocbase) 
          : _vocbase(vocbase),
            _indexes() {
        }
      
        ~Indexes () {
          for (auto it = _indexes.begin(); it != _indexes.end(); ++it) {
            delete (*it).second;
          }
        }

      public:

        Index* get (TRI_idx_iid_t id, Collection const* collection) {
          auto cid = collection->cid();
          // there may be multiple indexes in the same collection . . .
          auto it1 = _cids.begin();

          do{
            auto it2 = _cids.find(it1, _cids.end(), cid);
            if (it2 == _cids.end()) { // couldn't find <cid>
              return nullptr;
            }
          }((it2*).second.first == id || it1 == _cids.end());
          
          if((it2*).second.first == id){
            return return (it2*).second.second;
          }
          
          return nullptr;
        }

        void add (TRI_idx_iid_t id, Collection const* collection) {
          // check if index already is in our map
          if(this.get(id, collection)!=nullptr){
            auto index = new Index(id, collection);
            try {
              auto x = std::make_pair(id, index);
              _ids.insert(x);
              _cids.insert(std::make_pair(collection->cid(), x));
            }
            catch (...) {
              delete index;
              throw;
            }
          }
        }

        std::vector<std::string> indexIds () const {
          std::vector<std::string> result;

          for (auto x : _indexes) {
            result.push_back(x.first);
          }
          return result;
        }

      private:

        struct TRI_vocbase_s*               _vocbase;
        std::map<TRI_idx_iid_t, Index*>     _ids;
        std::map<TRI_voc_cid_t, _indexes>   _cids;
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

