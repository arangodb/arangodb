//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_DOCUMENT_GENERATOR_H
#define IRESEARCH_DOCUMENT_GENERATOR_H

#include "analysis/token_streams.hpp"
#include "utils/iterator.hpp"
#include "store/store_utils.hpp"
#include "index/index_writer.hpp"

#include <fstream>
#include <atomic>
#include <functional>

#include <boost/filesystem.hpp>

namespace iresearch {

struct data_output;
class flags;
class token_stream;

}

namespace tests {

namespace fs = ::boost::filesystem;
namespace ir = iresearch;

//////////////////////////////////////////////////////////////////////////////
/// @class ifield
/// @brief base interface for all fields
//////////////////////////////////////////////////////////////////////////////
struct ifield {
  DECLARE_SPTR(ifield);
  virtual ~ifield() {};

  virtual ir::string_ref name() const = 0;
  virtual float_t boost() const = 0;
  virtual bool write(ir::data_output& out) const = 0;
  virtual ir::token_stream& get_tokens() const = 0;
  virtual const ir::flags& features() const = 0;
}; // ifield

//////////////////////////////////////////////////////////////////////////////
/// @class field 
/// @brief base class for field implementations
//////////////////////////////////////////////////////////////////////////////
class field_base : public ifield {
 public:
  field_base() = default;
  virtual ~field_base();

  field_base(field_base&& rhs) NOEXCEPT;
  field_base& operator=(field_base&& rhs) NOEXCEPT;

  field_base(const field_base&) = default;
  field_base& operator=(const field_base&) = default;

  ir::string_ref name() const { return name_; }
  void name(std::string&& name) { name_ = std::move(name); }
  void name(const std::string& name) { name_ = name; }

  void boost(float_t value) { boost_ = value; }
  float_t boost() const { return boost_; }

  const ir::flags& features() const { return features_; };
  ir::flags& features() { return features_; }

 private:
  iresearch::flags features_;
  std::string name_;
  float_t boost_{ 1.f };
}; // field_base

//////////////////////////////////////////////////////////////////////////////
/// @class long_field 
/// @brief provides capabilities for storing & indexing int64_t values 
//////////////////////////////////////////////////////////////////////////////
class long_field: public field_base {
 public:
  typedef int64_t value_t;

  long_field() = default;

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(ir::data_output& out) const override;
  ir::token_stream& get_tokens() const;

 private:
  mutable ir::numeric_token_stream stream_;
  int64_t value_{};
}; // long_field 

//////////////////////////////////////////////////////////////////////////////
/// @class long_field 
/// @brief provides capabilities for storing & indexing int32_t values 
//////////////////////////////////////////////////////////////////////////////
class int_field: public field_base {
 public:
  typedef int32_t value_t;

  int_field() = default;
  int_field(int_field&& other) NOEXCEPT
    : field_base(std::move(other)),
      stream_(std::move(other.stream_)),
      value_(std::move(other.value_)) {
  }

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }

  bool write(ir::data_output& out) const override;
  ir::token_stream& get_tokens() const;

 private:
  mutable ir::numeric_token_stream stream_;
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

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }

  bool write(ir::data_output& out) const override;
  ir::token_stream& get_tokens() const;

 private:
  mutable ir::numeric_token_stream stream_;
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

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }

  bool write(ir::data_output& out) const override;
  ir::token_stream& get_tokens() const;

 private:
  mutable ir::numeric_token_stream stream_;
  float_t value_{};
}; // float_field 

//////////////////////////////////////////////////////////////////////////////
/// @class binary_field 
/// @brief provides capabilities for storing & indexing binary values 
//////////////////////////////////////////////////////////////////////////////
class binary_field: public field_base {
 public:
  binary_field() = default;

  const ir::bstring& value() const { return value_; }
  void value(const ir::bytes_ref& value) { value_ = value; }
  void value(ir::bstring&& value) { value_ = std::move(value); }

  template<typename Iterator>
  void value(Iterator first, Iterator last) {
    value_ = bytes(first, last);
  }
  
  bool write(ir::data_output& out) const override;
  ir::token_stream& get_tokens() const;

 private:
  mutable ir::string_token_stream stream_;
  ir::bstring value_;
}; // binary_field

/* -------------------------------------------------------------------
* document 
* ------------------------------------------------------------------*/

class particle : ir::util::noncopyable {
 public:
  typedef std::vector<ifield::ptr> fields_t;
  typedef ir::ptr_iterator<fields_t::const_iterator> const_iterator;
  typedef ir::ptr_iterator<fields_t::iterator> iterator;

  particle() = default;
  particle(particle&& rhs) NOEXCEPT;
  particle& operator=(particle&& rhs) NOEXCEPT;
  virtual ~particle();

  size_t size() const { return fields_.size(); }
  void clear() { fields_.clear(); }
  void reserve(size_t size) { fields_.reserve(size); }

  void push_back(const ifield::ptr& fld) { fields_.emplace_back(fld); }
  bool contains(const ir::string_ref& name) const;
  ifield* get(const ir::string_ref& name) const;
  std::vector<const ifield*> find(const ir::string_ref& name) const;
  void remove(const ir::string_ref& name);

  ifield& back() const { return *fields_.back(); }

  template<typename T>
  T& back() const {
    typedef typename std::enable_if<
      std::is_base_of<tests::ifield, T>::value, T
     >::type type;
      
    return static_cast<type&>(*fields_.back());
  }
  
  template<typename T>
  T& get(size_t i) const {
    typedef typename std::enable_if<
      std::is_base_of<tests::ifield, T>::value, T
     >::type type;
      
    return static_cast<type&>(*fields_[i]);
  }

  template<typename T>
  T* get(const ir::string_ref& name) const {
    typedef typename std::enable_if<
      std::is_base_of<tests::ifield, T>::value, T
     >::type type;

    return static_cast<type*>(get(name));
  }

  iterator begin() { return iterator(fields_.begin()); }
  iterator end() { return iterator(fields_.end()); }

  const_iterator begin() const { return const_iterator(fields_.begin()); }
  const_iterator end() const { return const_iterator(fields_.end()); }

 protected:
  fields_t fields_;
}; // particle 

struct document : ir::util::noncopyable {
  document() = default;
  document(document&& rhs) NOEXCEPT;

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
}; // document

/* -------------------------------------------------------------------
* GENERATORS 
* ------------------------------------------------------------------*/

/* Base class for document generators */
struct doc_generator_base {
  DECLARE_PTR( doc_generator_base );
  DECLARE_FACTORY( doc_generator_base );

  virtual const tests::document* next() = 0;
  virtual void reset() = 0;

  virtual ~doc_generator_base() { }
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
    const fs::path& file, 
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

  struct json_value {
    union {
      bool b;
      int i;
      unsigned ui;
      int64_t i64;
      uint64_t ui64;
      double_t dbl;
      irs::string_ref str{};
    };
    ValueType vt{ ValueType::NIL };

    json_value() NOEXCEPT { }

    bool is_bool() const NOEXCEPT {
      return ValueType::BOOL == vt;
    }

    bool is_null() const NOEXCEPT {
      return ValueType::NIL == vt;
    }

    bool is_string() const NOEXCEPT {
      return ValueType::STRING == vt;
    }

    bool is_number() const NOEXCEPT {
      return ValueType::INT == vt
        || ValueType::INT64 == vt
        || ValueType::UINT == vt
        || ValueType::UINT64 == vt
        || ValueType::DBL == vt;
    }

    template<typename T>
    T as_number() const NOEXCEPT {
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
      return 0.;
    }
  }; // json_value

  typedef std::function<void(
    tests::document&,
    const std::string&,
    const json_value&
  )> factory_f;

  json_doc_generator(
    const fs::path& file,
    const factory_f& factory
  );

  json_doc_generator(json_doc_generator&& rhs) NOEXCEPT;

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
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    return false; // break the loop
  };

  return writer.insert(inserter);
}

template<typename Indexed, typename Stored>
bool insert(
    irs::index_writer& writer,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    doc.insert<irs::Action::STORE>(sbegin, send);
    return false; // break the loop
  };

  return writer.insert(inserter);
}

template<typename Indexed>
bool update(
    irs::index_writer& writer,
    const irs::filter& filter,
    Indexed ibegin, Indexed iend) {
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    return false; // break the loop
  };

  return writer.update(filter, inserter);
}

template<typename Indexed, typename Stored>
bool update(
    irs::index_writer& writer,
    const irs::filter& filter,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    doc.insert<irs::Action::STORE>(sbegin, send);
    return false; // break the loop
  };

  return writer.update(filter, inserter);
}

template<typename Indexed>
bool update(
    irs::index_writer& writer,
    irs::filter::ptr&& filter,
    Indexed ibegin, Indexed iend) {
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    return false; // break the loop
  };

  return writer.update(std::move(filter), inserter);
}

template<typename Indexed, typename Stored>
bool update(
    irs::index_writer& writer,
    irs::filter::ptr&& filter,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    doc.insert<irs::Action::STORE>(sbegin, send);
    return false; // break the loop
  };

  return writer.update(std::move(filter), inserter);
}

template<typename Indexed>
bool update(
    irs::index_writer& writer,
    const std::shared_ptr<irs::filter>& filter,
    Indexed ibegin, Indexed iend) {
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    return false; // break the loop
  };

  return writer.update(filter, inserter);
}

template<typename Indexed, typename Stored>
bool update(
    irs::index_writer& writer,
    const std::shared_ptr<irs::filter>& filter,
    Indexed ibegin, Indexed iend,
    Stored sbegin, Stored send) {
  auto inserter = [&](irs::index_writer::document& doc) {
    doc.insert<irs::Action::INDEX>(ibegin, iend);
    doc.insert<irs::Action::STORE>(sbegin, send);
    return false; // break the loop
  };

  return writer.update(filter, inserter);
}

} // tests

#endif
