#include "test-helper.h"

#include <random>
#include <thread>
#include <vector>

static unsigned num_threads = std::thread::hardware_concurrency();
static constexpr unsigned num_rounds = 100000;

template <typename Tag>
struct FuzzerTests : testing::Test {
  void SetUp() override {
    value::created.store(0);
    value::destroyed.store(0);
  }
  
  struct value {
    static inline std::atomic<unsigned> created{0};
    static inline std::atomic<unsigned> destroyed{0};
    
    explicit value(int value) : v(value) {
      created.fetch_add(1, std::memory_order_relaxed);
    }
    
    ~value() {
      destroyed.fetch_add(1, std::memory_order_relaxed);
    }
    
    const int v;
  };
  
  struct alignas(64) slot {
    std::shared_ptr<mellon::future<std::unique_ptr<value>, Tag>> future;
    std::atomic<unsigned> round{0};
  };
  
  template <class C>
  void run_fuzz(C consumer) {
    ASSERT_TRUE(num_threads >= 2);
    
    std::atomic<unsigned> producer_idx{0};
    unsigned num_slots = 64 * num_threads;
    std::vector<typename FuzzerTests<Tag>::slot> slots(num_slots);
    std::atomic<unsigned> consumer_idx{0};
    std::atomic<unsigned> num_resolved_futures{0};
    
    std::vector<std::thread> threads;
    // producer
    for (unsigned i = 0; i < num_threads / 2; ++i) {
      threads.emplace_back(std::thread([&producer_idx, &slots, i, num_slots]() {
        std::mt19937 rand(i);
        for (unsigned r = 0; r < num_rounds; ++r) {
          auto [f, p] = mellon::make_promise<std::unique_ptr<value>, Tag>();
          
          auto idx = producer_idx.fetch_add(1);
          auto slot_round = (idx / num_slots) * 2;
          idx %= num_slots;
          while (slots[idx].round.load(std::memory_order_acquire) != slot_round) {
            // wait for the consumer to clear the future
            std::this_thread::yield();
          }
          ASSERT_EQ(nullptr, slots[idx].future);
          slots[idx].future = std::make_shared<mellon::future<std::unique_ptr<value>, Tag>>(std::move(f));
          slots[idx].round.store(slot_round + 1, std::memory_order_release);
          
          // simulate random delays to achieve different timings
          volatile auto work = rand() % 2048;
          for (unsigned x = 0; x < work; ++x) {
            [[maybe_unused]] auto _ = work;
          }
          
          std::move(p).fulfill(std::make_unique<value>(idx * (slot_round + 1)));
        }
      }));
    }
    
    // consumer
    for (unsigned i = 0; i < num_threads / 2; ++i) {
      threads.emplace_back(std::thread([consumer, &consumer_idx, &slots, i,
                                        &num_resolved_futures, num_slots]() {
        std::mt19937 rand(num_threads + i);
        for (unsigned r = 0; r < num_rounds; ++r) {
          auto idx = consumer_idx.fetch_add(1);
          auto slot_round = (idx / num_slots) * 2 + 1;
          idx %= num_slots;
          
          assert(slot_round % 2 != 0);
          while (slots[idx].round.load(std::memory_order_acquire) != slot_round) {
            // wait for the producer to store the future
            std::this_thread::yield();
          }
          auto f = std::move(slots[idx].future);
          ASSERT_NE(nullptr, f);
          slots[idx].round.store(slot_round + 1, std::memory_order_release);

          // simulate some random amount of work to achieve different timings
          volatile auto work = rand() % 2048;
          for (unsigned x = 0; x < work; ++x) {
            [[maybe_unused]] auto _ = work;
          }
          
          consumer(std::move(f), idx, slot_round, num_resolved_futures);
        }
      }));
    }
    
    for (auto& t : threads) {
      t.join();
    }
    
    EXPECT_EQ(num_rounds * num_threads / 2, num_resolved_futures.load());
    EXPECT_EQ(num_rounds * num_threads / 2, value::created.load());
    EXPECT_EQ(num_rounds * num_threads / 2, value::destroyed.load());
  }
};

TYPED_TEST_CASE(FuzzerTests, my_test_tags);

TYPED_TEST(FuzzerTests, finally) {
  using value = typename FuzzerTests<TypeParam>::value;
  this->run_fuzz([](auto future, unsigned idx, unsigned slot_round, auto& num_resolved_futures) {
    // some large data to be captured by the finally lambda so that it cannot be
    // embedded in the existing continuation step
    char data[64];

    auto f2 = future;
    std::move(*future)
      .and_then([slot_round](std::unique_ptr<value> x) noexcept { return x->v / slot_round; })
      .finally([idx, f2, data, &num_resolved_futures](unsigned x) noexcept {
        EXPECT_EQ(idx, x);
        num_resolved_futures.fetch_add(1, std::memory_order_relaxed);
      });
  });
}

TYPED_TEST(FuzzerTests, finalize) {
  using value = typename FuzzerTests<TypeParam>::value;
  this->run_fuzz([](auto future, unsigned idx, unsigned slot_round, auto& num_resolved_futures) {
    auto f2 = future;
    std::move(*future)
      .and_then([slot_round](std::unique_ptr<value> x) noexcept { return x->v / slot_round; })
      .and_then([idx, f2, &num_resolved_futures](unsigned x) noexcept {
        EXPECT_EQ(idx, x);
        num_resolved_futures.fetch_add(1, std::memory_order_relaxed);
        return x;
      })
      .finalize();
  });
}