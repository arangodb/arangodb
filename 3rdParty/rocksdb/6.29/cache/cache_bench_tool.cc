//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#ifdef GFLAGS
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <memory>
#include <set>
#include <sstream>

#include "db/db_impl/db_impl.h"
#include "monitoring/histogram.h"
#include "port/port.h"
#include "rocksdb/cache.h"
#include "rocksdb/convenience.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/secondary_cache.h"
#include "rocksdb/system_clock.h"
#include "rocksdb/table_properties.h"
#include "table/block_based/block_based_table_reader.h"
#include "table/block_based/cachable_entry.h"
#include "util/coding.h"
#include "util/gflags_compat.h"
#include "util/hash.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/stop_watch.h"
#include "util/string_util.h"

using GFLAGS_NAMESPACE::ParseCommandLineFlags;

static constexpr uint32_t KiB = uint32_t{1} << 10;
static constexpr uint32_t MiB = KiB << 10;
static constexpr uint64_t GiB = MiB << 10;

DEFINE_uint32(threads, 16, "Number of concurrent threads to run.");
DEFINE_uint64(cache_size, 1 * GiB,
              "Number of bytes to use as a cache of uncompressed data.");
DEFINE_uint32(num_shard_bits, 6, "shard_bits.");

DEFINE_double(resident_ratio, 0.25,
              "Ratio of keys fitting in cache to keyspace.");
DEFINE_uint64(ops_per_thread, 2000000U, "Number of operations per thread.");
DEFINE_uint32(value_bytes, 8 * KiB, "Size of each value added.");

DEFINE_uint32(skew, 5, "Degree of skew in key selection");
DEFINE_bool(populate_cache, true, "Populate cache before operations");

DEFINE_uint32(lookup_insert_percent, 87,
              "Ratio of lookup (+ insert on not found) to total workload "
              "(expressed as a percentage)");
DEFINE_uint32(insert_percent, 2,
              "Ratio of insert to total workload (expressed as a percentage)");
DEFINE_uint32(lookup_percent, 10,
              "Ratio of lookup to total workload (expressed as a percentage)");
DEFINE_uint32(erase_percent, 1,
              "Ratio of erase to total workload (expressed as a percentage)");
DEFINE_bool(gather_stats, false,
            "Whether to periodically simulate gathering block cache stats, "
            "using one more thread.");
DEFINE_uint32(
    gather_stats_sleep_ms, 1000,
    "How many milliseconds to sleep between each gathering of stats.");

DEFINE_uint32(gather_stats_entries_per_lock, 256,
              "For Cache::ApplyToAllEntries");
DEFINE_bool(skewed, false, "If true, skew the key access distribution");
#ifndef ROCKSDB_LITE
DEFINE_string(secondary_cache_uri, "",
              "Full URI for creating a custom secondary cache object");
static class std::shared_ptr<ROCKSDB_NAMESPACE::SecondaryCache> secondary_cache;
#endif  // ROCKSDB_LITE

DEFINE_bool(use_clock_cache, false, "");

// ## BEGIN stress_cache_key sub-tool options ##
DEFINE_bool(stress_cache_key, false,
            "If true, run cache key stress test instead");
DEFINE_uint32(sck_files_per_day, 2500000,
              "(-stress_cache_key) Simulated files generated per day");
DEFINE_uint32(sck_duration, 90,
              "(-stress_cache_key) Number of days to simulate in each run");
DEFINE_uint32(
    sck_min_collision, 15,
    "(-stress_cache_key) Keep running until this many collisions seen");
DEFINE_uint32(
    sck_file_size_mb, 32,
    "(-stress_cache_key) Simulated file size in MiB, for accounting purposes");
DEFINE_uint32(sck_reopen_nfiles, 100,
              "(-stress_cache_key) Re-opens DB average every n files");
DEFINE_uint32(
    sck_restarts_per_day, 24,
    "(-stress_cache_key) Simulated process restarts per day (across DBs)");
DEFINE_uint32(sck_db_count, 100,
              "(-stress_cache_key) Parallel DBs in operation");
DEFINE_uint32(sck_table_bits, 20,
              "(-stress_cache_key) Log2 number of tracked files");
DEFINE_uint32(sck_keep_bits, 50,
              "(-stress_cache_key) Number of cache key bits to keep");
DEFINE_bool(sck_randomize, false,
            "(-stress_cache_key) Randomize (hash) cache key");
DEFINE_bool(sck_footer_unique_id, false,
            "(-stress_cache_key) Simulate using proposed footer unique id");
// ## END stress_cache_key sub-tool options ##

namespace ROCKSDB_NAMESPACE {

class CacheBench;
namespace {
// State shared by all concurrent executions of the same benchmark.
class SharedState {
 public:
  explicit SharedState(CacheBench* cache_bench)
      : cv_(&mu_),
        num_initialized_(0),
        start_(false),
        num_done_(0),
        cache_bench_(cache_bench) {}

  ~SharedState() {}

  port::Mutex* GetMutex() { return &mu_; }

  port::CondVar* GetCondVar() { return &cv_; }

  CacheBench* GetCacheBench() const { return cache_bench_; }

  void IncInitialized() { num_initialized_++; }

  void IncDone() { num_done_++; }

  bool AllInitialized() const { return num_initialized_ >= FLAGS_threads; }

  bool AllDone() const { return num_done_ >= FLAGS_threads; }

  void SetStart() { start_ = true; }

  bool Started() const { return start_; }

 private:
  port::Mutex mu_;
  port::CondVar cv_;

  uint64_t num_initialized_;
  bool start_;
  uint64_t num_done_;

  CacheBench* cache_bench_;
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
  uint32_t tid;
  Random64 rnd;
  SharedState* shared;
  HistogramImpl latency_ns_hist;
  uint64_t duration_us = 0;

  ThreadState(uint32_t index, SharedState* _shared)
      : tid(index), rnd(1000 + index), shared(_shared) {}
};

struct KeyGen {
  char key_data[27];

  Slice GetRand(Random64& rnd, uint64_t max_key, int max_log) {
    uint64_t key = 0;
    if (!FLAGS_skewed) {
      uint64_t raw = rnd.Next();
      // Skew according to setting
      for (uint32_t i = 0; i < FLAGS_skew; ++i) {
        raw = std::min(raw, rnd.Next());
      }
      key = FastRange64(raw, max_key);
    } else {
      key = rnd.Skewed(max_log);
      if (key > max_key) {
        key -= max_key;
      }
    }
    // Variable size and alignment
    size_t off = key % 8;
    key_data[0] = char{42};
    EncodeFixed64(key_data + 1, key);
    key_data[9] = char{11};
    EncodeFixed64(key_data + 10, key);
    key_data[18] = char{4};
    EncodeFixed64(key_data + 19, key);
    return Slice(&key_data[off], sizeof(key_data) - off);
  }
};

char* createValue(Random64& rnd) {
  char* rv = new char[FLAGS_value_bytes];
  // Fill with some filler data, and take some CPU time
  for (uint32_t i = 0; i < FLAGS_value_bytes; i += 8) {
    EncodeFixed64(rv + i, rnd.Next());
  }
  return rv;
}

// Callbacks for secondary cache
size_t SizeFn(void* /*obj*/) { return FLAGS_value_bytes; }

Status SaveToFn(void* obj, size_t /*offset*/, size_t size, void* out) {
  memcpy(out, obj, size);
  return Status::OK();
}

// Different deleters to simulate using deleter to gather
// stats on the code origin and kind of cache entries.
void deleter1(const Slice& /*key*/, void* value) {
  delete[] static_cast<char*>(value);
}
void deleter2(const Slice& /*key*/, void* value) {
  delete[] static_cast<char*>(value);
}
void deleter3(const Slice& /*key*/, void* value) {
  delete[] static_cast<char*>(value);
}

Cache::CacheItemHelper helper1(SizeFn, SaveToFn, deleter1);
Cache::CacheItemHelper helper2(SizeFn, SaveToFn, deleter2);
Cache::CacheItemHelper helper3(SizeFn, SaveToFn, deleter3);
}  // namespace

class CacheBench {
  static constexpr uint64_t kHundredthUint64 =
      std::numeric_limits<uint64_t>::max() / 100U;

 public:
  CacheBench()
      : max_key_(static_cast<uint64_t>(FLAGS_cache_size / FLAGS_resident_ratio /
                                       FLAGS_value_bytes)),
        lookup_insert_threshold_(kHundredthUint64 *
                                 FLAGS_lookup_insert_percent),
        insert_threshold_(lookup_insert_threshold_ +
                          kHundredthUint64 * FLAGS_insert_percent),
        lookup_threshold_(insert_threshold_ +
                          kHundredthUint64 * FLAGS_lookup_percent),
        erase_threshold_(lookup_threshold_ +
                         kHundredthUint64 * FLAGS_erase_percent),
        skewed_(FLAGS_skewed) {
    if (erase_threshold_ != 100U * kHundredthUint64) {
      fprintf(stderr, "Percentages must add to 100.\n");
      exit(1);
    }

    max_log_ = 0;
    if (skewed_) {
      uint64_t max_key = max_key_;
      while (max_key >>= 1) max_log_++;
      if (max_key > (static_cast<uint64_t>(1) << max_log_)) max_log_++;
    }

    if (FLAGS_use_clock_cache) {
      cache_ = NewClockCache(FLAGS_cache_size, FLAGS_num_shard_bits);
      if (!cache_) {
        fprintf(stderr, "Clock cache not supported.\n");
        exit(1);
      }
    } else {
      LRUCacheOptions opts(FLAGS_cache_size, FLAGS_num_shard_bits, false, 0.5);
#ifndef ROCKSDB_LITE
      if (!FLAGS_secondary_cache_uri.empty()) {
        Status s = SecondaryCache::CreateFromString(
            ConfigOptions(), FLAGS_secondary_cache_uri, &secondary_cache);
        if (secondary_cache == nullptr) {
          fprintf(
              stderr,
              "No secondary cache registered matching string: %s status=%s\n",
              FLAGS_secondary_cache_uri.c_str(), s.ToString().c_str());
          exit(1);
        }
        opts.secondary_cache = secondary_cache;
      }
#endif  // ROCKSDB_LITE

      cache_ = NewLRUCache(opts);
    }
  }

  ~CacheBench() {}

  void PopulateCache() {
    Random64 rnd(1);
    KeyGen keygen;
    for (uint64_t i = 0; i < 2 * FLAGS_cache_size; i += FLAGS_value_bytes) {
      cache_->Insert(keygen.GetRand(rnd, max_key_, max_log_), createValue(rnd),
                     &helper1, FLAGS_value_bytes);
    }
  }

  bool Run() {
    const auto clock = SystemClock::Default().get();

    PrintEnv();
    SharedState shared(this);
    std::vector<std::unique_ptr<ThreadState> > threads(FLAGS_threads);
    for (uint32_t i = 0; i < FLAGS_threads; i++) {
      threads[i].reset(new ThreadState(i, &shared));
      std::thread(ThreadBody, threads[i].get()).detach();
    }

    HistogramImpl stats_hist;
    std::string stats_report;
    std::thread stats_thread(StatsBody, &shared, &stats_hist, &stats_report);

    uint64_t start_time;
    {
      MutexLock l(shared.GetMutex());
      while (!shared.AllInitialized()) {
        shared.GetCondVar()->Wait();
      }
      // Record start time
      start_time = clock->NowMicros();

      // Start all threads
      shared.SetStart();
      shared.GetCondVar()->SignalAll();

      // Wait threads to complete
      while (!shared.AllDone()) {
        shared.GetCondVar()->Wait();
      }
    }

    // Stats gathering is considered background work. This time measurement
    // is for foreground work, and not really ideal for that. See below.
    uint64_t end_time = clock->NowMicros();
    stats_thread.join();

    // Wall clock time - includes idle time if threads
    // finish at different times (not ideal).
    double elapsed_secs = static_cast<double>(end_time - start_time) * 1e-6;
    uint32_t ops_per_sec = static_cast<uint32_t>(
        1.0 * FLAGS_threads * FLAGS_ops_per_thread / elapsed_secs);
    printf("Complete in %.3f s; Rough parallel ops/sec = %u\n", elapsed_secs,
           ops_per_sec);

    // Total time in each thread (more accurate throughput measure)
    elapsed_secs = 0;
    for (uint32_t i = 0; i < FLAGS_threads; i++) {
      elapsed_secs += threads[i]->duration_us * 1e-6;
    }
    ops_per_sec = static_cast<uint32_t>(1.0 * FLAGS_threads *
                                        FLAGS_ops_per_thread / elapsed_secs);
    printf("Thread ops/sec = %u\n", ops_per_sec);

    printf("\nOperation latency (ns):\n");
    HistogramImpl combined;
    for (uint32_t i = 0; i < FLAGS_threads; i++) {
      combined.Merge(threads[i]->latency_ns_hist);
    }
    printf("%s", combined.ToString().c_str());

    if (FLAGS_gather_stats) {
      printf("\nGather stats latency (us):\n");
      printf("%s", stats_hist.ToString().c_str());
    }

    printf("\n%s", stats_report.c_str());

    return true;
  }

 private:
  std::shared_ptr<Cache> cache_;
  const uint64_t max_key_;
  // Cumulative thresholds in the space of a random uint64_t
  const uint64_t lookup_insert_threshold_;
  const uint64_t insert_threshold_;
  const uint64_t lookup_threshold_;
  const uint64_t erase_threshold_;
  const bool skewed_;
  int max_log_;

  // A benchmark version of gathering stats on an active block cache by
  // iterating over it. The primary purpose is to measure the impact of
  // gathering stats with ApplyToAllEntries on throughput- and
  // latency-sensitive Cache users. Performance of stats gathering is
  // also reported. The last set of gathered stats is also reported, for
  // manual sanity checking for logical errors or other unexpected
  // behavior of cache_bench or the underlying Cache.
  static void StatsBody(SharedState* shared, HistogramImpl* stats_hist,
                        std::string* stats_report) {
    if (!FLAGS_gather_stats) {
      return;
    }
    const auto clock = SystemClock::Default().get();
    uint64_t total_key_size = 0;
    uint64_t total_charge = 0;
    uint64_t total_entry_count = 0;
    std::set<Cache::DeleterFn> deleters;
    StopWatchNano timer(clock);

    for (;;) {
      uint64_t time;
      time = clock->NowMicros();
      uint64_t deadline = time + uint64_t{FLAGS_gather_stats_sleep_ms} * 1000;

      {
        MutexLock l(shared->GetMutex());
        for (;;) {
          if (shared->AllDone()) {
            std::ostringstream ostr;
            ostr << "Most recent cache entry stats:\n"
                 << "Number of entries: " << total_entry_count << "\n"
                 << "Total charge: " << BytesToHumanString(total_charge) << "\n"
                 << "Average key size: "
                 << (1.0 * total_key_size / total_entry_count) << "\n"
                 << "Average charge: "
                 << BytesToHumanString(static_cast<uint64_t>(
                        1.0 * total_charge / total_entry_count))
                 << "\n"
                 << "Unique deleters: " << deleters.size() << "\n";
            *stats_report = ostr.str();
            return;
          }
          if (clock->NowMicros() >= deadline) {
            break;
          }
          uint64_t diff = deadline - std::min(clock->NowMicros(), deadline);
          shared->GetCondVar()->TimedWait(diff + 1);
        }
      }

      // Now gather stats, outside of mutex
      total_key_size = 0;
      total_charge = 0;
      total_entry_count = 0;
      deleters.clear();
      auto fn = [&](const Slice& key, void* /*value*/, size_t charge,
                    Cache::DeleterFn deleter) {
        total_key_size += key.size();
        total_charge += charge;
        ++total_entry_count;
        // Something slightly more expensive as in (future) stats by category
        deleters.insert(deleter);
      };
      timer.Start();
      Cache::ApplyToAllEntriesOptions opts;
      opts.average_entries_per_lock = FLAGS_gather_stats_entries_per_lock;
      shared->GetCacheBench()->cache_->ApplyToAllEntries(fn, opts);
      stats_hist->Add(timer.ElapsedNanos() / 1000);
    }
  }

  static void ThreadBody(ThreadState* thread) {
    SharedState* shared = thread->shared;

    {
      MutexLock l(shared->GetMutex());
      shared->IncInitialized();
      if (shared->AllInitialized()) {
        shared->GetCondVar()->SignalAll();
      }
      while (!shared->Started()) {
        shared->GetCondVar()->Wait();
      }
    }
    thread->shared->GetCacheBench()->OperateCache(thread);

    {
      MutexLock l(shared->GetMutex());
      shared->IncDone();
      if (shared->AllDone()) {
        shared->GetCondVar()->SignalAll();
      }
    }
  }

  void OperateCache(ThreadState* thread) {
    // To use looked-up values
    uint64_t result = 0;
    // To hold handles for a non-trivial amount of time
    Cache::Handle* handle = nullptr;
    KeyGen gen;
    const auto clock = SystemClock::Default().get();
    uint64_t start_time = clock->NowMicros();
    StopWatchNano timer(clock);

    for (uint64_t i = 0; i < FLAGS_ops_per_thread; i++) {
      timer.Start();
      Slice key = gen.GetRand(thread->rnd, max_key_, max_log_);
      uint64_t random_op = thread->rnd.Next();
      Cache::CreateCallback create_cb =
          [](void* buf, size_t size, void** out_obj, size_t* charge) -> Status {
        *out_obj = reinterpret_cast<void*>(new char[size]);
        memcpy(*out_obj, buf, size);
        *charge = size;
        return Status::OK();
      };

      if (random_op < lookup_insert_threshold_) {
        if (handle) {
          cache_->Release(handle);
          handle = nullptr;
        }
        // do lookup
        handle = cache_->Lookup(key, &helper2, create_cb, Cache::Priority::LOW,
                                true);
        if (handle) {
          // do something with the data
          result += NPHash64(static_cast<char*>(cache_->Value(handle)),
                             FLAGS_value_bytes);
        } else {
          // do insert
          cache_->Insert(key, createValue(thread->rnd), &helper2,
                         FLAGS_value_bytes, &handle);
        }
      } else if (random_op < insert_threshold_) {
        if (handle) {
          cache_->Release(handle);
          handle = nullptr;
        }
        // do insert
        cache_->Insert(key, createValue(thread->rnd), &helper3,
                       FLAGS_value_bytes, &handle);
      } else if (random_op < lookup_threshold_) {
        if (handle) {
          cache_->Release(handle);
          handle = nullptr;
        }
        // do lookup
        handle = cache_->Lookup(key, &helper2, create_cb, Cache::Priority::LOW,
                                true);
        if (handle) {
          // do something with the data
          result += NPHash64(static_cast<char*>(cache_->Value(handle)),
                             FLAGS_value_bytes);
        }
      } else if (random_op < erase_threshold_) {
        // do erase
        cache_->Erase(key);
      } else {
        // Should be extremely unlikely (noop)
        assert(random_op >= kHundredthUint64 * 100U);
      }
      thread->latency_ns_hist.Add(timer.ElapsedNanos());
    }
    if (handle) {
      cache_->Release(handle);
      handle = nullptr;
    }
    // Ensure computations on `result` are not optimized away.
    if (result == 1) {
      printf("You are extremely unlucky(2). Try again.\n");
      exit(1);
    }
    thread->duration_us = clock->NowMicros() - start_time;
  }

  void PrintEnv() const {
    printf("RocksDB version     : %d.%d\n", kMajorVersion, kMinorVersion);
    printf("Number of threads   : %u\n", FLAGS_threads);
    printf("Ops per thread      : %" PRIu64 "\n", FLAGS_ops_per_thread);
    printf("Cache size          : %s\n",
           BytesToHumanString(FLAGS_cache_size).c_str());
    printf("Num shard bits      : %u\n", FLAGS_num_shard_bits);
    printf("Max key             : %" PRIu64 "\n", max_key_);
    printf("Resident ratio      : %g\n", FLAGS_resident_ratio);
    printf("Skew degree         : %u\n", FLAGS_skew);
    printf("Populate cache      : %d\n", int{FLAGS_populate_cache});
    printf("Lookup+Insert pct   : %u%%\n", FLAGS_lookup_insert_percent);
    printf("Insert percentage   : %u%%\n", FLAGS_insert_percent);
    printf("Lookup percentage   : %u%%\n", FLAGS_lookup_percent);
    printf("Erase percentage    : %u%%\n", FLAGS_erase_percent);
    std::ostringstream stats;
    if (FLAGS_gather_stats) {
      stats << "enabled (" << FLAGS_gather_stats_sleep_ms << "ms, "
            << FLAGS_gather_stats_entries_per_lock << "/lock)";
    } else {
      stats << "disabled";
    }
    printf("Gather stats        : %s\n", stats.str().c_str());
    printf("----------------------------\n");
  }
};

// TODO: better description (see PR #9126 for some info)
class StressCacheKey {
 public:
  void Run() {
    if (FLAGS_sck_footer_unique_id) {
      FLAGS_sck_db_count = 1;
    }

    uint64_t mb_per_day =
        uint64_t{FLAGS_sck_files_per_day} * FLAGS_sck_file_size_mb;
    printf("Total cache or DBs size: %gTiB  Writing %g MiB/s or %gTiB/day\n",
           FLAGS_sck_file_size_mb / 1024.0 / 1024.0 *
               std::pow(2.0, FLAGS_sck_table_bits),
           mb_per_day / 86400.0, mb_per_day / 1024.0 / 1024.0);
    multiplier_ = std::pow(2.0, 128 - FLAGS_sck_keep_bits) /
                  (FLAGS_sck_file_size_mb * 1024.0 * 1024.0);
    printf(
        "Multiply by %g to correct for simulation losses (but still assume "
        "whole file cached)\n",
        multiplier_);
    restart_nfiles_ = FLAGS_sck_files_per_day / FLAGS_sck_restarts_per_day;
    double without_ejection =
        std::pow(1.414214, FLAGS_sck_keep_bits) / FLAGS_sck_files_per_day;
    printf(
        "Without ejection, expect random collision after %g days (%g "
        "corrected)\n",
        without_ejection, without_ejection * multiplier_);
    double with_full_table =
        std::pow(2.0, FLAGS_sck_keep_bits - FLAGS_sck_table_bits) /
        FLAGS_sck_files_per_day;
    printf(
        "With ejection and full table, expect random collision after %g "
        "days (%g corrected)\n",
        with_full_table, with_full_table * multiplier_);
    collisions_ = 0;

    for (int i = 1; collisions_ < FLAGS_sck_min_collision; i++) {
      RunOnce();
      if (collisions_ == 0) {
        printf(
            "No collisions after %d x %u days                              "
            "                   \n",
            i, FLAGS_sck_duration);
      } else {
        double est = 1.0 * i * FLAGS_sck_duration / collisions_;
        printf("%" PRIu64
               " collisions after %d x %u days, est %g days between (%g "
               "corrected)        \n",
               collisions_, i, FLAGS_sck_duration, est, est * multiplier_);
      }
    }
  }

  void RunOnce() {
    const size_t db_count = FLAGS_sck_db_count;
    dbs_.reset(new TableProperties[db_count]{});
    const size_t table_mask = (size_t{1} << FLAGS_sck_table_bits) - 1;
    table_.reset(new uint64_t[table_mask + 1]{});
    if (FLAGS_sck_keep_bits > 64) {
      FLAGS_sck_keep_bits = 64;
    }
    uint32_t shift_away = 64 - FLAGS_sck_keep_bits;
    uint32_t shift_away_b = shift_away / 3;
    uint32_t shift_away_a = shift_away - shift_away_b;

    process_count_ = 0;
    session_count_ = 0;
    ResetProcess();

    Random64 r{std::random_device{}()};

    uint64_t max_file_count =
        uint64_t{FLAGS_sck_files_per_day} * FLAGS_sck_duration;
    uint64_t file_count = 0;
    uint32_t report_count = 0;
    uint32_t collisions_this_run = 0;
    // Round robin through DBs
    for (size_t db_i = 0;; ++db_i) {
      if (db_i >= db_count) {
        db_i = 0;
      }
      if (file_count >= max_file_count) {
        break;
      }
      if (!FLAGS_sck_footer_unique_id && r.OneIn(FLAGS_sck_reopen_nfiles)) {
        ResetSession(db_i);
      } else if (r.OneIn(restart_nfiles_)) {
        ResetProcess();
      }
      OffsetableCacheKey ock;
      dbs_[db_i].orig_file_number += 1;
      // skip some file numbers, unless 1 DB so that that can simulate
      // better (DB-independent) unique IDs
      if (db_count > 1) {
        dbs_[db_i].orig_file_number += (r.Next() & 3);
      }
      BlockBasedTable::SetupBaseCacheKey(&dbs_[db_i], "", 42, 42, &ock);
      CacheKey ck = ock.WithOffset(0);
      uint64_t stripped;
      if (FLAGS_sck_randomize) {
        stripped = GetSliceHash64(ck.AsSlice()) >> shift_away;
      } else if (FLAGS_sck_footer_unique_id) {
        uint32_t a = DecodeFixed32(ck.AsSlice().data() + 4) >> shift_away_a;
        uint32_t b = DecodeFixed32(ck.AsSlice().data() + 12) >> shift_away_b;
        stripped = (uint64_t{a} << 32) + b;
      } else {
        uint32_t a = DecodeFixed32(ck.AsSlice().data()) << shift_away_a;
        uint32_t b = DecodeFixed32(ck.AsSlice().data() + 12) >> shift_away_b;
        stripped = (uint64_t{a} << 32) + b;
      }
      if (stripped == 0) {
        // Unlikely, but we need to exclude tracking this value
        printf("Hit Zero!                                                  \n");
        continue;
      }
      file_count++;
      uint64_t h = NPHash64(reinterpret_cast<char*>(&stripped), 8);
      // Skew lifetimes
      size_t pos =
          std::min(Lower32of64(h) & table_mask, Upper32of64(h) & table_mask);
      if (table_[pos] == stripped) {
        collisions_this_run++;
        // To predict probability of no collisions, we have to get rid of
        // correlated collisions, which this takes care of:
        ResetProcess();
      } else {
        // Replace
        table_[pos] = stripped;
      }

      if (++report_count == FLAGS_sck_files_per_day) {
        report_count = 0;
        // Estimate fill %
        size_t incr = table_mask / 1000;
        size_t sampled_count = 0;
        for (size_t i = 0; i <= table_mask; i += incr) {
          if (table_[i] != 0) {
            sampled_count++;
          }
        }
        // Report
        printf(
            "%" PRIu64 " days, %" PRIu64 " proc, %" PRIu64
            " sess, %u coll, occ %g%%, ejected %g%%   \r",
            file_count / FLAGS_sck_files_per_day, process_count_,
            session_count_, collisions_this_run, 100.0 * sampled_count / 1000.0,
            100.0 * (1.0 - sampled_count / 1000.0 * table_mask / file_count));
        fflush(stdout);
      }
    }
    collisions_ += collisions_this_run;
  }

  void ResetSession(size_t i) {
    dbs_[i].db_session_id = DBImpl::GenerateDbSessionId(nullptr);
    session_count_++;
  }

  void ResetProcess() {
    process_count_++;
    DBImpl::TEST_ResetDbSessionIdGen();
    for (size_t i = 0; i < FLAGS_sck_db_count; ++i) {
      ResetSession(i);
    }
    if (FLAGS_sck_footer_unique_id) {
      dbs_[0].orig_file_number = 0;
    }
  }

 private:
  // Use db_session_id and orig_file_number from TableProperties
  std::unique_ptr<TableProperties[]> dbs_;
  std::unique_ptr<uint64_t[]> table_;
  uint64_t process_count_ = 0;
  uint64_t session_count_ = 0;
  uint64_t collisions_ = 0;
  uint32_t restart_nfiles_ = 0;
  double multiplier_ = 0.0;
};

int cache_bench_tool(int argc, char** argv) {
  ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_stress_cache_key) {
    // Alternate tool
    StressCacheKey().Run();
    return 0;
  }

  if (FLAGS_threads <= 0) {
    fprintf(stderr, "threads number <= 0\n");
    exit(1);
  }

  ROCKSDB_NAMESPACE::CacheBench bench;
  if (FLAGS_populate_cache) {
    bench.PopulateCache();
    printf("Population complete\n");
    printf("----------------------------\n");
  }
  if (bench.Run()) {
    return 0;
  } else {
    return 1;
  }
}  // namespace ROCKSDB_NAMESPACE
}  // namespace ROCKSDB_NAMESPACE

#endif  // GFLAGS
