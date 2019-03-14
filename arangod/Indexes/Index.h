////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_INDEXES_INDEX_H
#define ARANGOD_INDEXES_INDEX_H 1

#include "Aql/AstNode.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/StringRef.h>

#include <iosfwd>

namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

class IndexIterator;
class LogicalCollection;
struct IndexIteratorOptions;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class SortCondition;
struct Variable;
}  // namespace aql

namespace transaction {
class Methods;
}

/// @brief static limits for fulltext index
struct FulltextIndexLimits {
  /// @brief maximum length of an indexed word in characters
  static constexpr int maxWordLength = 40;
  /// @brief default minimum word length for a fulltext index
  static constexpr int minWordLengthDefault = 2;
  /// @brief maximum number of search words in a query
  static constexpr int maxSearchWords = 32;
};

class Index {
 public:
  Index() = delete;
  Index(Index const&) = delete;
  Index& operator=(Index const&) = delete;

  Index(TRI_idx_iid_t iid, LogicalCollection& collection, std::string const& name,
        std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
        bool unique, bool sparse);

  Index(TRI_idx_iid_t iid, LogicalCollection& collection,
        arangodb::velocypack::Slice const& slice);

  virtual ~Index();

 public:
  /// @brief index types
  enum IndexType {
    TRI_IDX_TYPE_UNKNOWN = 0,
    TRI_IDX_TYPE_PRIMARY_INDEX,
    TRI_IDX_TYPE_GEO_INDEX,
    TRI_IDX_TYPE_GEO1_INDEX,
    TRI_IDX_TYPE_GEO2_INDEX,
    TRI_IDX_TYPE_HASH_INDEX,
    TRI_IDX_TYPE_EDGE_INDEX,
    TRI_IDX_TYPE_FULLTEXT_INDEX,
    TRI_IDX_TYPE_SKIPLIST_INDEX,
    TRI_IDX_TYPE_TTL_INDEX,
    TRI_IDX_TYPE_PERSISTENT_INDEX,
#ifdef USE_IRESEARCH
    TRI_IDX_TYPE_IRESEARCH_LINK,
#endif
    TRI_IDX_TYPE_NO_ACCESS_INDEX
  };

  // mode to signal how operation should behave
  enum OperationMode { normal, internal, rollback };

 public:
  /// @brief return the index id
  inline TRI_idx_iid_t id() const { return _iid; }

  /// @brief return the index name
  inline std::string const& name() const {
    if (_name == StaticStrings::IndexNameEdgeFrom || _name == StaticStrings::IndexNameEdgeTo) {
      return StaticStrings::IndexNameEdge;
    }
    return _name;
  }

  /// @brief set the name, if it is currently unset
  void name(std::string const&);

  /// @brief return the index fields
  inline std::vector<std::vector<arangodb::basics::AttributeName>> const& fields() const {
    return _fields;
  }

  /// @brief return the fields covered by this index.
  ///        Typically just the fields, but e.g. EdgeIndex on _from also covers
  ///        _to
  virtual std::vector<std::vector<arangodb::basics::AttributeName>> const& coveredFields() const {
    return fields();
  }

  /// @brief return the index fields names
  inline std::vector<std::vector<std::string>> fieldNames() const {
    std::vector<std::vector<std::string>> result;

    for (auto const& it : _fields) {
      std::vector<std::string> parts;
      parts.reserve(it.size());
      for (auto const& it2 : it) {
        parts.emplace_back(it2.name);
      }
      result.emplace_back(std::move(parts));
    }
    return result;
  }

  /// @brief whether or not the ith attribute is expanded (somewhere)
  inline bool isAttributeExpanded(size_t i) const {
    if (i >= _fields.size()) {
      return false;
    }
    return TRI_AttributeNamesHaveExpansion(_fields[i]);
  }

  /// @brief whether or not any attribute is expanded
  inline bool isAttributeExpanded(std::vector<arangodb::basics::AttributeName> const& attribute) const {
    for (auto const& it : _fields) {
      if (!arangodb::basics::AttributeName::namesMatch(attribute, it)) {
        continue;
      }
      return TRI_AttributeNamesHaveExpansion(it);
    }
    return false;
  }

  /// @brief whether or not any attribute is expanded
  inline bool attributeMatches(std::vector<arangodb::basics::AttributeName> const& attribute,
                               bool isPrimary = false) const {
    for (auto const& it : _fields) {
      if (arangodb::basics::AttributeName::isIdentical(attribute, it, true)) {
        return true;
      }
    }
    if (isPrimary) {
      static std::vector<arangodb::basics::AttributeName> const vec_id{
          {StaticStrings::IdString, false}};
      return arangodb::basics::AttributeName::isIdentical(attribute, vec_id, true);
    }
    return false;
  }

  /// @brief whether or not any attribute is expanded
  inline bool hasExpansion() const { return _useExpansion; }

  /// @brief whether or not the index covers all the attributes passed in
  virtual bool covers(std::unordered_set<std::string> const& attributes) const;

  /// @brief return the underlying collection
  inline LogicalCollection& collection() const { return _collection; }

  /// @brief return a contextual string for logging
  std::string context() const;

  /// @brief whether or not the index is sparse
  inline bool sparse() const { return _sparse; }

  /// @brief whether or not the index is unique
  inline bool unique() const { return _unique; }

  /// @brief validate fields from slice
  static void validateFields(velocypack::Slice const& slice);

  /// @brief return the name of the index
  char const* oldtypeName() const { return oldtypeName(type()); }

  /// @brief return the index type based on a type name
  static IndexType type(char const* type, size_t len);

  static IndexType type(std::string const& type);

  virtual char const* typeName() const = 0;

  static bool allowExpansion(IndexType type) {
    return (type == TRI_IDX_TYPE_HASH_INDEX || type == TRI_IDX_TYPE_SKIPLIST_INDEX ||
            type == TRI_IDX_TYPE_PERSISTENT_INDEX);
  }

  virtual IndexType type() const = 0;

  /// @brief return the name of an index type
  static char const* oldtypeName(IndexType);

  /// @brief validate an index id
  static bool validateId(char const*);

  /// @brief validate an index handle (collection name + / + index id)
  static bool validateHandle(char const*, size_t*);

  /// @brief generate a new index id
  static TRI_idx_iid_t generateId();

  /// @brief check if two index definitions share any identifiers (_id, name)
  static bool CompareIdentifiers(velocypack::Slice const& lhs, velocypack::Slice const& rhs);

  /// @brief index comparator, used by the coordinator to detect if two index
  /// contents are the same
  static bool Compare(velocypack::Slice const& lhs, velocypack::Slice const& rhs);

  /// @brief whether or not the index is persistent (storage on durable media)
  /// or not (RAM only)
  virtual bool isPersistent() const = 0;

  virtual bool canBeDropped() const = 0;

  /// @brief whether or not the index provides an iterator that can extract
  /// attribute values from the index data, without having to refer to the
  /// actual document data
  /// By default, indexes do not have this type of iterator, but they can
  /// add it as a performance optimization
  virtual bool hasCoveringIterator() const { return false; }

  /// @brief Checks if this index is identical to the given definition
  virtual bool matchesDefinition(arangodb::velocypack::Slice const&) const;

  /// @brief whether or not the index is sorted
  virtual bool isSorted() const = 0;

  /// @brief if true this index should not be shown externally
  virtual bool isHidden() const = 0;

  /// @brief whether or not the index has a selectivity estimate
  virtual bool hasSelectivityEstimate() const = 0;

  /// @brief return the selectivity estimate of the index
  /// must only be called if hasSelectivityEstimate() returns true
  ///
  /// The extra arangodb::velocypack::StringRef is only used in the edge index
  /// as direction attribute attribute, a Slice would be more flexible.
  virtual double selectivityEstimate(arangodb::velocypack::StringRef const& extra =
                                         arangodb::velocypack::StringRef()) const;

  /// @brief update the cluster selectivity estimate
  virtual void updateClusterSelectivityEstimate(double /*estimate*/) {
    TRI_ASSERT(false);  // should never be called except on Coordinator
  }

  /// @brief whether or not the index is implicitly unique
  /// this can be the case if the index is not declared as unique,
  /// but contains a unique attribute such as _key
  bool implicitlyUnique() const;

  virtual size_t memory() const = 0;

  /// @brief serialization flags for indexes.
  /// note that these must be mutually exclusive when bit-ORed
  enum class Serialize : uint8_t {
    /// @brief serialize figures for index
    Basics = 0,
    /// @brief serialize figures for index
    Figures = 2,
    /// @brief serialize selectivity estimates
    Estimates = 4,
    /// @brief serialize object ids for persistence
    Internals = 8,
  };

  /// @brief helper for building flags
  template <typename... Args>
  static inline constexpr std::underlying_type<Serialize>::type makeFlags(Serialize flag,
                                                                          Args... args) {
    return static_cast<std::underlying_type<Serialize>::type>(flag) + makeFlags(args...);
  }

  static inline constexpr std::underlying_type<Serialize>::type makeFlags() {
    return static_cast<std::underlying_type<Serialize>::type>(Serialize::Basics);
  }

  static inline constexpr bool hasFlag(std::underlying_type<Serialize>::type flags,
                                       Serialize aflag) {
    return (flags & static_cast<std::underlying_type<Serialize>::type>(aflag)) != 0;
  }

  /// serialize an index to velocypack, using the serialization flags above
  virtual void toVelocyPack(arangodb::velocypack::Builder&,
                            std::underlying_type<Index::Serialize>::type flags) const;
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(
      std::underlying_type<Serialize>::type flags) const;

  virtual void toVelocyPackFigures(arangodb::velocypack::Builder&) const;
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPackFigures() const;

  virtual void load() = 0;
  virtual void unload() = 0;

  // called when the index is dropped
  virtual Result drop();

  /// @brief called after the collection was truncated
  /// @param tick at which truncate was applied
  virtual void afterTruncate(TRI_voc_tick_t tick){};

  // give index a hint about the expected size
  virtual Result sizeHint(transaction::Methods& trx, size_t size);

  virtual bool hasBatchInsert() const;

  virtual bool supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                       arangodb::aql::AstNode const*,
                                       arangodb::aql::Variable const*, size_t,
                                       size_t&, double&) const;

  virtual bool supportsSortCondition(arangodb::aql::SortCondition const*,
                                     arangodb::aql::Variable const*, size_t,
                                     double&, size_t&) const;

  virtual arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode*,
                                                      arangodb::aql::Variable const*) const;

  virtual IndexIterator* iteratorForCondition(transaction::Methods* trx,
                                              aql::AstNode const* condNode,
                                              aql::Variable const* var,
                                              IndexIteratorOptions const& opts) = 0;

  bool canUseConditionPart(arangodb::aql::AstNode const* access,
                           arangodb::aql::AstNode const* other,
                           arangodb::aql::AstNode const* op,
                           arangodb::aql::Variable const* reference,
                           std::unordered_set<std::string>& nonNullAttributes, bool) const;

  /// @brief Transform the list of search slices to search values.
  ///        This will multiply all IN entries and simply return all other
  ///        entries.
  void expandInSearchValues(arangodb::velocypack::Slice const,
                            arangodb::velocypack::Builder&) const;

  virtual void warmup(arangodb::transaction::Methods* trx,
                      std::shared_ptr<basics::LocalTaskQueue> queue);

  static size_t sortWeight(arangodb::aql::AstNode const* node);

 protected:
  /// @brief return the name of the (sole) index attribute
  /// it is only allowed to call this method if the index contains a
  /// single attribute
  std::string const& getAttribute() const;

  /// @brief generate error result
  /// @param code the error key
  /// @param key the conflicting key
  arangodb::Result& addErrorMsg(Result& r, int code,
                                std::string const& key = "") {
    if (code != TRI_ERROR_NO_ERROR) {
      r.reset(code);
      return addErrorMsg(r, key);
    }
    return r;
  }

  /// @brief generate error result
  /// @param key the conflicting key
  arangodb::Result& addErrorMsg(Result& r, std::string const& key = "");

  /// @brief extracts a timestamp value from a document
  /// returns a negative value if the document does not contain the specified
  /// attribute, or the attribute does not contain a valid timestamp or date string
  double getTimestamp(arangodb::velocypack::Slice const& doc,
                      std::string const& attributeName) const;

  TRI_idx_iid_t const _iid;
  LogicalCollection& _collection;
  std::string _name;
  std::vector<std::vector<arangodb::basics::AttributeName>> const _fields;
  bool const _useExpansion;

  mutable bool _unique;
  mutable bool _sparse;

  // use this with c++17  --  attributeMatches
  // static inline std::vector<arangodb::basics::AttributeName> const vec_id {{ StaticStrings::IdString, false }};
};

/// @brief simple struct that takes an AstNode of type comparison and
/// splits it into the comparison operator, the attribute access and the 
/// lookup value parts
/// only works for conditions such as  a.b == 2   or   45 < a.xx.c
/// the collection variable (a in the above examples) is passed in "variable"
struct AttributeAccessParts {
  AttributeAccessParts(arangodb::aql::AstNode const* comparison,
                       arangodb::aql::Variable const* variable);
  
  /// @brief comparison operation, e.g. NODE_TYPE_OPERATOR_BINARY_EQ
  arangodb::aql::AstNode const* comparison;
  
  /// @brief attribute access node
  arangodb::aql::AstNode const* attribute;
  
  /// @brief lookup value 
  arangodb::aql::AstNode const* value;

  /// @brief operation type
  arangodb::aql::AstNodeType opType;
};

}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::Index const*);
std::ostream& operator<<(std::ostream&, arangodb::Index const&);

#endif
