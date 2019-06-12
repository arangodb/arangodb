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

#ifndef IRESEARCH_TOKEN_ATTRIBUTES_H
#define IRESEARCH_TOKEN_ATTRIBUTES_H

#include "store/data_input.hpp"

#include "index/index_reader.hpp"
#include "index/iterators.hpp"

#include "utils/attributes.hpp"
#include "utils/attributes_provider.hpp"
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
    value = bytes_ref::NIL;
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @class document 
/// @brief contains a document identifier
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API document: basic_attribute<doc_id_t> {
  DECLARE_ATTRIBUTE_TYPE();

  document(irs::doc_id_t doc = irs::doc_limits::invalid()) NOEXCEPT
    : basic_attribute<doc_id_t>(doc) {
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @class frequency 
/// @brief how many times term appears in a document
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API frequency : basic_attribute<uint32_t> {
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
/// @brief this marker attribute is only used in field::features in order to
///        allow evaluation of the field normalization factor 
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API norm : stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();

  FORCE_INLINE static CONSTEXPR float_t DEFAULT() {
    return 1.f;
  }

  norm() NOEXCEPT;
  norm(norm&& rhs) NOEXCEPT;
  norm& operator=(norm&& rhs) NOEXCEPT;

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
/// @brief iterator represents term positions in a document
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API position
  : public attribute, public util::const_attribute_view_provider {
 public:
  typedef uint32_t value_t;

  DECLARE_REFERENCE(position);
  DECLARE_TYPE_ID(attribute::type_id);

  const irs::attribute_view& attributes() const NOEXCEPT override { return attrs_; }
  virtual void clear() = 0;
  virtual bool next() = 0;

  value_t seek(value_t target) {
    irs::seek(*this, target);

    return value();
  }

  virtual value_t value() const = 0;

 protected:
  position(size_t reserve_attrs) NOEXCEPT;

  attribute_view attrs_;
}; // position

NS_END // ROOT

#endif
