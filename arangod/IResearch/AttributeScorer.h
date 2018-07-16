////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__ATTRIBUTE_SCORER_H
#define ARANGOD_IRESEARCH__ATTRIBUTE_SCORER_H 1

#include "search/scorers.hpp"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

////////////////////////////////////////////////////////////////////////////////
/// @brief an iResearch scorer implementation based on jSON attributes of
///        ArangoDB documents
///        expects attributes: TransactionAttribute, AttributePathAttribute
////////////////////////////////////////////////////////////////////////////////
class AttributeScorer: public irs::sort {
 public:
  DECLARE_SORT_TYPE();

  // for use with irs::order::add<T>(...) and default args (static build)
  DECLARE_FACTORY_DEFAULT(
    std::vector<irs::stored_attribute::ptr>& storedAttrBuf,
    bool arangodbTypeOrder = false
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief for use with irs::order::add(...) (dynamic build)
  ///        or jSON args (static build)
  /// @param args: jSON [string enum, ...] precedence order of value types,
  ///              (string_ref::NIL == use ArangoDB defaults), any of:
  ///        array   - array attribute value in jSON document
  ///        boolean - boolean attribute value in jSON document
  ///        null    - null attribute value in jSON document
  ///        numeric - numeric attribute value in jSON document
  ///        object  - object attribute value in jSON document
  ///        string  - string attribute value in jSON document
  ///        unknown - missing or unsupported attribute value in jSON document
  ////////////////////////////////////////////////////////////////////////////////
  DECLARE_FACTORY_DEFAULT(const irs::string_ref& args);

  enum ValueType { ARRAY, BOOLEAN, NIL, NUMBER, OBJECT, STRING, UNKNOWN, eLast };

  explicit AttributeScorer();

  AttributeScorer& attributeNext(uint64_t offset);
  AttributeScorer& attributeNext(irs::string_ref const& attibute);
  AttributeScorer& orderNext(ValueType type) noexcept;
  virtual sort::prepared::ptr prepare() const override;

 private:
  struct AttributeItem {
    size_t _offset; // offset into _buf or jSON array
    size_t _size; // (std::numeric_limits<size_t>::max() -> offset into jSON array)
  };
  std::vector<AttributeItem> _attribute; // full attribute path to match
  std::string _buf;
  size_t _nextOrder;
  size_t _order[ValueType::eLast]; // type precedence order
  std::vector<irs::stored_attribute::ptr>* _storedAttrBuf; // buffer for runtime-created attributes
};

NS_END // iresearch
NS_END // arangodb

#endif
