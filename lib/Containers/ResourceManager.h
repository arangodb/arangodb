////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/cpu-relax.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <type_traits>
#include <vector>

namespace arangodb {

// ResourceManager provides safe concurrent access to a shared resource
// using epoch-based reclamation (EBR). It allows multiple readers
// to access the current version of a resource while writers can
// safely update it. The EBR technique ensures that resources are
// only reclaimed when no readers are accessing them, preventing
// use-after-free issues in lock-free code.

template<typename T>
class ResourceManager {
 private:
  // Alignment for cache line to prevent false sharing
  struct alignas(64) EpochSlot {
    std::atomic<uint64_t> epoch{0};
  };

  // Resource management
  std::atomic<T*> current_resource;
  std::atomic<uint64_t> global_epoch{1};  // Start at 1, 0 means "not reading"
  std::mutex writer_mutex;

  // Epoch slots for reader tracking
  std::vector<EpochSlot> epoch_slots;

  // Thread-local storage for slot index
  static thread_local std::optional<size_t> thread_slot_index;

  // Constants
  static constexpr size_t EPOCH_SLOTS = 128;

  // Get the thread's slot index, initializing if needed
  size_t get_thread_slot() {
    // Choose randomly at first call to ensure nice distribution of
    // threads amongst slots. Then store in a thread-local variable
    // to avoid expensive random operations for every read.
    if (!thread_slot_index) {
      // Random initialization to distribute threads across slots
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<size_t> dist(0, EPOCH_SLOTS - 1);
      thread_slot_index = dist(gen);
    }
    return *thread_slot_index;
  }

 public:
  // Check if all active readers are using newer epochs than the given one
  void wait_reclaim(uint64_t epoch) {
    for (const auto& slot : epoch_slots) {
      // If slot is reading (non-zero) and using epoch <= target, we can't
      // reclaim
      while (true) {
        uint64_t slot_epoch = slot.epoch.load(std::memory_order_acquire);
        // This synchronizes with the memory_order_release in the `read()`
        // method. We must be sure that no writes are reordered after
        // the release or reads reordered before this acquire here to
        // guarantee safe reclamation.
        if (slot_epoch == 0 || slot_epoch > epoch) {
          break;
        }
        basics::cpu_relax();
      }
    }
  }

  // Constructor with initial resource
  explicit ResourceManager(std::unique_ptr<T> initial_resource)
      : epoch_slots(EPOCH_SLOTS) {
    current_resource.store(initial_resource.release(),
                           std::memory_order_relaxed);
  }

  // Destructor
  ~ResourceManager() {
    auto [current, epoch] = update(nullptr);
    wait_reclaim(epoch);
  }

  // Delete copy/move constructors and assignment operators
  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;
  ResourceManager(ResourceManager&&) = delete;
  ResourceManager& operator=(ResourceManager&&) = delete;

  // Reader API: Get access to the resource
  template<typename F>
  auto read(F&& f) -> decltype(f(std::declval<const T&>())) {
    // Get thread's slot
    size_t slot = get_thread_slot();

    // Get current global epoch
    uint64_t current_epoch = global_epoch.load(std::memory_order_acquire);

    // Try to find an available slot
    while (true) {
      // Try to announce reading at this epoch using compare_exchange
      uint64_t expected = 0;  // expected: 0 (not in use)
      if (epoch_slots[slot].epoch.compare_exchange_weak(
              expected, current_epoch, std::memory_order_acquire,
              std::memory_order_relaxed)) {
        // We use acquire here, since we want to avoid that the subsequent
        // load of the resource pointer is not reordered before the store
        // to the epoch_slot!
        // Successfully claimed the slot
        // Read resource pointer and execute reader function
        T* resource_ptr = current_resource.load(std::memory_order_acquire);
        // We use memory_order_acquire here to synchronize with the writer's
        // store to be able to see stuff to which the pointer points.

        using ReturnType = decltype(f(std::declval<const T&>()));

        if constexpr (std::is_void_v<ReturnType>) {
          // Handle void return type
          if (resource_ptr != nullptr) {
            f(*resource_ptr);
          }

          // Mark slot as "not reading" with a simple write
          epoch_slots[slot].epoch.store(0, std::memory_order_release);
          // This synchronizes with the `wait` or the `load` in the
          // `wait_reclaim()` method.

          return;
        } else {
          // Note that for the case of a nullptr, the result type should be
          // default constructible!
          ReturnType result{};

          // Execute reader function
          if (resource_ptr != nullptr) {
            result = f(*resource_ptr);
          }

          // Mark slot as "not reading" with a simple write
          epoch_slots[slot].epoch.store(0, std::memory_order_release);
          // This synchronizes with the `wait` or the `load` in the
          // `wait_reclaim()` method.

          return result;
        }
      }

      // Slot is in use, try the next one:
      slot += 1;
      slot = slot < EPOCH_SLOTS ? slot : slot - EPOCH_SLOTS;
      // Continue the loop with the new slot
    }
  }

  // Writer API: Update the resource
  std::pair<std::unique_ptr<T>, uint64_t> update(
      std::unique_ptr<T> new_resource) {
    std::lock_guard<std::mutex> lock(writer_mutex);

    // Extract raw pointer from unique_ptr
    T* new_ptr = new_resource.release();

    // Swap pointers:
    T* old_ptr = current_resource.exchange(new_ptr, std::memory_order_release);
    // This release synchronizes with the acquire in the read method.

    // Advance global epoch with release to ensure all threads see the new epoch
    // and new current_resource:
    uint64_t retire_epoch =
        global_epoch.fetch_add(1, std::memory_order_release);

    // We need that everybody who still sees the old value also uses the old
    // epoch! Therefore it is crucial that we first write the new pointer here
    // before increasing the epoch. In the read method, we first load the epoch
    // and then load the pointer. Therefore, it is possible (and tolerable)
    // that a reader uses the new pointer value together with the old epoch,
    // but no harm results from this!
    return std::pair(std::unique_ptr<T>(old_ptr), retire_epoch);
  }
};

// Initialize the thread_local storage
template<typename T>
thread_local std::optional<size_t> ResourceManager<T>::thread_slot_index;

}  // namespace arangodb
