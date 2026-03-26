////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>

#include "analysis/token_attributes.hpp"
#include "index/norm.hpp"
#include "search/scorer.hpp"
#include "store/memory_directory.hpp"
#include "utils/empty.hpp"
#include "utils/small_vector.hpp"

namespace irs {

template<typename Producer>
class WandWriterImpl final : public WandWriter {
  using EntryType = typename Producer::Entry;

 public:
  WandWriterImpl(const Scorer& scorer, size_t max_levels)
    : levels_{max_levels + 1}, producer_{scorer} {
    IRS_ASSERT(max_levels != 0);
  }

  bool Prepare(const ColumnProvider& reader, const feature_map_t& features,
               const attribute_provider& attrs) final {
    return producer_.Prepare(reader, features, attrs);
  }

  void Reset() noexcept final {
    for (auto& entry : levels_) {
      entry = {};
    }
  }

  void Update() noexcept final {
    IRS_ASSERT(!levels_.empty());
    producer_.Produce(levels_.front());
  }

  void Write(size_t level, memory_index_output& out) final {
    IRS_ASSERT(level + 1 < levels_.size());
    auto& entry = levels_[level];
    Producer::Produce(entry, levels_[level + 1]);
    Producer::Write(entry, out);
    entry = {};
  }

  void WriteRoot(size_t level, index_output& out) final {
    IRS_ASSERT(level < levels_.size());
    auto& entry = levels_[level];
    Producer::Write(entry, out);
  }

  uint8_t Size(size_t level) const noexcept final {
    IRS_ASSERT(level + 1 < levels_.size());
    const auto& entry = levels_[level];
    return Producer::Size(entry);
  }

  uint8_t SizeRoot(size_t level) noexcept final {
    IRS_ASSERT(level < levels_.size());
    auto it = levels_.begin();
    for (auto end = it + level; it != end;) {
      const auto& from = *it;
      Producer::Produce(from, *++it);
    }
    return Producer::Size(*it);
  }

 private:
  // 9 -- current max skip list levels
  // 1 -- for whole skip list level
  irs::SmallVector<EntryType, 9 + 1> levels_;
  IRS_NO_UNIQUE_ADDRESS Producer producer_;
};

template<bool NeedScore>
struct WandScorer {
  explicit WandScorer(const Scorer& /*scorer*/) noexcept {}

  constexpr bool Prepare(const ColumnProvider& /*reader*/,
                         const feature_map_t& /*features*/,
                         const attribute_provider& /*attrs*/) noexcept {
    return true;
  }
};

template<>
struct WandScorer<true> {
  explicit WandScorer(const Scorer& scorer) : scorer_{scorer} {
    // TODO(MBkkt) alignment could be incorrect here
    stats_.resize(scorer.stats_size().first);
    scorer.collect(stats_.data(), nullptr, nullptr);
  }

  bool Prepare(const ColumnProvider& reader, const feature_map_t& features,
               const attribute_provider& attrs) {
    func_ =
      scorer_.prepare_scorer(reader, features, stats_.data(), attrs, kNoBoost);
    return !func_.IsDefault();
  }

  score_t GetScore() const noexcept {
    score_t score{};
    func_(&score);
    return score;
  }

 private:
  const Scorer& scorer_;
  ScoreFunction func_;
  irs::SmallVector<byte_type, 16> stats_;
};

enum WandTag : uint32_t {
  // What will be written?
  kWandTagFreq = 0U,
  kWandTagNorm = 1U << 0U,
  // How to Produce best Entry?
  // Produce max freq
  kWandTagMaxFreq = 1U << 1U,
  // Produce max freq, min norm, but norm >= freq
  kWandTagMinNorm = 1U << 2U,
  // Produce max freq/norm
  kWandTagDivNorm = 1U << 3U,
  // Produce max score
  kWandTagMaxScore = 1U << 4U,
};

template<uint32_t Tag>
class FreqNormProducer {
  static constexpr bool kMaxScore = (Tag & kWandTagMaxScore) != 0;
  static constexpr bool kDivNorm = (Tag & kWandTagDivNorm) != 0;
  static constexpr bool kMinNorm = (Tag & kWandTagMinNorm) != 0;
  static constexpr bool kMaxFreq = kMinNorm || (Tag & kWandTagMaxFreq) != 0;

  static constexpr bool kNorm =
    kDivNorm || kMinNorm || (Tag & kWandTagNorm) != 0;

 public:
  struct Entry {
    IRS_NO_UNIQUE_ADDRESS utils::Need<kMaxScore, score_t> score{0.f};
    uint32_t freq{1};
    IRS_NO_UNIQUE_ADDRESS utils::Need<kNorm, uint32_t> norm{
      std::numeric_limits<uint32_t>::max()};
  };

  static void Produce(const Entry& from, Entry& to) noexcept {
    if constexpr (kMaxFreq) {
      if (from.freq > to.freq) {
        to.freq = from.freq;
      }
      if constexpr (kMinNorm) {
        if (from.norm < to.norm) {
          to.norm = from.norm;
        }
        if (to.norm < to.freq) {
          to.norm = to.freq;
        }
      }
    } else if constexpr (kDivNorm) {
      if (static_cast<uint64_t>(from.freq) * to.norm >
          static_cast<uint64_t>(to.freq) * from.norm) {
        to.freq = from.freq;
        to.norm = from.norm;
      }
    } else if constexpr (kMaxScore) {
      if (from.score > to.score) {
        to.score = from.score;
        to.freq = from.freq;
        to.norm = from.norm;
      }
    }
  }

  template<typename Output>
  static void Write(Entry entry, Output& out) {
    // TODO(MBkkt) Compute difference second time looks unnecessary.
    IRS_ASSERT(entry.freq >= 1);
    out.write_vint(entry.freq);
    if constexpr (kNorm) {
      IRS_ASSERT(entry.norm >= entry.freq);
      if (entry.norm != entry.freq) {
        out.write_vint(entry.norm - entry.freq);
      }
    }
  }

  static uint8_t Size(Entry entry) noexcept {
    IRS_ASSERT(entry.freq >= 1);
    size_t size = bytes_io<uint32_t>::vsize(entry.freq);
    if constexpr (kNorm) {
      IRS_ASSERT(entry.norm >= entry.freq);
      if (entry.norm != entry.freq) {
        size += bytes_io<uint32_t>::vsize(entry.norm - entry.freq);
      }
    }
    return size;
  }

  explicit FreqNormProducer(const Scorer& scorer) : scorer_{scorer} {}

  bool Prepare(const ColumnProvider& reader, const feature_map_t& features,
               const attribute_provider& attrs) {
    freq_ = irs::get<frequency>(attrs);

    if (IRS_UNLIKELY(freq_ == nullptr)) {
      return false;
    }

    if constexpr (kNorm) {
      const auto* doc = irs::get<irs::document>(attrs);

      if (IRS_UNLIKELY(doc == nullptr)) {
        return false;
      }

      const auto it = features.find(irs::type<Norm2>::id());

      if (IRS_UNLIKELY(it == features.end() ||
                       !field_limits::valid(it->second))) {
        return false;
      }

      Norm2::Context ctx;
      if (IRS_UNLIKELY(!ctx.Reset(reader, it->second, *doc))) {
        return false;
      }

      norm_ = Norm2::MakeReader(std::move(ctx), [&](auto&& reader) {
        return fu2::unique_function<uint32_t()>{std::move(reader)};
      });
    }

    return scorer_.Prepare(reader, features, attrs);
  }

  void Produce(Entry& to) noexcept {
    if constexpr (kMaxFreq) {
      const auto freq = freq_->value;
      if (freq > to.freq) {
        to.freq = freq;
      }
      if constexpr (kMinNorm) {
        const auto norm = norm_();
        if (norm < to.norm) {
          to.norm = norm;
        }
        if (to.norm < to.freq) {
          to.norm = to.freq;
        }
      }
    } else if constexpr (kDivNorm) {
      const auto freq = freq_->value;
      const auto norm = norm_();
      if (static_cast<uint64_t>(freq) * to.norm >
          static_cast<uint64_t>(to.freq) * norm) {
        to.freq = freq;
        to.norm = norm;
      }
    } else if constexpr (kMaxScore) {
      const auto score = scorer_.GetScore();
      if (score > to.score) {
        to.score = score;
        to.freq = freq_->value;
        if constexpr (kNorm) {
          to.norm = norm_();
        }
      }
    }
  }

 private:
  const irs::frequency* freq_{};
  IRS_NO_UNIQUE_ADDRESS utils::Need<kNorm, fu2::unique_function<uint32_t()>>
    norm_{};
  IRS_NO_UNIQUE_ADDRESS WandScorer<kMaxScore> scorer_;
};

template<uint32_t Tag>
using FreqNormWriter = WandWriterImpl<FreqNormProducer<Tag>>;

template<uint32_t Tag>
class FreqNormSource final : public WandSource {
  static constexpr bool kNorm = (Tag & kWandTagNorm) != 0;

 public:
  attribute* get_mutable(type_info::type_id type) final {
    if (irs::type<frequency>::id() == type) {
      return &freq_;
    }
    if constexpr (kNorm) {
      if (irs::type<Norm2>::id() == type) {
        return &norm_;
      }
    }
    return nullptr;
  }

  void Read(data_input& in, size_t size) final {
    freq_.value = in.read_vint();
    // TODO(MBkkt) don't compute vsize here
    const auto read = bytes_io<uint32_t>::vsize(freq_.value);
    // We need to always try to read norm, because we have compatibility
    // between BM25 in the index and TFIDF in the query
    [[maybe_unused]] auto norm = freq_.value;
    IRS_ASSERT(read <= size);
    if (read != size) {
      // TODO(MBkkt) if (!kNorm) in.skip(read - size);
      norm += in.read_vint();
    }
    if constexpr (kNorm) {
      norm_.value = norm;
    }
  }

 private:
  frequency freq_;
  IRS_NO_UNIQUE_ADDRESS utils::Need<kNorm, Norm2> norm_;
};

}  // namespace irs
