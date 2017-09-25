
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

#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <iosfwd>

namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

class LogicalCollection;
class ManagedDocumentResult;
class StringRef;

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
};
}

namespace arangodb {
class IndexIterator;

class Index {
 public:
  Index() = delete;
  Index(Index const&) = delete;
  Index& operator=(Index const&) = delete;

  Index(TRI_idx_iid_t, LogicalCollection*,
        std::vector<std::vector<arangodb::basics::AttributeName>> const&,
        bool unique, bool sparse);

  Index(TRI_idx_iid_t, LogicalCollection*, arangodb::velocypack::Slice const&);

  explicit Index(arangodb::velocypack::Slice const&);

  virtual ~Index();

 public:
  /// @brief index types
  enum IndexType {
    TRI_IDX_TYPE_UNKNOWN = 0,
    TRI_IDX_TYPE_PRIMARY_INDEX,
    TRI_IDX_TYPE_GEO1_INDEX,
    TRI_IDX_TYPE_GEO2_INDEX,
    TRI_IDX_TYPE_HASH_INDEX,
    TRI_IDX_TYPE_EDGE_INDEX,
    TRI_IDX_TYPE_FULLTEXT_INDEX,
    TRI_IDX_TYPE_SKIPLIST_INDEX,
    TRI_IDX_TYPE_PERSISTENT_INDEX,
    TRI_IDX_TYPE_NO_ACCESS_INDEX
  };

 public:
  /// @brief return the index id
  inline TRI_idx_iid_t id() const { return _iid; }

  /// @brief return the index fields
  inline std::vector<std::vector<arangodb::basics::AttributeName>> const&
  fields() const {
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
    for (auto const& it : _fields) {
      for (auto const& it2 : it) {
        if (it2.shouldExpand) {
          return true;
        }
      }
    }
    return false;
  }

  /// @brief return the underlying collection
  inline LogicalCollection* collection() const { return _collection; }

  /// @brief return a contextual string for logging
  std::string context() const;

  /// @brief whether or not the index is sparse
  inline bool sparse() const { return _sparse; }

  /// @brief whether or not the index is unique
  inline bool unique() const { return _unique; }

  /// @brief validate fields from slice
  static void validateFields(VPackSlice const& slice);

  /// @brief return the name of the index
  char const* oldtypeName() const { return oldtypeName(type()); }

  /// @brief return the index type based on a type name
  static IndexType type(char const* type);

  static IndexType type(std::string const& type);

  virtual char const* typeName() const = 0;

  virtual bool allowExpansion() const = 0;

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
  static bool Compare(VPackSlice const& lhs, VPackSlice const& rhs);

  virtual bool isPersistent() const { return false; }
  virtual bool canBeDropped() const = 0;

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
  double selectivityEstimate(
      arangodb::StringRef const* extra = nullptr) const;
  
  virtual double selectivityEstimateLocal(
      arangodb::StringRef const* extra) const;

  /// @brief whether or not the index is implicitly unique
  /// this can be the case if the index is not declared as unique,
  /// but contains a unique attribute such as _key
  virtual bool implicitlyUnique() const;

  virtual size_t memory() const = 0;

  virtual void toVelocyPack(arangodb::velocypack::Builder&, bool withFigures, bool forPersistence) const;
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(bool withFigures, bool forPersistence) const;

  virtual void toVelocyPackFigures(arangodb::velocypack::Builder&) const;
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPackFigures() const;

  virtual Result insert(transaction::Methods*, TRI_voc_rid_t revisionId,
                     arangodb::velocypack::Slice const&, bool isRollback) = 0;
  virtual Result remove(transaction::Methods*, TRI_voc_rid_t revisionId,
                     arangodb::velocypack::Slice const&, bool isRollback) = 0;

  virtual void batchInsert(
      transaction::Methods*,
      std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue> queue);

  virtual void load() = 0;
  virtual void unload() = 0;

  // called when the index is dropped
  virtual int drop();

  // give index a hint about the expected size
  virtual int sizeHint(transaction::Methods*, size_t);

  virtual bool hasBatchInsert() const;

  virtual bool supportsFilterCondition(arangodb::aql::AstNode const*,
                                       arangodb::aql::Variable const*, size_t,
                                       size_t&, double&) const;

  virtual bool supportsSortCondition(arangodb::aql::SortCondition const*,
                                     arangodb::aql::Variable const*, size_t,
                                     double&, size_t&) const;

  virtual IndexIterator* iteratorForCondition(transaction::Methods*,
                                              ManagedDocumentResult*,
                                              arangodb::aql::AstNode const*,
                                              arangodb::aql::Variable const*,
                                              bool);

  virtual arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const;

  bool canUseConditionPart(arangodb::aql::AstNode const* access,
                           arangodb::aql::AstNode const* other,
                           arangodb::aql::AstNode const* op,
                           arangodb::aql::Variable const* reference,
                           std::unordered_set<std::string>& nonNullAttributes,
                           bool) const;

  /// @brief Transform the list of search slices to search values.
  ///        This will multiply all IN entries and simply return all other
  ///        entries.
  virtual void expandInSearchValues(arangodb::velocypack::Slice const,
                                    arangodb::velocypack::Builder&) const;

  virtual void warmup(arangodb::transaction::Methods* trx,
                      std::shared_ptr<basics::LocalTaskQueue> queue);

  // needs to be called when the _colllection is guaranteed to be valid!
  // unfortunatly access the logical collection on the coordinator is not always safe!
  std::pair<bool,double> updateClusterEstimate(double defaultValue = 0.1);

 protected:
  static size_t sortWeight(arangodb::aql::AstNode const* node);

  //returns estimate for index in cluster - the bool is true if the index was found

 private:
  /// @brief set fields from slice
  void setFields(VPackSlice const& slice, bool allowExpansion);

 protected:
  TRI_idx_iid_t const _iid;

  LogicalCollection* _collection;

  std::vector<std::vector<arangodb::basics::AttributeName>> _fields;

  mutable bool _unique;

  mutable bool _sparse;
  
  double _clusterSelectivity;
};
}

std::ostream& operator<<(std::ostream&, arangodb::Index const*);
std::ostream& operator<<(std::ostream&, arangodb::Index const&);

#endif
