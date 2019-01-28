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

#include "tfidf.hpp"

#include "scorers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"

NS_LOCAL

irs::sort::ptr make_from_bool(
    const rapidjson::Document& json,
    const irs::string_ref& //args
) {
  assert(json.IsBool());

  return irs::memory::make_shared<irs::tfidf_sort>(
    json.GetBool()
  );
}

irs::sort::ptr make_from_object(
    const rapidjson::Document& json,
    const irs::string_ref& args) {
  assert(json.IsObject());

  auto ptr = irs::memory::make_shared<irs::tfidf_sort>();

  #ifdef IRESEARCH_DEBUG
    auto& scorer = dynamic_cast<irs::tfidf_sort&>(*ptr);
  #else
    auto& scorer = static_cast<irs::tfidf_sort&>(*ptr);
  #endif

  {
    // optional bool
    const auto* key= "with-norms";

    if (json.HasMember(key)) {
      if (!json[key].IsBool()) {
        IR_FRMT_ERROR("Non-boolean value in '%s' while constructing tfidf scorer from jSON arguments: %s", key, args.c_str());

        return nullptr;
      }

      scorer.normalize(json[key].GetBool());
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

  if (size > 1) {
    // wrong number of arguments
    IR_FRMT_ERROR(
      "Wrong number of arguments while constructing tfidf scorer from jSON arguments (must be <= 1): %s",
      args.c_str()
    );
    return nullptr;
  }

  // default args
  auto norms = irs::tfidf_sort::WITH_NORMS();

  // parse `with-norms` optional argument
  if (!array.Empty()) {
    auto& arg = array[0];
    if (!arg.IsBool()) {
      IR_FRMT_ERROR(
        "Non-float value on position `0` while constructing bm25 scorer from jSON arguments: %s",
        args.c_str()
      );
      return nullptr;
    }

    norms = arg.GetBool();
  }

  return irs::memory::make_shared<irs::tfidf_sort>(norms);
}

irs::sort::ptr make_json(const irs::string_ref& args) {
  if (args.null()) {
    return irs::memory::make_shared<irs::tfidf_sort>();
  }

  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing tfidf scorer, arguments: %s", 
      args.c_str()
    );

    return nullptr;
  }

  switch (json.GetType()) {
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
      return make_from_bool(json, args);
    case rapidjson::kObjectType:
      return make_from_object(json, args);
    case rapidjson::kArrayType:
      return make_from_array(json, args);
    default: // wrong type
      IR_FRMT_ERROR(
        "Invalid jSON arguments passed while constructing tfidf scorer, arguments: %s", 
        args.c_str()
      );

      return nullptr;
  }
}

REGISTER_SCORER_JSON(irs::tfidf_sort, make_json);

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

  virtual void collect(
    const irs::sub_reader& segment,
    const irs::term_reader& field
  ) override {
    docs_with_field += field.docs_count();
  }

  virtual void collect(const irs::bytes_ref& in) override {
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
NS_BEGIN(tfidf)

// empty frequency
const frequency EMPTY_FREQ;

struct idf final : basic_stored_attribute<float_t> {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();
  idf() : basic_stored_attribute(0.f) { }

  void clear() { value = 0.f; }
};

DEFINE_ATTRIBUTE_TYPE(irs::tfidf::idf)
DEFINE_FACTORY_DEFAULT(idf)

typedef tfidf_sort::score_t score_t;

class scorer : public irs::sort::scorer_base<tfidf::score_t> {
 public:
  DEFINE_FACTORY_INLINE(scorer)

  scorer(
      irs::boost::boost_t boost,
      const tfidf::idf* idf,
      const frequency* freq) NOEXCEPT
    : idf_(boost * (idf ? idf->value : 1.f)), 
      freq_(freq ? freq : &EMPTY_FREQ) {
    assert(freq_);
  }

  virtual void score(byte_type* score_buf) NOEXCEPT override {
    score_cast(score_buf) = tfidf();
  }

 protected:
  FORCE_INLINE float_t tfidf() const NOEXCEPT {
   return idf_ * float_t(std::sqrt(freq_->value));
  }

 private:
  float_t idf_; // precomputed : boost * idf
  const frequency* freq_;
}; // scorer

class norm_scorer final : public scorer {
 public:
  DEFINE_FACTORY_INLINE(norm_scorer)

  norm_scorer(
      const irs::norm* norm,
      irs::boost::boost_t boost,
      const tfidf::idf* idf,
      const frequency* freq) NOEXCEPT
    : scorer(boost, idf, freq),
      norm_(norm) {
    assert(norm_);
  }

  virtual void score(byte_type* score_buf) NOEXCEPT override {
    score_cast(score_buf) = tfidf() * norm_->read();
  }

 private:
  const irs::norm* norm_;
}; // norm_scorer

class sort final: irs::sort::prepared_basic<tfidf::score_t> {
 public:
  DEFINE_FACTORY_INLINE(prepared)

  sort(bool normalize) NOEXCEPT
    : normalize_(normalize) {
  }

  virtual void collect(
      irs::attribute_store& filter_attrs,
      const irs::index_reader& index,
      const irs::sort::field_collector* field,
      const irs::sort::term_collector* term
  ) const override {
    auto& idf = filter_attrs.emplace<tfidf::idf>();

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

    idf->value += float_t(
      std::log((docs_with_field + 1) / double_t(docs_with_term + 1)) + 1.0
    );
    assert(idf->value >= 0);

    // add norm attribute if requested
    if (normalize_) {
      filter_attrs.emplace<norm>();
    }
  }

  virtual const flags& features() const override {
    static const irs::flags FEATURES[] = {
      irs::flags({ irs::frequency::type() }), // without normalization
      irs::flags({ irs::frequency::type(), irs::norm::type() }), // with normalization
    };

    return FEATURES[normalize_];
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
      return nullptr; // if there is no frequency then all the scores will be the same (e.g. filter irs::all)
    }

    auto& norm = query_attrs.get<iresearch::norm>();

    if (norm && norm->reset(segment, field.meta().norm, *doc_attrs.get<document>())) {
      return tfidf::scorer::make<tfidf::norm_scorer>(
        &*norm,
        boost::extract(query_attrs),
        query_attrs.get<tfidf::idf>().get(),
        doc_attrs.get<frequency>().get()
      );
    }

    return tfidf::scorer::make<tfidf::scorer>(
      boost::extract(query_attrs),
      query_attrs.get<tfidf::idf>().get(),
      doc_attrs.get<frequency>().get()
    );
  }

  virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
    return irs::memory::make_unique<term_collector>();
  }

 private:
  const std::function<bool(score_t, score_t)>* less_;
  bool normalize_;
}; // sort

NS_END // tfidf 

DEFINE_SORT_TYPE_NAMED(irs::tfidf_sort, "tfidf")
DEFINE_FACTORY_DEFAULT(irs::tfidf_sort)

tfidf_sort::tfidf_sort(bool normalize) NOEXCEPT
  : sort(tfidf_sort::type()),
    normalize_(normalize) {
}

/*static*/ void tfidf_sort::init() {
  REGISTER_SCORER_JSON(tfidf_sort, make_json); // match registration above
}

sort::prepared::ptr tfidf_sort::prepare() const {
  return tfidf::sort::make<tfidf::sort>(normalize_);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------