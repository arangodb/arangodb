////////////////////////////////////////////////////////////////////////////////
/// @brief collections
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
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_COLLECTION_H
#define ARANGODB_VOC_BASE_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"
#include "VocBase/datafile.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

////////////////////////////////////////////////////////////////////////////////
/// Data is stored in datafiles. A set of datafiles forms a collection. A
/// datafile can be read-only and sealed or read-write. All datafiles of a
/// collection are stored in a directory. This directory contains the following
/// files:
///
/// - parameter.json: The parameters of a collection.
///
/// - datafile-NNN.db: A read-only datafile. The number NNN is the datafile
///     identifier, see @ref TRI_datafile_t.
///
/// - journal-NNN.db: A read-write datafile used as journal. All new entries
///     of a collection are appended to a journal. The number NNN is the
///     datafile identifier, see @ref TRI_datafile_t.
///
/// - index-NNN.json: An index description. The number NNN is the index
///     identifier, see @ref TRI_index_t.
///
/// The structure @ref TRI_collection_t is abstract. Currently, there is one
/// concrete sub-class @ref TRI_document_collection_t.
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          forwards
// -----------------------------------------------------------------------------

struct TRI_json_t;
class TRI_vocbase_col_t;

namespace triagens {
  namespace arango {
    class CollectionInfo;
  }
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief collection name regex
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_REGEX      "[a-zA-Z_][0-9a-zA-Z_-]*"

////////////////////////////////////////////////////////////////////////////////
/// @brief collection version for ArangoDB >= 1.3
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_VERSION_13      (4)

////////////////////////////////////////////////////////////////////////////////
/// @brief collection version for ArangoDB >= 2.0
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_VERSION_20      (5)

////////////////////////////////////////////////////////////////////////////////
/// @brief current collection version
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_VERSION TRI_COL_VERSION_20

////////////////////////////////////////////////////////////////////////////////
/// @brief predefined system collection name for transactions
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_TRANSACTION "_trx"

////////////////////////////////////////////////////////////////////////////////
/// @brief predefined system collection name for replication
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_REPLICATION "_replication"

////////////////////////////////////////////////////////////////////////////////
/// @brief predefined collection name for users
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_USERS       "_users"

////////////////////////////////////////////////////////////////////////////////
/// @brief predefined collection name for statistics
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_STATISTICS  "_statistics"

////////////////////////////////////////////////////////////////////////////////
/// @brief default number of index buckets
////////////////////////////////////////////////////////////////////////////////

#define TRI_DEFAULT_INDEX_BUCKETS 8

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief collection file structure
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_col_file_structure_s {
  TRI_vector_string_t _journals;
  TRI_vector_string_t _compactors;
  TRI_vector_string_t _datafiles;
  TRI_vector_string_t _indexes;
}
TRI_col_file_structure_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief state of the datafile
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_COL_STATE_CLOSED      = 1,    // collection is closed
  TRI_COL_STATE_READ        = 2,    // collection is opened read only
  TRI_COL_STATE_WRITE       = 3,    // collection is opened read/append
  TRI_COL_STATE_OPEN_ERROR  = 4,    // an error has occurred while opening
  TRI_COL_STATE_WRITE_ERROR = 5     // an error has occurred while writing
}
TRI_col_state_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection version
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_col_version_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection enum
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_COL_TYPE_UNKNOWN          = 0, // only used when initializing
  TRI_COL_TYPE_SHAPE_DEPRECATED = 1, // not used since ArangoDB 1.5
  TRI_COL_TYPE_DOCUMENT         = 2,
  TRI_COL_TYPE_EDGE             = 3
}
TRI_col_type_e;


namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief collection info block saved to disk as json
////////////////////////////////////////////////////////////////////////////////

    class VocbaseCollectionInfo {

      private:
        TRI_col_version_t                     _version;         // collection version
        TRI_col_type_e                        _type;            // collection type
        TRI_voc_rid_t                         _revision;        // last revision id written
        TRI_voc_cid_t                         _cid;             // local collection identifier
        TRI_voc_cid_t                         _planId;          // cluster-wide collection identifier
        TRI_voc_size_t                        _maximalSize;     // maximal size of memory mapped file
        int64_t                               _initialCount;    // initial count, used when loading a collection
        uint32_t                              _indexBuckets;    // number of buckets used in hash tables for indexes

        char                                  _name[TRI_COL_PATH_LENGTH];  // name of the collection()
        std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> _keyOptions;      // options for key creation

        // flags
        bool                                  _isSystem;        // if true, this is a system collection
        bool                                  _deleted;         // if true, collection has been deleted
        bool                                  _doCompact;       // if true, collection will be compacted
        bool                                  _isVolatile;      // if true, collection is memory-only
        bool                                  _waitForSync;     // if true, wait for msync

      public:

        VocbaseCollectionInfo () {};

        explicit VocbaseCollectionInfo (CollectionInfo const&);


        VocbaseCollectionInfo (TRI_vocbase_t*,
                               char const*,
                               TRI_col_type_e,
                               TRI_voc_size_t,
                               VPackSlice const&);

        VocbaseCollectionInfo (TRI_vocbase_t*,
                               char const*,
                               VPackSlice const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new VocbaseCollectionInfo from the json content of a file
/// This function throws if the file cannot be parsed.
///
/// You must hold the @ref TRI_READ_LOCK_STATUS_VOCBASE_COL when calling this
/// function.
////////////////////////////////////////////////////////////////////////////////
        static VocbaseCollectionInfo fromFile (char const*,
                                               TRI_vocbase_t*,
                                               char const*,
                                               bool);

        virtual ~VocbaseCollectionInfo ();

        // collection version
        virtual TRI_col_version_t version () const;

        // collection type
        virtual TRI_col_type_e type () const;

        // local collection identifier
        virtual TRI_voc_cid_t id () const;

        // cluster-wide collection identifier
        virtual TRI_voc_cid_t planId () const;

        // last revision id written
        virtual TRI_voc_rid_t revision () const;

        // maximal size of memory mapped file
        virtual TRI_voc_size_t maximalSize () const;

        // initial count, used when loading a collection
        virtual int64_t initialCount () const;

        // number of buckets used in hash tables for indexes
        virtual uint32_t indexBuckets () const;

        // name of the collection
        virtual std::string name () const;

        // name of the collection as c string
        virtual char const* namec_str () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a copy of the key options
/// the caller is responsible for freeing it
////////////////////////////////////////////////////////////////////////////////
        virtual std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> keyOptions () const;

        // If true, collection has been deleted
        virtual bool deleted () const;

        // If true, collection will be compacted
        virtual bool doCompact () const;
        
        // If true, collection is a system collection
        virtual bool isSystem () const;

        // If true, collection is memory-only
        virtual bool isVolatile () const;

        // If true waits for mysnc
        virtual bool waitForSync () const;

        void setVersion (TRI_col_version_t);

        // Changes the name. Should only be called by TRI_RenameCollection
        // Use with caution!
        void rename (char const*);

        void setRevision (TRI_voc_rid_t,
                          bool);

        void setCollectionId (TRI_voc_cid_t);

        void updateCount (size_t);

        void setPlanId (TRI_voc_cid_t);

        void setDeleted (bool);

        void clearKeyOptions ();

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a parameter info block to file
////////////////////////////////////////////////////////////////////////////////

        int saveToFile (char const*,
                        bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief updates settings for this collection info.
///        If the second parameter is false it will only
///        update the values explicitly contained in the slice.
///        If the second parameter is true and the third is a nullptr,
///        it will use global default values for all missing options in the slice.
///        If the third parameter is not nullptr and the second is true, it will
///        use the defaults stored in the vocbase.
////////////////////////////////////////////////////////////////////////////////

        void update (VPackSlice const&,
                     bool,
                     TRI_vocbase_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates settings for this collection info with the content of the other
////////////////////////////////////////////////////////////////////////////////
        void update (VocbaseCollectionInfo const&);

    };

  } // namespace arango
} // namespace triagens

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

struct TRI_collection_t {
  triagens::arango::VocbaseCollectionInfo _info;

  TRI_vocbase_t*                   _vocbase;
  TRI_voc_tick_t                   _tickMax;

  TRI_col_state_e                  _state;        // state of the collection
  int                              _lastError;    // last (critical) error

  char*                            _directory;    // directory of the collection

  TRI_vector_pointer_t             _datafiles;    // all datafiles
  TRI_vector_pointer_t             _journals;     // all journals
  TRI_vector_pointer_t             _compactors;   // all compactor files
  TRI_vector_string_t              _indexFiles;   // all index filenames

  TRI_collection_t (
  ) {
  }

  explicit TRI_collection_t (
    triagens::arango::VocbaseCollectionInfo const& info
  ) : _info(info) {
  }

  ~TRI_collection_t () {
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the full directory name for a collection
///
/// it is the caller's responsibility to check if the returned string is NULL
/// and to free it if not.
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetDirectoryCollection (char const*,
                                  char const*,
                                  TRI_col_type_e,
                                  TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_CreateCollection (TRI_vocbase_t*,
                                        TRI_collection_t*,
                                        char const*,
                                        triagens::arango::VocbaseCollectionInfo const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCollection (TRI_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollection (TRI_collection_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return JSON information about the collection from the collection's
/// "parameter.json" file. This function does not require the collection to be
/// loaded.
/// The caller must make sure that the files is not modified while this
/// function is called.
////////////////////////////////////////////////////////////////////////////////

// TODO only temporary
struct TRI_json_t* TRI_ReadJsonCollectionInfo (TRI_vocbase_col_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over the index (JSON) files of a collection, using a callback
/// function for each.
/// This function does not require the collection to be loaded.
/// The caller must make sure that the files is not modified while this
/// function is called.
////////////////////////////////////////////////////////////////////////////////

int TRI_IterateJsonIndexesCollectionInfo (TRI_vocbase_col_t*,
                                          int (*)(TRI_vocbase_col_t*, char const*, void*),
                                          void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief jsonify a parameter info block
////////////////////////////////////////////////////////////////////////////////

// TODO only temporary
struct TRI_json_t* TRI_CreateJsonCollectionInfo (triagens::arango::VocbaseCollectionInfo const&);

std::shared_ptr<VPackBuilder> TRI_CreateVelocyPackCollectionInfo (triagens::arango::VocbaseCollectionInfo const&);

// Expects the builder to be in an open Object state
void TRI_CreateVelocyPackCollectionInfo (triagens::arango::VocbaseCollectionInfo const&,
                                         VPackBuilder&);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the parameter info block
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateCollectionInfo (TRI_vocbase_t*,
                              TRI_collection_t*,
                              VPackSlice const&,
                              bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollection (TRI_collection_t*, char const*);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file from the indexFiles vector
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveFileIndexCollection (TRI_collection_t*,
                                   TRI_idx_iid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateCollection (TRI_collection_t*,
                            bool (*)(TRI_df_marker_t const*, void*, TRI_datafile_t*),
                            void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over all index files of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateIndexCollection (TRI_collection_t*,
                                 bool (*)(char const*, void*),
                                 void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_OpenCollection (TRI_vocbase_t*,
                                      TRI_collection_t*,
                                      char const*,
                                      bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseCollection (TRI_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection files
///
/// Note that the collection must not be loaded
////////////////////////////////////////////////////////////////////////////////

TRI_col_file_structure_t TRI_FileStructureCollectionDirectory (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the information
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyFileStructureCollection (TRI_col_file_structure_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade a collection to ArangoDB 2.0+ format
////////////////////////////////////////////////////////////////////////////////

int TRI_UpgradeCollection20 (TRI_vocbase_t*,
                             char const*,
                             triagens::arango::VocbaseCollectionInfo&);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over the markers in the collection's journals
///
/// this function is called on server startup for all collections. we do this
/// to get the last tick used in a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateTicksCollection (const char* const,
                                 bool (*)(TRI_df_marker_t const*, void*, TRI_datafile_t*),
                                 void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection name is a system collection name
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemNameCollection (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a collection name is allowed
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameCollection (bool,
                                  char const*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
