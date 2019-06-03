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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <rapidjson/rapidjson/document.h> // for rapidjson::Document

#include "bm25.hpp"

#include "scorers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"

NS_LOCAL

irs::sort::ptr make_from_object(
    const rapidjson::Document& json,
    const irs::string_ref& args) {
  assert(json.IsObject());

  auto ptr = irs::memory::make_shared<irs::bm25_sort>();

  #ifdef IRESEARCH_DEBUG
    auto& scorer = dynamic_cast<irs::bm25_sort&>(*ptr);
  #else
    auto& scorer = static_cast<irs::bm25_sort&>(*ptr);
  #endif

  {
    // optional float
    const auto* key = "b";

    if (json.HasMember(key)) {
      if (!json[key].IsNumber()) {
        IR_FRMT_ERROR("Non-float value in '%s' while constructing bm25 scorer from jSON arguments: %s", key, args.c_str());

        return nullptr;
      }

      scorer.b(json[key].GetFloat());
    }
  }

  {
    // optional float
    const auto* key = "k";

    if (json.HasMember(key)) {
      if (!json[key].IsNumber()) {
        IR_FRMT_ERROR("Non-float value in '%s' while constructing bm25 scorer from jSON arguments: %s", key, args.c_str());

        return nullptr;
      }

      scorer.k(json[key].GetFloat());
    }
  }

  return ptr;
}

irs::sort::ptr make_from_array(
    const rapidjson::Document& json,
    const irs::string_ref& args) {
  assert(json.IsArray());

  const auto array = json.GetArray();
  const auto size = array.Size();

  if (size > 2) {
    // wrong number of arguments
    IR_FRMT_ERROR(
      "Wrong number of arguments while constructing bm25 scorer from jSON arguments (must be <= 2): %s",
      args.c_str()
    );
    return nullptr;
  }

  // default args
  auto k = irs::bm25_sort::K();
  auto b = irs::bm25_sort::B();

  for (rapidjson::SizeType i = 0; i < size; ++i) {
    auto& arg = array[i];

    switch (i) {
     case 0: // parse `b` coefficient
      if (!arg.IsNumber()) {
        IR_FRMT_ERROR(
          "Non-float value at position '%u' while constructing bm25 scorer from jSON arguments: %s",
          i, args.c_str()
        );

        return nullptr;
      }

      k = static_cast<float_t>(arg.GetDouble());
      break;
     case 1: // parse `b` coefficient
      if (!arg.IsNumber()) {
        IR_FRMT_ERROR(
          "Non-float value at position '%u' while constructing bm25 scorer from jSON arguments: %s",
          i, args.c_str()
        );

        return nullptr;
      }

      b = static_cast<float_t>(arg.GetDouble());
      break;
    }
  }

  return irs::memory::make_shared<irs::bm25_sort>(k, b);
}

irs::sort::ptr make_json(const irs::string_ref& args) {
  if (args.null()) {
    // default args
    return irs::memory::make_shared<irs::bm25_sort>();
  }

  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing bm25 scorer, arguments: %s", 
      args.c_str()
    );

    return nullptr;
  }

  switch (json.GetType()) {
    case rapidjson::kObjectType:
      return make_from_object(json, args);
    case rapidjson::kArrayType:
      return make_from_array(json, args);
    default: // wrong type
      IR_FRMT_ERROR(
        "Invalid jSON arguments passed while constructing bm25 scorer, arguments: %s", 
        args.c_str()
      );

      return nullptr;
  }
}

REGISTER_SCORER_JSON(irs::bm25_sort, make_json);

struct byte_ref_iterator
  : public std::iterator<std::input_iterator_tag, irs::byte_type, void, void, void> {
  const irs::byte_type* end_;
  const irs::byte_type* pos_;
  byte_ref_iterator(const irs::bytes_ref& in)
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
    const irs::sub_reader& segment,
    const irs::term_reader& field
  ) override {
    docs_with_field += field.docs_count();

    auto& freq = field.attributes().get<irs::frequency>();

    if (freq) {
      total_term_freq += freq->value;
    }
  }

  virtual void collect(const irs::bytes_ref& in) override {
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
    const irs::sub_reader& segment,
    const irs::term_reader& field,
    const irs::attribute_view& term_attrs
  ) override {
    auto& meta = term_attrs.get<irs::term_meta>();

    if (meta) {
      docs_with_term += meta->docs_count;
    }
  }

  virtual void collect(const irs::bytes_ref& in) override {
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

NS_END // LOCAL

NS_ROOT

// bm25 similarity
// bm25(doc, term) = idf(term) * ((k + 1) * tf(doc, term)) / (k * (1.0 - b + b * |doc|/avgDL) + tf(doc, term))

// inverted document frequency
// idf(term) = log(1 + (# documents with this field - # documents with this term + 0.5)/(# documents with this term + 0.5))

// term frequency
// tf(doc, term) = sqrt(frequency(doc, term));

// document length
// Current implementation is using the following as the document length
// |doc| = 1 / sqrt(# of terms in a field within a document)

// average document length
// avgDL = sum(field_term_count) / (# documents with this field)

NS_BEGIN(bm25)

// empty frequency
const frequency EMPTY_FREQ;

struct stats final : stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();

  stats() = default;

  virtual void clear() { 
    idf = 0.f;
    norm_const = 1.f;
    norm_length = 0.f;
  }
  
  float_t idf{ 0.f }; // precomputed idf value
  float_t norm_const{ 1.f }; // precomputed k*(1-b)
  float_t norm_length{ 0.f }; // precomputed k*b/avgD
}; // stats

DEFINE_ATTRIBUTE_TYPE(irs::bm25::stats)
DEFINE_FACTORY_DEFAULT(stats)

typedef bm25_sort::score_t score_t;

class const_scorer final : public irs::sort::scorer_base<bm25::score_t> {
 public:
  DEFINE_FACTORY_INLINE(scorer)

  explicit const_scorer(irs::boost::boost_t boost) NOEXCEPT
    : boost_(boost) {
  }

  virtual void score(byte_type* score_buf) NOEXCEPT override {
    score_cast(score_buf) = boost_;
  }

 private:
  const irs::boost::boost_t boost_;
}; // const_scorer

class scorer : public irs::sort::scorer_base<bm25::score_t> {
 public:
  DEFINE_FACTORY_INLINE(scorer)

  scorer(
      float_t k, 
      irs::boost::boost_t boost,
      const bm25::stats* stats,
      const frequency* freq) NOEXCEPT
    : freq_(freq ? freq : &EMPTY_FREQ),
      num_(boost * (k + 1) * (stats ? stats->idf : 1.f)),
      norm_const_(k) {
    assert(freq_);
  }

  virtual void score(byte_type* score_buf) NOEXCEPT override {
    const float_t freq = tf();
    score_cast(score_buf) = num_ * freq / (norm_const_ + freq);
  }

 protected:
  FORCE_INLINE float_t tf() const NOEXCEPT {
    return float_t(std::sqrt(freq_->value));
  }

  const frequency* freq_; // document frequency
  float_t num_; // partially precomputed numerator : boost * (k + 1) * idf
  float_t norm_const_; // 'k' factor
}; // scorer

class norm_scorer final : public scorer {
 public:
  DEFINE_FACTORY_INLINE(norm_scorer)

  norm_scorer(
      float_t k, 
      irs::boost::boost_t boost,
      const bm25::stats* stats,
      const frequency* freq,
      const irs::norm* norm) NOEXCEPT
    : scorer(k, boost, stats, freq),
      norm_(norm) {
    assert(norm_);

    // if there is no norms, assume that b==0
    if (!norm_->empty()) {
      norm_const_ = stats->norm_const;
      norm_length_ = stats->norm_length;
    }
  }

  virtual void score(byte_type* score_buf) NOEXCEPT override {
    const float_t freq = tf();
    score_cast(score_buf) = num_ * freq / (norm_const_ + norm_length_ * norm_->read() + freq);
  }

 private:
  const irs::norm* norm_;
  float_t norm_length_{ 0.f }; // precomputed 'k*b/avgD' if norms presetn, '0' otherwise
}; // norm_scorer

class sort final : irs::sort::prepared_basic<bm25::score_t> {
 public:
  DEFINE_FACTORY_INLINE(prepared)

  sort(float_t k, float_t b) NOEXCEPT
    : k_(k), b_(b) {
  }

  virtual void collect(
    irs::attribute_store& filter_attrs,
    const irs::index_reader& index,
    const irs::sort::field_collector* field,
    const irs::sort::term_collector* term
  ) const override {
    bool is_new; // stats weren't already initialized
    auto& stats = filter_attrs.try_emplace<bm25::stats>(is_new);

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
    stats->idf += float_t(std::log(
      1 + ((docs_with_field - docs_with_term + 0.5)/(docs_with_term + 0.5))
    ));
    assert(stats->idf >= 0);

    // - stats were already initialized
    // - BM15 without norms
    if (!is_new || b_ == 0.f) {
      return;
    }

    // precomputed length norm
    const float_t kb = k_ * b_;

    stats->norm_const = k_ - kb;
    stats->norm_length = kb;

    if (total_term_freq && docs_with_field) {
      stats->norm_length /= float_t(total_term_freq) / docs_with_field;
    }

    // add norm attribute
    filter_attrs.emplace<norm>();
  }

  virtual const flags& features() const override {
    static const irs::flags FEATURES[] = {
      irs::flags({ irs::frequency::type() }), // without normalization
      irs::flags({ irs::frequency::type(), irs::norm::type() }), // with normalization
    };

    return FEATURES[b_ != 0.f];
  }

  virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
    return irs::memory::make_unique<field_collector>();
  }

  virtual scorer::ptr prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_store& query_attrs,
      const attribute_view& doc_attrs
  ) const override {
    if (!doc_attrs.contains<frequency>()) {
      return nullptr;
    }

    if (b_ != 0.f) {
      auto& norm = query_attrs.get<irs::norm>();

      if (norm && norm->reset(segment, field.meta().norm, *doc_attrs.get<document>())) {
        return bm25::scorer::make<bm25::norm_scorer>(
          k_,
          boost::extract(query_attrs),
          query_attrs.get<bm25::stats>().get(),
          doc_attrs.get<frequency>().get(),
          &*norm
        );
      }
    }

    // BM11
    return bm25::scorer::make<bm25::scorer>(      
      k_, 
      boost::extract(query_attrs),
      query_attrs.get<bm25::stats>().get(),
      doc_attrs.get<frequency>().get()
    );
  }

  virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
    return irs::memory::make_unique<term_collector>();
  }

 private:
  float_t k_;
  float_t b_;
}; // sort

NS_END // bm25

DEFINE_SORT_TYPE_NAMED(irs::bm25_sort, "bm25")
DEFINE_FACTORY_DEFAULT(irs::bm25_sort)

bm25_sort::bm25_sort(
    float_t k /*= 1.2f*/,
    float_t b /*= 0.75f*/
) NOEXCEPT
  : sort(bm25_sort::type()),
    k_(k),
    b_(b) {
}

/*static*/ void bm25_sort::init() {
  REGISTER_SCORER_JSON(bm25_sort, make_json); // match registration above
}

sort::prepared::ptr bm25_sort::prepare() const {
  return bm25::sort::make<bm25::sort>(k_, b_);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
