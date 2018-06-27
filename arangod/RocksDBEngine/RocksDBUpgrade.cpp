////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBUpgrade.h"

#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/statistics.h>
#include <rocksdb/table.h>
#include <rocksdb/transaction_log.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/write_batch.h>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace {
  
  bool upgradeRoutine(rocksdb::TransactionDB* db,
                      char oldVersion, char currentVersion) {

    if (oldVersion == '0' && currentVersion == '1') {
      static_assert(std::is_same<char, std::underlying_type<RocksDBEndianness>::type>::value,
                    "RocksDBEndianness has wrong type");
      
      rocksDBKeyFormatEndianess = RocksDBEndianness::Little;
      // store endianess forever
      RocksDBKey key;
      key.constructSettingsValue(RocksDBSettingsType::Endianness);
      const char endVal = static_cast<char>(rocksDBKeyFormatEndianess);
      rocksdb::Status s;
      s = db->Put(rocksdb::WriteOptions(), RocksDBColumnFamily::definitions(),
                  key.string(), rocksdb::Slice(&endVal, sizeof(char)));
      if (!s.ok()) {
        LOG_TOPIC(FATAL, Logger::ENGINES) << "Error storing endianess";
        FATAL_ERROR_EXIT();
      }
      
      return true;
    }
    
    return false; // unhandled case
  }
}

void arangodb::rocksdbStartupVersionCheck(rocksdb::TransactionDB* db,
                                          bool dbExisted) {
  
  // try to find version
  char version = rocksDBFormatVersion();
  RocksDBKey key;
  key.constructSettingsValue(RocksDBSettingsType::Version);
  rocksdb::PinnableSlice oldVersion, endianess;
  rocksdb::Status s;
  s = db->Get(rocksdb::ReadOptions(), RocksDBColumnFamily::definitions(),
               key.string(), &oldVersion);
  if (dbExisted) {
    if (s.IsNotFound()) {
      LOG_TOPIC(FATAL, Logger::ENGINES) << "Your db directory is invalid";
      FATAL_ERROR_EXIT();
    } else if (oldVersion.data()[0] < version) {
      
      if (!upgradeRoutine(db, oldVersion.data()[0], version)) {
        LOG_TOPIC(FATAL, Logger::ENGINES) << "Your db directory is in an old "
        << "format. Please downgrade the server, "
        << "export & re-import your data.";
        FATAL_ERROR_EXIT();
      }
      
    } else if (oldVersion.data()[0] > version) {
      LOG_TOPIC(FATAL, Logger::ENGINES)
      << "You are using an old version of ArangoDB, please update "
      << "before opening this dir.";
      FATAL_ERROR_EXIT();
    } else if (oldVersion.data()[0] == version) {
      
      // read current endianess
      key.constructSettingsValue(RocksDBSettingsType::Endianness);
      s = db->Get(rocksdb::ReadOptions(), RocksDBColumnFamily::definitions(),
                  key.string(), &endianess);
      if (s.ok()) {
        rocksDBKeyFormatEndianess = static_cast<RocksDBEndianness>(endianess.data()[0]);
      } else if (s.IsNotFound()) {
        rocksDBKeyFormatEndianess = RocksDBEndianness::Little;
      } else {
        LOG_TOPIC(FATAL, Logger::ENGINES)
        << "Error reading key-format";
        FATAL_ERROR_EXIT();
      }
    }
    
  } else {
    // new DBs are always created with Big endian data-format
    rocksDBKeyFormatEndianess = RocksDBEndianness::Big;
  }
  
  // store current version
  s = db->Put(rocksdb::WriteOptions(), RocksDBColumnFamily::definitions(),
               key.string(), rocksdb::Slice(&version, sizeof(char)));
  TRI_ASSERT(s.ok());
  
}
