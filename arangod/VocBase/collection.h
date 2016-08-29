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
#include "Cluster/ClusterInfo.h"
#include "VocBase/Ditch.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class EdgeIndex;
class Index;
struct OperationOptions;
class PrimaryIndex;
class StringRef;
class Transaction;
namespace velocypack {
template <typename T>
class Buffer;
class Slice;
}
namespace wal {
struct DocumentOperation;
}
}

/// @brief predefined collection name for users
#define TRI_COL_NAME_USERS "_users"

namespace arangodb {

/// @brief collection info block saved to disk as json
class VocbaseCollectionInfo {
 private:
  TRI_col_type_e _type;         // collection type
  TRI_voc_cid_t _cid;           // local collection identifier
  TRI_voc_cid_t _planId;        // cluster-wide collection identifier
  TRI_voc_size_t _maximalSize;  // maximal size of memory mapped file
  int64_t _initialCount;        // initial count, used when loading a collection
  uint32_t _indexBuckets;  // number of buckets used in hash tables for indexes

  char _name[512];  // name of the collection
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
      _keyOptions;  // options for key creation

  // flags
  bool _isSystem;     // if true, this is a system collection
  bool _deleted;      // if true, collection has been deleted
  bool _doCompact;    // if true, collection will be compacted
  bool _isVolatile;   // if true, collection is memory-only
  bool _waitForSync;  // if true, wait for msync

 public:
  VocbaseCollectionInfo() = default;
  ~VocbaseCollectionInfo() = default;

  VocbaseCollectionInfo(TRI_vocbase_t*, std::string const&, TRI_col_type_e,
                        TRI_voc_size_t, arangodb::velocypack::Slice const&);

  VocbaseCollectionInfo(TRI_vocbase_t*, std::string const&,
                        arangodb::velocypack::Slice const&,
                        bool forceIsSystem);

  VocbaseCollectionInfo(TRI_vocbase_t*, std::string const&, TRI_col_type_e,
                        arangodb::velocypack::Slice const&,
                        bool forceIsSystem);

  std::shared_ptr<VPackBuilder> toVelocyPack() const;
  void toVelocyPack(VPackBuilder& builder) const;

  // collection version
  static constexpr uint32_t version() { return 5; }

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

  // Changes the name. Should only be called by TRI_collection_t::rename()
  // Use with caution!
  void rename(std::string const&);

  void setCollectionId(TRI_voc_cid_t);

  void setPlanId(TRI_voc_cid_t);

  void updateCount(size_t);


  void setDeleted(bool);

  void clearKeyOptions();

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

struct TRI_collection_t {
 public:
  TRI_collection_t(TRI_collection_t const&) = delete;
  TRI_collection_t& operator=(TRI_collection_t const&) = delete;
  TRI_collection_t() = delete;
  
  TRI_collection_t(TRI_vocbase_t* vocbase, arangodb::VocbaseCollectionInfo const& parameters);
  ~TRI_collection_t();

 public:
  /// @brief create a new collection
  static TRI_collection_t* create(TRI_vocbase_t*, arangodb::VocbaseCollectionInfo&, TRI_voc_cid_t);

  /// @brief opens an existing collection
  static TRI_collection_t* open(TRI_vocbase_t*, arangodb::LogicalCollection*, bool);

 /// @brief checks if a collection name is allowed
  /// returns true if the name is allowed and false otherwise
  static bool IsAllowedName(bool isSystem, std::string const& name);
  
  bool isFullyCollected();

  void setNextCompactionStartIndex(size_t);
  size_t getNextCompactionStartIndex();
  void setCompactionStatus(char const*);
  void getCompactionStatus(char const*&, char*, size_t);
  
  void figures(std::shared_ptr<arangodb::velocypack::Builder>& result);

  int beginRead();
  int endRead();
  int beginWrite();
  int endWrite();
  int beginReadTimed(uint64_t, uint64_t);
  int beginWriteTimed(uint64_t, uint64_t);

  // datafile management
  
  std::string const& path() const { return _path; }
  std::string label() const;

  double lastCompaction() const { return _lastCompaction; }
  void lastCompaction(double value) { _lastCompaction = value; }
  
  std::unique_ptr<arangodb::FollowerInfo> const& followers() const {
    return _followers;
  }
  
  arangodb::Ditches* ditches() { return &_ditches; }
  
  void setPath(std::string const& path) { _path = path; }

  /// @brief renames a collection
  int rename(std::string const& name);

 private:
  /// @brief creates the initial indexes for the collection
  int createInitialIndexes();

  int deleteSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);

 public:
  TRI_vocbase_t* _vocbase;
  TRI_voc_tick_t _tickMax;
 
  /// @brief a lock protecting the _info structure
  arangodb::basics::ReadWriteLock _infoLock;
  arangodb::VocbaseCollectionInfo _info;

 private: 
  std::string _path;

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<arangodb::FollowerInfo> _followers;

 public:
  std::atomic<int64_t> _uncollectedLogfileEntries;
  
 private:
  mutable arangodb::Ditches _ditches;
  mutable arangodb::basics::ReadWriteLock _lock;  // lock protecting the indexes

  arangodb::Mutex _compactionStatusLock;
  size_t _nextCompactionStartIndex;
  char const* _lastCompactionStatus;
  char _lastCompactionStamp[21];
  double _lastCompaction;
};

#endif
