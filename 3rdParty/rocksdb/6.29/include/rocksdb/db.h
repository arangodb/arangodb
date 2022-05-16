// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "rocksdb/iterator.h"
#include "rocksdb/listener.h"
#include "rocksdb/metadata.h"
#include "rocksdb/options.h"
#include "rocksdb/snapshot.h"
#include "rocksdb/sst_file_writer.h"
#include "rocksdb/thread_status.h"
#include "rocksdb/transaction_log.h"
#include "rocksdb/types.h"
#include "rocksdb/version.h"

#ifdef _WIN32
// Windows API macro interference
#undef DeleteFile
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ROCKSDB_DEPRECATED_FUNC __attribute__((__deprecated__))
#elif _WIN32
#define ROCKSDB_DEPRECATED_FUNC __declspec(deprecated)
#endif

namespace ROCKSDB_NAMESPACE {

struct ColumnFamilyOptions;
struct CompactionOptions;
struct CompactRangeOptions;
struct DBOptions;
struct ExternalSstFileInfo;
struct FlushOptions;
struct Options;
struct ReadOptions;
struct TableProperties;
struct WriteOptions;
#ifdef ROCKSDB_LITE
class CompactionJobInfo;
#endif
class Env;
class EventListener;
class FileSystem;
#ifndef ROCKSDB_LITE
class Replayer;
#endif
class StatsHistoryIterator;
#ifndef ROCKSDB_LITE
class TraceReader;
class TraceWriter;
#endif
class WriteBatch;

extern const std::string kDefaultColumnFamilyName;
extern const std::string kPersistentStatsColumnFamilyName;
struct ColumnFamilyDescriptor {
  std::string name;
  ColumnFamilyOptions options;
  ColumnFamilyDescriptor()
      : name(kDefaultColumnFamilyName), options(ColumnFamilyOptions()) {}
  ColumnFamilyDescriptor(const std::string& _name,
                         const ColumnFamilyOptions& _options)
      : name(_name), options(_options) {}
};

class ColumnFamilyHandle {
 public:
  virtual ~ColumnFamilyHandle() {}
  // Returns the name of the column family associated with the current handle.
  virtual const std::string& GetName() const = 0;
  // Returns the ID of the column family associated with the current handle.
  virtual uint32_t GetID() const = 0;
  // Fills "*desc" with the up-to-date descriptor of the column family
  // associated with this handle. Since it fills "*desc" with the up-to-date
  // information, this call might internally lock and release DB mutex to
  // access the up-to-date CF options.  In addition, all the pointer-typed
  // options cannot be referenced any longer than the original options exist.
  //
  // Note that this function is not supported in RocksDBLite.
  virtual Status GetDescriptor(ColumnFamilyDescriptor* desc) = 0;
  // Returns the comparator of the column family associated with the
  // current handle.
  virtual const Comparator* GetComparator() const = 0;
};

static const int kMajorVersion = __ROCKSDB_MAJOR__;
static const int kMinorVersion = __ROCKSDB_MINOR__;

// A range of keys
struct Range {
  Slice start;
  Slice limit;

  Range() {}
  Range(const Slice& s, const Slice& l) : start(s), limit(l) {}
};

struct RangePtr {
  const Slice* start;
  const Slice* limit;

  RangePtr() : start(nullptr), limit(nullptr) {}
  RangePtr(const Slice* s, const Slice* l) : start(s), limit(l) {}
};

// It is valid that files_checksums and files_checksum_func_names are both
// empty (no checksum information is provided for ingestion). Otherwise,
// their sizes should be the same as external_files. The file order should
// be the same in three vectors and guaranteed by the caller.
// Note that, we assume the temperatures of this batch of files to be
// ingested are the same.
struct IngestExternalFileArg {
  ColumnFamilyHandle* column_family = nullptr;
  std::vector<std::string> external_files;
  IngestExternalFileOptions options;
  std::vector<std::string> files_checksums;
  std::vector<std::string> files_checksum_func_names;
  Temperature file_temperature = Temperature::kUnknown;
};

struct GetMergeOperandsOptions {
  int expected_max_number_of_operands = 0;
};

// A collections of table properties objects, where
//  key: is the table's file name.
//  value: the table properties object of the given table.
using TablePropertiesCollection =
    std::unordered_map<std::string, std::shared_ptr<const TableProperties>>;

// A DB is a persistent, versioned ordered map from keys to values.
// A DB is safe for concurrent access from multiple threads without
// any external synchronization.
// DB is an abstract base class with one primary implementation (DBImpl)
// and a number of wrapper implementations.
class DB {
 public:
  // Open the database with the specified "name" for reads and writes.
  // Stores a pointer to a heap-allocated database in *dbptr and returns
  // OK on success.
  // Stores nullptr in *dbptr and returns a non-OK status on error, including
  // if the DB is already open (read-write) by another DB object. (This
  // guarantee depends on options.env->LockFile(), which might not provide
  // this guarantee in a custom Env implementation.)
  //
  // Caller must delete *dbptr when it is no longer needed.
  static Status Open(const Options& options, const std::string& name,
                     DB** dbptr);

  // Open the database for read only. All DB interfaces
  // that modify data, like put/delete, will return error.
  // If the db is opened in read only mode, then no compactions
  // will happen.
  //
  // While a given DB can be simultaneously open via OpenForReadOnly
  // by any number of readers, if a DB is simultaneously open by Open
  // and OpenForReadOnly, the read-only instance has undefined behavior
  // (though can often succeed if quickly closed) and the read-write
  // instance is unaffected. See also OpenAsSecondary.
  //
  // Not supported in ROCKSDB_LITE, in which case the function will
  // return Status::NotSupported.
  static Status OpenForReadOnly(const Options& options, const std::string& name,
                                DB** dbptr,
                                bool error_if_wal_file_exists = false);

  // Open the database for read only with column families. When opening DB with
  // read only, you can specify only a subset of column families in the
  // database that should be opened. However, you always need to specify default
  // column family. The default column family name is 'default' and it's stored
  // in ROCKSDB_NAMESPACE::kDefaultColumnFamilyName
  //
  // While a given DB can be simultaneously open via OpenForReadOnly
  // by any number of readers, if a DB is simultaneously open by Open
  // and OpenForReadOnly, the read-only instance has undefined behavior
  // (though can often succeed if quickly closed) and the read-write
  // instance is unaffected. See also OpenAsSecondary.
  //
  // Not supported in ROCKSDB_LITE, in which case the function will
  // return Status::NotSupported.
  static Status OpenForReadOnly(
      const DBOptions& db_options, const std::string& name,
      const std::vector<ColumnFamilyDescriptor>& column_families,
      std::vector<ColumnFamilyHandle*>* handles, DB** dbptr,
      bool error_if_wal_file_exists = false);

  // The following OpenAsSecondary functions create a secondary instance that
  // can dynamically tail the MANIFEST of a primary that must have already been
  // created. User can call TryCatchUpWithPrimary to make the secondary
  // instance catch up with primary (WAL tailing is NOT supported now) whenever
  // the user feels necessary. Column families created by the primary after the
  // secondary instance starts are currently ignored by the secondary instance.
  // Column families opened by secondary and dropped by the primary will be
  // dropped by secondary as well. However the user of the secondary instance
  // can still access the data of such dropped column family as long as they
  // do not destroy the corresponding column family handle.
  // WAL tailing is not supported at present, but will arrive soon.
  //
  // The options argument specifies the options to open the secondary instance.
  // The name argument specifies the name of the primary db that you have used
  // to open the primary instance.
  // The secondary_path argument points to a directory where the secondary
  // instance stores its info log.
  // The dbptr is an out-arg corresponding to the opened secondary instance.
  // The pointer points to a heap-allocated database, and the user should
  // delete it after use.
  // Open DB as secondary instance with only the default column family.
  // Return OK on success, non-OK on failures.
  static Status OpenAsSecondary(const Options& options, const std::string& name,
                                const std::string& secondary_path, DB** dbptr);

  // Open DB as secondary instance with column families. You can open a subset
  // of column families in secondary mode.
  // The db_options specify the database specific options.
  // The name argument specifies the name of the primary db that you have used
  // to open the primary instance.
  // The secondary_path argument points to a directory where the secondary
  // instance stores its info log.
  // The column_families argument specifies a list of column families to open.
  // If any of the column families does not exist, the function returns non-OK
  // status.
  // The handles is an out-arg corresponding to the opened database column
  // family handles.
  // The dbptr is an out-arg corresponding to the opened secondary instance.
  // The pointer points to a heap-allocated database, and the caller should
  // delete it after use. Before deleting the dbptr, the user should also
  // delete the pointers stored in handles vector.
  // Return OK on success, on-OK on failures.
  static Status OpenAsSecondary(
      const DBOptions& db_options, const std::string& name,
      const std::string& secondary_path,
      const std::vector<ColumnFamilyDescriptor>& column_families,
      std::vector<ColumnFamilyHandle*>* handles, DB** dbptr);

  // Open DB with column families.
  // db_options specify database specific options
  // column_families is the vector of all column families in the database,
  // containing column family name and options. You need to open ALL column
  // families in the database. To get the list of column families, you can use
  // ListColumnFamilies(). Also, you can open only a subset of column families
  // for read-only access.
  // The default column family name is 'default' and it's stored
  // in ROCKSDB_NAMESPACE::kDefaultColumnFamilyName.
  // If everything is OK, handles will on return be the same size
  // as column_families --- handles[i] will be a handle that you
  // will use to operate on column family column_family[i].
  // Before delete DB, you have to close All column families by calling
  // DestroyColumnFamilyHandle() with all the handles.
  static Status Open(const DBOptions& db_options, const std::string& name,
                     const std::vector<ColumnFamilyDescriptor>& column_families,
                     std::vector<ColumnFamilyHandle*>* handles, DB** dbptr);

  // Open DB and run the compaction.
  // It's a read-only operation, the result won't be installed to the DB, it
  // will be output to the `output_directory`. The API should only be used with
  // `options.CompactionService` to run compaction triggered by
  // `CompactionService`.
  static Status OpenAndCompact(
      const std::string& name, const std::string& output_directory,
      const std::string& input, std::string* output,
      const CompactionServiceOptionsOverride& override_options);

  virtual Status Resume() { return Status::NotSupported(); }

  // Close the DB by releasing resources, closing files etc. This should be
  // called before calling the destructor so that the caller can get back a
  // status in case there are any errors. This will not fsync the WAL files.
  // If syncing is required, the caller must first call SyncWAL(), or Write()
  // using an empty write batch with WriteOptions.sync=true.
  // Regardless of the return status, the DB must be freed.
  // If the return status is Aborted(), closing fails because there is
  // unreleased snapshot in the system. In this case, users can release
  // the unreleased snapshots and try again and expect it to succeed. For
  // other status, re-calling Close() will be no-op and return the original
  // close status. If the return status is NotSupported(), then the DB
  // implementation does cleanup in the destructor
  virtual Status Close() { return Status::NotSupported(); }

  // ListColumnFamilies will open the DB specified by argument name
  // and return the list of all column families in that DB
  // through column_families argument. The ordering of
  // column families in column_families is unspecified.
  static Status ListColumnFamilies(const DBOptions& db_options,
                                   const std::string& name,
                                   std::vector<std::string>* column_families);

  // Abstract class ctor
  DB() {}
  // No copying allowed
  DB(const DB&) = delete;
  void operator=(const DB&) = delete;

  virtual ~DB();

  // Create a column_family and return the handle of column family
  // through the argument handle.
  virtual Status CreateColumnFamily(const ColumnFamilyOptions& options,
                                    const std::string& column_family_name,
                                    ColumnFamilyHandle** handle);

  // Bulk create column families with the same column family options.
  // Return the handles of the column families through the argument handles.
  // In case of error, the request may succeed partially, and handles will
  // contain column family handles that it managed to create, and have size
  // equal to the number of created column families.
  virtual Status CreateColumnFamilies(
      const ColumnFamilyOptions& options,
      const std::vector<std::string>& column_family_names,
      std::vector<ColumnFamilyHandle*>* handles);

  // Bulk create column families.
  // Return the handles of the column families through the argument handles.
  // In case of error, the request may succeed partially, and handles will
  // contain column family handles that it managed to create, and have size
  // equal to the number of created column families.
  virtual Status CreateColumnFamilies(
      const std::vector<ColumnFamilyDescriptor>& column_families,
      std::vector<ColumnFamilyHandle*>* handles);

  // Drop a column family specified by column_family handle. This call
  // only records a drop record in the manifest and prevents the column
  // family from flushing and compacting.
  virtual Status DropColumnFamily(ColumnFamilyHandle* column_family);

  // Bulk drop column families. This call only records drop records in the
  // manifest and prevents the column families from flushing and compacting.
  // In case of error, the request may succeed partially. User may call
  // ListColumnFamilies to check the result.
  virtual Status DropColumnFamilies(
      const std::vector<ColumnFamilyHandle*>& column_families);

  // Close a column family specified by column_family handle and destroy
  // the column family handle specified to avoid double deletion. This call
  // deletes the column family handle by default. Use this method to
  // close column family instead of deleting column family handle directly
  virtual Status DestroyColumnFamilyHandle(ColumnFamilyHandle* column_family);

  // Set the database entry for "key" to "value".
  // If "key" already exists, it will be overwritten.
  // Returns OK on success, and a non-OK status on error.
  // Note: consider setting options.sync = true.
  virtual Status Put(const WriteOptions& options,
                     ColumnFamilyHandle* column_family, const Slice& key,
                     const Slice& value) = 0;
  virtual Status Put(const WriteOptions& options, const Slice& key,
                     const Slice& value) {
    return Put(options, DefaultColumnFamily(), key, value);
  }

  // Remove the database entry (if any) for "key".  Returns OK on
  // success, and a non-OK status on error.  It is not an error if "key"
  // did not exist in the database.
  // Note: consider setting options.sync = true.
  virtual Status Delete(const WriteOptions& options,
                        ColumnFamilyHandle* column_family,
                        const Slice& key) = 0;
  virtual Status Delete(const WriteOptions& options, const Slice& key) {
    return Delete(options, DefaultColumnFamily(), key);
  }

  // Remove the database entry for "key". Requires that the key exists
  // and was not overwritten. Returns OK on success, and a non-OK status
  // on error.  It is not an error if "key" did not exist in the database.
  //
  // If a key is overwritten (by calling Put() multiple times), then the result
  // of calling SingleDelete() on this key is undefined.  SingleDelete() only
  // behaves correctly if there has been only one Put() for this key since the
  // previous call to SingleDelete() for this key.
  //
  // This feature is currently an experimental performance optimization
  // for a very specific workload.  It is up to the caller to ensure that
  // SingleDelete is only used for a key that is not deleted using Delete() or
  // written using Merge().  Mixing SingleDelete operations with Deletes and
  // Merges can result in undefined behavior.
  //
  // Note: consider setting options.sync = true.
  virtual Status SingleDelete(const WriteOptions& options,
                              ColumnFamilyHandle* column_family,
                              const Slice& key) = 0;
  virtual Status SingleDelete(const WriteOptions& options, const Slice& key) {
    return SingleDelete(options, DefaultColumnFamily(), key);
  }

  // Removes the database entries in the range ["begin_key", "end_key"), i.e.,
  // including "begin_key" and excluding "end_key". Returns OK on success, and
  // a non-OK status on error. It is not an error if the database does not
  // contain any existing data in the range ["begin_key", "end_key").
  //
  // If "end_key" comes before "start_key" according to the user's comparator,
  // a `Status::InvalidArgument` is returned.
  //
  // WARNING: Do not use `Iterator::Refresh()` API on DBs where `DeleteRange()`
  // has been used or will be used. This feature combination is neither
  // supported nor programmatically prevented.
  //
  // This feature is now usable in production, with the following caveats:
  // 1) Accumulating many range tombstones in the memtable will degrade read
  // performance; this can be avoided by manually flushing occasionally.
  // 2) Limiting the maximum number of open files in the presence of range
  // tombstones can degrade read performance. To avoid this problem, set
  // max_open_files to -1 whenever possible.
  virtual Status DeleteRange(const WriteOptions& options,
                             ColumnFamilyHandle* column_family,
                             const Slice& begin_key, const Slice& end_key);

  // Merge the database entry for "key" with "value".  Returns OK on success,
  // and a non-OK status on error. The semantics of this operation is
  // determined by the user provided merge_operator when opening DB.
  // Note: consider setting options.sync = true.
  virtual Status Merge(const WriteOptions& options,
                       ColumnFamilyHandle* column_family, const Slice& key,
                       const Slice& value) = 0;
  virtual Status Merge(const WriteOptions& options, const Slice& key,
                       const Slice& value) {
    return Merge(options, DefaultColumnFamily(), key, value);
  }

  // Apply the specified updates to the database.
  // If `updates` contains no update, WAL will still be synced if
  // options.sync=true.
  // Returns OK on success, non-OK on failure.
  // Note: consider setting options.sync = true.
  virtual Status Write(const WriteOptions& options, WriteBatch* updates) = 0;

  // If the database contains an entry for "key" store the
  // corresponding value in *value and return OK.
  //
  // If timestamp is enabled and a non-null timestamp pointer is passed in,
  // timestamp is returned.
  //
  // If there is no entry for "key" leave *value unchanged and return
  // a status for which Status::IsNotFound() returns true.
  //
  // May return some other Status on an error.
  virtual inline Status Get(const ReadOptions& options,
                            ColumnFamilyHandle* column_family, const Slice& key,
                            std::string* value) {
    assert(value != nullptr);
    PinnableSlice pinnable_val(value);
    assert(!pinnable_val.IsPinned());
    auto s = Get(options, column_family, key, &pinnable_val);
    if (s.ok() && pinnable_val.IsPinned()) {
      value->assign(pinnable_val.data(), pinnable_val.size());
    }  // else value is already assigned
    return s;
  }
  virtual Status Get(const ReadOptions& options,
                     ColumnFamilyHandle* column_family, const Slice& key,
                     PinnableSlice* value) = 0;
  virtual Status Get(const ReadOptions& options, const Slice& key,
                     std::string* value) {
    return Get(options, DefaultColumnFamily(), key, value);
  }

  // Get() methods that return timestamp. Derived DB classes don't need to worry
  // about this group of methods if they don't care about timestamp feature.
  virtual inline Status Get(const ReadOptions& options,
                            ColumnFamilyHandle* column_family, const Slice& key,
                            std::string* value, std::string* timestamp) {
    assert(value != nullptr);
    PinnableSlice pinnable_val(value);
    assert(!pinnable_val.IsPinned());
    auto s = Get(options, column_family, key, &pinnable_val, timestamp);
    if (s.ok() && pinnable_val.IsPinned()) {
      value->assign(pinnable_val.data(), pinnable_val.size());
    }  // else value is already assigned
    return s;
  }
  virtual Status Get(const ReadOptions& /*options*/,
                     ColumnFamilyHandle* /*column_family*/,
                     const Slice& /*key*/, PinnableSlice* /*value*/,
                     std::string* /*timestamp*/) {
    return Status::NotSupported(
        "Get() that returns timestamp is not implemented.");
  }
  virtual Status Get(const ReadOptions& options, const Slice& key,
                     std::string* value, std::string* timestamp) {
    return Get(options, DefaultColumnFamily(), key, value, timestamp);
  }

  // Returns all the merge operands corresponding to the key. If the
  // number of merge operands in DB is greater than
  // merge_operands_options.expected_max_number_of_operands
  // no merge operands are returned and status is Incomplete. Merge operands
  // returned are in the order of insertion.
  // merge_operands- Points to an array of at-least
  //             merge_operands_options.expected_max_number_of_operands and the
  //             caller is responsible for allocating it. If the status
  //             returned is Incomplete then number_of_operands will contain
  //             the total number of merge operands found in DB for key.
  virtual Status GetMergeOperands(
      const ReadOptions& options, ColumnFamilyHandle* column_family,
      const Slice& key, PinnableSlice* merge_operands,
      GetMergeOperandsOptions* get_merge_operands_options,
      int* number_of_operands) = 0;

  // Consistent Get of many keys across column families without the need
  // for an explicit snapshot. NOTE: the implementation of this MultiGet API
  // does not have the performance benefits of the void-returning MultiGet
  // functions.
  //
  // If keys[i] does not exist in the database, then the i'th returned
  // status will be one for which Status::IsNotFound() is true, and
  // (*values)[i] will be set to some arbitrary value (often ""). Otherwise,
  // the i'th returned status will have Status::ok() true, and (*values)[i]
  // will store the value associated with keys[i].
  //
  // (*values) will always be resized to be the same size as (keys).
  // Similarly, the number of returned statuses will be the number of keys.
  // Note: keys will not be "de-duplicated". Duplicate keys will return
  // duplicate values in order.
  virtual std::vector<Status> MultiGet(
      const ReadOptions& options,
      const std::vector<ColumnFamilyHandle*>& column_family,
      const std::vector<Slice>& keys, std::vector<std::string>* values) = 0;
  virtual std::vector<Status> MultiGet(const ReadOptions& options,
                                       const std::vector<Slice>& keys,
                                       std::vector<std::string>* values) {
    return MultiGet(
        options,
        std::vector<ColumnFamilyHandle*>(keys.size(), DefaultColumnFamily()),
        keys, values);
  }

  virtual std::vector<Status> MultiGet(
      const ReadOptions& /*options*/,
      const std::vector<ColumnFamilyHandle*>& /*column_family*/,
      const std::vector<Slice>& keys, std::vector<std::string>* /*values*/,
      std::vector<std::string>* /*timestamps*/) {
    return std::vector<Status>(
        keys.size(), Status::NotSupported(
                         "MultiGet() returning timestamps not implemented."));
  }
  virtual std::vector<Status> MultiGet(const ReadOptions& options,
                                       const std::vector<Slice>& keys,
                                       std::vector<std::string>* values,
                                       std::vector<std::string>* timestamps) {
    return MultiGet(
        options,
        std::vector<ColumnFamilyHandle*>(keys.size(), DefaultColumnFamily()),
        keys, values, timestamps);
  }

  // Overloaded MultiGet API that improves performance by batching operations
  // in the read path for greater efficiency. Currently, only the block based
  // table format with full filters are supported. Other table formats such
  // as plain table, block based table with block based filters and
  // partitioned indexes will still work, but will not get any performance
  // benefits.
  // Parameters -
  // options - ReadOptions
  // column_family - ColumnFamilyHandle* that the keys belong to. All the keys
  //                 passed to the API are restricted to a single column family
  // num_keys - Number of keys to lookup
  // keys - Pointer to C style array of key Slices with num_keys elements
  // values - Pointer to C style array of PinnableSlices with num_keys elements
  // statuses - Pointer to C style array of Status with num_keys elements
  // sorted_input - If true, it means the input keys are already sorted by key
  //                order, so the MultiGet() API doesn't have to sort them
  //                again. If false, the keys will be copied and sorted
  //                internally by the API - the input array will not be
  //                modified
  virtual void MultiGet(const ReadOptions& options,
                        ColumnFamilyHandle* column_family,
                        const size_t num_keys, const Slice* keys,
                        PinnableSlice* values, Status* statuses,
                        const bool /*sorted_input*/ = false) {
    std::vector<ColumnFamilyHandle*> cf;
    std::vector<Slice> user_keys;
    std::vector<Status> status;
    std::vector<std::string> vals;

    for (size_t i = 0; i < num_keys; ++i) {
      cf.emplace_back(column_family);
      user_keys.emplace_back(keys[i]);
    }
    status = MultiGet(options, cf, user_keys, &vals);
    std::copy(status.begin(), status.end(), statuses);
    for (auto& value : vals) {
      values->PinSelf(value);
      values++;
    }
  }

  virtual void MultiGet(const ReadOptions& options,
                        ColumnFamilyHandle* column_family,
                        const size_t num_keys, const Slice* keys,
                        PinnableSlice* values, std::string* timestamps,
                        Status* statuses, const bool /*sorted_input*/ = false) {
    std::vector<ColumnFamilyHandle*> cf;
    std::vector<Slice> user_keys;
    std::vector<Status> status;
    std::vector<std::string> vals;
    std::vector<std::string> tss;

    for (size_t i = 0; i < num_keys; ++i) {
      cf.emplace_back(column_family);
      user_keys.emplace_back(keys[i]);
    }
    status = MultiGet(options, cf, user_keys, &vals, &tss);
    std::copy(status.begin(), status.end(), statuses);
    std::copy(tss.begin(), tss.end(), timestamps);
    for (auto& value : vals) {
      values->PinSelf(value);
      values++;
    }
  }

  // Overloaded MultiGet API that improves performance by batching operations
  // in the read path for greater efficiency. Currently, only the block based
  // table format with full filters are supported. Other table formats such
  // as plain table, block based table with block based filters and
  // partitioned indexes will still work, but will not get any performance
  // benefits.
  // Parameters -
  // options - ReadOptions
  // column_family - ColumnFamilyHandle* that the keys belong to. All the keys
  //                 passed to the API are restricted to a single column family
  // num_keys - Number of keys to lookup
  // keys - Pointer to C style array of key Slices with num_keys elements
  // values - Pointer to C style array of PinnableSlices with num_keys elements
  // statuses - Pointer to C style array of Status with num_keys elements
  // sorted_input - If true, it means the input keys are already sorted by key
  //                order, so the MultiGet() API doesn't have to sort them
  //                again. If false, the keys will be copied and sorted
  //                internally by the API - the input array will not be
  //                modified
  virtual void MultiGet(const ReadOptions& options, const size_t num_keys,
                        ColumnFamilyHandle** column_families, const Slice* keys,
                        PinnableSlice* values, Status* statuses,
                        const bool /*sorted_input*/ = false) {
    std::vector<ColumnFamilyHandle*> cf;
    std::vector<Slice> user_keys;
    std::vector<Status> status;
    std::vector<std::string> vals;

    for (size_t i = 0; i < num_keys; ++i) {
      cf.emplace_back(column_families[i]);
      user_keys.emplace_back(keys[i]);
    }
    status = MultiGet(options, cf, user_keys, &vals);
    std::copy(status.begin(), status.end(), statuses);
    for (auto& value : vals) {
      values->PinSelf(value);
      values++;
    }
  }
  virtual void MultiGet(const ReadOptions& options, const size_t num_keys,
                        ColumnFamilyHandle** column_families, const Slice* keys,
                        PinnableSlice* values, std::string* timestamps,
                        Status* statuses, const bool /*sorted_input*/ = false) {
    std::vector<ColumnFamilyHandle*> cf;
    std::vector<Slice> user_keys;
    std::vector<Status> status;
    std::vector<std::string> vals;
    std::vector<std::string> tss;

    for (size_t i = 0; i < num_keys; ++i) {
      cf.emplace_back(column_families[i]);
      user_keys.emplace_back(keys[i]);
    }
    status = MultiGet(options, cf, user_keys, &vals, &tss);
    std::copy(status.begin(), status.end(), statuses);
    std::copy(tss.begin(), tss.end(), timestamps);
    for (auto& value : vals) {
      values->PinSelf(value);
      values++;
    }
  }

  // If the key definitely does not exist in the database, then this method
  // returns false, else true. If the caller wants to obtain value when the key
  // is found in memory, a bool for 'value_found' must be passed. 'value_found'
  // will be true on return if value has been set properly.
  // This check is potentially lighter-weight than invoking DB::Get(). One way
  // to make this lighter weight is to avoid doing any IOs.
  // Default implementation here returns true and sets 'value_found' to false
  virtual bool KeyMayExist(const ReadOptions& /*options*/,
                           ColumnFamilyHandle* /*column_family*/,
                           const Slice& /*key*/, std::string* /*value*/,
                           std::string* /*timestamp*/,
                           bool* value_found = nullptr) {
    if (value_found != nullptr) {
      *value_found = false;
    }
    return true;
  }

  virtual bool KeyMayExist(const ReadOptions& options,
                           ColumnFamilyHandle* column_family, const Slice& key,
                           std::string* value, bool* value_found = nullptr) {
    return KeyMayExist(options, column_family, key, value,
                       /*timestamp=*/nullptr, value_found);
  }

  virtual bool KeyMayExist(const ReadOptions& options, const Slice& key,
                           std::string* value, bool* value_found = nullptr) {
    return KeyMayExist(options, DefaultColumnFamily(), key, value, value_found);
  }

  virtual bool KeyMayExist(const ReadOptions& options, const Slice& key,
                           std::string* value, std::string* timestamp,
                           bool* value_found = nullptr) {
    return KeyMayExist(options, DefaultColumnFamily(), key, value, timestamp,
                       value_found);
  }

  // Return a heap-allocated iterator over the contents of the database.
  // The result of NewIterator() is initially invalid (caller must
  // call one of the Seek methods on the iterator before using it).
  //
  // Caller should delete the iterator when it is no longer needed.
  // The returned iterator should be deleted before this db is deleted.
  virtual Iterator* NewIterator(const ReadOptions& options,
                                ColumnFamilyHandle* column_family) = 0;
  virtual Iterator* NewIterator(const ReadOptions& options) {
    return NewIterator(options, DefaultColumnFamily());
  }
  // Returns iterators from a consistent database state across multiple
  // column families. Iterators are heap allocated and need to be deleted
  // before the db is deleted
  virtual Status NewIterators(
      const ReadOptions& options,
      const std::vector<ColumnFamilyHandle*>& column_families,
      std::vector<Iterator*>* iterators) = 0;

  // Return a handle to the current DB state.  Iterators created with
  // this handle will all observe a stable snapshot of the current DB
  // state.  The caller must call ReleaseSnapshot(result) when the
  // snapshot is no longer needed.
  //
  // nullptr will be returned if the DB fails to take a snapshot or does
  // not support snapshot.
  virtual const Snapshot* GetSnapshot() = 0;

  // Release a previously acquired snapshot.  The caller must not
  // use "snapshot" after this call.
  virtual void ReleaseSnapshot(const Snapshot* snapshot) = 0;

#ifndef ROCKSDB_LITE
  // Contains all valid property arguments for GetProperty() or
  // GetMapProperty(). Each is a "string" property for retrieval with
  // GetProperty() unless noted as a "map" property, for GetMapProperty().
  //
  // NOTE: Property names cannot end in numbers since those are interpreted as
  //       arguments, e.g., see kNumFilesAtLevelPrefix.
  struct Properties {
    //  "rocksdb.num-files-at-level<N>" - returns string containing the number
    //      of files at level <N>, where <N> is an ASCII representation of a
    //      level number (e.g., "0").
    static const std::string kNumFilesAtLevelPrefix;

    //  "rocksdb.compression-ratio-at-level<N>" - returns string containing the
    //      compression ratio of data at level <N>, where <N> is an ASCII
    //      representation of a level number (e.g., "0"). Here, compression
    //      ratio is defined as uncompressed data size / compressed file size.
    //      Returns "-1.0" if no open files at level <N>.
    static const std::string kCompressionRatioAtLevelPrefix;

    //  "rocksdb.stats" - returns a multi-line string containing the data
    //      described by kCFStats followed by the data described by kDBStats.
    static const std::string kStats;

    //  "rocksdb.sstables" - returns a multi-line string summarizing current
    //      SST files.
    static const std::string kSSTables;

    //  "rocksdb.cfstats" - Raw data from "rocksdb.cfstats-no-file-histogram"
    //      and "rocksdb.cf-file-histogram" as a "map" property.
    static const std::string kCFStats;

    //  "rocksdb.cfstats-no-file-histogram" - returns a multi-line string with
    //      general column family stats per-level over db's lifetime ("L<n>"),
    //      aggregated over db's lifetime ("Sum"), and aggregated over the
    //      interval since the last retrieval ("Int").
    static const std::string kCFStatsNoFileHistogram;

    //  "rocksdb.cf-file-histogram" - print out how many file reads to every
    //      level, as well as the histogram of latency of single requests.
    static const std::string kCFFileHistogram;

    //  "rocksdb.dbstats" - As a string property, returns a multi-line string
    //      with general database stats, both cumulative (over the db's
    //      lifetime) and interval (since the last retrieval of kDBStats).
    //      As a map property, returns cumulative stats only and does not
    //      update the baseline for the interval stats.
    static const std::string kDBStats;

    //  "rocksdb.levelstats" - returns multi-line string containing the number
    //      of files per level and total size of each level (MB).
    static const std::string kLevelStats;

    //  "rocksdb.block-cache-entry-stats" - returns a multi-line string or
    //      map with statistics on block cache usage.
    static const std::string kBlockCacheEntryStats;

    //  "rocksdb.num-immutable-mem-table" - returns number of immutable
    //      memtables that have not yet been flushed.
    static const std::string kNumImmutableMemTable;

    //  "rocksdb.num-immutable-mem-table-flushed" - returns number of immutable
    //      memtables that have already been flushed.
    static const std::string kNumImmutableMemTableFlushed;

    //  "rocksdb.mem-table-flush-pending" - returns 1 if a memtable flush is
    //      pending; otherwise, returns 0.
    static const std::string kMemTableFlushPending;

    //  "rocksdb.num-running-flushes" - returns the number of currently running
    //      flushes.
    static const std::string kNumRunningFlushes;

    //  "rocksdb.compaction-pending" - returns 1 if at least one compaction is
    //      pending; otherwise, returns 0.
    static const std::string kCompactionPending;

    //  "rocksdb.num-running-compactions" - returns the number of currently
    //      running compactions.
    static const std::string kNumRunningCompactions;

    //  "rocksdb.background-errors" - returns accumulated number of background
    //      errors.
    static const std::string kBackgroundErrors;

    //  "rocksdb.cur-size-active-mem-table" - returns approximate size of active
    //      memtable (bytes).
    static const std::string kCurSizeActiveMemTable;

    //  "rocksdb.cur-size-all-mem-tables" - returns approximate size of active
    //      and unflushed immutable memtables (bytes).
    static const std::string kCurSizeAllMemTables;

    //  "rocksdb.size-all-mem-tables" - returns approximate size of active,
    //      unflushed immutable, and pinned immutable memtables (bytes).
    static const std::string kSizeAllMemTables;

    //  "rocksdb.num-entries-active-mem-table" - returns total number of entries
    //      in the active memtable.
    static const std::string kNumEntriesActiveMemTable;

    //  "rocksdb.num-entries-imm-mem-tables" - returns total number of entries
    //      in the unflushed immutable memtables.
    static const std::string kNumEntriesImmMemTables;

    //  "rocksdb.num-deletes-active-mem-table" - returns total number of delete
    //      entries in the active memtable.
    static const std::string kNumDeletesActiveMemTable;

    //  "rocksdb.num-deletes-imm-mem-tables" - returns total number of delete
    //      entries in the unflushed immutable memtables.
    static const std::string kNumDeletesImmMemTables;

    //  "rocksdb.estimate-num-keys" - returns estimated number of total keys in
    //      the active and unflushed immutable memtables and storage.
    static const std::string kEstimateNumKeys;

    //  "rocksdb.estimate-table-readers-mem" - returns estimated memory used for
    //      reading SST tables, excluding memory used in block cache (e.g.,
    //      filter and index blocks).
    static const std::string kEstimateTableReadersMem;

    //  "rocksdb.is-file-deletions-enabled" - returns 0 if deletion of obsolete
    //      files is enabled; otherwise, returns a non-zero number.
    static const std::string kIsFileDeletionsEnabled;

    //  "rocksdb.num-snapshots" - returns number of unreleased snapshots of the
    //      database.
    static const std::string kNumSnapshots;

    //  "rocksdb.oldest-snapshot-time" - returns number representing unix
    //      timestamp of oldest unreleased snapshot.
    static const std::string kOldestSnapshotTime;

    //  "rocksdb.oldest-snapshot-sequence" - returns number representing
    //      sequence number of oldest unreleased snapshot.
    static const std::string kOldestSnapshotSequence;

    //  "rocksdb.num-live-versions" - returns number of live versions. `Version`
    //      is an internal data structure. See version_set.h for details. More
    //      live versions often mean more SST files are held from being deleted,
    //      by iterators or unfinished compactions.
    static const std::string kNumLiveVersions;

    //  "rocksdb.current-super-version-number" - returns number of current LSM
    //  version. It is a uint64_t integer number, incremented after there is
    //  any change to the LSM tree. The number is not preserved after restarting
    //  the DB. After DB restart, it will start from 0 again.
    static const std::string kCurrentSuperVersionNumber;

    //  "rocksdb.estimate-live-data-size" - returns an estimate of the amount of
    //      live data in bytes. For BlobDB, it also includes the exact value of
    //      live bytes in the blob files of the version.
    static const std::string kEstimateLiveDataSize;

    //  "rocksdb.min-log-number-to-keep" - return the minimum log number of the
    //      log files that should be kept.
    static const std::string kMinLogNumberToKeep;

    //  "rocksdb.min-obsolete-sst-number-to-keep" - return the minimum file
    //      number for an obsolete SST to be kept. The max value of `uint64_t`
    //      will be returned if all obsolete files can be deleted.
    static const std::string kMinObsoleteSstNumberToKeep;

    //  "rocksdb.total-sst-files-size" - returns total size (bytes) of all SST
    //      files.
    //  WARNING: may slow down online queries if there are too many files.
    static const std::string kTotalSstFilesSize;

    //  "rocksdb.live-sst-files-size" - returns total size (bytes) of all SST
    //      files belong to the latest LSM tree.
    static const std::string kLiveSstFilesSize;

    // "rocksdb.live_sst_files_size_at_temperature" - returns total size (bytes)
    //      of SST files at all certain file temperature
    static const std::string kLiveSstFilesSizeAtTemperature;

    //  "rocksdb.base-level" - returns number of level to which L0 data will be
    //      compacted.
    static const std::string kBaseLevel;

    //  "rocksdb.estimate-pending-compaction-bytes" - returns estimated total
    //      number of bytes compaction needs to rewrite to get all levels down
    //      to under target size. Not valid for other compactions than level-
    //      based.
    static const std::string kEstimatePendingCompactionBytes;

    //  "rocksdb.aggregated-table-properties" - returns a string or map
    //      representation of the aggregated table properties of the target
    //      column family. Only properties that make sense for aggregation
    //      are included.
    static const std::string kAggregatedTableProperties;

    //  "rocksdb.aggregated-table-properties-at-level<N>", same as the previous
    //      one but only returns the aggregated table properties of the
    //      specified level "N" at the target column family.
    static const std::string kAggregatedTablePropertiesAtLevel;

    //  "rocksdb.actual-delayed-write-rate" - returns the current actual delayed
    //      write rate. 0 means no delay.
    static const std::string kActualDelayedWriteRate;

    //  "rocksdb.is-write-stopped" - Return 1 if write has been stopped.
    static const std::string kIsWriteStopped;

    //  "rocksdb.estimate-oldest-key-time" - returns an estimation of
    //      oldest key timestamp in the DB. Currently only available for
    //      FIFO compaction with
    //      compaction_options_fifo.allow_compaction = false.
    static const std::string kEstimateOldestKeyTime;

    //  "rocksdb.block-cache-capacity" - returns block cache capacity.
    static const std::string kBlockCacheCapacity;

    //  "rocksdb.block-cache-usage" - returns the memory size for the entries
    //      residing in block cache.
    static const std::string kBlockCacheUsage;

    // "rocksdb.block-cache-pinned-usage" - returns the memory size for the
    //      entries being pinned.
    static const std::string kBlockCachePinnedUsage;

    // "rocksdb.options-statistics" - returns multi-line string
    //      of options.statistics
    static const std::string kOptionsStatistics;

    // "rocksdb.num-blob-files" - returns number of blob files in the current
    //      version.
    static const std::string kNumBlobFiles;

    // "rocksdb.blob-stats" - return the total number and size of all blob
    //      files, and total amount of garbage (bytes) in the blob files in
    //      the current version.
    static const std::string kBlobStats;

    // "rocksdb.total-blob-file-size" - returns the total size of all blob
    //      files over all versions.
    static const std::string kTotalBlobFileSize;

    // "rocksdb.live-blob-file-size" - returns the total size of all blob
    //      files in the current version.
    static const std::string kLiveBlobFileSize;
  };
#endif /* ROCKSDB_LITE */

  // DB implementations export properties about their state via this method.
  // If "property" is a valid "string" property understood by this DB
  // implementation (see Properties struct above for valid options), fills
  // "*value" with its current value and returns true.  Otherwise, returns
  // false.
  virtual bool GetProperty(ColumnFamilyHandle* column_family,
                           const Slice& property, std::string* value) = 0;
  virtual bool GetProperty(const Slice& property, std::string* value) {
    return GetProperty(DefaultColumnFamily(), property, value);
  }

  // Like GetProperty but for valid "map" properties. (Some properties can be
  // accessed as either "string" properties or "map" properties.)
  virtual bool GetMapProperty(ColumnFamilyHandle* column_family,
                              const Slice& property,
                              std::map<std::string, std::string>* value) = 0;
  virtual bool GetMapProperty(const Slice& property,
                              std::map<std::string, std::string>* value) {
    return GetMapProperty(DefaultColumnFamily(), property, value);
  }

  // Similar to GetProperty(), but only works for a subset of properties whose
  // return value is an integer. Return the value by integer. Supported
  // properties:
  //  "rocksdb.num-immutable-mem-table"
  //  "rocksdb.mem-table-flush-pending"
  //  "rocksdb.compaction-pending"
  //  "rocksdb.background-errors"
  //  "rocksdb.cur-size-active-mem-table"
  //  "rocksdb.cur-size-all-mem-tables"
  //  "rocksdb.size-all-mem-tables"
  //  "rocksdb.num-entries-active-mem-table"
  //  "rocksdb.num-entries-imm-mem-tables"
  //  "rocksdb.num-deletes-active-mem-table"
  //  "rocksdb.num-deletes-imm-mem-tables"
  //  "rocksdb.estimate-num-keys"
  //  "rocksdb.estimate-table-readers-mem"
  //  "rocksdb.is-file-deletions-enabled"
  //  "rocksdb.num-snapshots"
  //  "rocksdb.oldest-snapshot-time"
  //  "rocksdb.num-live-versions"
  //  "rocksdb.current-super-version-number"
  //  "rocksdb.estimate-live-data-size"
  //  "rocksdb.min-log-number-to-keep"
  //  "rocksdb.min-obsolete-sst-number-to-keep"
  //  "rocksdb.total-sst-files-size"
  //  "rocksdb.live-sst-files-size"
  //  "rocksdb.base-level"
  //  "rocksdb.estimate-pending-compaction-bytes"
  //  "rocksdb.num-running-compactions"
  //  "rocksdb.num-running-flushes"
  //  "rocksdb.actual-delayed-write-rate"
  //  "rocksdb.is-write-stopped"
  //  "rocksdb.estimate-oldest-key-time"
  //  "rocksdb.block-cache-capacity"
  //  "rocksdb.block-cache-usage"
  //  "rocksdb.block-cache-pinned-usage"
  //
  //  Properties dedicated for BlobDB:
  //  "rocksdb.num-blob-files"
  //  "rocksdb.total-blob-file-size"
  //  "rocksdb.live-blob-file-size"
  virtual bool GetIntProperty(ColumnFamilyHandle* column_family,
                              const Slice& property, uint64_t* value) = 0;
  virtual bool GetIntProperty(const Slice& property, uint64_t* value) {
    return GetIntProperty(DefaultColumnFamily(), property, value);
  }

  // Reset internal stats for DB and all column families.
  // Note this doesn't reset options.statistics as it is not owned by
  // DB.
  virtual Status ResetStats() {
    return Status::NotSupported("Not implemented");
  }

  // Same as GetIntProperty(), but this one returns the aggregated int
  // property from all column families.
  virtual bool GetAggregatedIntProperty(const Slice& property,
                                        uint64_t* value) = 0;

  // Flags for DB::GetSizeApproximation that specify whether memtable
  // stats should be included, or file stats approximation or both
  enum SizeApproximationFlags : uint8_t {
    NONE = 0,
    INCLUDE_MEMTABLES = 1 << 0,
    INCLUDE_FILES = 1 << 1
  };

  // For each i in [0,n-1], store in "sizes[i]", the approximate
  // file system space used by keys in "[range[i].start .. range[i].limit)"
  // in a single column family.
  //
  // Note that the returned sizes measure file system space usage, so
  // if the user data compresses by a factor of ten, the returned
  // sizes will be one-tenth the size of the corresponding user data size.
  virtual Status GetApproximateSizes(const SizeApproximationOptions& options,
                                     ColumnFamilyHandle* column_family,
                                     const Range* ranges, int n,
                                     uint64_t* sizes) = 0;

  // Simpler versions of the GetApproximateSizes() method above.
  // The include_flags argument must of type DB::SizeApproximationFlags
  // and can not be NONE.
  virtual Status GetApproximateSizes(ColumnFamilyHandle* column_family,
                                     const Range* ranges, int n,
                                     uint64_t* sizes,
                                     uint8_t include_flags = INCLUDE_FILES) {
    SizeApproximationOptions options;
    options.include_memtabtles =
        (include_flags & SizeApproximationFlags::INCLUDE_MEMTABLES) != 0;
    options.include_files =
        (include_flags & SizeApproximationFlags::INCLUDE_FILES) != 0;
    return GetApproximateSizes(options, column_family, ranges, n, sizes);
  }
  virtual Status GetApproximateSizes(const Range* ranges, int n,
                                     uint64_t* sizes,
                                     uint8_t include_flags = INCLUDE_FILES) {
    return GetApproximateSizes(DefaultColumnFamily(), ranges, n, sizes,
                               include_flags);
  }

  // The method is similar to GetApproximateSizes, except it
  // returns approximate number of records in memtables.
  virtual void GetApproximateMemTableStats(ColumnFamilyHandle* column_family,
                                           const Range& range,
                                           uint64_t* const count,
                                           uint64_t* const size) = 0;
  virtual void GetApproximateMemTableStats(const Range& range,
                                           uint64_t* const count,
                                           uint64_t* const size) {
    GetApproximateMemTableStats(DefaultColumnFamily(), range, count, size);
  }

  // Deprecated versions of GetApproximateSizes
  ROCKSDB_DEPRECATED_FUNC virtual void GetApproximateSizes(
      const Range* range, int n, uint64_t* sizes, bool include_memtable) {
    uint8_t include_flags = SizeApproximationFlags::INCLUDE_FILES;
    if (include_memtable) {
      include_flags |= SizeApproximationFlags::INCLUDE_MEMTABLES;
    }
    GetApproximateSizes(DefaultColumnFamily(), range, n, sizes, include_flags);
  }
  ROCKSDB_DEPRECATED_FUNC virtual void GetApproximateSizes(
      ColumnFamilyHandle* column_family, const Range* range, int n,
      uint64_t* sizes, bool include_memtable) {
    uint8_t include_flags = SizeApproximationFlags::INCLUDE_FILES;
    if (include_memtable) {
      include_flags |= SizeApproximationFlags::INCLUDE_MEMTABLES;
    }
    GetApproximateSizes(column_family, range, n, sizes, include_flags);
  }

  // Compact the underlying storage for the key range [*begin,*end].
  // The actual compaction interval might be superset of [*begin, *end].
  // In particular, deleted and overwritten versions are discarded,
  // and the data is rearranged to reduce the cost of operations
  // needed to access the data.  This operation should typically only
  // be invoked by users who understand the underlying implementation.
  // This call blocks until the operation completes successfully, fails,
  // or is aborted (Status::Incomplete). See DisableManualCompaction.
  //
  // begin==nullptr is treated as a key before all keys in the database.
  // end==nullptr is treated as a key after all keys in the database.
  // Therefore the following call will compact the entire database:
  //    db->CompactRange(options, nullptr, nullptr);
  // Note that after the entire database is compacted, all data are pushed
  // down to the last level containing any data. If the total data size after
  // compaction is reduced, that level might not be appropriate for hosting all
  // the files. In this case, client could set options.change_level to true, to
  // move the files back to the minimum level capable of holding the data set
  // or a given level (specified by non-negative options.target_level).
  virtual Status CompactRange(const CompactRangeOptions& options,
                              ColumnFamilyHandle* column_family,
                              const Slice* begin, const Slice* end) = 0;
  virtual Status CompactRange(const CompactRangeOptions& options,
                              const Slice* begin, const Slice* end) {
    return CompactRange(options, DefaultColumnFamily(), begin, end);
  }

  ROCKSDB_DEPRECATED_FUNC virtual Status CompactRange(
      ColumnFamilyHandle* column_family, const Slice* begin, const Slice* end,
      bool change_level = false, int target_level = -1,
      uint32_t target_path_id = 0) {
    CompactRangeOptions options;
    options.change_level = change_level;
    options.target_level = target_level;
    options.target_path_id = target_path_id;
    return CompactRange(options, column_family, begin, end);
  }

  ROCKSDB_DEPRECATED_FUNC virtual Status CompactRange(
      const Slice* begin, const Slice* end, bool change_level = false,
      int target_level = -1, uint32_t target_path_id = 0) {
    CompactRangeOptions options;
    options.change_level = change_level;
    options.target_level = target_level;
    options.target_path_id = target_path_id;
    return CompactRange(options, DefaultColumnFamily(), begin, end);
  }

  virtual Status SetOptions(
      ColumnFamilyHandle* /*column_family*/,
      const std::unordered_map<std::string, std::string>& /*new_options*/) {
    return Status::NotSupported("Not implemented");
  }
  virtual Status SetOptions(
      const std::unordered_map<std::string, std::string>& new_options) {
    return SetOptions(DefaultColumnFamily(), new_options);
  }

  virtual Status SetDBOptions(
      const std::unordered_map<std::string, std::string>& new_options) = 0;

  // CompactFiles() inputs a list of files specified by file numbers and
  // compacts them to the specified level. A small difference compared to
  // CompactRange() is that CompactFiles() performs the compaction job
  // using the CURRENT thread, so is not considered a "background" job.
  //
  // @see GetDataBaseMetaData
  // @see GetColumnFamilyMetaData
  virtual Status CompactFiles(
      const CompactionOptions& compact_options,
      ColumnFamilyHandle* column_family,
      const std::vector<std::string>& input_file_names, const int output_level,
      const int output_path_id = -1,
      std::vector<std::string>* const output_file_names = nullptr,
      CompactionJobInfo* compaction_job_info = nullptr) = 0;

  virtual Status CompactFiles(
      const CompactionOptions& compact_options,
      const std::vector<std::string>& input_file_names, const int output_level,
      const int output_path_id = -1,
      std::vector<std::string>* const output_file_names = nullptr,
      CompactionJobInfo* compaction_job_info = nullptr) {
    return CompactFiles(compact_options, DefaultColumnFamily(),
                        input_file_names, output_level, output_path_id,
                        output_file_names, compaction_job_info);
  }

  // This function will wait until all currently running background processes
  // finish. After it returns, no background process will be run until
  // ContinueBackgroundWork is called, once for each preceding OK-returning
  // call to PauseBackgroundWork.
  virtual Status PauseBackgroundWork() = 0;
  virtual Status ContinueBackgroundWork() = 0;

  // This function will enable automatic compactions for the given column
  // families if they were previously disabled. The function will first set the
  // disable_auto_compactions option for each column family to 'false', after
  // which it will schedule a flush/compaction.
  //
  // NOTE: Setting disable_auto_compactions to 'false' through SetOptions() API
  // does NOT schedule a flush/compaction afterwards, and only changes the
  // parameter itself within the column family option.
  //
  virtual Status EnableAutoCompaction(
      const std::vector<ColumnFamilyHandle*>& column_family_handles) = 0;

  // After this function call, CompactRange() or CompactFiles() will not
  // run compactions and fail. Calling this function will tell outstanding
  // manual compactions to abort and will wait for them to finish or abort
  // before returning.
  virtual void DisableManualCompaction() = 0;
  // Re-enable CompactRange() and ComapctFiles() that are disabled by
  // DisableManualCompaction(). This function must be called as many times
  // as DisableManualCompaction() has been called in order to re-enable
  // manual compactions, and must not be called more times than
  // DisableManualCompaction() has been called.
  virtual void EnableManualCompaction() = 0;

  // Number of levels used for this DB.
  virtual int NumberLevels(ColumnFamilyHandle* column_family) = 0;
  virtual int NumberLevels() { return NumberLevels(DefaultColumnFamily()); }

  // Maximum level to which a new compacted memtable is pushed if it
  // does not create overlap.
  virtual int MaxMemCompactionLevel(ColumnFamilyHandle* column_family) = 0;
  virtual int MaxMemCompactionLevel() {
    return MaxMemCompactionLevel(DefaultColumnFamily());
  }

  // Number of files in level-0 that would stop writes.
  virtual int Level0StopWriteTrigger(ColumnFamilyHandle* column_family) = 0;
  virtual int Level0StopWriteTrigger() {
    return Level0StopWriteTrigger(DefaultColumnFamily());
  }

  // Get DB name -- the exact same name that was provided as an argument to
  // DB::Open()
  virtual const std::string& GetName() const = 0;

  // Get Env object from the DB
  virtual Env* GetEnv() const = 0;

  virtual FileSystem* GetFileSystem() const;

  // Get DB Options that we use.  During the process of opening the
  // column family, the options provided when calling DB::Open() or
  // DB::CreateColumnFamily() will have been "sanitized" and transformed
  // in an implementation-defined manner.
  virtual Options GetOptions(ColumnFamilyHandle* column_family) const = 0;
  virtual Options GetOptions() const {
    return GetOptions(DefaultColumnFamily());
  }

  virtual DBOptions GetDBOptions() const = 0;

  // Flush all mem-table data.
  // Flush a single column family, even when atomic flush is enabled. To flush
  // multiple column families, use Flush(options, column_families).
  virtual Status Flush(const FlushOptions& options,
                       ColumnFamilyHandle* column_family) = 0;
  virtual Status Flush(const FlushOptions& options) {
    return Flush(options, DefaultColumnFamily());
  }
  // Flushes multiple column families.
  // If atomic flush is not enabled, Flush(options, column_families) is
  // equivalent to calling Flush(options, column_family) multiple times.
  // If atomic flush is enabled, Flush(options, column_families) will flush all
  // column families specified in 'column_families' up to the latest sequence
  // number at the time when flush is requested.
  // Note that RocksDB 5.15 and earlier may not be able to open later versions
  // with atomic flush enabled.
  virtual Status Flush(
      const FlushOptions& options,
      const std::vector<ColumnFamilyHandle*>& column_families) = 0;

  // Flush the WAL memory buffer to the file. If sync is true, it calls SyncWAL
  // afterwards.
  virtual Status FlushWAL(bool /*sync*/) {
    return Status::NotSupported("FlushWAL not implemented");
  }
  // Sync the wal. Note that Write() followed by SyncWAL() is not exactly the
  // same as Write() with sync=true: in the latter case the changes won't be
  // visible until the sync is done.
  // Currently only works if allow_mmap_writes = false in Options.
  virtual Status SyncWAL() = 0;

  // Lock the WAL. Also flushes the WAL after locking.
  virtual Status LockWAL() {
    return Status::NotSupported("LockWAL not implemented");
  }

  // Unlock the WAL.
  virtual Status UnlockWAL() {
    return Status::NotSupported("UnlockWAL not implemented");
  }

  // The sequence number of the most recent transaction.
  virtual SequenceNumber GetLatestSequenceNumber() const = 0;

  // Instructs DB to preserve deletes with sequence numbers >= passed seqnum.
  // Has no effect if DBOptions.preserve_deletes is set to false.
  // This function assumes that user calls this function with monotonically
  // increasing seqnums (otherwise we can't guarantee that a particular delete
  // hasn't been already processed); returns true if the value was successfully
  // updated, false if user attempted to call if with seqnum <= current value.
  virtual bool SetPreserveDeletesSequenceNumber(SequenceNumber seqnum) = 0;

  // Prevent file deletions. Compactions will continue to occur,
  // but no obsolete files will be deleted. Calling this multiple
  // times have the same effect as calling it once.
  virtual Status DisableFileDeletions() = 0;

  // Increase the full_history_ts of column family. The new ts_low value should
  // be newer than current full_history_ts value.
  virtual Status IncreaseFullHistoryTsLow(ColumnFamilyHandle* column_family,
                                          std::string ts_low) = 0;

  // Get current full_history_ts value.
  virtual Status GetFullHistoryTsLow(ColumnFamilyHandle* column_family,
                                     std::string* ts_low) = 0;

  // Allow compactions to delete obsolete files.
  // If force == true, the call to EnableFileDeletions() will guarantee that
  // file deletions are enabled after the call, even if DisableFileDeletions()
  // was called multiple times before.
  // If force == false, EnableFileDeletions will only enable file deletion
  // after it's been called at least as many times as DisableFileDeletions(),
  // enabling the two methods to be called by two threads concurrently without
  // synchronization -- i.e., file deletions will be enabled only after both
  // threads call EnableFileDeletions()
  virtual Status EnableFileDeletions(bool force = true) = 0;

#ifndef ROCKSDB_LITE
  // GetLiveFiles followed by GetSortedWalFiles can generate a lossless backup

  // Retrieve the list of all files in the database. The files are
  // relative to the dbname and are not absolute paths. Despite being relative
  // paths, the file names begin with "/". The valid size of the manifest file
  // is returned in manifest_file_size. The manifest file is an ever growing
  // file, but only the portion specified by manifest_file_size is valid for
  // this snapshot. Setting flush_memtable to true does Flush before recording
  // the live files. Setting flush_memtable to false is useful when we don't
  // want to wait for flush which may have to wait for compaction to complete
  // taking an indeterminate time.
  //
  // In case you have multiple column families, even if flush_memtable is true,
  // you still need to call GetSortedWalFiles after GetLiveFiles to compensate
  // for new data that arrived to already-flushed column families while other
  // column families were flushing
  virtual Status GetLiveFiles(std::vector<std::string>&,
                              uint64_t* manifest_file_size,
                              bool flush_memtable = true) = 0;

  // Retrieve the sorted list of all wal files with earliest file first
  virtual Status GetSortedWalFiles(VectorLogPtr& files) = 0;

  // Retrieve information about the current wal file
  //
  // Note that the log might have rolled after this call in which case
  // the current_log_file would not point to the current log file.
  //
  // Additionally, for the sake of optimization current_log_file->StartSequence
  // would always be set to 0
  virtual Status GetCurrentWalFile(
      std::unique_ptr<LogFile>* current_log_file) = 0;

  // Retrieves the creation time of the oldest file in the DB.
  // This API only works if max_open_files = -1, if it is not then
  // Status returned is Status::NotSupported()
  // The file creation time is set using the env provided to the DB.
  // If the DB was created from a very old release then its possible that
  // the SST files might not have file_creation_time property and even after
  // moving to a newer release its possible that some files never got compacted
  // and may not have file_creation_time property. In both the cases
  // file_creation_time is considered 0 which means this API will return
  // creation_time = 0 as there wouldn't be a timestamp lower than 0.
  virtual Status GetCreationTimeOfOldestFile(uint64_t* creation_time) = 0;

  // Note: this API is not yet consistent with WritePrepared transactions.
  // Sets iter to an iterator that is positioned at a write-batch containing
  // seq_number. If the sequence number is non existent, it returns an iterator
  // at the first available seq_no after the requested seq_no
  // Returns Status::OK if iterator is valid
  // Must set WAL_ttl_seconds or WAL_size_limit_MB to large values to
  // use this api, else the WAL files will get
  // cleared aggressively and the iterator might keep getting invalid before
  // an update is read.
  virtual Status GetUpdatesSince(
      SequenceNumber seq_number, std::unique_ptr<TransactionLogIterator>* iter,
      const TransactionLogIterator::ReadOptions& read_options =
          TransactionLogIterator::ReadOptions()) = 0;

// Windows API macro interference
#undef DeleteFile
  // WARNING: This API is planned for removal in RocksDB 7.0 since it does not
  // operate at the proper level of abstraction for a key-value store, and its
  // contract/restrictions are poorly documented. For example, it returns non-OK
  // `Status` for non-bottommost files and files undergoing compaction. Since we
  // do not plan to maintain it, the contract will likely remain underspecified
  // until its removal. Any user is encouraged to read the implementation
  // carefully and migrate away from it when possible.
  //
  // Delete the file name from the db directory and update the internal state to
  // reflect that. Supports deletion of sst and log files only. 'name' must be
  // path relative to the db directory. eg. 000001.sst, /archive/000003.log
  virtual Status DeleteFile(std::string name) = 0;

  // Returns a list of all table files with their level, start key
  // and end key
  virtual void GetLiveFilesMetaData(
      std::vector<LiveFileMetaData>* /*metadata*/) {}

  // Return a list of all table and blob files checksum info.
  // Note: This function might be of limited use because it cannot be
  // synchronized with GetLiveFiles.
  virtual Status GetLiveFilesChecksumInfo(FileChecksumList* checksum_list) = 0;

  // EXPERIMENTAL: This function is not yet feature-complete.
  // Get information about all live files that make up a DB, for making
  // live copies (Checkpoint, backups, etc.) or other storage-related purposes.
  // Use DisableFileDeletions() before and EnableFileDeletions() after to
  // preserve the files for live copy.
  virtual Status GetLiveFilesStorageInfo(
      const LiveFilesStorageInfoOptions& opts,
      std::vector<LiveFileStorageInfo>* files) = 0;

  // Obtains the meta data of the specified column family of the DB.
  virtual void GetColumnFamilyMetaData(ColumnFamilyHandle* /*column_family*/,
                                       ColumnFamilyMetaData* /*metadata*/) {}

  // Get the metadata of the default column family.
  void GetColumnFamilyMetaData(ColumnFamilyMetaData* metadata) {
    GetColumnFamilyMetaData(DefaultColumnFamily(), metadata);
  }

  // Obtains the meta data of all column families for the DB.
  // The returned map contains one entry for each column family indexed by the
  // name of the column family.
  virtual void GetAllColumnFamilyMetaData(
      std::vector<ColumnFamilyMetaData>* /*metadata*/) {}

  // IngestExternalFile() will load a list of external SST files (1) into the DB
  // Two primary modes are supported:
  // - Duplicate keys in the new files will overwrite exiting keys (default)
  // - Duplicate keys will be skipped (set ingest_behind=true)
  // In the first mode we will try to find the lowest possible level that
  // the file can fit in, and ingest the file into this level (2). A file that
  // have a key range that overlap with the memtable key range will require us
  // to Flush the memtable first before ingesting the file.
  // In the second mode we will always ingest in the bottom most level (see
  // docs to IngestExternalFileOptions::ingest_behind).
  //
  // (1) External SST files can be created using SstFileWriter
  // (2) We will try to ingest the files to the lowest possible level
  //     even if the file compression doesn't match the level compression
  // (3) If IngestExternalFileOptions->ingest_behind is set to true,
  //     we always ingest at the bottommost level, which should be reserved
  //     for this purpose (see DBOPtions::allow_ingest_behind flag).
  virtual Status IngestExternalFile(
      ColumnFamilyHandle* column_family,
      const std::vector<std::string>& external_files,
      const IngestExternalFileOptions& options) = 0;

  virtual Status IngestExternalFile(
      const std::vector<std::string>& external_files,
      const IngestExternalFileOptions& options) {
    return IngestExternalFile(DefaultColumnFamily(), external_files, options);
  }

  // IngestExternalFiles() will ingest files for multiple column families, and
  // record the result atomically to the MANIFEST.
  // If this function returns OK, all column families' ingestion must succeed.
  // If this function returns NOK, or the process crashes, then non-of the
  // files will be ingested into the database after recovery.
  // Note that it is possible for application to observe a mixed state during
  // the execution of this function. If the user performs range scan over the
  // column families with iterators, iterator on one column family may return
  // ingested data, while iterator on other column family returns old data.
  // Users can use snapshot for a consistent view of data.
  // If your db ingests multiple SST files using this API, i.e. args.size()
  // > 1, then RocksDB 5.15 and earlier will not be able to open it.
  //
  // REQUIRES: each arg corresponds to a different column family: namely, for
  // 0 <= i < j < len(args), args[i].column_family != args[j].column_family.
  virtual Status IngestExternalFiles(
      const std::vector<IngestExternalFileArg>& args) = 0;

  // CreateColumnFamilyWithImport() will create a new column family with
  // column_family_name and import external SST files specified in metadata into
  // this column family.
  // (1) External SST files can be created using SstFileWriter.
  // (2) External SST files can be exported from a particular column family in
  //     an existing DB using Checkpoint::ExportColumnFamily.
  // Option in import_options specifies whether the external files are copied or
  // moved (default is copy). When option specifies copy, managing files at
  // external_file_path is caller's responsibility. When option specifies a
  // move, the call makes a best effort to delete the specified files at
  // external_file_path on successful return, logging any failure to delete
  // rather than returning in Status. Files are not modified on any error
  // return, and a best effort is made to remove any newly-created files.
  // On error return, column family handle returned will be nullptr.
  // ColumnFamily will be present on successful return and will not be present
  // on error return. ColumnFamily may be present on any crash during this call.
  virtual Status CreateColumnFamilyWithImport(
      const ColumnFamilyOptions& options, const std::string& column_family_name,
      const ImportColumnFamilyOptions& import_options,
      const ExportImportFilesMetaData& metadata,
      ColumnFamilyHandle** handle) = 0;

  // Verify the checksums of files in db. Currently the whole-file checksum of
  // table files are checked.
  virtual Status VerifyFileChecksums(const ReadOptions& /*read_options*/) {
    return Status::NotSupported("File verification not supported");
  }

  // Verify the block checksums of files in db. The block checksums of table
  // files are checked.
  virtual Status VerifyChecksum(const ReadOptions& read_options) = 0;

  virtual Status VerifyChecksum() { return VerifyChecksum(ReadOptions()); }

  // AddFile() is deprecated, please use IngestExternalFile()
  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      ColumnFamilyHandle* column_family,
      const std::vector<std::string>& file_path_list, bool move_file = false,
      bool skip_snapshot_check = false) {
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(column_family, file_path_list, ifo);
  }

  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      const std::vector<std::string>& file_path_list, bool move_file = false,
      bool skip_snapshot_check = false) {
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(DefaultColumnFamily(), file_path_list, ifo);
  }

  // AddFile() is deprecated, please use IngestExternalFile()
  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      ColumnFamilyHandle* column_family, const std::string& file_path,
      bool move_file = false, bool skip_snapshot_check = false) {
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(column_family, {file_path}, ifo);
  }

  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      const std::string& file_path, bool move_file = false,
      bool skip_snapshot_check = false) {
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(DefaultColumnFamily(), {file_path}, ifo);
  }

  // Load table file with information "file_info" into "column_family"
  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      ColumnFamilyHandle* column_family,
      const std::vector<ExternalSstFileInfo>& file_info_list,
      bool move_file = false, bool skip_snapshot_check = false) {
    std::vector<std::string> external_files;
    for (const ExternalSstFileInfo& file_info : file_info_list) {
      external_files.push_back(file_info.file_path);
    }
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(column_family, external_files, ifo);
  }

  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      const std::vector<ExternalSstFileInfo>& file_info_list,
      bool move_file = false, bool skip_snapshot_check = false) {
    std::vector<std::string> external_files;
    for (const ExternalSstFileInfo& file_info : file_info_list) {
      external_files.push_back(file_info.file_path);
    }
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(DefaultColumnFamily(), external_files, ifo);
  }

  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      ColumnFamilyHandle* column_family, const ExternalSstFileInfo* file_info,
      bool move_file = false, bool skip_snapshot_check = false) {
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(column_family, {file_info->file_path}, ifo);
  }

  ROCKSDB_DEPRECATED_FUNC virtual Status AddFile(
      const ExternalSstFileInfo* file_info, bool move_file = false,
      bool skip_snapshot_check = false) {
    IngestExternalFileOptions ifo;
    ifo.move_files = move_file;
    ifo.snapshot_consistency = !skip_snapshot_check;
    ifo.allow_global_seqno = false;
    ifo.allow_blocking_flush = false;
    return IngestExternalFile(DefaultColumnFamily(), {file_info->file_path},
                              ifo);
  }

#endif  // ROCKSDB_LITE

  // Returns the unique ID which is read from IDENTITY file during the opening
  // of database by setting in the identity variable
  // Returns Status::OK if identity could be set properly
  virtual Status GetDbIdentity(std::string& identity) const = 0;

  // Return a unique identifier for each DB object that is opened
  // This DB session ID should be unique among all open DB instances on all
  // hosts, and should be unique among re-openings of the same or other DBs.
  // (Two open DBs have the same identity from other function GetDbIdentity when
  // one is physically copied from the other.)
  virtual Status GetDbSessionId(std::string& session_id) const = 0;

  // Returns default column family handle
  virtual ColumnFamilyHandle* DefaultColumnFamily() const = 0;

#ifndef ROCKSDB_LITE

  virtual Status GetPropertiesOfAllTables(ColumnFamilyHandle* column_family,
                                          TablePropertiesCollection* props) = 0;
  virtual Status GetPropertiesOfAllTables(TablePropertiesCollection* props) {
    return GetPropertiesOfAllTables(DefaultColumnFamily(), props);
  }
  virtual Status GetPropertiesOfTablesInRange(
      ColumnFamilyHandle* column_family, const Range* range, std::size_t n,
      TablePropertiesCollection* props) = 0;

  virtual Status SuggestCompactRange(ColumnFamilyHandle* /*column_family*/,
                                     const Slice* /*begin*/,
                                     const Slice* /*end*/) {
    return Status::NotSupported("SuggestCompactRange() is not implemented.");
  }

  virtual Status PromoteL0(ColumnFamilyHandle* /*column_family*/,
                           int /*target_level*/) {
    return Status::NotSupported("PromoteL0() is not implemented.");
  }

  // Trace DB operations. Use EndTrace() to stop tracing.
  virtual Status StartTrace(const TraceOptions& /*options*/,
                            std::unique_ptr<TraceWriter>&& /*trace_writer*/) {
    return Status::NotSupported("StartTrace() is not implemented.");
  }

  virtual Status EndTrace() {
    return Status::NotSupported("EndTrace() is not implemented.");
  }

  // IO Tracing operations. Use EndIOTrace() to stop tracing.
  virtual Status StartIOTrace(const TraceOptions& /*options*/,
                              std::unique_ptr<TraceWriter>&& /*trace_writer*/) {
    return Status::NotSupported("StartIOTrace() is not implemented.");
  }

  virtual Status EndIOTrace() {
    return Status::NotSupported("EndIOTrace() is not implemented.");
  }

  // Trace block cache accesses. Use EndBlockCacheTrace() to stop tracing.
  virtual Status StartBlockCacheTrace(
      const TraceOptions& /*options*/,
      std::unique_ptr<TraceWriter>&& /*trace_writer*/) {
    return Status::NotSupported("StartBlockCacheTrace() is not implemented.");
  }

  virtual Status EndBlockCacheTrace() {
    return Status::NotSupported("EndBlockCacheTrace() is not implemented.");
  }

  // Create a default trace replayer.
  virtual Status NewDefaultReplayer(
      const std::vector<ColumnFamilyHandle*>& /*handles*/,
      std::unique_ptr<TraceReader>&& /*reader*/,
      std::unique_ptr<Replayer>* /*replayer*/) {
    return Status::NotSupported("NewDefaultReplayer() is not implemented.");
  }

#endif  // ROCKSDB_LITE

  // Needed for StackableDB
  virtual DB* GetRootDB() { return this; }

  // Given a window [start_time, end_time), setup a StatsHistoryIterator
  // to access stats history. Note the start_time and end_time are epoch
  // time measured in seconds, and end_time is an exclusive bound.
  virtual Status GetStatsHistory(
      uint64_t /*start_time*/, uint64_t /*end_time*/,
      std::unique_ptr<StatsHistoryIterator>* /*stats_iterator*/) {
    return Status::NotSupported("GetStatsHistory() is not implemented.");
  }

#ifndef ROCKSDB_LITE
  // Make the secondary instance catch up with the primary by tailing and
  // replaying the MANIFEST and WAL of the primary.
  // Column families created by the primary after the secondary instance starts
  // will be ignored unless the secondary instance closes and restarts with the
  // newly created column families.
  // Column families that exist before secondary instance starts and dropped by
  // the primary afterwards will be marked as dropped. However, as long as the
  // secondary instance does not delete the corresponding column family
  // handles, the data of the column family is still accessible to the
  // secondary.
  // TODO: we will support WAL tailing soon.
  virtual Status TryCatchUpWithPrimary() {
    return Status::NotSupported("Supported only by secondary instance");
  }
#endif  // !ROCKSDB_LITE
};

// Destroy the contents of the specified database.
// Be very careful using this method.
Status DestroyDB(const std::string& name, const Options& options,
                 const std::vector<ColumnFamilyDescriptor>& column_families =
                     std::vector<ColumnFamilyDescriptor>());

#ifndef ROCKSDB_LITE
// If a DB cannot be opened, you may attempt to call this method to
// resurrect as much of the contents of the database as possible.
// Some data may be lost, so be careful when calling this function
// on a database that contains important information.
//
// With this API, we will warn and skip data associated with column families not
// specified in column_families.
//
// @param column_families Descriptors for known column families
Status RepairDB(const std::string& dbname, const DBOptions& db_options,
                const std::vector<ColumnFamilyDescriptor>& column_families);

// @param unknown_cf_opts Options for column families encountered during the
//                        repair that were not specified in column_families.
Status RepairDB(const std::string& dbname, const DBOptions& db_options,
                const std::vector<ColumnFamilyDescriptor>& column_families,
                const ColumnFamilyOptions& unknown_cf_opts);

// @param options These options will be used for the database and for ALL column
//                families encountered during the repair
Status RepairDB(const std::string& dbname, const Options& options);

#endif

}  // namespace ROCKSDB_NAMESPACE
