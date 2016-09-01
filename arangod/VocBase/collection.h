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
#include "Cluster/ClusterInfo.h"
#include "VocBase/Ditch.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/vocbase.h"

namespace arangodb {
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



  void clearKeyOptions();
};

}  // namespace arangodb

struct TRI_collection_t {
 public:
  TRI_collection_t(TRI_collection_t const&) = delete;
  TRI_collection_t& operator=(TRI_collection_t const&) = delete;
  TRI_collection_t() = delete;
  
  explicit TRI_collection_t(TRI_vocbase_t* vocbase);
  ~TRI_collection_t();

 public:
  /// @brief create a new collection
  static TRI_collection_t* create(TRI_vocbase_t*, TRI_voc_cid_t);

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

  // datafile management
  
  std::string const& path() const { return _path; }

  double lastCompaction() const { return _lastCompaction; }
  void lastCompaction(double value) { _lastCompaction = value; }
  
  arangodb::Ditches* ditches() { return &_ditches; }
  
 public:
  TRI_vocbase_t* _vocbase;
  TRI_voc_tick_t _tickMax;
 
 private: 
  std::string _path;

 public:
  std::atomic<int64_t> _uncollectedLogfileEntries;
  
 private:
  mutable arangodb::Ditches _ditches;

  arangodb::Mutex _compactionStatusLock;
  size_t _nextCompactionStartIndex;
  char const* _lastCompactionStatus;
  char _lastCompactionStamp[21];
  double _lastCompaction;
};

#endif
