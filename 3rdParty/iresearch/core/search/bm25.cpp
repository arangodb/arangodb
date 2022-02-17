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

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/vpack.h"
#include "velocypack/velocypack-aliases.h"

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/norm.hpp"
#include "index/field_meta.hpp"
#include "utils/math_utils.hpp"

namespace {

const auto kSQRT = irs::cache_func<uint32_t, 2048>(
  0, [](uint32_t i) noexcept { return std::sqrt(static_cast<float_t>(i)); });

irs::sort::ptr make_from_object(const VPackSlice slice) {
  assert(slice.isObject());

  auto ptr = irs::memory::make_unique<irs::bm25_sort>();

  #ifdef IRESEARCH_DEBUG
    auto& scorer = dynamic_cast<irs::bm25_sort&>(*ptr);
  #else
    auto& scorer = static_cast<irs::bm25_sort&>(*ptr);
  #endif

  {
    // optional float
    const auto* key = "b";

    if (slice.hasKey(key)) {
      if (!slice.get(key).isNumber()) {
        IR_FRMT_ERROR("Non-float value in '%s' while constructing bm25 scorer from VPack arguments", key);

        return nullptr;
      }

      scorer.b(slice.get(key).getNumber<float>());
    }
  }

  {
    // optional float
    const auto* key = "k";

    if (slice.hasKey(key)) {
      if (!slice.get(key).isNumber()) {
        IR_FRMT_ERROR("Non-float value in '%s' while constructing bm25 scorer from VPack arguments", key);

        return nullptr;
      }

      scorer.k(slice.get(key).getNumber<float>());
    }
  }

  return ptr;
}

irs::sort::ptr make_from_array(const VPackSlice slice) {
  assert(slice.isArray());

  VPackArrayIterator array(slice);
  VPackValueLength size = array.size();
  if (size > 2) {
    // wrong number of arguments
    IR_FRMT_ERROR(
      "Wrong number of arguments while constructing bm25 scorer from VPack arguments (must be <= 2)");
    return nullptr;
  }

  // default args
  auto k = irs::bm25_sort::K();
  auto b = irs::bm25_sort::B();
  int i = 0;
  for (auto arg_slice : array) {

    switch (i) {
     case 0: // parse `k` coefficient
      if (!arg_slice.isNumber<decltype (k)>()) {
        IR_FRMT_ERROR(
          "Non-float value at position '%u' while constructing bm25 scorer from VPack arguments", i);
        return nullptr;
      }

      k = static_cast<float_t>(arg_slice.getNumber<decltype (k)>());
      ++i;
      break;
     case 1: // parse `b` coefficient
      if (!arg_slice.isNumber<decltype (b)>()) {
        IR_FRMT_ERROR(
          "Non-float value at position '%u' while constructing bm25 scorer from VPack arguments", i);
        return nullptr;
      }

      b = static_cast<float_t>(arg_slice.getNumber<decltype (b)>());
      break;
    }
  }

  return irs::memory::make_unique<irs::bm25_sort>(k, b);
}

irs::sort::ptr make_vpack(const VPackSlice slice) {

  switch (slice.type()) {
    case VPackValueType::Object:
      return make_from_object(slice);
    case VPackValueType::Array:
      return make_from_array(slice);
    default: // wrong type
      IR_FRMT_ERROR(
        "Invalid VPack arguments passed while constructing bm25 scorer");
      return nullptr;
  }
}

irs::sort::ptr make_vpack(irs::string_ref args) {
  if (args.null()) {
    // default args
    return irs::memory::make_unique<irs::bm25_sort>();
  } else {
    VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
    return make_vpack(slice);
  }
}

irs::sort::ptr make_json(irs::string_ref args) {
  if (args.null()) {
    // default args
    return irs::memory::make_unique<irs::bm25_sort>();
  } else {
    try {
      auto vpack = VPackParser::fromJson(args.c_str(), args.size());
      return make_vpack(vpack->slice());
    } catch(const VPackException& ex) {
        IR_FRMT_ERROR(
          "Caught error '%s' while constructing VPack from JSON for bm25 scorer",
          ex.what());
    } catch(...) {
        IR_FRMT_ERROR(
          "Caught error while constructing VPack from JSON for bm25 scorer");
    }
    return nullptr;
  }
}

REGISTER_SCORER_JSON(irs::bm25_sort, make_json);
REGISTER_SCORER_VPACK(irs::bm25_sort, make_vpack);

struct byte_ref_iterator {
  using iterator_category = std::input_iterator_tag;
  using value_type = irs::byte_type;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = void;

  const irs::byte_type* end_;
  const irs::byte_type* pos_;

  explicit byte_ref_iterator(irs::bytes_ref in)
    : end_(in.c_str() + in.size()), pos_(in.c_str()) {
  }

  irs::byte_type operator*() {
    if (pos_ >= end_) {
      throw irs::io_error("invalid read past end of input");
    }

    return *pos_;

  }

  void operator++() { ++pos_; }
};

struct field_collector final: public irs::sort::field_collector {
  uint64_t docs_with_field = 0; // number of documents containing the matched field (possibly without matching terms)
  uint64_t total_term_freq = 0; // number of terms for processed field

  virtual void collect(
      const irs::sub_reader& /*segment*/,
      const irs::term_reader& field) override {
    docs_with_field += field.docs_count();

    auto* freq = irs::get<irs::frequency>(field);

    if (freq) {
      total_term_freq += freq->value;
    }
  }

  virtual void reset() noexcept override {
    docs_with_field = 0;
    total_term_freq = 0;
  }

  virtual void collect(irs::bytes_ref in) override {
    byte_ref_iterator itr(in);
    auto docs_with_field_value = irs::vread<uint64_t>(itr);
    auto total_term_freq_value = irs::vread<uint64_t>(itr);

    if (itr.pos_ != itr.end_) {
      throw irs::io_error("input not read fully");
    }

    docs_with_field += docs_with_field_value;
    total_term_freq += total_term_freq_value;
  }

  virtual void write(irs::data_output& out) const override {
    out.write_vlong(docs_with_field);
    out.write_vlong(total_term_freq);
  }
};

struct term_collector final: public irs::sort::term_collector {
  uint64_t docs_with_term = 0; // number of documents containing the matched term

  virtual void collect(
      const irs::sub_reader& /*segment*/,
      const irs::term_reader& /*field*/,
      const irs::attribute_provider& term_attrs) override {
    auto* meta = irs::get<irs::term_meta>(term_attrs);

    if (meta) {
      docs_with_term += meta->docs_count;
    }
  }

  virtual void reset() noexcept override {
    docs_with_term = 0;
  }

  virtual void collect(irs::bytes_ref in) override {
    byte_ref_iterator itr(in);
    auto docs_with_term_value = irs::vread<uint64_t>(itr);

    if (itr.pos_ != itr.end_) {
      throw irs::io_error("input not read fully");
    }

    docs_with_term += docs_with_term_value;
  }

  virtual void write(irs::data_output& out) const override {
    out.write_vlong(docs_with_term);
  }
};

} // LOCAL

namespace iresearch {

// BM25 similarity
// bm25(doc, term) = idf(term) * ((k + 1) * tf(doc, term)) / (k * (1 - b + b * |doc|/avgDL) + tf(doc, term))
//
// Inverted document frequency
// idf(term) = log(1 + (#documents with this field - #documents with this term + 0.5)/(#documents with this term + 0.5))
//
// Term frequency
//   Norm2: tf(doc, term) = frequency(doc, term);
//   Norm:  tf(doc, term) = sqrt(frequency(doc, term));
//
// Document length
//   Norm2: |doc| # of terms in a field within a document
//   Norm:  |doc| = 1 / sqrt(# of terms in a field within a document)
//
// Average document length
// avgDL = sum(field_term_count) / (#documents with this field)

namespace bm25 {

// Empty frequency
const frequency kEmptyFreq;

struct stats final {
  // precomputed idf value
  float_t idf;
  // precomputed k*(1-b)
  float_t norm_const;
  // precomputed k*b/avgD
  float_t norm_length;
  // precomputed 1/(k*(1-b+b*|doc|/avgDL)) for |doc| E [0..255]
  float_t norm_cache[256];
}; // stats

using score_t = bm25_sort::score_t;

struct BM15Context : public irs::score_ctx {
  BM15Context(
      byte_type* score_buf,
      float_t k,
      irs::boost_t boost,
      const bm25::stats& stats,
      const frequency* freq,
      const filter_boost* fb = nullptr) noexcept
    : score_buf{score_buf},
      freq{freq ? freq : &kEmptyFreq},
      filter_boost{fb},
      num{boost * (k + 1) * stats.idf},
      norm_const{k}  {
    assert(this->freq);
    assert(this->score_buf);
  }

  byte_type* score_buf;
  const frequency* freq; // document frequency
  const irs::filter_boost* filter_boost;
  float_t num; // partially precomputed numerator : boost * (k + 1) * idf
  float_t norm_const; // 'k' factor
};

template<typename Norm>
struct BM25Context final : public BM15Context {
  BM25Context(
      byte_type* score_buf,
      float_t k,
      irs::boost_t boost,
      const bm25::stats& stats,
      const frequency* freq,
      Norm&& norm,
      const irs::filter_boost* filter_boost = nullptr) noexcept
    : BM15Context{score_buf, k, boost, stats, freq, filter_boost},
      norm{std::move(norm)},
      norm_length{stats.norm_length},
      norm_cache{stats.norm_cache} {
    assert(stats.norm_const);
    norm_const = stats.norm_const;
  }

  Norm norm;
  float_t norm_length; // precomputed 'k*b/avgD'
  const float_t* norm_cache;
};

enum class NormType {
  // Norm2 values
  kNorm2 = 0,
  // Norm2 values fit 1-byte
  kNorm2Tiny,
  // Old norms, 1/sqrt(|doc|)
  kNorm
};

template<typename Reader, NormType Type>
struct NormAdapter : Reader {
  static constexpr auto kType = Type;

  explicit NormAdapter(Reader&& reader)
    : Reader{std::move(reader)} {
  }

  FORCE_INLINE auto operator()() -> std::invoke_result_t<Reader> {
    if constexpr (kType < NormType::kNorm) {
      // norms are stored |doc| as uint32_t
      return Reader::operator()();
    } else {
      // norms are stored 1/sqrt(|doc|) as float
      return 1.f/Reader::operator()();
    }
   }
};

template<NormType Type, typename Reader>
auto MakeNormAdapter(Reader&& reader) {
  return NormAdapter<Reader, Type>(std::move(reader));
}

template<typename Ctx>
struct MakeScoreFunctionImpl{
  template<bool HasFilterBoost, typename... Args>
  static score_function Make(Args&&... args);
};

template<>
struct MakeScoreFunctionImpl<BM15Context> {
  using Ctx = BM15Context;

  template<bool HasFilterBoost, typename... Args>
  static score_function Make(Args&&... args) {
    return {
        memory::make_unique<Ctx>(std::forward<Args>(args)...),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          auto& state = *static_cast<Ctx*>(ctx);

          const float_t tf = static_cast<float_t>(state.freq->value);

          float_t c0;
          if constexpr (HasFilterBoost) {
            assert(state.filter_boost);
            c0 = state.filter_boost->value * state.num;
          } else {
            c0 = state.num;
          }

          const float_t c1 = state.norm_const;

          sort::score_cast<score_t>(state.score_buf) = c0 - c0 / (1.f + tf / c1);

          return state.score_buf;
        }
    };
  }
};

template<typename Norm>
struct MakeScoreFunctionImpl<BM25Context<Norm>> {
  using Ctx = BM25Context<Norm>;

  template<bool HasFilterBoost, typename... Args>
  static score_function Make(Args&&... args) {
    return {
        memory::make_unique<Ctx>(std::forward<Args>(args)...),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          auto& state = *static_cast<Ctx*>(ctx);

          float_t tf;
          if constexpr (Norm::kType < NormType::kNorm) {
            tf = static_cast<float_t>(state.freq->value);
          } else {
            tf = ::kSQRT.get<true>(state.freq->value);
          }

          float_t c0;
          if constexpr (HasFilterBoost) {
            assert(state.filter_boost);
            c0 = state.filter_boost->value * state.num;
          } else {
            c0 = state.num;
          }

          auto& buf = sort::score_cast<score_t>(state.score_buf);

          if constexpr (NormType::kNorm2Tiny == Norm::kType) {
            static_assert(std::is_same_v<uint32_t, decltype(state.norm())>);
            const float_t inv_c1 = state.norm_cache[state.norm() & uint32_t{0xFF}];

            buf = c0 - c0 / (1.f + tf * inv_c1);
          } else {
            const float_t c1 = state.norm_const + state.norm_length * state.norm();

            buf = c0 - c0 * c1 / (c1 + tf);
          }

          return state.score_buf;
        }
    };
  }
};

template<typename Ctx, typename... Args>
score_function MakeScoreFunction(const filter_boost* filter_boost,
                                 Args&&... args) noexcept {
  if (filter_boost) {
    return MakeScoreFunctionImpl<Ctx>::template Make<true>(
        std::forward<Args>(args)..., filter_boost);
  }

  return MakeScoreFunctionImpl<Ctx>::template Make<false>(
      std::forward<Args>(args)...);
}

class sort final : public irs::prepared_sort_basic<bm25::score_t, bm25::stats> {
 public:
  sort(float_t k, float_t b, bool boost_as_score) noexcept
    : k_{k}, b_{b}, boost_as_score_{boost_as_score} {
  }

  virtual void collect(
      byte_type* stats_buf,
      const irs::index_reader& /*index*/,
      const irs::sort::field_collector* field,
      const irs::sort::term_collector* term) const override {
    auto& stats = stats_cast(stats_buf);

#ifdef IRESEARCH_DEBUG
    auto* field_ptr = dynamic_cast<const field_collector*>(field);
    assert(!field || field_ptr);
    auto* term_ptr = dynamic_cast<const term_collector*>(term);
    assert(!term || term_ptr);
#else
    auto* field_ptr = static_cast<const field_collector*>(field);
    auto* term_ptr = static_cast<const term_collector*>(term);
#endif

    const auto docs_with_field = field_ptr ? field_ptr->docs_with_field : 0; // nullptr possible if e.g. 'all' filter
    const auto docs_with_term = term_ptr ? term_ptr->docs_with_term : 0; // nullptr possible if e.g.'by_column_existence' filter
    const auto total_term_freq = field_ptr ? field_ptr->total_term_freq : 0; // nullptr possible if e.g. 'all' filter

    // precomputed idf value
    stats.idf += float_t(std::log1p(
      (docs_with_field - docs_with_term + 0.5)/(docs_with_term + 0.5)));
    assert(stats.idf >= 0.f);

    // - stats were already initialized
    // - BM15 without norms
    if (b_ == 0.f) {
      stats.norm_const = 1.f;
      return;
    }

    // precomputed length norm
    const float_t kb = k_ * b_;

    stats.norm_const = k_ - kb;
    if (total_term_freq && docs_with_field) {
      const float_t avg_dl = float_t(total_term_freq) / docs_with_field;
      stats.norm_length = kb / avg_dl;
    } else {
      stats.norm_length = kb;
    }

    for (uint32_t i = 0; auto& norm : stats.norm_cache) {
      norm = 1.f/(stats.norm_const + stats.norm_length * i++);
    }
  }

  virtual IndexFeatures features() const noexcept override {
    return IndexFeatures::FREQ;
  }

  virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
    return irs::memory::make_unique<field_collector>();
  }

  virtual score_function prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const byte_type* query_stats,
      byte_type* score_buf,
      const attribute_provider& doc_attrs,
      boost_t boost) const override {
    auto* freq = irs::get<frequency>(doc_attrs);

    if (!freq) {
      if (!boost_as_score_ || 0.f == boost) {
        return { nullptr, nullptr };
      }

      // if there is no frequency then all the scores will be the same (e.g. filter irs::all)
      irs::sort::score_cast<score_t>(score_buf) = boost;

      return {
        reinterpret_cast<BM15Context*>(score_buf),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          return reinterpret_cast<byte_type*>(ctx);
        }
      };
    }

    auto& stats = stats_cast(query_stats);
    auto* filter_boost = irs::get<irs::filter_boost>(doc_attrs);

    if (b_ != 0.f) {
      auto* doc = irs::get<document>(doc_attrs);

      if (!doc) {
        // We need 'document' attribute to be exposed.
        return { nullptr, nullptr };
      }

      auto prepare_norm_scorer = [&]<typename Norm>(Norm&& norm) -> score_function {
        return MakeScoreFunction<BM25Context<Norm>>(
            filter_boost, score_buf, k_, boost, stats, freq, std::move(norm));
      };

      const auto& features = field.meta().features;

      if (auto it = features.find(irs::type<Norm2>::id()); it != features.end()) {
        if (Norm2ReaderContext ctx; ctx.Reset(segment, it->second, *doc)) {
          if (ctx.max_num_bytes == sizeof(byte_type)) {
            return Norm2::MakeReader(std::move(ctx), [&](auto&& reader) {
                return prepare_norm_scorer(MakeNormAdapter<NormType::kNorm2Tiny>(std::move(reader))); });
          }

          return Norm2::MakeReader(std::move(ctx), [&](auto&& reader) {
              return prepare_norm_scorer(MakeNormAdapter<NormType::kNorm2>(std::move(reader))); });
        }
      }

      if (auto it = features.find(irs::type<Norm>::id()); it != features.end()) {
        if (NormReaderContext ctx; ctx.Reset(segment, it->second, *doc)) {
          return prepare_norm_scorer(MakeNormAdapter<NormType::kNorm>(Norm::MakeReader(std::move(ctx))));
        }
      }

      // No norms, pretend all fields have the same length 1.
      return prepare_norm_scorer(MakeNormAdapter<NormType::kNorm2Tiny>([](){ return 1U; }));
    }

    // BM15
    return MakeScoreFunction<BM15Context>(
        filter_boost, score_buf, k_, boost, stats, freq);
  }

  virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
    return irs::memory::make_unique<term_collector>();
  }

 private:
  float_t k_;
  float_t b_;
  bool boost_as_score_;
}; // sort

} // bm25

/*static*/ sort::ptr bm25_sort::make(
    float_t k, float_t b, bool boost_as_score) {
  return std::make_unique<bm25_sort>(k, b, boost_as_score);
}

bm25_sort::bm25_sort(
    float_t k /*= 1.2f*/,
    float_t b /*= 0.75f*/,
    bool boost_as_score /*= false*/) noexcept
  : sort{irs::type<bm25_sort>::get()},
    k_{k},
    b_{b},
    boost_as_score_{boost_as_score} {
}

/*static*/ void bm25_sort::init() {
  REGISTER_SCORER_JSON(bm25_sort, make_json); // match registration above
  REGISTER_SCORER_VPACK(bm25_sort, make_vpack); // match registration above
}

sort::prepared::ptr bm25_sort::prepare() const {
  return memory::make_unique<bm25::sort>(k_, b_, boost_as_score_);
}

} // ROOT
