////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_COLLECTION_H
#define ARANGOD_VOC_BASE_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/datafile.h"
#include "VocBase/vocbase.h"

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
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
class CollectionInfo;
namespace velocypack {
template <typename T>
class Buffer;
class Slice;
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief collection version for ArangoDB >= 2.0
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_VERSION_20 (5)

////////////////////////////////////////////////////////////////////////////////
/// @brief current collection version
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_VERSION TRI_COL_VERSION_20

////////////////////////////////////////////////////////////////////////////////
/// @brief predefined collection name for users
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_USERS "_users"

////////////////////////////////////////////////////////////////////////////////
/// @brief predefined collection name for statistics
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_NAME_STATISTICS "_statistics"

////////////////////////////////////////////////////////////////////////////////
/// @brief default number of index buckets
////////////////////////////////////////////////////////////////////////////////

#define TRI_DEFAULT_INDEX_BUCKETS 8

////////////////////////////////////////////////////////////////////////////////
/// @brief collection file structure
////////////////////////////////////////////////////////////////////////////////

struct TRI_col_file_structure_t {
  std::vector<std::string> journals;
  std::vector<std::string> compactors;
  std::vector<std::string> datafiles;
  std::vector<std::string> indexes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief state of the datafile
////////////////////////////////////////////////////////////////////////////////

enum TRI_col_state_e {
  TRI_COL_STATE_CLOSED = 1,      // collection is closed
  TRI_COL_STATE_READ = 2,        // collection is opened read only
  TRI_COL_STATE_WRITE = 3,       // collection is opened read/append
  TRI_COL_STATE_OPEN_ERROR = 4,  // an error has occurred while opening
  TRI_COL_STATE_WRITE_ERROR = 5  // an error has occurred while writing
};

////////////////////////////////////////////////////////////////////////////////
/// @brief collection version
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_col_version_t;

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief collection info block saved to disk as json
////////////////////////////////////////////////////////////////////////////////

class VocbaseCollectionInfo {
 private:
  TRI_col_version_t _version;   // collection version
  TRI_col_type_e _type;         // collection type
  TRI_voc_rid_t _revision;      // last revision id written
  TRI_voc_cid_t _cid;           // local collection identifier
  TRI_voc_cid_t _planId;        // cluster-wide collection identifier
  TRI_voc_size_t _maximalSize;  // maximal size of memory mapped file
  int64_t _initialCount;        // initial count, used when loading a collection
  uint32_t _indexBuckets;  // number of buckets used in hash tables for indexes

  char _name[TRI_COL_PATH_LENGTH];  // name of the collection()
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
      _keyOptions;  // options for key creation

  // flags
  bool _isSystem;     // if true, this is a system collection
  bool _deleted;      // if true, collection has been deleted
  bool _doCompact;    // if true, collection will be compacted
  bool _isVolatile;   // if true, collection is memory-only
  bool _waitForSync;  // if true, wait for msync

 public:
  VocbaseCollectionInfo(){};
  ~VocbaseCollectionInfo(){};

  explicit VocbaseCollectionInfo(CollectionInfo const&);

  VocbaseCollectionInfo(TRI_vocbase_t*, char const*, TRI_col_type_e,
                        TRI_voc_size_t, arangodb::velocypack::Slice const&);

  VocbaseCollectionInfo(TRI_vocbase_t*, char const*,
                        arangodb::velocypack::Slice const&);

  VocbaseCollectionInfo(TRI_vocbase_t*, char const*, TRI_col_type_e,
                        arangodb::velocypack::Slice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Creates a new VocbaseCollectionInfo from the json content of a file
  /// This function throws if the file cannot be parsed.
  ///
  /// You must hold the @ref TRI_READ_LOCK_STATUS_VOCBASE_COL when calling this
  /// function.
  //////////////////////////////////////////////////////////////////////////////

  static VocbaseCollectionInfo fromFile(char const*, TRI_vocbase_t*,
                                        char const*, bool);

  // collection version
  TRI_col_version_t version() const;

  // collection type
  TRI_col_type_e type() const;

  // local collection identifier
  TRI_voc_cid_t id() const;

  // cluster-wide collection identifier
  TRI_voc_cid_t planId() const;

  // last revision id written
  TRI_voc_rid_t revision() const;

  // maximal size of memory mapped file
  TRI_voc_size_t maximalSize() const;

  // initial count, used when loading a collection
  int64_t initialCount() const;

  // number of buckets used in hash tables for indexes
  uint32_t indexBuckets() const;

  // name of the collection
  std::string name() const;

  // name of the collection as c string
  char const* namec_str() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a copy of the key options
  /// the caller is responsible for freeing it
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> keyOptions()
      const;

  // If true, collection has been deleted
  bool deleted() const;

  // If true, collection will be compacted
  bool doCompact() const;

  // If true, collection is a system collection
  bool isSystem() const;

  // If true, collection is memory-only
  bool isVolatile() const;

  // If true waits for mysnc
  bool waitForSync() const;

  void setVersion(TRI_col_version_t);

  // Changes the name. Should only be called by TRI_RenameCollection
  // Use with caution!
  void rename(char const*);

  void setIsSystem(bool value) { _isSystem = value; }

  void setRevision(TRI_voc_rid_t, bool);

  void setCollectionId(TRI_voc_cid_t);

  void updateCount(size_t);

  void setPlanId(TRI_voc_cid_t);

  void setDeleted(bool);

  void clearKeyOptions();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief saves a parameter info block to file
  //////////////////////////////////////////////////////////////////////////////

  int saveToFile(std::string const&, bool) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates settings for this collection info.
  ///        If the second parameter is false it will only
  ///        update the values explicitly contained in the slice.
  ///        If the second parameter is true and the third is a nullptr,
  ///        it will use global default values for all missing options in the
  ///        slice.
  ///        If the third parameter is not nullptr and the second is true, it
  ///        will
  ///        use the defaults stored in the vocbase.
  //////////////////////////////////////////////////////////////////////////////

  void update(arangodb::velocypack::Slice const&, bool, TRI_vocbase_t const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates settings for this collection info with the content of the
  /// other
  //////////////////////////////////////////////////////////////////////////////

  void update(VocbaseCollectionInfo const&);
};

}  // namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

struct TRI_collection_t {
 public:
  TRI_collection_t(TRI_collection_t const&) = delete;
  TRI_collection_t& operator=(TRI_collection_t const&) = delete;

  TRI_collection_t() = delete;
  
  explicit TRI_collection_t(TRI_vocbase_t* vocbase)
      : _vocbase(vocbase), _tickMax(0), _state(TRI_COL_STATE_WRITE), _lastError(0) {}

  TRI_collection_t(TRI_vocbase_t* vocbase, arangodb::VocbaseCollectionInfo const& info)
      : _vocbase(vocbase), _tickMax(0), _info(info), _state(TRI_COL_STATE_WRITE), _lastError(0) {}

  ~TRI_collection_t() = default;

 public:
  void iterateIndexes(std::function<bool(std::string const&, void*)> const&, void*);

  /// @brief updates the parameter info block
  int updateCollectionInfo(TRI_vocbase_t* vocbase,
                           arangodb::velocypack::Slice const& slice, bool doSync);

  // datafile management
  
  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal();

  /// @brief sync the active journal - will do nothing if there is no journal
  /// or if the journal is volatile
  int syncActiveJournal();
  int reserveJournalSpace(TRI_voc_tick_t tick, TRI_voc_size_t size,
                          char*& resultPosition, TRI_datafile_t*& resultDatafile);

  /// @brief create compactor file
  TRI_datafile_t* createCompactor(TRI_voc_fid_t fid, TRI_voc_size_t maximalSize);
  /// @brief close an existing compactor
  int closeCompactor(TRI_datafile_t* datafile);
  /// @brief replace a datafile with a compactor
  int replaceDatafileWithCompactor(TRI_datafile_t* datafile, TRI_datafile_t* compactor);

  bool removeCompactor(TRI_datafile_t*);
  bool removeDatafile(TRI_datafile_t*);
  void addIndexFile(std::string const&);
  int removeIndexFile(TRI_idx_iid_t);

 private:
  /// @brief seal a datafile
  int sealDatafile(TRI_datafile_t* datafile, bool isCompactor);
  /// @brief creates a datafile
  TRI_datafile_t* createDatafile(TRI_voc_fid_t fid,
                                 TRI_voc_size_t journalSize, 
                                 bool isCompactor);

 public:
  TRI_vocbase_t* _vocbase;
  TRI_voc_tick_t _tickMax;
 
  /// @brief a lock protecting the _info structure
  arangodb::basics::ReadWriteLock _infoLock;
  arangodb::VocbaseCollectionInfo _info;

  TRI_col_state_e _state;  // state of the collection
  int _lastError;          // last (critical) error

  std::string _directory;  // directory of the collection

  arangodb::basics::ReadWriteLock _filesLock;
  std::vector<TRI_datafile_t*> _datafiles;   // all datafiles
  std::vector<TRI_datafile_t*> _journals;    // all journals
  std::vector<TRI_datafile_t*> _compactors;  // all compactor files
 private:
  std::vector<std::string> _indexFiles;   // all index filenames

};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_CreateCollection(TRI_vocbase_t*, TRI_collection_t*,
                                       char const*,
                                       arangodb::VocbaseCollectionInfo const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCollection(TRI_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollection(TRI_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a parameter info block to velocypack
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<arangodb::velocypack::Builder>
TRI_CreateVelocyPackCollectionInfo(arangodb::VocbaseCollectionInfo const&);

// Expects the builder to be in an open Object state
void TRI_CreateVelocyPackCollectionInfo(arangodb::VocbaseCollectionInfo const&,
                                        arangodb::velocypack::Builder&);

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameCollection(TRI_collection_t*, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateCollection(TRI_collection_t*, bool (*)(TRI_df_marker_t const*,
                                                       void*, TRI_datafile_t*),
                           void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_OpenCollection(TRI_vocbase_t*, TRI_collection_t*,
                                     char const*, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseCollection(TRI_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection files
///
/// Note that the collection must not be loaded
////////////////////////////////////////////////////////////////////////////////

TRI_col_file_structure_t TRI_FileStructureCollectionDirectory(char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over the markers in the collection's journals
///
/// this function is called on server startup for all collections. we do this
/// to get the last tick used in a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateTicksCollection(char const* const,
                                bool (*)(TRI_df_marker_t const*, void*,
                                         TRI_datafile_t*),
                                void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection name is a system collection name
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSystemNameCollection(char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a collection name is allowed
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsAllowedNameCollection(bool, char const*);

#endif
