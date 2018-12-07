//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_DOCUMENT_H
#define ARANGOD_IRESEARCH__IRESEARCH_DOCUMENT_H 1

#include "VocBase/voc-types.h"

#include "IResearchLinkMeta.h"
#include "VelocyPackHelper.h"

#include "search/filter.hpp"
#include "store/data_output.hpp"

NS_BEGIN(iresearch)

class boolean_filter; // forward declaration
struct data_output; // forward declaration
class token_stream; // forward declaration

NS_END // iresearch

NS_BEGIN(arangodb)
NS_BEGIN(aql)

struct AstNode; // forward declaration
class SortCondition; // forward declaration

NS_END // aql
NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction
NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

////////////////////////////////////////////////////////////////////////////////
/// @brief the delimiter used to separate jSON nesting levels when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
constexpr char const NESTING_LEVEL_DELIMITER = '.';

////////////////////////////////////////////////////////////////////////////////
/// @brief the prefix used to denote start of jSON list offset when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
constexpr char const NESTING_LIST_OFFSET_PREFIX = '[';

////////////////////////////////////////////////////////////////////////////////
/// @brief the suffix used to denote end of jSON list offset when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
constexpr char const NESTING_LIST_OFFSET_SUFFIX = ']';

struct IResearchViewMeta; // forward declaration
struct DocumentPrimaryKey; // forward declaration

////////////////////////////////////////////////////////////////////////////////
/// @brief indexed/stored document field adapter for IResearch
////////////////////////////////////////////////////////////////////////////////
struct Field {
  struct init_stream_t{}; // initialize stream

  static void setCidValue(Field& field, TRI_voc_cid_t const& cid);
  static void setCidValue(Field& field, TRI_voc_cid_t const& cid, init_stream_t);
  static void setPkValue(Field& field, DocumentPrimaryKey const& pk);
  static void setPkValue(Field& field, DocumentPrimaryKey const& pk, init_stream_t);

  Field() = default;
  Field(Field&& rhs);
  Field& operator=(Field&& rhs);

  irs::string_ref const& name() const noexcept {
    return _name;
  }

  irs::flags const& features() const {
    TRI_ASSERT(_features);
    return *_features;
  }

  irs::token_stream& get_tokens() const {
    TRI_ASSERT(_analyzer);
    return *_analyzer;
  }

  bool write(irs::data_output& out) const noexcept {
    if (!_value.null()) {
      out.write_bytes(_value.c_str(), _value.size());
    }

    return true;
  }

  irs::flags const* _features{ &irs::flags::empty_instance() };
  std::shared_ptr<irs::token_stream> _analyzer;
  irs::string_ref _name;
  irs::bytes_ref _value;
  ValueStorage _storeValues;
}; // Field

////////////////////////////////////////////////////////////////////////////////
/// @brief allows to iterate over the provided VPack accoring the specified
///        IResearchLinkMeta
////////////////////////////////////////////////////////////////////////////////
class FieldIterator : public std::iterator<std::forward_iterator_tag, Field const> {
 public:
  static FieldIterator const END; // unified end for all field iterators

  explicit FieldIterator();
  FieldIterator(arangodb::velocypack::Slice const& doc, IResearchLinkMeta const& linkMeta);

  Field const& operator*() const noexcept {
    return _value;
  }

  FieldIterator& operator++() {
    next();
    return *this;
  }

  // We don't support postfix increment since it requires
  // deep copy of all buffers and analyzers which is quite
  // expensive and useless

  bool valid() const noexcept {
    return !_stack.empty();
  }

  bool operator==(FieldIterator const& rhs) const noexcept {
    return _stack == rhs._stack;
  }

  bool operator!=(FieldIterator const& rhs) const noexcept {
    return !(*this == rhs);
  }

  void reset(
    arangodb::velocypack::Slice const& doc,
    IResearchLinkMeta const& linkMeta
  );

 private:
  typedef IResearchAnalyzerFeature::AnalyzerPool::ptr const* AnalyzerIterator;

  typedef bool(*Filter)(
    std::string& buffer,
    IResearchLinkMeta const*& rootMeta,
    IteratorValue const& value
  );

  struct Level {
    Level(
        arangodb::velocypack::Slice slice,
        size_t nameLength,
        IResearchLinkMeta const& meta,
        Filter filter
    ) : it(slice),
        nameLength(nameLength),
        meta(&meta),
        filter(filter) {
    }

    bool operator==(Level const& rhs) const noexcept {
      return it == rhs.it;
    }

    bool operator !=(Level const& rhs) const noexcept {
      return !(*this == rhs);
    }

    Iterator it;
    size_t nameLength; // length of the name at the current level
    IResearchLinkMeta const* meta; // metadata
    Filter filter;
  }; // Level

  Level& top() noexcept {
    TRI_ASSERT(!_stack.empty());
    return _stack.back();
  }

  IteratorValue const& topValue() noexcept {
    return top().it.value();
  }

  std::string& nameBuffer() noexcept {
    TRI_ASSERT(_name);
    return *_name;
  }

  // disallow copy and assign
  FieldIterator(FieldIterator const&) = delete;
  FieldIterator& operator=(FieldIterator const&) = delete;

  void next();
  bool pushAndSetValue(arangodb::velocypack::Slice slice, IResearchLinkMeta const*& topMeta);
  bool setRegularAttribute(IResearchLinkMeta const& context);

  void resetAnalyzers(IResearchLinkMeta const& context) {
    auto const& analyzers = context._analyzers;

    _begin = analyzers.data();
    _end = _begin + analyzers.size();
  }

  AnalyzerIterator _begin{};
  AnalyzerIterator _end{};
  std::vector<Level> _stack;
  std::shared_ptr<std::string> _name; // buffer for field name
  Field _value; // iterator's value
}; // FieldIterator

////////////////////////////////////////////////////////////////////////////////
/// @brief represents stored primary key of the ArangoDB document
////////////////////////////////////////////////////////////////////////////////
struct DocumentPrimaryKey : std::pair<TRI_voc_cid_t, TRI_voc_rid_t> {
  typedef std::pair<TRI_voc_cid_t, TRI_voc_rid_t> type; // underlying PK type

  static irs::string_ref const& PK() noexcept; // stored primary key column
  static irs::string_ref const& CID() noexcept; // stored collection id column

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief creates a filter matching 'cid'
  ////////////////////////////////////////////////////////////////////////////////
  static irs::filter::ptr filter(TRI_voc_cid_t cid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief creates a filter matching 'cid' + 'rid' pair
  ////////////////////////////////////////////////////////////////////////////////
  static irs::filter::ptr filter(TRI_voc_cid_t cid, TRI_voc_rid_t rid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief reads and decodes PK from a specified buffer
  /// @returns 'true' on success, 'false' otherwise
  /// @note PLEASE NOTE that 'in.c_str()' MUST HAVE alignment >= alignof(uint64_t)
  ////////////////////////////////////////////////////////////////////////////////
  static bool read(type& value, irs::bytes_ref const& in) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief creates PK with properly encoded cid & rid
  ////////////////////////////////////////////////////////////////////////////////
  DocumentPrimaryKey(TRI_voc_cid_t cid, TRI_voc_rid_t rid) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief coverts a PK to corresponding irs::bytes_ref
  ////////////////////////////////////////////////////////////////////////////////
  explicit operator irs::bytes_ref() const noexcept {
    return irs::bytes_ref(
      reinterpret_cast<irs::byte_type const*>(this),
      sizeof(*this)
    );
  }
}; // DocumentPrimaryKey

NS_END // iresearch
NS_END // arangodb

#endif