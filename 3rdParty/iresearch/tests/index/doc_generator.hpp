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

#ifndef IRESEARCH_DOCUMENT_GENERATOR_H
#define IRESEARCH_DOCUMENT_GENERATOR_H

#include "analysis/analyzer.hpp"
#include "analysis/token_streams.hpp"
#include "utils/iterator.hpp"
#include "utils/utf8_path.hpp"
#include "store/store_utils.hpp"
#include "index/index_writer.hpp"

#include <fstream>
#include <atomic>
#include <functional>

namespace iresearch {

struct data_output;
class flags;
class token_stream;

} // namespace iresearch {

namespace tests {

//////////////////////////////////////////////////////////////////////////////
/// @class ifield
/// @brief base interface for all fields
//////////////////////////////////////////////////////////////////////////////
struct ifield {
  using ptr = std::shared_ptr<ifield>;
  virtual ~ifield() = default;

  virtual const irs::flags& features() const = 0;
  virtual irs::token_stream& get_tokens() const = 0;
  virtual irs::string_ref name() const = 0;
  virtual bool write(irs::data_output& out) const = 0;
}; // ifield

//////////////////////////////////////////////////////////////////////////////
/// @class field 
/// @brief base class for field implementations
//////////////////////////////////////////////////////////////////////////////
class field_base : public ifield {
 public:
  field_base() = default;

  field_base(field_base&& rhs) noexcept;
  field_base& operator=(field_base&& rhs) noexcept;

  field_base(const field_base&) = default;
  field_base& operator=(const field_base&) = default;

  irs::flags& features() { return features_; }
  const irs::flags& features() const { return features_; }

  irs::string_ref name() const { return name_; }
  void name(std::string&& name) { name_ = std::move(name); }
  void name(const std::string& name) { name_ = name; }

 private:
  iresearch::flags features_;
  std::string name_;
}; // field_base

//////////////////////////////////////////////////////////////////////////////
/// @class long_field 
/// @brief provides capabilities for storing & indexing int64_t values 
//////////////////////////////////////////////////////////////////////////////
class long_field: public field_base {
 public:
  typedef int64_t value_t;

  long_field() = default;

  irs::token_stream& get_tokens() const override;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const override;

 private:
  mutable irs::numeric_token_stream stream_;
  int64_t value_{};
}; // long_field 

//////////////////////////////////////////////////////////////////////////////
/// @class long_field 
/// @brief provides capabilities for storing & indexing int32_t values 
//////////////////////////////////////////////////////////////////////////////
class int_field: public field_base {
 public:
  typedef int32_t value_t;

  int_field()
    : stream_(std::make_shared<irs::numeric_token_stream>()) {
  }
  int_field(int_field&& other) noexcept
    : field_base(std::move(other)),
      stream_(std::move(other.stream_)),
      value_(std::move(other.value_)) {
  }

  irs::token_stream& get_tokens() const override;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const override;

 private:
  mutable std::shared_ptr<irs::numeric_token_stream> stream_;
  int32_t value_{};
}; // int_field 

//////////////////////////////////////////////////////////////////////////////
/// @class long_field 
/// @brief provides capabilities for storing & indexing double_t values 
//////////////////////////////////////////////////////////////////////////////
class double_field: public field_base {
 public:
  typedef double_t value_t;

  double_field() = default;

  irs::token_stream& get_tokens() const override;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const override;

 private:
  mutable irs::numeric_token_stream stream_;
  double_t value_{};
}; // double_field

//////////////////////////////////////////////////////////////////////////////
/// @class long_field 
/// @brief provides capabilities for storing & indexing double_t values 
//////////////////////////////////////////////////////////////////////////////
class float_field: public field_base {
 public:
  typedef float_t value_t;

  float_field() = default;

  irs::token_stream& get_tokens() const override;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const override;

 private:
  mutable irs::numeric_token_stream stream_;
  float_t value_{};
}; // float_field 

//////////////////////////////////////////////////////////////////////////////
/// @class binary_field 
/// @brief provides capabilities for storing & indexing binary values 
//////////////////////////////////////////////////////////////////////////////
class binary_field: public field_base {
 public:
  binary_field() = default;

  irs::token_stream& get_tokens() const override;
  const irs::bstring& value() const { return value_; }
  void value(const irs::bytes_ref& value) { value_ = value; }
  void value(irs::bstring&& value) { value_ = std::move(value); }

  template<typename Iterator>
  void value(Iterator first, Iterator last) {
    value_ = bytes(first, last);
  }

  bool write(irs::data_output& out) const override;

 private:
  mutable irs::string_token_stream stream_;
  irs::bstring value_;
}; // binary_field

/* -------------------------------------------------------------------
* document 
* ------------------------------------------------------------------*/

class particle: irs::util::noncopyable {
 public:
  typedef std::vector<ifield::ptr> fields_t;
  typedef irs::ptr_iterator<fields_t::const_iterator> const_iterator;
  typedef irs::ptr_iterator<fields_t::iterator> iterator;

  particle() = default;
  particle(particle&& rhs) noexcept;
  particle& operator=(particle&& rhs) noexcept;
  virtual ~particle() = default;

  size_t size() const { return fields_.size(); }
  void clear() { fields_.clear(); }
  void reserve(size_t size) { fields_.reserve(size); }
  void push_back(const ifield::ptr& fld) { fields_.emplace_back(fld); }

  ifield& back() const { return *fields_.back(); }
  bool contains(const irs::string_ref& name) const;
  std::vector<ifield::ptr> find(const irs::string_ref& name) const;

  template<typename T>
  T& back() const {
    typedef typename std::enable_if<
      std::is_base_of<tests::ifield, T>::value, T
     >::type type;

    return static_cast<type&>(*fields_.back());
  }

  ifield* get(const irs::string_ref& name) const;

  template<typename T>
  T& get(size_t i) const {
    typedef typename std::enable_if<
      std::is_base_of<tests::ifield, T>::value, T
     >::type type;

    return static_cast<type&>(*fields_[i]);
  }

  template<typename T>
  T* get(const irs::string_ref& name) const {
    typedef typename std::enable_if<
      std::is_base_of<tests::ifield, T>::value, T
     >::type type;

    return static_cast<type*>(get(name));
  }

  void remove(const irs::string_ref& name);

  iterator begin() { return iterator(fields_.begin()); }
  iterator end() { return iterator(fields_.end()); }

  const_iterator begin() const { return const_iterator(fields_.begin()); }
  const_iterator end() const { return const_iterator(fields_.end()); }

 protected:
  fields_t fields_;
}; // particle 

struct document: irs::util::noncopyable {
  document() = default;
  document(document&& rhs) noexcept;
  virtual ~document() = default;

  void insert(const ifield::ptr& field, bool indexed = true, bool stored = true) {
    if (indexed) this->indexed.push_back(field);
    if (stored) this->stored.push_back(field);
  }

  void reserve(size_t size) {
    indexed.reserve(size);
    stored.reserve(size);
  }

  void clear() {
    indexed.clear();
    stored.clear();
  }

  particle indexed;
  particle stored;
  ifield::ptr sorted;
}; // document

/* -------------------------------------------------------------------
* GENERATORS 
* ------------------------------------------------------------------*/

/* Base class for document generators */
struct doc_generator_base {
  using ptr = std::unique_ptr<doc_generator_base>;

  virtual const tests::document* next() = 0;
  virtual void reset() = 0;

  virtual ~doc_generator_base() = default;
};

class limiting_doc_generator : public doc_generator_base {
 public:
  limiting_doc_generator(doc_generator_base& gen, size_t offset, size_t limit)
    : gen_(&gen), begin_(offset), end_(offset + limit) {
  }

  virtual const tests::document* next() override {
    while (pos_ < begin_) {
      if (!gen_->next()) {
        // exhausted
        pos_ = end_;
        return nullptr;
      }

      ++pos_;
    }

    if (pos_ < end_) {
      auto* doc = gen_->next();
      if (!doc) {
        // exhausted
        pos_ = end_;
        return nullptr;
      }
      ++pos_;
      return doc;
    }

    return nullptr;
  }

  virtual void reset() override {
    pos_ = 0;
    gen_->reset();
  }

 private:
  doc_generator_base* gen_;
  size_t pos_{0};
  const size_t begin_;
  const size_t end_;
};

/* Generates documents from UTF-8 encoded file 
 * with strings of the following format:
 * <title>;<date>:<body> */
class delim_doc_generator : public doc_generator_base {
 public:
  struct doc_template : document {
    virtual void init() = 0;
    virtual void value(size_t idx, const std::string& value) = 0;
    virtual void end() {}
    virtual void reset() {} 
  }; // doc_template

  delim_doc_generator(
    const irs::utf8_path& file,
    doc_template& doc,
    uint32_t delim = 0x0009);

  virtual const tests::document* next() override; 
  virtual void reset() override;

 private:
  std::string str_;
  std::ifstream ifs_;  
  doc_template* doc_;
  uint32_t delim_;
}; // delim_doc_generator

// Generates documents from a CSV file
class csv_doc_generator: public doc_generator_base {
 public:
  struct doc_template: document {
    virtual void init() = 0;
    virtual void value(size_t idx, const irs::string_ref& value) = 0;
    virtual void end() {}
    virtual void reset() {}
  }; // doc_template

  csv_doc_generator(const irs::utf8_path& file, doc_template& doc);
  virtual const tests::document* next() override;
  virtual void reset() override;
  bool skip(); // skip a single document, return if anything was skiped, false == EOF

 private:
  doc_template& doc_;
  std::ifstream ifs_;
  std::string line_;
  irs::analysis::analyzer::ptr stream_;
};

/* Generates documents from json file based on type of JSON value */
class json_doc_generator: public doc_generator_base {
 public:
  enum class ValueType {
    NIL,
    BOOL,
    INT,
    UINT,
    INT64,
    UINT64,
    DBL,
    STRING,
    RAWNUM
  }; // ValueType

  // an irs::string_ref for union inclusion without a user-defined constructor
  // and non-trivial default constructor for compatibility with MSVC 2013
  struct json_string {
    const char* data;
    size_t size;

    json_string& operator=(const irs::string_ref& ref) {
      data = ref.c_str();
      size = ref.size();
      return *this;
    }

    operator irs::string_ref() const { return irs::string_ref(data, size); }
    operator std::string() const { return std::string(data, size); }
  };

  struct json_value {
    union {
      bool b;
      int i;
      unsigned ui;
      int64_t i64;
      uint64_t ui64;
      double_t dbl;
      json_string str;
    };

    ValueType vt{ ValueType::NIL };

    json_value() noexcept { }

    bool is_bool() const noexcept {
      return ValueType::BOOL == vt;
    }

    bool is_null() const noexcept {
      return ValueType::NIL == vt;
    }

    bool is_string() const noexcept {
      return ValueType::STRING == vt;
    }

    bool is_number() const noexcept {
      return ValueType::INT == vt
        || ValueType::INT64 == vt
        || ValueType::UINT == vt
        || ValueType::UINT64 == vt
        || ValueType::DBL == vt;
    }

    template<typename T>
    T as_number() const noexcept {
      assert(is_number());

      switch (vt) {
        case ValueType::NIL:    break;
        case ValueType::BOOL:   break;
        case ValueType::INT:    return static_cast<T>(i);
        case ValueType::UINT:   return static_cast<T>(ui);
        case ValueType::INT64:  return static_cast<T>(i64);
        case ValueType::UINT64: return static_cast<T>(ui64);
        case ValueType::DBL:    return static_cast<T>(dbl);
        case ValueType::STRING: break;
        case ValueType::RAWNUM: break;
      }

      assert(false);
      return T(0.);
    }
  }; // json_value

  typedef std::function<void(
    tests::document&,
    const std::string&,
    const json_value&
  )> factory_f;

  json_doc_generator(
    const irs::utf8_path& file,
    const factory_f& factory
  );

  json_doc_generator(
    const char* data,
    const factory_f& factory
  );

  json_doc_generator(json_doc_generator&& rhs) noexcept;

  virtual const tests::document* next() override;
  virtual void reset() override;

 private:
  json_doc_generator(const json_doc_generator&) = delete;

  std::vector<document> docs_;
  std::vector<document>::const_iterator prev_;
  std::vector<document>::const_iterator next_;
}; // json_doc_generator

template<typename Indexed>
bool insert(
    irs::index_writer& writer,
    Indexed ibegin, Indexed iend) {
  auto ctx = writer.documents();
  auto doc = ctx.insert();

  return doc.insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool insert(
    irs::index_writer& writer,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto ctx = writer.documents();
  auto doc = ctx.insert();

  return doc.insert<irs::Action::INDEX>(ibegin, iend)
         && doc.insert<irs::Action::STORE>(sbegin, send);
}

template<typename Indexed, typename Stored, typename Sorted>
bool insert(
    irs::index_writer& writer,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send,
    Sorted sorted = nullptr) {
  auto ctx = writer.documents();
  auto doc = ctx.insert();

  if (sorted && !doc.insert<irs::Action::STORE_SORTED>(*sorted)) {
    return false;
  }

  return doc.insert<irs::Action::INDEX>(ibegin, iend)
         && doc.insert<irs::Action::STORE>(sbegin, send);
}

template<typename Indexed>
bool update(
    irs::index_writer& writer,
    const irs::filter& filter,
    Indexed ibegin, Indexed iend) {
  auto ctx = writer.documents();
  auto doc = ctx.replace(filter);

  return doc.insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool update(
    irs::index_writer& writer,
    const irs::filter& filter,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto ctx = writer.documents();
  auto doc = ctx.replace(filter);

  return doc.insert<irs::Action::INDEX>(ibegin, iend)
         && doc.insert<irs::Action::STORE>(sbegin, send);
}

template<typename Indexed>
bool update(
    irs::index_writer& writer,
    irs::filter::ptr&& filter,
    Indexed ibegin, Indexed iend) {
  auto ctx = writer.documents();
  auto doc = ctx.replace(std::move(filter));

  return doc.insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool update(
    irs::index_writer& writer,
    irs::filter::ptr&& filter,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto ctx = writer.documents();
  auto doc = ctx.replace(std::move(filter));

  return doc.insert<irs::Action::INDEX>(ibegin, iend)
         && doc.insert<irs::Action::STORE>(sbegin, send);
}

template<typename Indexed>
bool update(
    irs::index_writer& writer,
    const std::shared_ptr<irs::filter>& filter,
    Indexed ibegin, Indexed iend) {
  auto ctx = writer.documents();
  auto doc = ctx.replace(filter);

  return doc.insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool update(
    irs::index_writer& writer,
    const std::shared_ptr<irs::filter>& filter,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto ctx = writer.documents();
  auto doc = ctx.replace(filter);

  return doc.insert<irs::Action::INDEX>(ibegin, iend)
         && doc.insert<irs::Action::STORE>(sbegin, send);
}

} // tests

#endif
