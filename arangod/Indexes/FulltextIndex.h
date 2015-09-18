////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext index
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

#ifndef ARANGODB_INDEXES_FULLTEXT_INDEX_H
#define ARANGODB_INDEXES_FULLTEXT_INDEX_H 1

#include "Basics/Common.h"
#include "FulltextIndex/fulltext-common.h"
#include "Indexes/Index.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"
 
struct TRI_fulltext_wordlist_s;

// -----------------------------------------------------------------------------
// --SECTION--                                               class FulltextIndex
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class FulltextIndex : public Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        FulltextIndex () = delete;

        FulltextIndex (TRI_idx_iid_t,
                       struct TRI_document_collection_t*,
                       std::string const&,
                       int);

        ~FulltextIndex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:
        
        IndexType type () const override final {
          return Index::TRI_IDX_TYPE_FULLTEXT_INDEX;
        }

        bool hasSelectivityEstimate () const override final {
          return false;
        }
        
        bool dumpFields () const override final {
          return true;
        }

        size_t memory () const override final;

        triagens::basics::Json toJson (TRI_memory_zone_t*, bool) const override final;
        triagens::basics::Json toJsonFigures (TRI_memory_zone_t*) const override final;
  
        int insert (struct TRI_doc_mptr_t const*, bool) override final;
         
        int remove (struct TRI_doc_mptr_t const*, bool) override final;
        
        int cleanup () override final;

        bool isSame (std::string const& field, int minWordLength) const {
          std::string fieldString;
          TRI_AttributeNamesToString(fields()[0], fieldString);
          return (_minWordLength == minWordLength && fieldString == field);
        }

        TRI_fts_index_t* internals () {
          return _fulltextIndex;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

        struct TRI_fulltext_wordlist_s* wordlist (struct TRI_doc_mptr_t const*);
        
// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the indexed attribute (path)
////////////////////////////////////////////////////////////////////////////////

        TRI_shape_pid_t _pid;

////////////////////////////////////////////////////////////////////////////////
/// @brief the fulltext index
////////////////////////////////////////////////////////////////////////////////
        
        TRI_fts_index_t* _fulltextIndex;

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum word length
////////////////////////////////////////////////////////////////////////////////

        int _minWordLength;

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
