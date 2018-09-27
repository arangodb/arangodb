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

#include "iterators.hpp"
#include "field_meta.hpp"
#include "formats/formats.hpp"
#include "search/cost.hpp"
#include "utils/type_limits.hpp"
#include "utils/singleton.hpp"

NS_ROOT
NS_LOCAL

irs::cost empty_cost() NOEXCEPT {
  irs::cost cost;
  cost.value(0);
  return cost;
}

irs::attribute_view empty_doc_iterator_attributes() {
  static irs::cost cost = empty_cost();

  irs::attribute_view attrs(1); // cost
  attrs.emplace(cost);

  return attrs;
}

//////////////////////////////////////////////////////////////////////////////
/// @class empty_doc_iterator
/// @brief represents an iterator with no documents 
//////////////////////////////////////////////////////////////////////////////
struct empty_doc_iterator : doc_iterator {
  virtual doc_id_t value() const override { return type_limits<type_t::doc_id_t>::eof(); }
  virtual bool next() override { return false; }
  virtual doc_id_t seek(doc_id_t) override { return type_limits<type_t::doc_id_t>::eof(); }
  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    static irs::attribute_view empty = empty_doc_iterator_attributes();
    return empty;
  }
};

//////////////////////////////////////////////////////////////////////////////
/// @class empty_term_iterator
/// @brief represents an iterator without terms
//////////////////////////////////////////////////////////////////////////////
struct empty_term_iterator : term_iterator {
  virtual const bytes_ref& value() const override { return bytes_ref::NIL; }
  virtual doc_iterator::ptr postings(const flags&) const override {
    return doc_iterator::empty();
  }
  virtual void read() override { }
  virtual bool next() override { return false; }
  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return irs::attribute_view::empty_instance();
  }
};

struct empty_term_reader : singleton<empty_term_reader>, term_reader {
  virtual iresearch::seek_term_iterator::ptr iterator() const override { return nullptr; }
  virtual const iresearch::field_meta& meta() const override { 
    return irs::field_meta::EMPTY;
  }

  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return irs::attribute_view::empty_instance();
  }

  // total number of terms
  virtual size_t size() const override { return 0; }

  // total number of documents
  virtual uint64_t docs_count() const override { return 0; }

  // less significant term
  virtual const iresearch::bytes_ref& (min)() const override { 
    return iresearch::bytes_ref::NIL; 
  }

  // most significant term
  virtual const iresearch::bytes_ref& (max)() const override { 
    return iresearch::bytes_ref::NIL; 
  }
}; // empty_term_reader

struct empty_field_iterator : field_iterator {
  virtual const term_reader& value() const override {
    return empty_term_reader::instance();
  }

  virtual bool seek(const string_ref&) override {
    return false;
  }

  virtual bool next() override {
    return false;
  }
};

struct empty_column_iterator : column_iterator {
  virtual const column_meta& value() const override {
    static column_meta empty;
    return empty;
  }

  virtual bool seek(const string_ref&) override {
    return false;
  }

  virtual bool next() override {
    return false;
  }
};
 
NS_END // LOCAL

// ----------------------------------------------------------------------------
// --SECTION--                                              basic_term_iterator
// ----------------------------------------------------------------------------

term_iterator::ptr term_iterator::empty() {
  static empty_term_iterator instance;

  return memory::make_managed<irs::term_iterator, false>(&instance);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                seek_doc_iterator 
// ----------------------------------------------------------------------------

doc_iterator::ptr doc_iterator::empty() {
  static doc_iterator::ptr instance = std::make_shared<empty_doc_iterator>();

  return instance;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                   field_iterator 
// ----------------------------------------------------------------------------

field_iterator::ptr field_iterator::empty() {
  static empty_field_iterator instance;

  return memory::make_managed<irs::field_iterator, false>(&instance);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                  column_iterator 
// ----------------------------------------------------------------------------

column_iterator::ptr column_iterator::empty() {
  static empty_column_iterator instance;

  return memory::make_managed<irs::column_iterator, false>(&instance);
}

NS_END // ROOT 
