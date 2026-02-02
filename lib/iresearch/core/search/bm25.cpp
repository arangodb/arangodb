////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "bm25.hpp"

#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/vpack.h>

#include <cstdint>

#include "analysis/token_attributes.hpp"
#include "formats/wand_writer.hpp"
#include "index/field_meta.hpp"
#include "index/index_reader.hpp"
#include "index/norm.hpp"
#include "scorer.hpp"
#include "search/scorer_impl.hpp"
#include "utils/math_utils.hpp"
#include "utils/misc.hpp"

namespace irs {
namespace {

struct BM25FieldCollector final : FieldCollector {
  // number of documents containing the matched field
  // (possibly without matching terms)
  uint64_t docs_with_field = 0;
  // number of terms for processed field
  uint64_t total_term_freq = 0;

  void collect(const SubReader& /*segment*/,
               const term_reader& field) noexcept final {
    docs_with_field += field.docs_count();
    if (auto* freq = get<frequency>(field); freq != nullptr) {
      total_term_freq += freq->value;
    }
  }

  void reset() noexcept final {
    docs_with_field = 0;
    total_term_freq = 0;
  }

  void collect(bytes_view in) final {
    ByteRefIterator itr{in};
    const auto docs_with_field_value = vread<uint64_t>(itr);
    const auto total_term_freq_value = vread<uint64_t>(itr);
    if (itr.pos_ != itr.end_) {
      throw io_error{"input not read fully"};
    }
    docs_with_field += docs_with_field_value;
    total_term_freq += total_term_freq_value;
  }

  void write(data_output& out) const final {
    out.write_vlong(docs_with_field);
    out.write_vlong(total_term_freq);
  }
};

// TODO(MBkkt) deduplicate with tfidf.cpp
const auto kSQRT = irs::cache_func<uint32_t, 2048>(
  0, [](uint32_t i) noexcept { return std::sqrt(static_cast<float_t>(i)); });

irs::Scorer::ptr make_from_object(const VPackSlice slice) {
  IRS_ASSERT(slice.isObject());

  float_t k{BM25::K()};
  float_t b{BM25::B()};

  auto get = [&](std::string_view key, float_t& coefficient) {
    auto v = slice.get(key);
    if (v.isNone()) {
      return true;
    }
    if (!v.isNumber<float_t>()) {
      IRS_LOG_ERROR(
        absl::StrCat("Non-float value in '", key,
                     "' while constructing bm25 scorer from VPack arguments"));
      return false;
    }
    coefficient = v.getNumber<float_t>();
    return true;
  };
  if (!get("k", k) || !get("b", b)) {
    return nullptr;
  }

  return std::make_unique<BM25>(k, b);
}

Scorer::ptr make_from_array(const VPackSlice slice) {
  IRS_ASSERT(slice.isArray());

  VPackArrayIterator array(slice);
  VPackValueLength size = array.size();
  if (size > 2) {
    // wrong number of arguments
    IRS_LOG_ERROR(
      "Wrong number of arguments while constructing bm25 scorer from VPack "
      "arguments (must be <= 2)");
    return nullptr;
  }

  // default args
  auto k = BM25::K();
  auto b = BM25::B();
  uint8_t i = 0;
  for (auto arg_slice : array) {
    if (!arg_slice.isNumber<decltype(k)>()) {
      IRS_LOG_ERROR(absl::StrCat("Non-float value at position '", i,
                                 "' while constructing bm25 scorer "
                                 "from VPack arguments"));
      return nullptr;
    }

    switch (i) {
      case 0:  // parse `k` coefficient
        k = static_cast<float_t>(arg_slice.getNumber<decltype(k)>());
        ++i;
        break;
      case 1:  // parse `b` coefficient
        b = static_cast<float_t>(arg_slice.getNumber<decltype(b)>());
        break;
    }
  }

  return std::make_unique<BM25>(k, b);
}

Scorer::ptr make_vpack(const VPackSlice slice) {
  switch (slice.type()) {
    case VPackValueType::Object:
      return make_from_object(slice);
    case VPackValueType::Array:
      return make_from_array(slice);
    default:  // wrong type
      IRS_LOG_ERROR(
        "Invalid VPack arguments passed while constructing bm25 scorer");
      return nullptr;
  }
}

Scorer::ptr make_vpack(std::string_view args) {
  if (irs::IsNull(args)) {
    // default args
    return std::make_unique<irs::BM25>();
  } else {
    VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
    return make_vpack(slice);
  }
}

Scorer::ptr make_json(std::string_view args) {
  if (irs::IsNull(args)) {
    // default args
    return std::make_unique<irs::BM25>();
  } else {
    try {
      auto vpack = VPackParser::fromJson(args.data(), args.size());
      return make_vpack(vpack->slice());
    } catch (const VPackException& ex) {
      IRS_LOG_ERROR(
        absl::StrCat("Caught error '", ex.what(),
                     "' while constructing VPack from JSON for bm25 scorer"));
    } catch (...) {
      IRS_LOG_ERROR(
        "Caught error while constructing VPack from JSON for bm25 scorer");
    }
    return nullptr;
  }
}

REGISTER_SCORER_JSON(irs::BM25, make_json);
REGISTER_SCORER_VPACK(irs::BM25, make_vpack);

struct BM1Context : public irs::score_ctx {
  BM1Context(float_t k, irs::score_t boost, const BM25Stats& stats,
             const irs::filter_boost* fb = nullptr) noexcept
    : filter_boost{fb}, num{boost * (k + 1) * stats.idf} {}

  const irs::filter_boost* filter_boost;
  float_t num;  // partially precomputed numerator : boost * (k + 1) * idf
};

struct BM15Context : public BM1Context {
  BM15Context(float_t k, irs::score_t boost, const BM25Stats& stats,
              const frequency* freq,
              const irs::filter_boost* fb = nullptr) noexcept
    : BM1Context{k, boost, stats, fb},
      freq{freq ? freq : &kEmptyFreq},
      norm_const{stats.norm_const} {
    IRS_ASSERT(this->freq);
  }

  const frequency* freq;  // document frequency
  float_t norm_const;     // 'k' factor
};

template<typename Norm>
struct BM25Context final : public BM15Context {
  BM25Context(float_t k, irs::score_t boost, const BM25Stats& stats,
              const frequency* freq, Norm&& norm,
              const irs::filter_boost* filter_boost = nullptr) noexcept
    : BM15Context{k, boost, stats, freq, filter_boost},
      norm{std::move(norm)},
      norm_length{stats.norm_length},
      norm_cache{stats.norm_cache} {}

  Norm norm;
  float_t norm_length;  // precomputed 'k*b/avg_dl'
  const float_t* norm_cache;
};

template<typename Reader, NormType Type>
struct BM25NormAdapter final {
  static constexpr auto kType = Type;

  explicit BM25NormAdapter(Reader&& reader) : reader{std::move(reader)} {}

  IRS_FORCE_INLINE decltype(auto) operator()() {
    if constexpr (kType < NormType::kNorm) {
      // norms are stored |doc| as uint32_t
      return reader();
    } else {
      // norms are stored 1/sqrt(|doc|) as float
      return 1.f / reader();
    }
  }

  IRS_NO_UNIQUE_ADDRESS Reader reader;
};

template<NormType Type, typename Reader>
auto MakeBM25NormAdapter(Reader&& reader) {
  return BM25NormAdapter<Reader, Type>(std::move(reader));
}

}  // namespace

template<>
struct MakeScoreFunctionImpl<BM1Context> {
  using Ctx = BM1Context;

  template<bool HasFilterBoost, typename... Args>
  static auto Make(Args&&... args) {
    if constexpr (HasFilterBoost) {
      return ScoreFunction::Make<Ctx>(
        [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
          IRS_ASSERT(res);
          IRS_ASSERT(ctx);

          auto& state = *static_cast<Ctx*>(ctx);

          IRS_ASSERT(state.filter_boost);
          *res = state.filter_boost->value * state.num;
        },
        ScoreFunction::DefaultMin, std::forward<Args>(args)...);
    } else {
      Ctx ctx{std::forward<Args>(args)...};
      return ScoreFunction::Constant(ctx.num);
    }
  }
};

template<>
struct MakeScoreFunctionImpl<BM15Context> {
  using Ctx = BM15Context;

  template<bool HasFilterBoost, typename... Args>
  static auto Make(Args&&... args) {
    return ScoreFunction::Make<Ctx>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        IRS_ASSERT(res);
        IRS_ASSERT(ctx);

        auto& state = *static_cast<Ctx*>(ctx);

        const float_t tf = static_cast<float_t>(state.freq->value);

        float_t c0;
        if constexpr (HasFilterBoost) {
          IRS_ASSERT(state.filter_boost);
          c0 = state.filter_boost->value * state.num;
        } else {
          c0 = state.num;
        }

        const float_t c1 = state.norm_const;
        IRS_ASSERT(c1 != 0.f);

        *res = c0 - c0 / (1.f + tf / c1);
      },
      ScoreFunction::DefaultMin, std::forward<Args>(args)...);
  }
};

template<typename Norm>
struct MakeScoreFunctionImpl<BM25Context<Norm>> {
  using Ctx = BM25Context<Norm>;

  template<bool HasFilterBoost, typename... Args>
  static auto Make(Args&&... args) {
    return ScoreFunction::Make<Ctx>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        IRS_ASSERT(res);
        IRS_ASSERT(ctx);

        auto& state = *static_cast<Ctx*>(ctx);

        float_t tf;
        if constexpr (Norm::kType < NormType::kNorm) {
          tf = static_cast<float_t>(state.freq->value);
        } else {
          tf = kSQRT.get<true>(state.freq->value);
        }

        // FIXME(gnusi): we don't need c0 for WAND evaluation
        float_t c0;
        if constexpr (HasFilterBoost) {
          IRS_ASSERT(state.filter_boost);
          c0 = state.filter_boost->value * state.num;
        } else {
          c0 = state.num;
        }

        if constexpr (NormType::kNorm2Tiny == Norm::kType) {
          static_assert(std::is_same_v<uint32_t, decltype(state.norm())>);
          IRS_ASSERT((state.norm() & 0xFFU) != 0U);
          const float_t inv_c1 = state.norm_cache[state.norm() & 0xFFU];

          *res = c0 - c0 / (1.f + tf * inv_c1);
        } else {
          const float_t c1 =
            state.norm_const +
            state.norm_length * static_cast<float_t>(state.norm());

          *res = c0 - c0 * c1 / (c1 + tf);
        }
      },
      ScoreFunction::DefaultMin, std::forward<Args>(args)...);
  }
};

void BM25::collect(byte_type* stats_buf, const irs::FieldCollector* field,
                   const irs::TermCollector* term) const {
  auto* stats = stats_cast(stats_buf);

  const auto* field_ptr = DownCast<BM25FieldCollector>(field);
  const auto* term_ptr = DownCast<TermCollectorImpl>(term);

  // nullptr possible if e.g. 'all' filter
  const auto docs_with_field = field_ptr ? field_ptr->docs_with_field : 0;
  // nullptr possible if e.g.'by_column_existence' filter
  const auto docs_with_term = term_ptr ? term_ptr->docs_with_term : 0;
  // nullptr possible if e.g. 'all' filter
  const auto total_term_freq = field_ptr ? field_ptr->total_term_freq : 0;

  // precomputed idf value
  stats->idf += float_t(
    std::log1p((static_cast<double>(docs_with_field - docs_with_term) + 0.5) /
               (static_cast<double>(docs_with_term) + 0.5)));
  IRS_ASSERT(stats->idf >= 0.f);

  // - stats were already initialized
  if (!NeedsNorm()) {
    stats->norm_const = k_;
    return;
  }

  // precomputed length norm
  const float_t kb = k_ * b_;

  stats->norm_const = k_ - kb;
  if (total_term_freq && docs_with_field) {
    const auto avg_dl = static_cast<float_t>(total_term_freq) /
                        static_cast<float_t>(docs_with_field);
    stats->norm_length = kb / avg_dl;
  } else {
    stats->norm_length = kb;
  }

  auto it = std::begin(stats->norm_cache);
  *it++ = 0.f;
  const auto end = std::end(stats->norm_cache);
  for (float_t i = 1.f; it != end;) {
    *it++ = 1.f / (stats->norm_const + stats->norm_length * i++);
  }
}

FieldCollector::ptr BM25::prepare_field_collector() const {
  return std::make_unique<BM25FieldCollector>();
}

ScoreFunction BM25::prepare_scorer(const ColumnProvider& segment,
                                   const irs::feature_map_t& features,
                                   const byte_type* query_stats,
                                   const attribute_provider& doc_attrs,
                                   score_t boost) const {
  auto* freq = irs::get<frequency>(doc_attrs);

  if (!freq) {
    if (!boost_as_score_ || 0.f == boost) {
      return ScoreFunction::Default(1);
    }

    // if there is no frequency then all the scores
    // will be the same (e.g. filter irs::all)
    return ScoreFunction::Constant(boost);
  }

  auto* stats = stats_cast(query_stats);
  auto* filter_boost = irs::get<irs::filter_boost>(doc_attrs);

  if (IsBM1()) {
    return MakeScoreFunction<BM1Context>(filter_boost, k_, boost, *stats);
  }

  if (IsBM15()) {
    return MakeScoreFunction<BM15Context>(filter_boost, k_, boost, *stats,
                                          freq);
  }

  auto prepare_norm_scorer = [&]<typename Norm>(Norm&& norm) -> ScoreFunction {
    return MakeScoreFunction<BM25Context<Norm>>(filter_boost, k_, boost, *stats,
                                                freq, std::move(norm));
  };

  // Check if norms are present in attributes
  if (auto* norm = irs::get<Norm2>(doc_attrs); norm) {
    return prepare_norm_scorer(MakeBM25NormAdapter<NormType::kNorm2>(
      [norm]() noexcept { return norm->value; }));
  }

  // Fallback to reading from columnstore
  auto* doc = irs::get<document>(doc_attrs);

  if (IRS_UNLIKELY(!doc)) {
    // We need 'document' attribute to be exposed.
    return ScoreFunction::Default(1);
  }

  if (auto it = features.find(irs::type<Norm2>::id()); it != features.end()) {
    if (Norm2ReaderContext ctx; ctx.Reset(segment, it->second, *doc)) {
      if (ctx.max_num_bytes == sizeof(byte_type)) {
        return Norm2::MakeReader(std::move(ctx), [&](auto&& reader) {
          return prepare_norm_scorer(
            MakeBM25NormAdapter<NormType::kNorm2Tiny>(std::move(reader)));
        });
      }

      return Norm2::MakeReader(std::move(ctx), [&](auto&& reader) {
        return prepare_norm_scorer(
          MakeBM25NormAdapter<NormType::kNorm2>(std::move(reader)));
      });
    }
  }

  if (auto it = features.find(irs::type<Norm>::id()); it != features.end()) {
    if (NormReaderContext ctx; ctx.Reset(segment, it->second, *doc)) {
      return prepare_norm_scorer(
        MakeBM25NormAdapter<NormType::kNorm>(Norm::MakeReader(std::move(ctx))));
    }
  }

  // No norms, pretend all fields have the same length 1.
  return prepare_norm_scorer(
    MakeBM25NormAdapter<NormType::kNorm2Tiny>([]() { return 1U; }));
}

void BM25::get_features(feature_set_t& features) const {
  if (NeedsNorm()) {
    features.emplace(irs::type<Norm2>::id());
  }
}

WandWriter::ptr BM25::prepare_wand_writer(size_t max_levels) const {
  if (IsBM1()) {
    return {};
  }
  if (IsBM15()) {
    return std::make_unique<FreqNormWriter<kWandTagMaxFreq>>(*this, max_levels);
  }
  if (IsBM11()) {
    // idf * (k + 1) * tf / (k * (1 - b + b * dl / avg_dl) + tf)
    // idf * (k + 1) -- doesn't affect compare
    // tf / (k * (1 - b + b * dl / avg_dl) + tf)
    // replacement tf = x * dl
    // x * dl / (k * (1 - b + b * dl / avg_dl) + x * dl)
    // divide by dl
    // x / (k * ((1 - b) / dl + b / avg_dl) + x)
    // b == 1
    // x / (k / avg_dl + x)
    return std::make_unique<FreqNormWriter<kWandTagDivNorm>>(*this, max_levels);
  }
  // Approximation that suited for any BM25
  return std::make_unique<FreqNormWriter<kWandTagMinNorm>>(*this, max_levels);
}

WandSource::ptr BM25::prepare_wand_source() const {
  if (IsBM1()) {
    return {};
  }
  if (IsBM15()) {
    return std::make_unique<FreqNormSource<kWandTagFreq>>();
  }
  return std::make_unique<FreqNormSource<kWandTagNorm>>();
}

TermCollector::ptr BM25::prepare_term_collector() const {
  return std::make_unique<TermCollectorImpl>();
}

Scorer::WandType BM25::wand_type() const noexcept {
  if (IsBM1()) {
    return WandType::kNone;
  }
  if (IsBM15()) {
    return WandType::kMaxFreq;
  }
  if (IsBM11()) {
    return WandType::kDivNorm;
  }
  return WandType::kMinNorm;
}

bool BM25::equals(const Scorer& other) const noexcept {
  if (!Scorer::equals(other)) {
    return false;
  }
  const auto& p = DownCast<BM25>(other);
  return p.k_ == k_ && p.b_ == b_;
}

void BM25::init() {
  REGISTER_SCORER_JSON(BM25, make_json);    // match registration above
  REGISTER_SCORER_VPACK(BM25, make_vpack);  // match registration above
}

}  // namespace irs
