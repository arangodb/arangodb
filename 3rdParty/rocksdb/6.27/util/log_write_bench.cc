//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#ifndef GFLAGS
#include <cstdio>
int main() {
  fprintf(stderr, "Please install gflags to run rocksdb tools\n");
  return 1;
}
#else

#include "file/writable_file_writer.h"
#include "monitoring/histogram.h"
#include "rocksdb/env.h"
#include "rocksdb/system_clock.h"
#include "test_util/testharness.h"
#include "test_util/testutil.h"
#include "util/gflags_compat.h"

using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::SetUsageMessage;

// A simple benchmark to simulate transactional logs

DEFINE_int32(num_records, 6000, "Number of records.");
DEFINE_int32(record_size, 249, "Size of each record.");
DEFINE_int32(record_interval, 10000, "Interval between records (microSec)");
DEFINE_int32(bytes_per_sync, 0, "bytes_per_sync parameter in EnvOptions");
DEFINE_bool(enable_sync, false, "sync after each write.");

namespace ROCKSDB_NAMESPACE {
void RunBenchmark() {
  std::string file_name = test::PerThreadDBPath("log_write_benchmark.log");
  DBOptions options;
  Env* env = Env::Default();
  const auto& clock = env->GetSystemClock();
  EnvOptions env_options = env->OptimizeForLogWrite(EnvOptions(), options);
  env_options.bytes_per_sync = FLAGS_bytes_per_sync;
  std::unique_ptr<WritableFile> file;
  env->NewWritableFile(file_name, &file, env_options);
  std::unique_ptr<WritableFileWriter> writer;
  writer.reset(new WritableFileWriter(std::move(file), file_name, env_options,
                                      clock, nullptr /* stats */,
                                      options.listeners));

  std::string record;
  record.assign(FLAGS_record_size, 'X');

  HistogramImpl hist;

  uint64_t start_time = clock->NowMicros();
  for (int i = 0; i < FLAGS_num_records; i++) {
    uint64_t start_nanos = clock->NowNanos();
    writer->Append(record);
    writer->Flush();
    if (FLAGS_enable_sync) {
      writer->Sync(false);
    }
    hist.Add(clock->NowNanos() - start_nanos);

    if (i % 1000 == 1) {
      fprintf(stderr, "Wrote %d records...\n", i);
    }

    int time_to_sleep =
        (i + 1) * FLAGS_record_interval - (clock->NowMicros() - start_time);
    if (time_to_sleep > 0) {
      clock->SleepForMicroseconds(time_to_sleep);
    }
  }

  fprintf(stderr, "Distribution of latency of append+flush: \n%s",
          hist.ToString().c_str());
}
}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  SetUsageMessage(std::string("\nUSAGE:\n") + std::string(argv[0]) +
                  " [OPTIONS]...");
  ParseCommandLineFlags(&argc, &argv, true);

  ROCKSDB_NAMESPACE::RunBenchmark();
  return 0;
}

#endif  // GFLAGS
