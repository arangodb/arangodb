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

#include "Basics/Common.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/StringRef.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <iosfwd>

namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

class IndexIterator;
class LogicalCollection;
class ManagedDocumentResult;
struct IndexIteratorOptions;

namespace velocypack {
class Builder;
class Slice;
}

namespace aql {
struct AstNode;
class SortCondition;
struct Variable;
}

namespace transaction {
class Methods;
}

class Index {
 public:
  Index() = delete;
  Index(Index const&) = delete;
  Index& operator=(Index const&) = delete;

  Index(
    TRI_idx_iid_t iid,
    LogicalCollection& collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique,
    bool sparse
  );

  Index(
    TRI_idx_iid_t iid,
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& slice
  );

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
    TRI_IDX_TYPE_PERSISTENT_INDEX,
#ifdef USE_IRESEARCH
    TRI_IDX_TYPE_IRESEARCH_LINK,
#endif
    TRI_IDX_TYPE_NO_ACCESS_INDEX
  };

  // mode to signal how operation should behave
  enum OperationMode {
    normal,
    internal,
    rollback
  };

 public:
  /// @brief return the index id
  inline TRI_idx_iid_t id() const { return _iid; }

  /// @brief return the index fields
  inline std::vector<std::vector<arangodb::basics::AttributeName>> const& fields() const {
    return _fields;
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
  inline bool isAttributeExpanded(
      std::vector<arangodb::basics::AttributeName> const& attribute) const {
    for (auto const& it : _fields) {
      if (!arangodb::basics::AttributeName::namesMatch(attribute, it)) {
        continue;
      }
      return TRI_AttributeNamesHaveExpansion(it);
    }
    return false;
  }

  /// @brief whether or not any attribute is expanded
  inline bool attributeMatches(
      std::vector<arangodb::basics::AttributeName> const& attribute) const {
    for (auto const& it : _fields) {
      if (arangodb::basics::AttributeName::isIdentical(attribute, it, true)) {
        return true;
      }
    }
    return false;
  }

  /// @brief whether or not any attribute is expanded
  inline bool hasExpansion() const {
    return _useExpansion;
  }

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
  static IndexType type(char const* type);

  static IndexType type(std::string const& type);

  static bool isGeoIndex(IndexType type) {
    return type == TRI_IDX_TYPE_GEO1_INDEX ||
           type == TRI_IDX_TYPE_GEO2_INDEX ||
           type == TRI_IDX_TYPE_GEO_INDEX;
  }

  virtual char const* typeName() const = 0;

  static bool allowExpansion(IndexType type) {
    return (type == TRI_IDX_TYPE_HASH_INDEX ||
            type == TRI_IDX_TYPE_SKIPLIST_INDEX ||
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

  /// @brief index comparator, used by the coordinator to detect if two index
  /// contents are the same
  static bool Compare(velocypack::Slice const& lhs,
                      velocypack::Slice const& rhs);

  virtual bool isPersistent() const { return false; }
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

  /// @brief whether or not the index has a selectivity estimate
  virtual bool hasSelectivityEstimate() const = 0;

  /// @brief return the selectivity estimate of the index
  /// must only be called if hasSelectivityEstimate() returns true
  ///
  /// The extra StringRef is only used in the edge index as direction
  /// attribute attribute, a Slice would be more flexible.
  virtual double selectivityEstimate(
      arangodb::StringRef const& extra = arangodb::StringRef()) const;
  
  /// @brief update the cluster selectivity estimate
  virtual void updateClusterSelectivityEstimate(double /*estimate*/) {
    TRI_ASSERT(false); // should never be called except on Coordinator
  }

  /// @brief whether or not the index is implicitly unique
  /// this can be the case if the index is not declared as unique,
  /// but contains a unique attribute such as _key
  virtual bool implicitlyUnique() const;

  virtual size_t memory() const = 0;

  /// @brief serialization flags for indexes.
  /// note that these must be mutually exclusive when bit-ORed
  enum class Serialize : uint8_t {
    /// @brief serialize figures for index
    Basics = 0,
    /// @brief serialize figures for index
    Figures = 2,
    /// @brief serialize object ids for persistence
    ObjectId = 4,
    /// @brief serialize selectivity estimates
    Estimates = 8
  };

  /// @brief helper for building flags
  template <typename... Args>
  static inline constexpr std::underlying_type<Serialize>::type makeFlags(Serialize flag, Args... args) {
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
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(std::underlying_type<Serialize>::type flags) const;

  virtual void toVelocyPackFigures(arangodb::velocypack::Builder&) const;
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPackFigures() const;

  virtual void batchInsert(
    transaction::Methods& trx,
    std::vector<std::pair<LocalDocumentId, arangodb::velocypack::Slice>> const& docs,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue
  );

  virtual Result insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    OperationMode mode
  ) = 0;

  virtual Result remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    OperationMode mode
  ) = 0;

  virtual void load() = 0;
  virtual void unload() = 0;

  // called when the index is dropped
  virtual Result drop();

  /// @brief called after the collection was truncated
  /// @param tick at which truncate was applied
  virtual void afterTruncate(TRI_voc_tick_t tick) {};

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

  virtual IndexIterator* iteratorForCondition(
    transaction::Methods* trx,
    ManagedDocumentResult* result,
    aql::AstNode const* condNode,
    aql::Variable const* var,
    IndexIteratorOptions const& opts
  ) = 0;

  bool canUseConditionPart(arangodb::aql::AstNode const* access,
                           arangodb::aql::AstNode const* other,
                           arangodb::aql::AstNode const* op,
                           arangodb::aql::Variable const* reference,
                           std::unordered_set<std::string>& nonNullAttributes,
                           bool) const;

  /// @brief Transform the list of search slices to search values.
  ///        This will multiply all IN entries and simply return all other
  ///        entries.
  void expandInSearchValues(arangodb::velocypack::Slice const,
                            arangodb::velocypack::Builder&) const;

  virtual void warmup(arangodb::transaction::Methods* trx,
                      std::shared_ptr<basics::LocalTaskQueue> queue);

  static size_t sortWeight(arangodb::aql::AstNode const* node);

 protected:

  /// @brief generate error result
  /// @param code the error key
  /// @param key the conflicting key
  arangodb::Result& addErrorMsg(Result& r, int code, std::string const& key = "") {
    if (code != TRI_ERROR_NO_ERROR) {
      r.reset(code);
      return addErrorMsg(r, key);
    }
    return r;
  }

  /// @brief generate error result
  /// @param key the conflicting key
  arangodb::Result& addErrorMsg(Result& r, std::string const& key = "");

  TRI_idx_iid_t const _iid;
  LogicalCollection& _collection;
  std::vector<std::vector<arangodb::basics::AttributeName>> const _fields;
  bool const _useExpansion;

  mutable bool _unique;
  mutable bool _sparse;
};
}

std::ostream& operator<<(std::ostream&, arangodb::Index const*);
std::ostream& operator<<(std::ostream&, arangodb::Index const&);

#endif