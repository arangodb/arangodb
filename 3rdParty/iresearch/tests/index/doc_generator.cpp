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

#include <sstream>
#include <iomanip>
#include <numeric>

#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <rapidjson/istreamwrapper.h>
#include <utf8.h>

#include "index/norm.hpp"
#include "index/field_data.hpp"
#include "analysis/token_streams.hpp"
#include "store/store_utils.hpp"
#include "utils/file_utils.hpp"

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

const document* delim_doc_generator::next() {
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

const document* csv_doc_generator::next() {
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
  typedef std::vector<document> documents_t;

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

  bool EndObject(rapidjson::SizeType /*memberCount*/) {
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
  std::ifstream input(irs::utf8_path(file).u8string().c_str(), std::ios::in | std::ios::binary);
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

const document* json_doc_generator::next() {
  if (docs_.end() == next_) {
    return nullptr;
  }

  prev_ = next_, ++next_;
  return &*prev_;
}

void json_doc_generator::reset() {
  next_ = docs_.begin();
}

token_stream_payload::token_stream_payload(irs::token_stream* impl)
  : impl_(impl) {
    assert(impl_);
    term_ = irs::get<irs::term_attribute>(*impl_);
    assert(term_);
}

irs::attribute* token_stream_payload::get_mutable(irs::type_info::type_id type) {
  if (irs::type<irs::payload>::id() == type) {
    return &pay_;
  }

  return impl_->get_mutable(type);
}

bool token_stream_payload::next() {
  if (impl_->next()) {
    pay_.value = term_->value;
    return true;
  }
  pay_.value = irs::bytes_ref::NIL;
  return false;
}

string_field::string_field(
    const std::string& name,
    irs::IndexFeatures index_features,
    const std::vector<irs::type_info::type_id>& extra_features) {
  index_features_ = (irs::IndexFeatures::FREQ | irs::IndexFeatures::POS) | index_features;
  features_ = extra_features;
  name_ = name;
}

string_field::string_field(
    const std::string& name,
    const irs::string_ref& value,
    irs::IndexFeatures index_features,
    const std::vector<irs::type_info::type_id>& extra_features)
  : value_(value) {
  index_features_ = (irs::IndexFeatures::FREQ | irs::IndexFeatures::POS) | index_features;
  features_ = extra_features;
  name_ = name;
}

// reject too long terms
void string_field::value(const irs::string_ref& str) {
  const auto size_len = irs::bytes_io<uint32_t>::vsize(irs::byte_block_pool::block_type::SIZE);
  const auto max_len = (std::min)(str.size(), size_t(irs::byte_block_pool::block_type::SIZE - size_len));
  auto begin = str.begin();
  auto end = str.begin() + max_len;
  value_.assign(begin, end);
}

bool string_field::write(irs::data_output& out) const {
  irs::write_string(out, value_);
  return true;
}

irs::token_stream& string_field::get_tokens() const {
  REGISTER_TIMER_DETAILED();

  stream_.reset(value_);
  return stream_;
}

string_ref_field::string_ref_field(
    const std::string& name,
    irs::IndexFeatures extra_index_features,
    const std::vector<irs::type_info::type_id>& extra_features) {
  index_features_ = (irs::IndexFeatures::FREQ | irs::IndexFeatures::POS) | extra_index_features;
  features_ = extra_features;
  name_ = name;
}

string_ref_field::string_ref_field(
    const std::string& name,
    const irs::string_ref& value,
    irs::IndexFeatures extra_index_features,
    const std::vector<irs::type_info::type_id>& extra_features)
  : value_(value) {
  index_features_ = (irs::IndexFeatures::FREQ | irs::IndexFeatures::POS) | extra_index_features;
  features_ = extra_features;
  name_ = name;
}

// truncate very long terms
void string_ref_field::value(const irs::string_ref& str) {
  const auto size_len = irs::bytes_io<uint32_t>::vsize(irs::byte_block_pool::block_type::SIZE);
  const auto max_len = (std::min)(str.size(), size_t(irs::byte_block_pool::block_type::SIZE - size_len));

  value_ = irs::string_ref(str.c_str(), max_len);
}

bool string_ref_field::write(irs::data_output& out) const {
  irs::write_string(out, value_);
  return true;
}

irs::token_stream& string_ref_field::get_tokens() const {
  REGISTER_TIMER_DETAILED();

  stream_.reset(value_);
  return stream_;
}

void europarl_doc_template::init() {
  clear();
  indexed.push_back(std::make_shared<string_field>("title"));
  indexed.push_back(std::make_shared<text_ref_field>("title_anl", false));
  indexed.push_back(std::make_shared<text_ref_field>("title_anl_pay", true));
  indexed.push_back(std::make_shared<text_ref_field>("body_anl", false));
  indexed.push_back(std::make_shared<text_ref_field>("body_anl_pay", true));
  {
    insert(std::make_shared<long_field>());
    auto& field = static_cast<long_field&>(indexed.back());
    field.name("date");
  }
  insert(std::make_shared<string_field>("datestr"));
  insert(std::make_shared<string_field>("body"));
  {
    insert(std::make_shared<int_field>());
    auto& field = static_cast<int_field&>(indexed.back());
    field.name("id");
  }
  insert(std::make_shared<string_field>("idstr"));
}

void europarl_doc_template::value(size_t idx, const std::string& value) {
  static auto get_time = [](const std::string& src) {
    std::istringstream ss(src);
    std::tm tmb{};
    char c;
    ss >> tmb.tm_year >> c >> tmb.tm_mon >> c >> tmb.tm_mday;
    return std::mktime( &tmb );
  };

  switch (idx) {
    case 0: // title
      title_ = value;
      indexed.get<string_field>("title")->value(title_);
      indexed.get<text_ref_field>("title_anl")->value(title_);
      indexed.get<text_ref_field>("title_anl_pay")->value(title_);
      break;
    case 1: // date
      indexed.get<long_field>("date")->value(get_time(value));
      indexed.get<string_field>("datestr")->value(value);
      break;
    case 2: // body
      body_ = value;
      indexed.get<string_field>("body")->value(body_);
      indexed.get<text_ref_field>("body_anl")->value(body_);
      indexed.get<text_ref_field>("body_anl_pay")->value(body_);
      break;
  }
}

void europarl_doc_template::end() {
  ++idval_;
  indexed.get<int_field>("id")->value(idval_);
  indexed.get<string_field>("idstr")->value(std::to_string(idval_));
}

void europarl_doc_template::reset() {
  idval_ = 0;
}

void generic_json_field_factory(
    document& doc,
    const std::string& name,
    const json_doc_generator::json_value& data) {
  if (json_doc_generator::ValueType::STRING == data.vt) {
    doc.insert(std::make_shared<string_field>(name, data.str));
  } else if (json_doc_generator::ValueType::NIL == data.vt) {
    doc.insert(std::make_shared<binary_field>());
    auto& field = (doc.indexed.end() - 1).as<binary_field>();
    field.name(name);
    field.value(irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()));
  } else if (json_doc_generator::ValueType::BOOL == data.vt && data.b) {
    doc.insert(std::make_shared<binary_field>());
    auto& field = (doc.indexed.end() - 1).as<binary_field>();
    field.name(name);
    field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
  } else if (json_doc_generator::ValueType::BOOL == data.vt && !data.b) {
    doc.insert(std::make_shared<binary_field>());
    auto& field = (doc.indexed.end() - 1).as<binary_field>();
    field.name(name);
    field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
  } else if (data.is_number()) {
    // 'value' can be interpreted as a double
    doc.insert(std::make_shared<double_field>());
    auto& field = (doc.indexed.end() - 1).as<double_field>();
    field.name(name);
    field.value(data.as_number<double_t>());
  }
}

void payloaded_json_field_factory(
    document& doc,
    const std::string& name,
    const json_doc_generator::json_value& data) {
  typedef text_field<std::string> text_field;

  if (json_doc_generator::ValueType::STRING == data.vt) {
    // analyzed && pyaloaded
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.c_str()) + "_anl_pay",
      data.str, true));

    // analyzed field
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.c_str()) + "_anl",
      data.str));

    // not analyzed field
    doc.insert(std::make_shared<string_field>(name, data.str));
  } else if (json_doc_generator::ValueType::NIL == data.vt) {
    doc.insert(std::make_shared<binary_field>());
    auto& field = (doc.indexed.end() - 1).as<binary_field>();
    field.name(name);
    field.value(irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()));
  } else if (json_doc_generator::ValueType::BOOL == data.vt && data.b) {
    doc.insert(std::make_shared<binary_field>());
    auto& field = (doc.indexed.end() - 1).as<binary_field>();
    field.name(name);
    field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
  } else if (json_doc_generator::ValueType::BOOL == data.vt && !data.b) {
    doc.insert(std::make_shared<binary_field>());
    auto& field = (doc.indexed.end() - 1).as<binary_field>();
    field.name(name);
    field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false()));
  } else if (data.is_number()) {
    // 'value' can be interpreted as a double
    doc.insert(std::make_shared<double_field>());
    auto& field = (doc.indexed.end() - 1).as<double_field>();
    field.name(name);
    field.value(data.as_number<double_t>());
  }
}

void normalized_string_json_field_factory(
    document& doc,
    const std::string& name,
    const json_doc_generator::json_value& data) {
  if (json_doc_generator::ValueType::STRING == data.vt) {
    doc.insert(
      std::make_shared<string_field>(
        name,
        data.str,
        irs::IndexFeatures::NONE,
        std::vector<irs::type_info::type_id>{ irs::type<irs::Norm>::id() }));;
  } else {
    generic_json_field_factory(doc, name, data);
  }
}

} // tests
