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

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBFormat.h"

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

void arangodb::rocksdbStartupVersionCheck(rocksdb::TransactionDB* db, bool dbExisted) {
  static_assert(std::is_same<char, std::underlying_type<RocksDBEndianness>::type>::value,
                "RocksDBEndianness has wrong type");

  // try to find version, using the version key
  char const version = rocksDBFormatVersion();
  RocksDBKey versionKey, endianKey;
  versionKey.constructSettingsValue(RocksDBSettingsType::Version);
  endianKey.constructSettingsValue(RocksDBSettingsType::Endianness);

  // default endianess for existing DBs
  RocksDBEndianness endianess = RocksDBEndianness::Invalid;
  
  if (dbExisted) {
    rocksdb::PinnableSlice oldVersion;
    rocksdb::Status s = db->Get(rocksdb::ReadOptions(),
                                RocksDBColumnFamily::definitions(),
                                versionKey.string(), &oldVersion);

    if (s.IsNotFound() || oldVersion.size() != 1) {
      LOG_TOPIC("614d7", FATAL, Logger::ENGINES) 
        << "Error reading stored version from database";
      FATAL_ERROR_EXIT();
    } else if (oldVersion.data()[0] < version) {
      // Performing 'upgrade' routine
      if (oldVersion.data()[0] == '0' && version == '1') {
        endianess = RocksDBEndianness::Little;
      } else {
        LOG_TOPIC("c30ee", FATAL, Logger::ENGINES)
            << "Your database is in an old "
            << "format. Please downgrade the server, "
            << "dump & restore the data";
        FATAL_ERROR_EXIT();
      }

    } else if (oldVersion.data()[0] > version) {
      LOG_TOPIC("c9009", FATAL, Logger::ENGINES)
          << "You are using an old version of ArangoDB, please update "
          << "before opening this database";
      FATAL_ERROR_EXIT();
    } else {
      TRI_ASSERT(oldVersion.data()[0] == version);

      // read current endianess
      rocksdb::PinnableSlice endianSlice;
      s = db->Get(rocksdb::ReadOptions(), RocksDBColumnFamily::definitions(),
                  endianKey.string(), &endianSlice);
      if (s.ok() && endianSlice.size() == 1) {
        TRI_ASSERT(endianSlice.data()[0] == 'L' || endianSlice.data()[0] == 'B');
        endianess = static_cast<RocksDBEndianness>(endianSlice.data()[0]);
      } else {
        LOG_TOPIC("b0083", FATAL, Logger::ENGINES)
            << "Error reading key-format, your db directory is invalid";
        FATAL_ERROR_EXIT();
      }
    }

  } else {
    // new DBs are always created with Big endian data-format
    endianess = RocksDBEndianness::Big;
  }
  // enable correct key format
  TRI_ASSERT(endianess == RocksDBEndianness::Little || endianess == RocksDBEndianness::Big);
  rocksutils::setRocksDBKeyFormatEndianess(endianess);

  if (!dbExisted) {
    // store endianess forever
    TRI_ASSERT(endianess == RocksDBEndianness::Big);

    char const endVal = static_cast<char>(endianess);
    rocksdb::Status s = db->Put(rocksdb::WriteOptions(),
                                RocksDBColumnFamily::definitions(),
                                endianKey.string(), rocksdb::Slice(&endVal, sizeof(char)));

    if (s.ok()) {
      // store current version
      s = db->Put(rocksdb::WriteOptions(), RocksDBColumnFamily::definitions(),
                  versionKey.string(), rocksdb::Slice(&version, sizeof(char)));
    }
  
    if (!s.ok()) {
      LOG_TOPIC("3d88b", FATAL, Logger::ENGINES) 
          << "Error storing endianess/version: " 
          << rocksutils::convertStatus(s).errorMessage();
      FATAL_ERROR_EXIT();
    }
  }
}
