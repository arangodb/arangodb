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

#ifndef IRESEARCH_TOKEN_ATTRIBUTES_H
#define IRESEARCH_TOKEN_ATTRIBUTES_H

#include "store/data_input.hpp"

#include "index/index_reader.hpp"
#include "index/iterators.hpp"

#include "utils/attributes.hpp"
#include "utils/string.hpp"
#include "utils/type_limits.hpp"
#include "utils/iterator.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class offset 
/// @brief represents token offset in a stream 
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API offset : attribute {
  static const uint32_t INVALID_OFFSET = integer_traits< uint32_t >::const_max;

  DECLARE_ATTRIBUTE_TYPE();   

  void clear() {
    start = 0;
    end = 0;
  }

  uint32_t start{ 0 };
  uint32_t end{ 0 };
};

//////////////////////////////////////////////////////////////////////////////
/// @class offset 
/// @brief represents token increment in a stream 
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API increment : basic_attribute<uint32_t> {
  DECLARE_ATTRIBUTE_TYPE();

  increment() NOEXCEPT;

  void clear() { value = 1U; }
};

//////////////////////////////////////////////////////////////////////////////
/// @class term_attribute 
/// @brief represents term value in a stream 
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API term_attribute : attribute {
  DECLARE_ATTRIBUTE_TYPE();

  term_attribute() NOEXCEPT;

  const bytes_ref& value() const {
    return value_;
  }

 protected:
  bytes_ref value_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class term_attribute 
/// @brief represents an arbitrary byte sequence associated with
///        the particular term position in a field
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API payload : basic_attribute<bytes_ref> {
  DECLARE_ATTRIBUTE_TYPE();

  void clear() {
    value = bytes_ref::nil;
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @class document 
/// @brief contains a document identifier
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API document: basic_attribute<doc_id_t> {
  DECLARE_ATTRIBUTE_TYPE();

  document() NOEXCEPT;
};

//////////////////////////////////////////////////////////////////////////////
/// @class frequency 
/// @brief how many times term appears in a document
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API frequency : basic_attribute<uint64_t> {
  DECLARE_ATTRIBUTE_TYPE();

  frequency() = default;
}; // frequency

//////////////////////////////////////////////////////////////////////////////
/// @class granularity_prefix
/// @brief indexed tokens are prefixed with one byte indicating granularity
///        this is marker attribute only used in field::features and by_range
///        exact values are prefixed with 0
///        the less precise the token the greater its granularity prefix value
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API granularity_prefix : attribute {
  DECLARE_ATTRIBUTE_TYPE();
  granularity_prefix() = default;
}; // granularity_prefix

//////////////////////////////////////////////////////////////////////////////
/// @class norm
/// @brief this is marker attribute only used in field::features in order to
///        allow evaluation of the field normalization factor 
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API norm : stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();

  FORCE_INLINE static CONSTEXPR float_t DEFAULT() {
    return 1.f;
  }

  norm() NOEXCEPT;

  bool reset(const sub_reader& segment, field_id column, const document& doc);
  float_t read() const;
  bool empty() const;

  void clear() {
    reset();
  }

 private:
  void reset();

  columnstore_reader::values_reader_f column_;
  const document* doc_;
}; // norm

//////////////////////////////////////////////////////////////////////////////
/// @class position 
/// @brief represents a term positions in document (iterator)
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API position : public attribute {
 public:
  typedef uint32_t value_t;

  class IRESEARCH_API impl : 
      public iterator<value_t>, 
      public util::const_attribute_view_provider {
   public:
    DECLARE_PTR(impl);

    impl() = default;
    explicit impl(size_t reserve_attrs);
    virtual void clear() = 0;

    const attribute_view& attributes() const NOEXCEPT {
      return attrs_;
    }

   protected:
    attribute_view attrs_;
  };

  static const uint32_t INVALID = integer_traits<value_t>::const_max;
  static const uint32_t NO_MORE = INVALID - 1;

  DECLARE_CREF(position);
  DECLARE_TYPE_ID(attribute::type_id);

  position() = default;

  bool next() const { return impl_->next(); }

  value_t seek(value_t target) const {
    struct skewed_comparer: std::less<value_t> {
      bool operator()(value_t lhs, value_t rhs) {
        typedef std::less<value_t> Pred;
        return Pred::operator()(1 + lhs, 1 + rhs);
      }
    };

    typedef skewed_comparer pos_less;
    iresearch::seek(*impl_, target, pos_less());
    return impl_->value();
  }

  value_t value() const { return impl_->value(); }

  void clear() {
    assert(impl_);
    impl_->clear();
  }

  void reset(impl::ptr&& impl = nullptr) NOEXCEPT {
    impl_ = std::move(impl);
  }

  explicit operator bool() const NOEXCEPT {
    return impl_.get();
  }

  impl* get() NOEXCEPT {
    return impl_.get();
  }

  const impl* get() const NOEXCEPT {
    return impl_.get();
  }

  const attribute_view& attributes() const NOEXCEPT {
    return impl_->attributes();
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  mutable impl::ptr impl_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // position

NS_END // ROOT

#endif
