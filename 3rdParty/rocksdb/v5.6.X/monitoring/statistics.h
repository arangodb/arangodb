//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//  This source code is also licensed under the GPLv2 license found in the
//  COPYING file in the root directory of this source tree.
//
#pragma once
#include "rocksdb/statistics.h"

#include <vector>
#include <atomic>
#include <string>

#include "monitoring/histogram.h"
#include "port/likely.h"
#include "port/port.h"
#include "util/core_local.h"
#include "util/mutexlock.h"

#ifdef __clang__
#define ROCKSDB_FIELD_UNUSED __attribute__((__unused__))
#else
#define ROCKSDB_FIELD_UNUSED
#endif  // __clang__

namespace rocksdb {

enum TickersInternal : uint32_t {
  INTERNAL_TICKER_ENUM_START = TICKER_ENUM_MAX,
  INTERNAL_TICKER_ENUM_MAX
};

enum HistogramsInternal : uint32_t {
  INTERNAL_HISTOGRAM_START = HISTOGRAM_ENUM_MAX,
  INTERNAL_HISTOGRAM_ENUM_MAX
};


class StatisticsImpl : public Statistics {
 public:
  StatisticsImpl(std::shared_ptr<Statistics> stats,
                 bool enable_internal_stats);
  virtual ~StatisticsImpl();

  virtual uint64_t getTickerCount(uint32_t ticker_type) const override;
  virtual void histogramData(uint32_t histogram_type,
                             HistogramData* const data) const override;
  std::string getHistogramString(uint32_t histogram_type) const override;

  virtual void setTickerCount(uint32_t ticker_type, uint64_t count) override;
  virtual uint64_t getAndResetTickerCount(uint32_t ticker_type) override;
  virtual void recordTick(uint32_t ticker_type, uint64_t count) override;
  virtual void measureTime(uint32_t histogram_type, uint64_t value) override;

  virtual Status Reset() override;
  virtual std::string ToString() const override;
  virtual bool HistEnabledForType(uint32_t type) const override;

 private:
  // If non-nullptr, forwards updates to the object pointed to by `stats_`.
  std::shared_ptr<Statistics> stats_;
  // TODO(ajkr): clean this up since there are no internal stats anymore
  bool enable_internal_stats_;
  // Synchronizes anything that operates across other cores' local data,
  // such that operations like Reset() can be performed atomically.
  mutable port::Mutex aggregate_lock_;

  // The ticker/histogram data are stored in this structure, which we will store
  // per-core. It is cache-aligned, so tickers/histograms belonging to different
  // cores can never share the same cache line.
  //
  // Alignment attributes expand to nothing depending on the platform
  struct StatisticsData {
    std::atomic_uint_fast64_t tickers_[INTERNAL_TICKER_ENUM_MAX] = {{0}};
    HistogramImpl histograms_[INTERNAL_HISTOGRAM_ENUM_MAX];
    char
        padding[(CACHE_LINE_SIZE -
                 (INTERNAL_TICKER_ENUM_MAX * sizeof(std::atomic_uint_fast64_t) +
                  INTERNAL_HISTOGRAM_ENUM_MAX * sizeof(HistogramImpl)) %
                     CACHE_LINE_SIZE) %
                CACHE_LINE_SIZE] ROCKSDB_FIELD_UNUSED;
  };

  static_assert(sizeof(StatisticsData) % 64 == 0, "Expected 64-byte aligned");

  CoreLocalArray<StatisticsData> per_core_stats_;

  uint64_t getTickerCountLocked(uint32_t ticker_type) const;
  std::unique_ptr<HistogramImpl> getHistogramImplLocked(
      uint32_t histogram_type) const;
  void setTickerCountLocked(uint32_t ticker_type, uint64_t count);
};

// Utility functions
inline void MeasureTime(Statistics* statistics, uint32_t histogram_type,
                        uint64_t value) {
  if (statistics) {
    statistics->measureTime(histogram_type, value);
  }
}

inline void RecordTick(Statistics* statistics, uint32_t ticker_type,
                       uint64_t count = 1) {
  if (statistics) {
    statistics->recordTick(ticker_type, count);
  }
}

inline void SetTickerCount(Statistics* statistics, uint32_t ticker_type,
                           uint64_t count) {
  if (statistics) {
    statistics->setTickerCount(ticker_type, count);
  }
}

}
