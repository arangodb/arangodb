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

#include "tfidf.hpp"

#include <cmath>
#include <string_view>

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/vpack.h"
#include "velocypack/velocypack-aliases.h"

#include "scorers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/norm.hpp"
#include "index/field_meta.hpp"
#include "utils/math_utils.hpp"
#include "utils/misc.hpp"

namespace {

const auto kSQRT = irs::cache_func<uint32_t, 2048>(
  0, [](uint32_t i) noexcept { return std::sqrt(static_cast<float_t>(i)); });

const auto kRSQRT = irs::cache_func<uint32_t, 2048>(
  1, [](uint32_t i) noexcept { return 1.f/std::sqrt(static_cast<float_t>(i)); });

irs::sort::ptr make_from_bool(const VPackSlice slice) {
  assert(slice.isBool());

  return irs::memory::make_unique<irs::tfidf_sort>(slice.getBool());
}

constexpr std::string_view WITH_NORMS_PARAM_NAME("withNorms");

irs::sort::ptr make_from_object(const VPackSlice slice) {
  assert(slice.isObject());

  auto ptr = irs::memory::make_unique<irs::tfidf_sort>();

  #ifdef IRESEARCH_DEBUG
    auto& scorer = dynamic_cast<irs::tfidf_sort&>(*ptr);
  #else
    auto& scorer = static_cast<irs::tfidf_sort&>(*ptr);
  #endif

  {
    // optional bool
    if (slice.hasKey(WITH_NORMS_PARAM_NAME)) {
      if (!slice.get(WITH_NORMS_PARAM_NAME).isBool()) {
        IR_FRMT_ERROR("Non-boolean value in '%s' while constructing tfidf scorer from VPack arguments",
          WITH_NORMS_PARAM_NAME.data());

        return nullptr;
      }

      scorer.normalize(slice.get(WITH_NORMS_PARAM_NAME).getBool());
    }
  }

  return ptr;
}

irs::sort::ptr make_from_array(const VPackSlice slice) {
  assert(slice.isArray());

  VPackArrayIterator array = VPackArrayIterator(slice);
  VPackValueLength size = array.size();

  if (size > 1) {
    // wrong number of arguments
    IR_FRMT_ERROR(
      "Wrong number of arguments while constructing tfidf scorer from VPack arguments (must be <= 1)");
    return nullptr;
  }

  // default args
  auto norms = irs::tfidf_sort::WITH_NORMS();

  // parse `withNorms` optional argument
  for (auto arg_slice : array) {
    if (!arg_slice.isBool()) {
      IR_FRMT_ERROR(
        "Non-bool value on position `0` while constructing tfidf scorer from VPack arguments");
      return nullptr;
    }

    norms = arg_slice.getBool();
  }

  return irs::memory::make_unique<irs::tfidf_sort>(norms);
}

irs::sort::ptr make_vpack(const VPackSlice slice) {
  switch (slice.type()) {
    case VPackValueType::Bool:
      return make_from_bool(slice);
    case VPackValueType::Object:
      return make_from_object(slice);
    case VPackValueType::Array:
      return make_from_array(slice);
    default: // wrong type
      IR_FRMT_ERROR(
        "Invalid VPack arguments passed while constructing tfidf scorer, arguments");
      return nullptr;
  }
}

irs::sort::ptr make_vpack(irs::string_ref args) {
  if (args.null()) {
    // default args
    return irs::memory::make_unique<irs::tfidf_sort>();
  } else {
    VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
    return make_vpack(slice);
  }
}

irs::sort::ptr make_json(irs::string_ref args) {
  if (args.null()) {
    // default args
    return irs::memory::make_unique<irs::tfidf_sort>();
  } else {
    try {
      auto vpack = VPackParser::fromJson(args.c_str(), args.size());
      return make_vpack(vpack->slice());
    } catch(const VPackException& ex) {
        IR_FRMT_ERROR(
          "Caught error '%s' while constructing VPack from JSON for tfidf scorer",
          ex.what());
    } catch(...) {
        IR_FRMT_ERROR(
          "Caught error while constructing VPack from JSON for tfidf scorer");
    }
    return nullptr;
  }
}

REGISTER_SCORER_JSON(irs::tfidf_sort, make_json);
REGISTER_SCORER_VPACK(irs::tfidf_sort, make_vpack);

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

  virtual void collect(const irs::sub_reader& /*segment*/,
                       const irs::term_reader& field) override {
    docs_with_field += field.docs_count();
  }

  virtual void reset() noexcept override {
    docs_with_field = 0;
  }

  virtual void collect(irs::bytes_ref in) override {
    byte_ref_iterator itr(in);
    auto docs_with_field_value = irs::vread<uint64_t>(itr);

    if (itr.pos_ != itr.end_) {
      throw irs::io_error("input not read fully");
    }

    docs_with_field += docs_with_field_value;
  }

  virtual void write(irs::data_output& out) const override {
    out.write_vlong(docs_with_field);
  }
};

struct term_collector final: public irs::sort::term_collector {
  uint64_t docs_with_term = 0; // number of documents containing the matched term

  virtual void collect(const irs::sub_reader& /*segment*/,
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

FORCE_INLINE float_t tfidf(uint32_t freq, float_t idf) noexcept {
  return idf * kSQRT.get<true>(freq);
}

} // LOCAL

namespace iresearch {
namespace tfidf {

// empty frequency
const frequency EMPTY_FREQ;

struct idf final {
  float_t value;
};

using score_t = tfidf_sort::score_t;

struct ScoreContext : public irs::score_ctx {
  ScoreContext(
      byte_type* score_buf,
      irs::boost_t boost,
      const tfidf::idf& idf,
      const frequency* freq,
      const irs::filter_boost* filter_boost = nullptr) noexcept
    : score_buf{score_buf},
      freq{freq ? *freq : EMPTY_FREQ},
      filter_boost{filter_boost},
      idf{boost * idf.value} {
    assert(freq);
  }

  ScoreContext(const ScoreContext&) = delete;
  ScoreContext& operator=(const ScoreContext&) = delete;

  byte_type* score_buf;
  const frequency& freq;
  const irs::filter_boost* filter_boost;
  float_t idf; // precomputed : boost * idf
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
struct NormAdapter {
  static constexpr auto kType = Type;

  explicit NormAdapter(Reader&& reader)
    : reader{std::move(reader)} {
  }

  FORCE_INLINE float_t operator()() {
    if constexpr (kType < NormType::kNorm) {
      return kRSQRT.get<kType != NormType::kNorm2Tiny>(reader());
    } else {
      return reader();
    }
  }

  Reader reader;
};

template<NormType Type, typename Reader>
auto MakeNormAdapter(Reader&& reader) {
  return NormAdapter<Reader, Type>(std::move(reader));
}

template<typename Norm>
struct NormScoreContext final : public ScoreContext {
  NormScoreContext(
      byte_type* score_buf,
      Norm&& norm,
      boost_t boost,
      const tfidf::idf& idf,
      const frequency* freq,
      const irs::filter_boost* filter_boost = nullptr) noexcept
    : ScoreContext{score_buf, boost, idf, freq, filter_boost},
      norm{std::move(norm)} {
  }

  Norm norm;
};

template<typename Ctx>
struct MakeScoreFunctionImpl {
  template<bool HasFilterBoost, typename... Args>
  static score_function Make(Args&&... args) {
    return {
        memory::make_unique<Ctx>(std::forward<Args>(args)...),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          auto& state = *static_cast<Ctx*>(ctx);

          float_t idf;
          if constexpr (HasFilterBoost) {
            assert(state.filter_boost);
            idf = state.idf * state.filter_boost->value;
          } else {
            idf = state.idf;
          }

          auto& buf = sort::score_cast<score_t>(state.score_buf);

          if constexpr (std::is_same_v<Ctx, ScoreContext>) {
            buf = ::tfidf(state.freq.value, idf);
          } else {
            buf = ::tfidf(state.freq.value, idf) * state.norm();
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

class sort final: public irs::prepared_sort_basic<tfidf::score_t, tfidf::idf> {
 public:
  explicit sort(bool normalize, bool boost_as_score) noexcept
    : normalize_{normalize},
      boost_as_score_{boost_as_score} {
  }

  virtual void collect(
      byte_type* stats_buf,
      const irs::index_reader& /*index*/,
      const irs::sort::field_collector* field,
      const irs::sort::term_collector* term) const override {
    auto& idf = stats_cast(stats_buf);

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

    idf.value += float_t(
      std::log((docs_with_field + 1) / double_t(docs_with_term + 1)) + 1.0);
    assert(idf.value >= 0.f);
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
      const byte_type* stats_buf,
      byte_type* score_buf,
      const attribute_provider& doc_attrs,
      boost_t boost) const override {
    auto* freq = irs::get<frequency>(doc_attrs);

    if (!freq) {
      if (!boost_as_score_ || 0.f == boost) {
        return { nullptr, nullptr };
      }

      // if there is no frequency then all the scores will be the same (e.g. filter irs::all)
      irs::sort::score_cast<tfidf::score_t>(score_buf) = boost;

      return {
        reinterpret_cast<ScoreContext*>(score_buf),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          return reinterpret_cast<byte_type*>(ctx);
        }
      };
    }

    auto& stats = stats_cast(stats_buf);
    auto* filter_boost = irs::get<irs::filter_boost>(doc_attrs);

    // add norm attribute if requested
    if (normalize_) {
      auto* doc = irs::get<document>(doc_attrs);

      if (!doc) {
        // we need 'document' attribute to be exposed
        return { nullptr, nullptr };
      }

      auto prepare_norm_scorer = [&]<typename Norm>(Norm&& norm) -> score_function {
        return MakeScoreFunction<NormScoreContext<Norm>>(
            filter_boost, score_buf, std::move(norm), boost, stats, freq);
      };

      const auto& features = field.meta().features;

      if (auto it = features.find(irs::type<Norm2>::id()); it != features.end()) {
         if (Norm2::Context ctx; ctx.Reset(segment, it->second, *doc)) {
           if (ctx.max_num_bytes == sizeof(byte_type)) {
             return Norm2::MakeReader(std::move(ctx), [&](auto&& reader) {
                 return prepare_norm_scorer(MakeNormAdapter<NormType::kNorm2Tiny>(std::move(reader))); });
           }

           return Norm2::MakeReader(std::move(ctx), [&](auto&& reader) {
               return prepare_norm_scorer(MakeNormAdapter<NormType::kNorm2>(std::move(reader))); });
         }
      }

      if (auto it = features.find(irs::type<Norm>::id()); it != features.end()) {
         if (Norm::Context ctx; ctx.Reset(segment, it->second, *doc)) {
           return prepare_norm_scorer(MakeNormAdapter<NormType::kNorm>(
               Norm::MakeReader(std::move(ctx))));
         }
      }
    }

    return MakeScoreFunction<ScoreContext>(
        filter_boost, score_buf, boost, stats, freq);
  }

  virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
    return irs::memory::make_unique<term_collector>();
  }

 private:
  bool normalize_;
  bool boost_as_score_;
}; // sort

} // tfidf 

/*static*/ sort::ptr tfidf_sort::make(bool normalize, bool boost_as_score) {
  return std::make_unique<tfidf_sort>(normalize, boost_as_score);
}

tfidf_sort::tfidf_sort(bool normalize, bool boost_as_score) noexcept
  : sort{irs::type<tfidf_sort>::get()},
    normalize_{normalize},
    boost_as_score_{boost_as_score} {
}

/*static*/ void tfidf_sort::init() {
  REGISTER_SCORER_JSON(tfidf_sort, make_json); // match registration above
  REGISTER_SCORER_VPACK(tfidf_sort, make_vpack); // match registration above
}

sort::prepared::ptr tfidf_sort::prepare() const {
  return memory::make_unique<tfidf::sort>(normalize_, boost_as_score_);
}

} // ROOT
