////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "proxy_filter.hpp"

#include <bit>

#include "cost.hpp"
#include "utils/bitset.hpp"

#include <absl/synchronization/mutex.h>

namespace irs {

// Bitset expecting doc iterator to be able only to move forward.
// So in case of "seek" to the still unfilled word
// internally it does bunch of "next" calls.
class lazy_filter_bitset : private util::noncopyable {
 public:
  using word_t = size_t;

  lazy_filter_bitset(const ExecutionContext& ctx,
                     const filter::prepared& filter)
    : manager_{ctx.memory} {
    const size_t bits = ctx.segment.docs_count() + doc_limits::min();
    words_ = bitset::bits_to_words(bits);

    auto bytes = sizeof(*this) + sizeof(word_t) * words_;
    manager_.Increase(bytes);
    Finally decrease = [&]() noexcept { ctx.memory.DecreaseChecked(bytes); };

    // TODO(MBkkt) use mask from segment manually to avoid virtual call
    real_doc_itr_ = ctx.segment.mask(filter.execute(ctx));

    real_doc_ = irs::get<document>(*real_doc_itr_);
    cost_ = cost::extract(*real_doc_itr_);

    set_ = std::allocator<word_t>{}.allocate(words_);
    std::memset(set_, 0, sizeof(word_t) * words_);
    begin_ = set_;
    end_ = begin_;

    bytes = 0;
  }

  ~lazy_filter_bitset() {
    std::allocator<word_t>{}.deallocate(set_, words_);
    manager_.Decrease(sizeof(*this) + sizeof(word_t) * words_);
  }

  bool get(size_t word_idx, word_t* data) {
    constexpr auto kBits{bits_required<word_t>()};
    IRS_ASSERT(set_);
    if (word_idx >= words_) {
      return false;
    }

    word_t* requested = set_ + word_idx;
    if (requested >= end_) {
      auto block_limit = ((word_idx + 1) * kBits) - 1;
      while (real_doc_itr_->next()) {
        auto doc_id = real_doc_->value;
        set_bit(set_[doc_id / kBits], doc_id % kBits);
        if (doc_id >= block_limit) {
          break;  // we've filled requested word
        }
      }
      end_ = requested + 1;
    }
    *data = *requested;
    return true;
  }

  cost::cost_t get_cost() const noexcept { return cost_; }

 private:
  IResourceManager& manager_;

  doc_iterator::ptr real_doc_itr_;
  const document* real_doc_{nullptr};
  cost::cost_t cost_;

  word_t* set_{nullptr};
  size_t words_{0};
  const word_t* begin_{nullptr};
  const word_t* end_{nullptr};
};

class lazy_filter_bitset_iterator : public doc_iterator,
                                    private util::noncopyable {
 public:
  explicit lazy_filter_bitset_iterator(lazy_filter_bitset& bitset) noexcept
    : bitset_(bitset), cost_(bitset_.get_cost()) {
    reset();
  }

  bool next() final {
    while (!word_) {
      if (bitset_.get(word_idx_, &word_)) {
        ++word_idx_;  // move only if ok. Or we could be overflowed!
        base_ += bits_required<lazy_filter_bitset::word_t>();
        doc_.value = base_ - 1;
        continue;
      }
      doc_.value = doc_limits::eof();
      word_ = 0;
      return false;
    }
    const doc_id_t delta = doc_id_t(std::countr_zero(word_));
    IRS_ASSERT(delta < bits_required<lazy_filter_bitset::word_t>());
    word_ = (word_ >> delta) >> 1;
    doc_.value += 1 + delta;
    return true;
  }

  doc_id_t seek(doc_id_t target) final {
    word_idx_ = target / bits_required<lazy_filter_bitset::word_t>();
    if (bitset_.get(word_idx_, &word_)) {
      const doc_id_t bit_idx =
        target % bits_required<lazy_filter_bitset::word_t>();
      base_ = word_idx_ * bits_required<lazy_filter_bitset::word_t>();
      word_ >>= bit_idx;
      doc_.value = base_ - 1 + bit_idx;
      ++word_idx_;  // mark this word as consumed
      // FIXME consider inlining to speedup
      next();
      return doc_.value;
    } else {
      doc_.value = doc_limits::eof();
      word_ = 0;
      return doc_.value;
    }
  }

  doc_id_t value() const noexcept final { return doc_.value; }

  attribute* get_mutable(type_info::type_id id) noexcept final {
    if (type<document>::id() == id) {
      return &doc_;
    }
    return type<cost>::id() == id ? &cost_ : nullptr;
  }

  void reset() noexcept {
    word_idx_ = 0;
    word_ = 0;
    // before the first word
    base_ = doc_limits::invalid() - bits_required<lazy_filter_bitset::word_t>();
    doc_.value = doc_limits::invalid();
  }

 private:
  lazy_filter_bitset& bitset_;
  cost cost_;
  document doc_;
  doc_id_t word_idx_{0};
  lazy_filter_bitset::word_t word_{0};
  doc_id_t base_{doc_limits::invalid()};
};

struct proxy_query_cache {
  proxy_query_cache(IResourceManager& memory, filter::ptr&& ptr) noexcept
    : real_filter_{std::move(ptr)}, readers_{Alloc{memory}} {}

  using Alloc = ManagedTypedAllocator<
    std::pair<const SubReader* const, std::unique_ptr<lazy_filter_bitset>>>;

  filter::ptr real_filter_;
  filter::prepared::ptr real_filter_prepared_;
  absl::Mutex readers_lock_;
  absl::flat_hash_map<
    const SubReader*, std::unique_ptr<lazy_filter_bitset>,
    absl::container_internal::hash_default_hash<const SubReader*>,
    absl::container_internal::hash_default_eq<const SubReader*>, Alloc>
    readers_;
};

class proxy_query : public filter::prepared {
 public:
  explicit proxy_query(proxy_filter::cache_ptr cache) : cache_{cache} {
    IRS_ASSERT(cache_);
    IRS_ASSERT(cache_->real_filter_prepared_);
  }

  doc_iterator::ptr execute(const ExecutionContext& ctx) const final {
    auto* cache_bitset = [&]() -> lazy_filter_bitset* {
      absl::ReaderMutexLock lock{&cache_->readers_lock_};
      auto it = cache_->readers_.find(&ctx.segment);
      if (it != cache_->readers_.end()) {
        return it->second.get();
      }
      return nullptr;
    }();
    if (!cache_bitset) {
      auto bitset = std::make_unique<lazy_filter_bitset>(
        ctx, *cache_->real_filter_prepared_);
      cache_bitset = bitset.get();
      absl::WriterMutexLock lock{&cache_->readers_lock_};
      IRS_ASSERT(!cache_->readers_.contains(&ctx.segment));
      cache_->readers_.emplace(&ctx.segment, std::move(bitset));
    }
    return memory::make_tracked<lazy_filter_bitset_iterator>(ctx.memory,
                                                             *cache_bitset);
  }

  void visit(const SubReader&, PreparedStateVisitor&, score_t) const final {
    // No terms to visit
  }

  score_t boost() const noexcept final { return kNoBoost; }

 private:
  proxy_filter::cache_ptr cache_;
};

filter::prepared::ptr proxy_filter::prepare(const PrepareContext& ctx) const {
  // Currently we do not support caching scores.
  // Proxy filter should not be used with scorers!
  IRS_ASSERT(ctx.scorers.empty());
  if (!cache_ || !ctx.scorers.empty()) {
    return filter::prepared::empty();
  }
  if (!cache_->real_filter_prepared_) {
    cache_->real_filter_prepared_ = cache_->real_filter_->prepare(ctx);
    cache_->real_filter_.reset();
  }
  return memory::make_tracked<proxy_query>(ctx.memory, cache_);
}

filter& proxy_filter::cache_filter(IResourceManager& memory,
                                   filter::ptr&& real) {
  cache_ = std::make_shared<proxy_query_cache>(memory, std::move(real));
  IRS_ASSERT(cache_->real_filter_);
  return *cache_->real_filter_;
}

}  // namespace irs
