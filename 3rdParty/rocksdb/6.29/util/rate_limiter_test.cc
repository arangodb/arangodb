//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/rate_limiter.h"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <limits>

#include "db/db_test_util.h"
#include "options/options_parser.h"
#include "port/port.h"
#include "rocksdb/convenience.h"
#include "rocksdb/system_clock.h"
#include "rocksdb/utilities/options_type.h"
#include "test_util/sync_point.h"
#include "test_util/testharness.h"
#include "util/random.h"

namespace ROCKSDB_NAMESPACE {

// TODO(yhchiang): the rate will not be accurate when we run test in parallel.
class RateLimiterTest : public testing::Test {
 protected:
  ~RateLimiterTest() override {
    SyncPoint::GetInstance()->DisableProcessing();
    SyncPoint::GetInstance()->ClearAllCallBacks();
  }
};

TEST_F(RateLimiterTest, OverflowRate) {
  GenericRateLimiter limiter(port::kMaxInt64, 1000, 10,
                             RateLimiter::Mode::kWritesOnly,
                             SystemClock::Default(), false /* auto_tuned */);
  ASSERT_GT(limiter.GetSingleBurstBytes(), 1000000000ll);
}

TEST_F(RateLimiterTest, StartStop) {
  std::unique_ptr<RateLimiter> limiter(NewGenericRateLimiter(100, 100, 10));
}

TEST_F(RateLimiterTest, GetTotalBytesThrough) {
  std::unique_ptr<RateLimiter> limiter(NewGenericRateLimiter(
      200 /* rate_bytes_per_sec */, 1000 * 1000 /* refill_period_us */,
      10 /* fairness */));
  for (int i = Env::IO_LOW; i <= Env::IO_TOTAL; ++i) {
    ASSERT_EQ(limiter->GetTotalBytesThrough(static_cast<Env::IOPriority>(i)),
              0);
  }

  std::int64_t request_byte = 200;
  std::int64_t request_byte_sum = 0;
  for (int i = Env::IO_LOW; i < Env::IO_TOTAL; ++i) {
    limiter->Request(request_byte, static_cast<Env::IOPriority>(i),
                     nullptr /* stats */, RateLimiter::OpType::kWrite);
    request_byte_sum += request_byte;
  }

  for (int i = Env::IO_LOW; i < Env::IO_TOTAL; ++i) {
    EXPECT_EQ(limiter->GetTotalBytesThrough(static_cast<Env::IOPriority>(i)),
              request_byte)
        << "Failed to track total_bytes_through_ correctly when IOPriority = "
        << static_cast<Env::IOPriority>(i);
  }
  EXPECT_EQ(limiter->GetTotalBytesThrough(Env::IO_TOTAL), request_byte_sum)
      << "Failed to track total_bytes_through_ correctly when IOPriority = "
         "Env::IO_TOTAL";
}

TEST_F(RateLimiterTest, GetTotalRequests) {
  std::unique_ptr<RateLimiter> limiter(NewGenericRateLimiter(
      200 /* rate_bytes_per_sec */, 1000 * 1000 /* refill_period_us */,
      10 /* fairness */));
  for (int i = Env::IO_LOW; i <= Env::IO_TOTAL; ++i) {
    ASSERT_EQ(limiter->GetTotalRequests(static_cast<Env::IOPriority>(i)), 0);
  }

  std::int64_t total_requests_sum = 0;
  for (int i = Env::IO_LOW; i < Env::IO_TOTAL; ++i) {
    limiter->Request(200, static_cast<Env::IOPriority>(i), nullptr /* stats */,
                     RateLimiter::OpType::kWrite);
    total_requests_sum += 1;
  }

  for (int i = Env::IO_LOW; i < Env::IO_TOTAL; ++i) {
    EXPECT_EQ(limiter->GetTotalRequests(static_cast<Env::IOPriority>(i)), 1)
        << "Failed to track total_requests_ correctly when IOPriority = "
        << static_cast<Env::IOPriority>(i);
  }
  EXPECT_EQ(limiter->GetTotalRequests(Env::IO_TOTAL), total_requests_sum)
      << "Failed to track total_requests_ correctly when IOPriority = "
         "Env::IO_TOTAL";
}

TEST_F(RateLimiterTest, GetTotalPendingRequests) {
  std::unique_ptr<RateLimiter> limiter(NewGenericRateLimiter(
      200 /* rate_bytes_per_sec */, 1000 * 1000 /* refill_period_us */,
      10 /* fairness */));
  int64_t total_pending_requests = 0;
  for (int i = Env::IO_LOW; i <= Env::IO_TOTAL; ++i) {
    ASSERT_OK(limiter->GetTotalPendingRequests(
        &total_pending_requests, static_cast<Env::IOPriority>(i)));
    ASSERT_EQ(total_pending_requests, 0);
  }
  // This is a variable for making sure the following callback is called
  // and the assertions in it are indeed excuted
  bool nonzero_pending_requests_verified = false;
  SyncPoint::GetInstance()->SetCallBack(
      "GenericRateLimiter::Request:PostEnqueueRequest", [&](void* arg) {
        port::Mutex* request_mutex = (port::Mutex*)arg;
        // We temporarily unlock the mutex so that the following
        // GetTotalPendingRequests() can acquire it
        request_mutex->Unlock();
        for (int i = Env::IO_LOW; i <= Env::IO_TOTAL; ++i) {
          EXPECT_OK(limiter->GetTotalPendingRequests(
              &total_pending_requests, static_cast<Env::IOPriority>(i)))
              << "Failed to return total pending requests for priority level = "
              << static_cast<Env::IOPriority>(i);
          if (i == Env::IO_USER || i == Env::IO_TOTAL) {
            EXPECT_EQ(total_pending_requests, 1)
                << "Failed to correctly return total pending requests for "
                   "priority level = "
                << static_cast<Env::IOPriority>(i);
          } else {
            EXPECT_EQ(total_pending_requests, 0)
                << "Failed to correctly return total pending requests for "
                   "priority level = "
                << static_cast<Env::IOPriority>(i);
          }
        }
        // We lock the mutex again so that the request thread can resume running
        // with the mutex locked
        request_mutex->Lock();
        nonzero_pending_requests_verified = true;
      });

  SyncPoint::GetInstance()->EnableProcessing();
  limiter->Request(200, Env::IO_USER, nullptr /* stats */,
                   RateLimiter::OpType::kWrite);
  ASSERT_EQ(nonzero_pending_requests_verified, true);
  for (int i = Env::IO_LOW; i <= Env::IO_TOTAL; ++i) {
    EXPECT_OK(limiter->GetTotalPendingRequests(&total_pending_requests,
                                               static_cast<Env::IOPriority>(i)))
        << "Failed to return total pending requests for priority level = "
        << static_cast<Env::IOPriority>(i);
    EXPECT_EQ(total_pending_requests, 0)
        << "Failed to correctly return total pending requests for priority "
           "level = "
        << static_cast<Env::IOPriority>(i);
  }
  SyncPoint::GetInstance()->DisableProcessing();
  SyncPoint::GetInstance()->ClearCallBack(
      "GenericRateLimiter::Request:PostEnqueueRequest");
}

TEST_F(RateLimiterTest, Modes) {
  for (auto mode : {RateLimiter::Mode::kWritesOnly,
                    RateLimiter::Mode::kReadsOnly, RateLimiter::Mode::kAllIo}) {
    GenericRateLimiter limiter(2000 /* rate_bytes_per_sec */,
                               1000 * 1000 /* refill_period_us */,
                               10 /* fairness */, mode, SystemClock::Default(),
                               false /* auto_tuned */);
    limiter.Request(1000 /* bytes */, Env::IO_HIGH, nullptr /* stats */,
                    RateLimiter::OpType::kRead);
    if (mode == RateLimiter::Mode::kWritesOnly) {
      ASSERT_EQ(0, limiter.GetTotalBytesThrough(Env::IO_HIGH));
    } else {
      ASSERT_EQ(1000, limiter.GetTotalBytesThrough(Env::IO_HIGH));
    }

    limiter.Request(1000 /* bytes */, Env::IO_HIGH, nullptr /* stats */,
                    RateLimiter::OpType::kWrite);
    if (mode == RateLimiter::Mode::kAllIo) {
      ASSERT_EQ(2000, limiter.GetTotalBytesThrough(Env::IO_HIGH));
    } else {
      ASSERT_EQ(1000, limiter.GetTotalBytesThrough(Env::IO_HIGH));
    }
  }
}

TEST_F(RateLimiterTest, GeneratePriorityIterationOrder) {
  std::unique_ptr<RateLimiter> limiter(NewGenericRateLimiter(
      200 /* rate_bytes_per_sec */, 1000 * 1000 /* refill_period_us */,
      10 /* fairness */));

  bool possible_random_one_in_fairness_results_for_high_mid_pri[4][2] = {
      {false, false}, {false, true}, {true, false}, {true, true}};
  std::vector<Env::IOPriority> possible_priority_iteration_orders[4] = {
      {Env::IO_USER, Env::IO_HIGH, Env::IO_MID, Env::IO_LOW},
      {Env::IO_USER, Env::IO_HIGH, Env::IO_LOW, Env::IO_MID},
      {Env::IO_USER, Env::IO_MID, Env::IO_LOW, Env::IO_HIGH},
      {Env::IO_USER, Env::IO_LOW, Env::IO_MID, Env::IO_HIGH}};

  for (int i = 0; i < 4; ++i) {
    // These are variables for making sure the following callbacks are called
    // and the assertion in the last callback is indeed excuted
    bool high_pri_iterated_after_mid_low_pri_set = false;
    bool mid_pri_itereated_after_low_pri_set = false;
    bool pri_iteration_order_verified = false;
    SyncPoint::GetInstance()->SetCallBack(
        "GenericRateLimiter::GeneratePriorityIterationOrder::"
        "PostRandomOneInFairnessForHighPri",
        [&](void* arg) {
          bool* high_pri_iterated_after_mid_low_pri = (bool*)arg;
          *high_pri_iterated_after_mid_low_pri =
              possible_random_one_in_fairness_results_for_high_mid_pri[i][0];
          high_pri_iterated_after_mid_low_pri_set = true;
        });

    SyncPoint::GetInstance()->SetCallBack(
        "GenericRateLimiter::GeneratePriorityIterationOrder::"
        "PostRandomOneInFairnessForMidPri",
        [&](void* arg) {
          bool* mid_pri_itereated_after_low_pri = (bool*)arg;
          *mid_pri_itereated_after_low_pri =
              possible_random_one_in_fairness_results_for_high_mid_pri[i][1];
          mid_pri_itereated_after_low_pri_set = true;
        });

    SyncPoint::GetInstance()->SetCallBack(
        "GenericRateLimiter::GeneratePriorityIterationOrder::"
        "PreReturnPriIterationOrder",
        [&](void* arg) {
          std::vector<Env::IOPriority>* pri_iteration_order =
              (std::vector<Env::IOPriority>*)arg;
          EXPECT_EQ(*pri_iteration_order, possible_priority_iteration_orders[i])
              << "Failed to generate priority iteration order correctly when "
                 "high_pri_iterated_after_mid_low_pri = "
              << possible_random_one_in_fairness_results_for_high_mid_pri[i][0]
              << ", mid_pri_itereated_after_low_pri = "
              << possible_random_one_in_fairness_results_for_high_mid_pri[i][1]
              << std::endl;
          pri_iteration_order_verified = true;
        });

    SyncPoint::GetInstance()->EnableProcessing();
    limiter->Request(200 /* request max bytes to drain so that refill and order
                           generation will be triggered every time
                           GenericRateLimiter::Request() is called */
                     ,
                     Env::IO_USER, nullptr /* stats */,
                     RateLimiter::OpType::kWrite);
    ASSERT_EQ(high_pri_iterated_after_mid_low_pri_set, true);
    ASSERT_EQ(mid_pri_itereated_after_low_pri_set, true);
    ASSERT_EQ(pri_iteration_order_verified, true);
    SyncPoint::GetInstance()->DisableProcessing();
    SyncPoint::GetInstance()->ClearCallBack(
        "GenericRateLimiter::GeneratePriorityIterationOrder::"
        "PreReturnPriIterationOrder");
    SyncPoint::GetInstance()->ClearCallBack(
        "GenericRateLimiter::GeneratePriorityIterationOrder::"
        "PostRandomOneInFairnessForMidPri");
    SyncPoint::GetInstance()->ClearCallBack(
        "GenericRateLimiter::GeneratePriorityIterationOrder::"
        "PostRandomOneInFairnessForHighPri");
  }
}

TEST_F(RateLimiterTest, Rate) {
  auto* env = Env::Default();
  struct Arg {
    Arg(int32_t _target_rate, int _burst)
        : limiter(NewGenericRateLimiter(_target_rate /* rate_bytes_per_sec */,
                                        100 * 1000 /* refill_period_us */,
                                        10 /* fairness */)),
          request_size(_target_rate /
                       10 /* refill period here is 1/10 second */),
          burst(_burst) {}
    std::unique_ptr<RateLimiter> limiter;
    int32_t request_size;
    int burst;
  };

  auto writer = [](void* p) {
    const auto& thread_clock = SystemClock::Default();
    auto* arg = static_cast<Arg*>(p);
    // Test for 2 seconds
    auto until = thread_clock->NowMicros() + 2 * 1000000;
    Random r((uint32_t)(thread_clock->NowNanos() %
                        std::numeric_limits<uint32_t>::max()));
    while (thread_clock->NowMicros() < until) {
      for (int i = 0; i < static_cast<int>(r.Skewed(arg->burst * 2) + 1); ++i) {
        arg->limiter->Request(r.Uniform(arg->request_size - 1) + 1,
                              Env::IO_USER, nullptr /* stats */,
                              RateLimiter::OpType::kWrite);
      }

      for (int i = 0; i < static_cast<int>(r.Skewed(arg->burst) + 1); ++i) {
        arg->limiter->Request(r.Uniform(arg->request_size - 1) + 1,
                              Env::IO_HIGH, nullptr /* stats */,
                              RateLimiter::OpType::kWrite);
      }

      for (int i = 0; i < static_cast<int>(r.Skewed(arg->burst / 2 + 1) + 1);
           ++i) {
        arg->limiter->Request(r.Uniform(arg->request_size - 1) + 1, Env::IO_MID,
                              nullptr /* stats */, RateLimiter::OpType::kWrite);
      }

      arg->limiter->Request(r.Uniform(arg->request_size - 1) + 1, Env::IO_LOW,
                            nullptr /* stats */, RateLimiter::OpType::kWrite);
    }
  };

  int samples = 0;
  int samples_at_minimum = 0;

  for (int i = 1; i <= 16; i *= 2) {
    int32_t target = i * 1024 * 10;
    Arg arg(target, i / 4 + 1);
    int64_t old_total_bytes_through = 0;
    for (int iter = 1; iter <= 2; ++iter) {
      // second iteration changes the target dynamically
      if (iter == 2) {
        target *= 2;
        arg.limiter->SetBytesPerSecond(target);
      }
      auto start = env->NowMicros();
      for (int t = 0; t < i; ++t) {
        env->StartThread(writer, &arg);
      }
      env->WaitForJoin();

      auto elapsed = env->NowMicros() - start;
      double rate =
          (arg.limiter->GetTotalBytesThrough() - old_total_bytes_through) *
          1000000.0 / elapsed;
      old_total_bytes_through = arg.limiter->GetTotalBytesThrough();
      fprintf(stderr,
              "request size [1 - %" PRIi32 "], limit %" PRIi32
              " KB/sec, actual rate: %lf KB/sec, elapsed %.2lf seconds\n",
              arg.request_size - 1, target / 1024, rate / 1024,
              elapsed / 1000000.0);

      ++samples;
      if (rate / target >= 0.80) {
        ++samples_at_minimum;
      }
      ASSERT_LE(rate / target, 1.25);
    }
  }

  // This can fail in heavily loaded CI environments
  bool skip_minimum_rate_check =
#if (defined(TRAVIS) || defined(CIRCLECI)) && defined(OS_MACOSX)
      true;
#else
      getenv("SANDCASTLE");
#endif
  if (skip_minimum_rate_check) {
    fprintf(stderr, "Skipped minimum rate check (%d / %d passed)\n",
            samples_at_minimum, samples);
  } else {
    ASSERT_EQ(samples_at_minimum, samples);
  }
}

TEST_F(RateLimiterTest, LimitChangeTest) {
  // starvation test when limit changes to a smaller value
  int64_t refill_period = 1000 * 1000;
  auto* env = Env::Default();
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
  struct Arg {
    Arg(int32_t _request_size, Env::IOPriority _pri,
        std::shared_ptr<RateLimiter> _limiter)
        : request_size(_request_size), pri(_pri), limiter(_limiter) {}
    int32_t request_size;
    Env::IOPriority pri;
    std::shared_ptr<RateLimiter> limiter;
  };

  auto writer = [](void* p) {
    auto* arg = static_cast<Arg*>(p);
    arg->limiter->Request(arg->request_size, arg->pri, nullptr /* stats */,
                          RateLimiter::OpType::kWrite);
  };

  for (uint32_t i = 1; i <= 16; i <<= 1) {
    int32_t target = i * 1024 * 10;
    // refill per second
    for (int iter = 0; iter < 2; iter++) {
      std::shared_ptr<RateLimiter> limiter =
          std::make_shared<GenericRateLimiter>(
              target, refill_period, 10, RateLimiter::Mode::kWritesOnly,
              SystemClock::Default(), false /* auto_tuned */);
      ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->LoadDependency(
          {{"GenericRateLimiter::Request",
            "RateLimiterTest::LimitChangeTest:changeLimitStart"},
           {"RateLimiterTest::LimitChangeTest:changeLimitEnd",
            "GenericRateLimiter::RefillBytesAndGrantRequests"}});
      Arg arg(target, Env::IO_HIGH, limiter);
      // The idea behind is to start a request first, then before it refills,
      // update limit to a different value (2X/0.5X). No starvation should
      // be guaranteed under any situation
      // TODO(lightmark): more test cases are welcome.
      env->StartThread(writer, &arg);
      int32_t new_limit = (target << 1) >> (iter << 1);
      TEST_SYNC_POINT("RateLimiterTest::LimitChangeTest:changeLimitStart");
      arg.limiter->SetBytesPerSecond(new_limit);
      TEST_SYNC_POINT("RateLimiterTest::LimitChangeTest:changeLimitEnd");
      env->WaitForJoin();
      fprintf(stderr,
              "[COMPLETE] request size %" PRIi32 " KB, new limit %" PRIi32
              "KB/sec, refill period %" PRIi64 " ms\n",
              target / 1024, new_limit / 1024, refill_period / 1000);
    }
  }
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->DisableProcessing();
}

TEST_F(RateLimiterTest, AutoTuneIncreaseWhenFull) {
  const std::chrono::seconds kTimePerRefill(1);
  const int kRefillsPerTune = 100;  // needs to match util/rate_limiter.cc

  SpecialEnv special_env(Env::Default(), /*time_elapse_only_sleep*/ true);

  auto stats = CreateDBStatistics();
  std::unique_ptr<RateLimiter> rate_limiter(new GenericRateLimiter(
      1000 /* rate_bytes_per_sec */,
      std::chrono::microseconds(kTimePerRefill).count(), 10 /* fairness */,
      RateLimiter::Mode::kWritesOnly, special_env.GetSystemClock(),
      true /* auto_tuned */));

  // Rate limiter uses `CondVar::TimedWait()`, which does not have access to the
  // `Env` to advance its time according to the fake wait duration. The
  // workaround is to install a callback that advance the `Env`'s mock time.
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "GenericRateLimiter::Request:PostTimedWait", [&](void* arg) {
        int64_t time_waited_us = *static_cast<int64_t*>(arg);
        special_env.SleepForMicroseconds(static_cast<int>(time_waited_us));
      });
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();

  // verify rate limit increases after a sequence of periods where rate limiter
  // is always drained
  int64_t orig_bytes_per_sec = rate_limiter->GetSingleBurstBytes();
  rate_limiter->Request(orig_bytes_per_sec, Env::IO_HIGH, stats.get(),
                        RateLimiter::OpType::kWrite);
  while (std::chrono::microseconds(special_env.NowMicros()) <=
         kRefillsPerTune * kTimePerRefill) {
    rate_limiter->Request(orig_bytes_per_sec, Env::IO_HIGH, stats.get(),
                          RateLimiter::OpType::kWrite);
  }
  int64_t new_bytes_per_sec = rate_limiter->GetSingleBurstBytes();
  ASSERT_GT(new_bytes_per_sec, orig_bytes_per_sec);

  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->DisableProcessing();
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->ClearCallBack(
      "GenericRateLimiter::Request:PostTimedWait");

  // decreases after a sequence of periods where rate limiter is not drained
  orig_bytes_per_sec = new_bytes_per_sec;
  special_env.SleepForMicroseconds(static_cast<int>(
      kRefillsPerTune * std::chrono::microseconds(kTimePerRefill).count()));
  // make a request so tuner can be triggered
  rate_limiter->Request(1 /* bytes */, Env::IO_HIGH, stats.get(),
                        RateLimiter::OpType::kWrite);
  new_bytes_per_sec = rate_limiter->GetSingleBurstBytes();
  ASSERT_LT(new_bytes_per_sec, orig_bytes_per_sec);
}

TEST_F(RateLimiterTest, CreateGenericRateLimiterFromString) {
  std::shared_ptr<RateLimiter> limiter;
  ConfigOptions config_options;
  std::string limiter_id = GenericRateLimiter::kClassName();
  ASSERT_OK(RateLimiter::CreateFromString(config_options, limiter_id + ":1024",
                                          &limiter));
  ASSERT_NE(limiter, nullptr);
  ASSERT_EQ(limiter->GetBytesPerSecond(), 1024U);
#ifndef ROCKSDB_LITE
  ASSERT_OK(RateLimiter::CreateFromString(
      config_options, "rate_bytes_per_sec=2048;id=" + limiter_id, &limiter));
  ASSERT_NE(limiter, nullptr);
  ASSERT_EQ(limiter->GetBytesPerSecond(), 2048U);
  ASSERT_NOK(RateLimiter::CreateFromString(
      config_options, "rate_bytes_per_sec=0;id=" + limiter_id, &limiter));
  ASSERT_NOK(RateLimiter::CreateFromString(
      config_options, "rate_bytes_per_sec=2048;fairness=0;id=" + limiter_id,
      &limiter));

  ASSERT_OK(
      RateLimiter::CreateFromString(config_options,
                                    "rate_bytes_per_sec=2048;refill_period_us="
                                    "1024;fairness=42;auto_tuned=true;"
                                    "mode=kReadsOnly;id=" +
                                        limiter_id,
                                    &limiter));
  ASSERT_NE(limiter, nullptr);
  auto opts =
      limiter->GetOptions<GenericRateLimiter::GenericRateLimiterOptions>();
  ASSERT_NE(opts, nullptr);
  ASSERT_EQ(opts->max_bytes_per_sec, 2048);
  ASSERT_EQ(opts->refill_period_us, 1024);
  ASSERT_EQ(opts->fairness, 42);
  ASSERT_EQ(opts->auto_tuned, true);
  ASSERT_TRUE(limiter->IsRateLimited(RateLimiter::OpType::kRead));
  ASSERT_FALSE(limiter->IsRateLimited(RateLimiter::OpType::kWrite));
#endif  // ROCKSDB_LITE
}

#ifndef ROCKSDB_LITE
// This test is for a rate limiter that has no name (Name() returns "").
// When the default Name() method is deprecated, this test should be removed.
TEST_F(RateLimiterTest, NoNameRateLimiter) {
  static std::unordered_map<std::string, OptionTypeInfo> dummy_limiter_options =
      {
          {"dummy",
           {0, OptionType::kInt, OptionVerificationType::kNormal,
            OptionTypeFlags::kNone}},
      };
  class NoNameRateLimiter : public RateLimiter {
   public:
    explicit NoNameRateLimiter(bool do_register) {
      if (do_register) {
        RegisterOptions("", &dummy, &dummy_limiter_options);
      }
    }
    void SetBytesPerSecond(int64_t /*bytes_per_second*/) override {}
    int64_t GetSingleBurstBytes() const override { return 0; }
    int64_t GetTotalBytesThrough(const Env::IOPriority /*pri*/) const override {
      return 0;
    }
    int64_t GetTotalRequests(const Env::IOPriority /*pri*/) const override {
      return 0;
    }
    int64_t GetBytesPerSecond() const override { return 0; }

   private:
    int dummy;
  };

  ConfigOptions config_options;
  DBOptions db_opts, copy;
  db_opts.rate_limiter.reset(new NoNameRateLimiter(false));
  ASSERT_EQ(db_opts.rate_limiter->GetId(), "");
  ASSERT_EQ(db_opts.rate_limiter->ToString(config_options), "");
  db_opts.rate_limiter.reset(new NoNameRateLimiter(true));
  ASSERT_EQ(db_opts.rate_limiter->GetId(), "");
  ASSERT_EQ(db_opts.rate_limiter->ToString(config_options), "");
  std::string opt_str;
  ASSERT_OK(GetStringFromDBOptions(config_options, db_opts, &opt_str));
  ASSERT_OK(
      GetDBOptionsFromString(config_options, DBOptions(), opt_str, &copy));
  ASSERT_OK(
      RocksDBOptionsParser::VerifyDBOptions(config_options, db_opts, copy));
  ASSERT_EQ(copy.rate_limiter, nullptr);
  ASSERT_NE(copy.rate_limiter, db_opts.rate_limiter);
}
#endif  // ROCKSDB_LITE

}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
