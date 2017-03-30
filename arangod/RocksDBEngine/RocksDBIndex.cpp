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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include "StorageEngine/EngineSelectorFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Logger/Logger.h"

#include <rocksdb/convenience.h>
#include <rocksdb/comparator.h>

using namespace arangodb;

RocksDBIndex::RocksDBIndex(
    TRI_idx_iid_t id, LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique, bool sparse, uint64_t objectId)
    : Index(id, collection, attributes, unique, sparse),
      _objectId(TRI_NewTickServer()), // TODO!
      _cmp(static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->cmp()) {}

RocksDBIndex::RocksDBIndex(TRI_idx_iid_t id, LogicalCollection* collection, VPackSlice const& info)
: Index(id, collection, info),
_objectId(TRI_NewTickServer()), // TODO!
_cmp(static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->cmp()) {}

int RocksDBIndex::removeLargeRange(RocksDBKeyBounds bounds) {
  
  try {
    
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    RocksDBEngine *rocks = static_cast<RocksDBEngine*>(engine);
    rocksdb::DB* db = rocks->db();
    
    // delete files in range lower..upper
    rocksdb::Slice lower(bounds.start());
    rocksdb::Slice upper(bounds.end());
    {
      rocksdb::Status status = rocksdb::DeleteFilesInRange(db, db->DefaultColumnFamily(),
                                                           &lower, &upper);
      if (!status.ok()) {
        // if file deletion failed, we will still iterate over the remaining keys, so we
        // don't need to abort and raise an error here
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "RocksDB file deletion failed";
      }
    }
    
    // go on and delete the remaining keys (delete files in range does not necessarily
    // find them all, just complete files)
    
    
    rocksdb::WriteBatch batch;
    std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(rocksdb::ReadOptions()));
    
    it->Seek(lower);
    while (it->Valid()) {
      int res = _cmp->Compare(it->key(), upper);
      
      if (res >= 0) {
        break;
      }
      
      batch.Delete(it->key());
      
      it->Next();
    }
    
    // now apply deletion batch
    rocksdb::Status status = db->Write(rocksdb::WriteOptions(), &batch);
    
    if (!status.ok()) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "RocksDB key deletion failed: " << status.ToString();
      return TRI_ERROR_INTERNAL;
    }
    
    return TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return ex.code();
  } catch (std::exception const& ex) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception during RocksDB key prefix deletion: " << ex.what();
    return TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception during RocksDB key prefix deletion";
    return TRI_ERROR_INTERNAL;
  }
}
