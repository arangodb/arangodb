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

#include "doc_generator.hpp"
#include "analysis/analyzers.hpp"
#include "index/field_data.hpp"
#include "analysis/token_streams.hpp"
#include "store/store_utils.hpp"
#include "utils/file_utils.hpp"

#include <sstream>
#include <iomanip>
#include <numeric>

#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <rapidjson/istreamwrapper.h>

#include <utf8.h>

namespace utf8 {
namespace unchecked {

template<typename octet_iterator>
class break_iterator {
 public:
  using utf8iterator = unchecked::iterator<octet_iterator>;

  using iterator_category = std::forward_iterator_tag;
  using value_type = std::string;
  using pointer = value_type*;
  using reference = value_type&;
  using difference_type = void;

  break_iterator(utf8::uint32_t delim, const octet_iterator& begin, const octet_iterator& end)
    : delim_(delim), wbegin_(begin), wend_(begin), end_(end) {
    if (!done()) {
      next();
    }
  }

  explicit break_iterator(const octet_iterator& end)
    : wbegin_(end), wend_(end), end_(end) {
  }

  const std::string& operator*() const { return res_; }

  const std::string* operator->() const { return &res_; }

  bool operator==(const break_iterator& rhs) const {
    assert(end_ == rhs.end_);
    return (wbegin_ == rhs.wbegin_ && wend_ == rhs.wend_);
  }

  bool operator!=(const break_iterator& rhs) const {
    return !(operator==(rhs));
  }

  bool done() const { return wbegin_ == end_; }

  break_iterator& operator++() {
    next();
    return *this;
  }

  break_iterator operator++(int) {
    break_iterator tmp(delim_, wbegin_, end_);
    next();
    return tmp;
  }

  private:
  void next() {
    wbegin_ = wend_;
    wend_ = std::find(wbegin_, end_, delim_);
    if (wend_ != end_) {
      res_.assign(wbegin_.base(), wend_.base());
      ++wend_;
    } else {
      res_.assign(wbegin_.base(), end_.base());
    }
  }

  utf8::uint32_t delim_;
  std::string res_;
  utf8iterator wbegin_;
  utf8iterator wend_;
  utf8iterator end_;
};

} // unchecked
} // utf8

namespace tests {

// -----------------------------------------------------------------------------
// --SECTION--                                           document implementation
// -----------------------------------------------------------------------------

document::document(document&& rhs) noexcept
  : indexed(std::move(rhs.indexed)), 
    stored(std::move(rhs.stored)),
    sorted(std::move(rhs.sorted)) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                         field_base implementation
// -----------------------------------------------------------------------------

field_base::field_base(field_base&& rhs) noexcept
  : features_(std::move(rhs.features_)),
    name_(std::move(rhs.name_)) {
}

field_base& field_base::operator=(field_base&& rhs) noexcept {
  if (this != &rhs) {
    features_ = std::move(rhs.features_);
    name_ = std::move(rhs.name_);
  }

  return *this;
}

// -----------------------------------------------------------------------------
// --SECTION--                                         long_field implementation
// -----------------------------------------------------------------------------

irs::token_stream& long_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

bool long_field::write(irs::data_output& out) const {
  irs::write_zvlong(out, value_);
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                         int_field implementation
// -----------------------------------------------------------------------------

irs::token_stream& int_field::get_tokens() const {
  stream_->reset(value_);
  return *stream_;
}

bool int_field::write(irs::data_output& out) const {
  irs::write_zvint(out, value_);
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       double_field implementation
// -----------------------------------------------------------------------------

irs::token_stream& double_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

bool double_field::write(irs::data_output& out) const {
  irs::write_zvdouble(out, value_);
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        float_field implementation
// -----------------------------------------------------------------------------

irs::token_stream& float_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

bool float_field::write(irs::data_output& out) const {
  irs::write_zvfloat(out, value_);
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       binary_field implementation
// -----------------------------------------------------------------------------

irs::token_stream& binary_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

bool binary_field::write(irs::data_output& out) const {
  irs::write_string(out, value_);
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                           particle implementation
// -----------------------------------------------------------------------------

particle::particle(particle&& rhs) noexcept
  : fields_(std::move(rhs.fields_)) {
}

particle& particle::operator=(particle&& rhs) noexcept {
  if (this != &rhs) {
    fields_ = std::move(rhs.fields_);
  }

  return *this;
}

bool particle::contains(const irs::string_ref& name) const {
  return fields_.end() != std::find_if(
    fields_.begin(), fields_.end(),
    [&name] (const ifield::ptr& fld) {
      return name == fld->name();
  });
}

std::vector<ifield::ptr> particle::find(const irs::string_ref& name) const {
  std::vector<ifield::ptr> fields;
  std::for_each(
    fields_.begin(), fields_.end(),
    [&fields, &name] (ifield::ptr fld) {
      if (name == fld->name()) {
        fields.emplace_back(fld);
      }
  });

  return fields;
}

ifield* particle::get(const irs::string_ref& name) const {
  auto it = std::find_if(
    fields_.begin(), fields_.end(),
    [&name] (const ifield::ptr& fld) {
      return name == fld->name();
  });

  return fields_.end() == it ? nullptr : it->get();
}

void particle::remove(const irs::string_ref& name) {
  fields_.erase(
    std::remove_if(fields_.begin(), fields_.end(),
      [&name] (const ifield::ptr& fld) {
        return name == fld->name();
    })
  );
}

// -----------------------------------------------------------------------------
// --SECTION--                                delim_doc_generator implementation
// -----------------------------------------------------------------------------

delim_doc_generator::delim_doc_generator(
    const irs::utf8_path& file,
    doc_template& doc,
    uint32_t delim /* = 0x0009 */)
  : ifs_(file.native(), std::ifstream::in | std::ifstream::binary),
    doc_(&doc),
    delim_(delim) {
  doc_->init();
  doc_->reset();
}

const tests::document* delim_doc_generator::next() {
  if (!getline(ifs_, str_)) {
    return nullptr;
  } 

  {
    const std::string::const_iterator end = utf8::find_invalid(str_.begin(), str_.end());
    if (end != str_.end()) {
      /* invalid utf8 string */
      return nullptr;
    }
  }

  using word_iterator = utf8::unchecked::break_iterator<std::string::const_iterator>;

  const word_iterator end(str_.end());
  word_iterator begin(delim_, str_.begin(), str_.end());
  for (size_t i = 0; begin != end; ++begin, ++i) {
    doc_->value(i, *begin);
  }
  doc_->end();
  return doc_;
}
 
void delim_doc_generator::reset() {
  ifs_.clear();
  ifs_.seekg(ifs_.beg);
  doc_->reset();
}

// -----------------------------------------------------------------------------
// --SECTION--                                  csv_doc_generator implementation
// -----------------------------------------------------------------------------

csv_doc_generator::csv_doc_generator(
    const irs::utf8_path& file,
    doc_template& doc)
  : doc_(doc),
    ifs_(file.native(), std::ifstream::in | std::ifstream::binary),
    stream_(irs::analysis::analyzers::get("delimiter", irs::type<irs::text_format::text>::get(), ",")) {
  doc_.init();
  doc_.reset();
}

const tests::document* csv_doc_generator::next() {
  if (!getline(ifs_, line_) || !stream_) {
    return nullptr;
  }

  auto* term = irs::get<irs::term_attribute>(*stream_);

  if (!term || !stream_->reset(line_)) {
    return nullptr;
  }

  for (size_t i = 0; stream_->next(); ++i) {
    doc_.value(i, irs::ref_cast<char>(term->value));
  }

  return &doc_;
}

void csv_doc_generator::reset() {
  ifs_.clear();
  ifs_.seekg(ifs_.beg);
  doc_.reset();
}

bool csv_doc_generator::skip() {
  return false == !getline(ifs_, line_);
}

//////////////////////////////////////////////////////////////////////////////
/// @class parse_json_handler
/// @brief rapdijson campatible visitor for
///        JSON document-derived column value types
//////////////////////////////////////////////////////////////////////////////
class parse_json_handler : irs::util::noncopyable {
 public:
  typedef std::vector<tests::document> documents_t;

  parse_json_handler(const json_doc_generator::factory_f& factory, documents_t& docs)
    : factory_(factory), docs_(docs) {
  }

  bool Null() {
    val_.vt = json_doc_generator::ValueType::NIL;
    AddField();
    return true;
  }

  bool Bool(bool b) {
    val_.vt = json_doc_generator::ValueType::BOOL;
    val_.b = b;
    AddField();
    return true;
  }

  bool Int(int i) {
    val_.vt = json_doc_generator::ValueType::INT;
    val_.i = i;
    AddField();
    return true;
  }

  bool Uint(unsigned u) {
    val_.vt = json_doc_generator::ValueType::UINT;
    val_.ui = u;
    AddField();
    return true;
  }

  bool Int64(int64_t i) {
    val_.vt = json_doc_generator::ValueType::INT64;
    val_.i64 = i;
    AddField();
    return true;
  }

  bool Uint64(uint64_t u) {
    val_.vt = json_doc_generator::ValueType::UINT64;
    val_.ui64 = u;
    AddField();
    return true;
  }

  bool Double(double d) {
    val_.vt = json_doc_generator::ValueType::DBL;
    val_.dbl = d;
    AddField();
    return true;
  }

  bool RawNumber(const char* str, rapidjson::SizeType length, bool /*copy*/) {
    val_.vt = json_doc_generator::ValueType::RAWNUM;
    val_.str = irs::string_ref(str, length);
    AddField();
    return true;
  }

  bool String(const char* str, rapidjson::SizeType length, bool /*copy*/) {
    val_.vt = json_doc_generator::ValueType::STRING;
    val_.str = irs::string_ref(str, length);
    AddField();
    return true;
  }

  bool StartObject() {
    if (1 == level_) {
      docs_.emplace_back();
    }

    ++level_;
    return true;
  }

  bool StartArray() {
    ++level_;
    return true;
  }

  bool Key(const char* str, rapidjson::SizeType length, bool) {
    if (level_-1 > path_.size()) {
      path_.emplace_back(str, length);
    } else {
      path_.back().assign(str, length);
    }
    return true;
  }

  bool EndObject(rapidjson::SizeType memberCount) {
    --level_;

    if (!path_.empty()) {
      path_.pop_back();
    }
    return true;
  }

  bool EndArray(rapidjson::SizeType elementCount) {
    return EndObject(elementCount);
  }

 private:
  void AddField() {
    factory_(docs_.back(), path_.back(), val_);
  }

  const json_doc_generator::factory_f& factory_;
  documents_t& docs_;
  std::vector<std::string> path_;
  size_t level_{};
  json_doc_generator::json_value val_;
}; // parse_json_handler

json_doc_generator::json_doc_generator(
    const irs::utf8_path& file,
    const json_doc_generator::factory_f& factory) {
  std::ifstream input(irs::utf8_path(file).utf8().c_str(), std::ios::in | std::ios::binary);
  assert(input);

  rapidjson::IStreamWrapper stream(input);
  parse_json_handler handler(factory, docs_);
  rapidjson::Reader reader;

  const auto res = reader.Parse(stream, handler);
  assert(!res.IsError());

  next_ = docs_.begin();
}

json_doc_generator::json_doc_generator(
    const char* data,
    const json_doc_generator::factory_f& factory) {
  assert(data);

  rapidjson::StringStream stream(data);
  parse_json_handler handler(factory, docs_);
  rapidjson::Reader reader;

  const auto res = reader.Parse(stream, handler);
  assert(!res.IsError());

  next_ = docs_.begin();
}

json_doc_generator::json_doc_generator(json_doc_generator&& rhs) noexcept
  : docs_(std::move(rhs.docs_)), 
    prev_(std::move(rhs.prev_)), 
    next_(std::move(rhs.next_)) {
}

const tests::document* json_doc_generator::next() {
  if (docs_.end() == next_) {
    return nullptr;
  }

  prev_ = next_, ++next_;
  return &*prev_;
}

void json_doc_generator::reset() {
  next_ = docs_.begin();
}

} // tests
