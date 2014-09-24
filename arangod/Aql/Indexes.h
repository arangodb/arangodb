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
      
        Indexes () 
          : _indexes(){
        }
      
        ~Indexes () {
          for (auto x: _indexes) {
            for (auto y: x.second){
              delete y.second;
            }
          }
        }

      public:

        Index* get (TRI_idx_iid_t id, Collection const* collection) {
          // there may be multiple indexes in the same collection . . .
          auto it1 = _indexes.find(collection->cid());
          
          if(it1 == _indexes.end()){
            return nullptr;
          }
          
          auto it2 = it1->second.find(id);

          if(it2 == it1->second.end()){
            return nullptr;
          }
          
          return (*it2).second;
        }

        void add (TRI_idx_iid_t id, Collection const* collection) {
          auto it = _indexes.find(collection->cid());

          if (it == _indexes.end()){
            _indexes.insert(std::make_pair(collection->cid(), 
                  std::unordered_map<TRI_idx_iid_t, Index*>()));
          }
          
          auto index = new Index(id, collection);
          try {
            it->second.insert(std::make_pair(id, index));
          }
          catch (...) {
            delete index;
            throw;
          }
        }

        std::vector<TRI_idx_iid_t> indexIds () const {
          std::vector<TRI_idx_iid_t> result;

          for (auto x : _indexes) {
            for (auto y : x.second) {
              result.push_back(y.first);
            }
          }
          return result;
        }

      private:

        std::unordered_map<TRI_voc_cid_t, std::unordered_map<TRI_idx_iid_t, Index*>> _indexes;
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

