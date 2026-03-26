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

#pragma once

#include <atomic>
#include <boost/iterator/iterator_facade.hpp>
#include <filesystem>
#include <fstream>
#include <functional>

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "index/index_writer.hpp"
#include "store/store_utils.hpp"
#include "utils/iterator.hpp"

namespace irs {

struct data_output;
class token_stream;

}  // namespace irs

namespace tests {

//////////////////////////////////////////////////////////////////////////////
/// @class ifield
/// @brief base interface for all fields
//////////////////////////////////////////////////////////////////////////////
struct ifield {
  using ptr = std::shared_ptr<ifield>;
  virtual ~ifield() = default;

  virtual irs::features_t features() const = 0;
  virtual irs::IndexFeatures index_features() const = 0;
  virtual irs::token_stream& get_tokens() const = 0;
  virtual std::string_view name() const = 0;
  virtual bool write(irs::data_output& out) const = 0;
};

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

  irs::features_t features() const noexcept final {
    return {features_.data(), features_.size()};
  }

  irs::IndexFeatures index_features() const noexcept final {
    return index_features_;
  }

  std::string_view name() const noexcept final { return name_; }

  void name(std::string name) noexcept { name_ = std::move(name); }

  std::vector<irs::type_info::type_id> features_;
  std::string name_;
  irs::IndexFeatures index_features_{irs::IndexFeatures::NONE};
};

//////////////////////////////////////////////////////////////////////////////
/// @class long_field
/// @brief provides capabilities for storing & indexing int64_t values
//////////////////////////////////////////////////////////////////////////////
class long_field : public field_base {
 public:
  typedef int64_t value_t;

  long_field() = default;

  irs::token_stream& get_tokens() const final;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const final;

 private:
  mutable irs::numeric_token_stream stream_;
  int64_t value_{};
};

//////////////////////////////////////////////////////////////////////////////
/// @class long_field
/// @brief provides capabilities for storing & indexing int32_t values
//////////////////////////////////////////////////////////////////////////////
class int_field : public field_base {
 public:
  typedef int32_t value_t;

  explicit int_field(std::string_view name = "", int32_t value = 0,
                     irs::type_info::type_id extra_feature = {})
    : stream_(std::make_shared<irs::numeric_token_stream>()), value_{value} {
    this->name(std::string{name});
    if (extra_feature) {
      this->features_.emplace_back(extra_feature);
    }
  }
  int_field(int_field&& other) = default;

  irs::token_stream& get_tokens() const final;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const final;

 private:
  mutable std::shared_ptr<irs::numeric_token_stream> stream_;
  int32_t value_{};
};

//////////////////////////////////////////////////////////////////////////////
/// @class long_field
/// @brief provides capabilities for storing & indexing double_t values
//////////////////////////////////////////////////////////////////////////////
class double_field : public field_base {
 public:
  typedef double_t value_t;

  double_field() = default;

  irs::token_stream& get_tokens() const final;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const final;

 private:
  mutable irs::numeric_token_stream stream_;
  double_t value_{};
};

//////////////////////////////////////////////////////////////////////////////
/// @class long_field
/// @brief provides capabilities for storing & indexing double_t values
//////////////////////////////////////////////////////////////////////////////
class float_field : public field_base {
 public:
  typedef float_t value_t;

  float_field() = default;

  irs::token_stream& get_tokens() const final;
  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }
  bool write(irs::data_output& out) const final;

 private:
  mutable irs::numeric_token_stream stream_;
  float_t value_{};
};

//////////////////////////////////////////////////////////////////////////////
/// @class binary_field
/// @brief provides capabilities for storing & indexing binary values
//////////////////////////////////////////////////////////////////////////////
class binary_field : public field_base {
 public:
  binary_field() = default;

  irs::token_stream& get_tokens() const final;
  const irs::bstring& value() const { return value_; }
  void value(irs::bytes_view value) { value_ = value; }
  void value(irs::bstring&& value) { value_ = std::move(value); }

  template<typename Iterator>
  void value(Iterator first, Iterator last) {
    value_ = bytes(first, last);
  }

  bool write(irs::data_output& out) const final;

 private:
  mutable irs::string_token_stream stream_;
  irs::bstring value_;
};

namespace detail {

template<typename Ptr>
struct extract_element_type {
  using value_type = typename Ptr::element_type;
  using reference = typename Ptr::element_type&;
  using pointer = typename Ptr::element_type*;
};

template<typename Ptr>
struct extract_element_type<const Ptr> {
  using value_type = const typename Ptr::element_type;
  using reference = const typename Ptr::element_type&;
  using pointer = const typename Ptr::element_type*;
};

template<typename Ptr>
struct extract_element_type<Ptr*> {
  using value_type = Ptr;
  using reference = Ptr&;
  using pointer = Ptr*;
};

}  // namespace detail

//////////////////////////////////////////////////////////////////////////////
/// @class const_ptr_iterator
/// @brief iterator adapter for containers with the smart pointers
//////////////////////////////////////////////////////////////////////////////
template<typename IteratorImpl>
class ptr_iterator
  : public ::boost::iterator_facade<
      ptr_iterator<IteratorImpl>,
      typename detail::extract_element_type<
        std::remove_reference_t<typename IteratorImpl::reference>>::value_type,
      ::boost::random_access_traversal_tag> {
 private:
  using element_type = detail::extract_element_type<
    std::remove_reference_t<typename IteratorImpl::reference>>;

  using base_element_type = typename element_type::value_type;

  using base =
    ::boost::iterator_facade<ptr_iterator<IteratorImpl>, base_element_type,
                             ::boost::random_access_traversal_tag>;

  using iterator_facade = typename base::iterator_facade_;

  template<typename T>
  struct adjust_const : irs::irstd::adjust_const<base_element_type, T> {};

 public:
  using reference = typename iterator_facade::reference;
  using difference_type = typename iterator_facade::difference_type;

  ptr_iterator(const IteratorImpl& it) : it_(it) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns downcasted reference to the iterator's value
  //////////////////////////////////////////////////////////////////////////////
  template<typename T>
  typename adjust_const<T>::reference as() const {
    static_assert(std::is_base_of_v<base_element_type, T>);
    return irs::DownCast<T>(dereference());
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns downcasted pointer to the iterator's value
  ///        or nullptr if there is no available conversion
  //////////////////////////////////////////////////////////////////////////////
  template<typename T>
  typename adjust_const<T>::pointer safe_as() const {
    static_assert(std::is_base_of_v<base_element_type, T>);
    reference it = dereference();
    return dynamic_cast<typename adjust_const<T>::pointer>(&it);
  }

  bool is_null() const noexcept { return *it_ == nullptr; }

 private:
  friend class ::boost::iterator_core_access;

  reference dereference() const {
    IRS_ASSERT(*it_);
    return **it_;
  }
  void advance(difference_type n) { it_ += n; }
  difference_type distance_to(const ptr_iterator& rhs) const {
    return rhs.it_ - it_;
  }
  void increment() { ++it_; }
  void decrement() { --it_; }
  bool equal(const ptr_iterator& rhs) const { return it_ == rhs.it_; }

  IteratorImpl it_;
};

/* -------------------------------------------------------------------
 * document
 * ------------------------------------------------------------------*/

class particle : irs::util::noncopyable {
 public:
  typedef std::vector<ifield::ptr> fields_t;
  typedef ptr_iterator<fields_t::const_iterator> const_iterator;
  typedef ptr_iterator<fields_t::iterator> iterator;

  particle() = default;
  particle(particle&& rhs) noexcept;
  particle& operator=(particle&& rhs) noexcept;
  virtual ~particle() = default;

  size_t size() const { return fields_.size(); }
  void clear() { fields_.clear(); }
  void reserve(size_t size) { fields_.reserve(size); }
  void push_back(const ifield::ptr& fld) { fields_.emplace_back(fld); }

  ifield& back() const { return *fields_.back(); }
  bool contains(const std::string_view& name) const;
  std::vector<ifield::ptr> find(const std::string_view& name) const;

  template<typename T>
  T& back() const {
    typedef
      typename std::enable_if<std::is_base_of<tests::ifield, T>::value, T>::type
        type;

    return static_cast<type&>(*fields_.back());
  }

  ifield* get(const std::string_view& name) const;

  template<typename T>
  T& get(size_t i) const {
    typedef
      typename std::enable_if<std::is_base_of<tests::ifield, T>::value, T>::type
        type;

    return static_cast<type&>(*fields_[i]);
  }

  template<typename T>
  T* get(const std::string_view& name) const {
    typedef
      typename std::enable_if<std::is_base_of<tests::ifield, T>::value, T>::type
        type;

    return static_cast<type*>(get(name));
  }

  void remove(const std::string_view& name);

  iterator begin() { return iterator(fields_.begin()); }
  iterator end() { return iterator(fields_.end()); }

  const_iterator begin() const { return const_iterator(fields_.begin()); }
  const_iterator end() const { return const_iterator(fields_.end()); }

 protected:
  fields_t fields_;
};

struct document : irs::util::noncopyable {
  document() = default;
  document(document&& rhs) noexcept;
  virtual ~document() = default;

  void insert(const ifield::ptr& field, bool indexed = true,
              bool stored = true) {
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
};

struct doc_generator_base {
  using ptr = std::unique_ptr<doc_generator_base>;

  virtual ~doc_generator_base() = default;

  virtual const tests::document* next() = 0;
  virtual void reset() = 0;
};

class limiting_doc_generator : public doc_generator_base {
 public:
  limiting_doc_generator(doc_generator_base& gen, size_t offset, size_t limit)
    : gen_(&gen), begin_(offset), end_(offset + limit) {}

  const tests::document* next() final {
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

  void reset() final {
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
  };

  delim_doc_generator(const std::filesystem::path& file, doc_template& doc,
                      uint32_t delim = 0x0009);

  const tests::document* next() final;
  void reset() final;

 private:
  std::string str_;
  std::ifstream ifs_;
  doc_template* doc_;
  uint32_t delim_;
};

// Generates documents from a CSV file
class csv_doc_generator : public doc_generator_base {
 public:
  struct doc_template : document {
    virtual void init() = 0;
    virtual void value(size_t idx, const std::string_view& value) = 0;
    virtual void end() {}
    virtual void reset() {}
  };

  csv_doc_generator(const std::filesystem::path& file, doc_template& doc);
  const tests::document* next() final;
  void reset() final;
  bool skip();  // skip a single document, return if anything was skiped, false
                // == EOF

 private:
  doc_template& doc_;
  std::ifstream ifs_;
  std::string line_;
  irs::analysis::analyzer::ptr stream_;
};

/* Generates documents from json file based on type of JSON value */
class json_doc_generator : public doc_generator_base {
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
  };

  // an std::string_view for union inclusion without a user-defined constructor
  // and non-trivial default constructor for compatibility with MSVC 2013
  struct json_string {
    const char* data;
    size_t size;

    json_string& operator=(std::string_view ref) {
      data = ref.data();
      size = ref.size();
      return *this;
    }

    operator std::string_view() const { return std::string_view(data, size); }
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

    ValueType vt{ValueType::NIL};

    json_value() noexcept {}

    bool is_bool() const noexcept { return ValueType::BOOL == vt; }

    bool is_null() const noexcept { return ValueType::NIL == vt; }

    bool is_string() const noexcept { return ValueType::STRING == vt; }

    bool is_number() const noexcept {
      return ValueType::INT == vt || ValueType::INT64 == vt ||
             ValueType::UINT == vt || ValueType::UINT64 == vt ||
             ValueType::DBL == vt;
    }

    template<typename T>
    T as_number() const noexcept {
      IRS_ASSERT(is_number());

      switch (vt) {
        case ValueType::NIL:
          break;
        case ValueType::BOOL:
          break;
        case ValueType::INT:
          return static_cast<T>(i);
        case ValueType::UINT:
          return static_cast<T>(ui);
        case ValueType::INT64:
          return static_cast<T>(i64);
        case ValueType::UINT64:
          return static_cast<T>(ui64);
        case ValueType::DBL:
          return static_cast<T>(dbl);
        case ValueType::STRING:
          break;
        case ValueType::RAWNUM:
          break;
      }

      IRS_ASSERT(false);
      return T(0.);
    }
  };

  typedef std::function<void(tests::document&, const std::string&,
                             const json_value&)>
    factory_f;

  json_doc_generator(const std::filesystem::path& file,
                     const factory_f& factory);

  json_doc_generator(const char* data, const factory_f& factory);

  json_doc_generator(json_doc_generator&& rhs) noexcept;

  const tests::document* next() final;
  void reset() final;

 private:
  json_doc_generator(const json_doc_generator&) = delete;

  std::vector<document> docs_;
  std::vector<document>::const_iterator prev_;
  std::vector<document>::const_iterator next_;
};

// stream wrapper which sets payload equal to term value
class token_stream_payload final : public irs::token_stream {
 public:
  explicit token_stream_payload(irs::token_stream* impl);
  bool next();

  virtual irs::attribute* get_mutable(irs::type_info::type_id type);

 private:
  const irs::term_attribute* term_;
  irs::payload pay_;
  irs::token_stream* impl_;
};

// field which uses text analyzer for tokenization and stemming
template<typename T>
class text_field : public tests::field_base {
 public:
  text_field(const std::string& name, bool payload = false,
             std::vector<irs::type_info::type_id> features = {})
    : token_stream_(irs::analysis::analyzers::get(
        "text", irs::type<irs::text_format::json>::get(),
        "{\"locale\":\"C\", \"stopwords\":[]}")) {
    if (payload) {
      if (!token_stream_->reset(value_)) {
        throw irs::illegal_state{"Failed to reset stream."};
      }
      pay_stream_.reset(new token_stream_payload(token_stream_.get()));
    }
    this->name(name);
    this->features_ = std::move(features);
    index_features_ = irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
                      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;
  }

  text_field(const std::string& name, const T& value, bool payload = false,
             std::vector<irs::type_info::type_id> features = {})
    : token_stream_(irs::analysis::analyzers::get(
        "text", irs::type<irs::text_format::json>::get(),
        "{\"locale\":\"C\", \"stopwords\":[]}")),
      value_(value) {
    if (payload) {
      if (!token_stream_->reset(value_)) {
        throw irs::illegal_state{"Failed to reset stream."};
      }
      pay_stream_.reset(new token_stream_payload(token_stream_.get()));
    }
    this->name(name);
    this->features_ = std::move(features);
    index_features_ = irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
                      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;
  }

  text_field(text_field&& other) = default;

  std::string_view value() const { return value_; }
  void value(const T& value) { value_ = value; }
  void value(T&& value) { value_ = std::move(value); }

  irs::token_stream& get_tokens() const {
    token_stream_->reset(value_);

    return pay_stream_ ? static_cast<irs::token_stream&>(*pay_stream_)
                       : *token_stream_;
  }

 private:
  virtual bool write(irs::data_output&) const { return false; }

  std::unique_ptr<token_stream_payload> pay_stream_;
  irs::analysis::analyzer::ptr token_stream_;
  T value_;
};

// field which uses simple analyzer without tokenization
class string_field : public tests::field_base {
 public:
  string_field(
    std::string_view name,
    irs::IndexFeatures extra_index_features = irs::IndexFeatures::NONE,
    const std::vector<irs::type_info::type_id>& extra_features = {});
  string_field(
    std::string_view name, std::string_view value,
    irs::IndexFeatures extra_index_features = irs::IndexFeatures::NONE,
    const std::vector<irs::type_info::type_id>& extra_features = {});

  void value(std::string_view str);
  std::string_view value() const { return value_; }

  irs::token_stream& get_tokens() const final;
  bool write(irs::data_output& out) const final;

 private:
  mutable irs::string_token_stream stream_;
  std::string value_;
};

// field which uses simple analyzer without tokenization
class string_view_field : public tests::field_base {
 public:
  string_view_field(
    const std::string& name,
    irs::IndexFeatures extra_index_features = irs::IndexFeatures::NONE,
    const std::vector<irs::type_info::type_id>& extra_features = {});
  string_view_field(
    const std::string& name, const std::string_view& value,
    irs::IndexFeatures index_features = irs::IndexFeatures::NONE,
    const std::vector<irs::type_info::type_id>& extra_features = {});

  void value(std::string_view str);
  std::string_view value() const { return value_; }

  irs::token_stream& get_tokens() const final;
  bool write(irs::data_output& out) const final;

 private:
  mutable irs::string_token_stream stream_;
  std::string_view value_;
};

// document template for europarl.subset.text
class europarl_doc_template : public delim_doc_generator::doc_template {
 public:
  typedef text_field<std::string_view> text_ref_field;

  virtual void init();
  virtual void value(size_t idx, const std::string& value);
  virtual void end();
  virtual void reset();

 private:
  std::string title_;  // current title
  std::string body_;   // current body
  irs::doc_id_t idval_ = 0;
};

void generic_json_field_factory(tests::document& doc, const std::string& name,
                                const json_doc_generator::json_value& data);

void payloaded_json_field_factory(tests::document& doc, const std::string& name,
                                  const json_doc_generator::json_value& data);

void normalized_string_json_field_factory(
  tests::document& doc, const std::string& name,
  const json_doc_generator::json_value& data);

void norm2_string_json_field_factory(
  tests::document& doc, const std::string& name,
  const json_doc_generator::json_value& data);

template<typename Indexed>
bool insert(irs::IndexWriter& writer, Indexed ibegin, Indexed iend) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Insert();

  return doc.Insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool insert(irs::IndexWriter& writer, Indexed ibegin, Indexed iend,
            Stored sbegin, Stored send) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Insert();

  return doc.Insert<irs::Action::INDEX>(ibegin, iend) &&
         doc.Insert<irs::Action::STORE>(sbegin, send);
}

template<typename Indexed, typename Stored, typename Sorted>
bool insert(irs::IndexWriter& writer, Indexed ibegin, Indexed iend,
            Stored sbegin, Stored send, Sorted sorted = nullptr) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Insert();

  if (sorted && !doc.Insert<irs::Action::STORE_SORTED>(*sorted)) {
    return false;
  }

  return doc.Insert<irs::Action::INDEX>(ibegin, iend) &&
         doc.Insert<irs::Action::STORE>(sbegin, send);
}

template<typename Doc>
bool insert(irs::IndexWriter& writer, const Doc& doc, size_t count = 1,
            bool has_sort = false) {
  for (; count; --count) {
    if (!insert(writer, std::begin(doc.indexed), std::end(doc.indexed),
                std::begin(doc.stored), std::end(doc.stored),
                has_sort ? doc.sorted.get() : nullptr)) {
      return false;
    }
  }
  return true;
}

template<typename Indexed>
bool update(irs::IndexWriter& writer, const irs::filter& filter, Indexed ibegin,
            Indexed iend) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Replace(filter);

  return doc.Insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool update(irs::IndexWriter& writer, const irs::filter& filter, Indexed ibegin,
            Indexed iend, Stored sbegin, Stored send) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Replace(filter);

  return doc.Insert<irs::Action::INDEX>(ibegin, iend) &&
         doc.Insert<irs::Action::STORE>(sbegin, send);
}

template<typename Indexed>
bool update(irs::IndexWriter& writer, irs::filter::ptr&& filter, Indexed ibegin,
            Indexed iend) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Replace(std::move(filter));

  return doc.Insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool update(irs::IndexWriter& writer, irs::filter::ptr&& filter, Indexed ibegin,
            Indexed iend, Stored sbegin, Stored send) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Replace(std::move(filter));

  return doc.Insert<irs::Action::INDEX>(ibegin, iend) &&
         doc.Insert<irs::Action::STORE>(sbegin, send);
}

template<typename Indexed>
bool update(irs::IndexWriter& writer,
            const std::shared_ptr<irs::filter>& filter, Indexed ibegin,
            Indexed iend) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Replace(filter);

  return doc.Insert<irs::Action::INDEX>(ibegin, iend);
}

template<typename Indexed, typename Stored>
bool update(irs::IndexWriter& writer,
            const std::shared_ptr<irs::filter>& filter, Indexed ibegin,
            Indexed iend, Stored sbegin, Stored send) {
  auto ctx = writer.GetBatch();
  auto doc = ctx.Replace(filter);

  return doc.Insert<irs::Action::INDEX>(ibegin, iend) &&
         doc.Insert<irs::Action::STORE>(sbegin, send);
}

}  // namespace tests
