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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_SKIPLIST_INDEX_H
#define ARANGOD_MMFILES_SKIPLIST_INDEX_H 1

#include "Aql/AstNode.h"
#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesPathBasedIndex.h"
#include "MMFiles/MMFilesSkiplist.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <list>

namespace arangodb {
namespace aql {
class SortCondition;
struct Variable;
}

class MMFilesSkiplistIndex;
namespace transaction {
class Methods;
}

/// @brief Abstract Builder for lookup values in skiplist index
class MMFilesBaseSkiplistLookupBuilder {
 protected:
  bool _isEquality;
  bool _includeLower;
  bool _includeUpper;

  transaction::BuilderLeaser _lowerBuilder;
  arangodb::velocypack::Slice _lowerSlice;

  transaction::BuilderLeaser _upperBuilder;
  arangodb::velocypack::Slice _upperSlice;

 public:
  explicit MMFilesBaseSkiplistLookupBuilder(transaction::Methods* trx)
      : _lowerBuilder(trx), _upperBuilder(trx) {
    _isEquality = true;
    _includeUpper = true;
    _includeLower = true;

    _lowerBuilder->clear();
    _upperBuilder->clear();
  }

  virtual ~MMFilesBaseSkiplistLookupBuilder() {}

  /// @brief Compute the next lookup values
  ///        If returns false there is no further lookup
  virtual bool next() = 0;

  /// @brief Returns if we only have equality checks (== or IN)
  bool isEquality() const;

  /// @brief Get the lookup value for the lower bound.
  arangodb::velocypack::Slice const* getLowerLookup() const;

  /// @brief Test if the lower bound should be included.
  ///        If there is no lower bound given returns true
  ///        as well.
  bool includeLower() const;

  /// @brief Get the lookup value for the upper bound.
  arangodb::velocypack::Slice const* getUpperLookup() const;

  /// @brief Test if the upper bound should be included.
  ///        If there is no upper bound given returns true
  ///        as well.
  bool includeUpper() const;
};

/// @brief Builder for lookup values in skiplist index
///        Offers lower and upper bound lookup values
///        and handles multiplication of IN search values.
///        Also makes sure that the lookup values are
///        returned in the correct ordering. And no
///        lookup is returned twice.

class MMFilesSkiplistLookupBuilder final
    : public MMFilesBaseSkiplistLookupBuilder {
 public:
  MMFilesSkiplistLookupBuilder(
      transaction::Methods* trx,
      std::vector<std::vector<arangodb::aql::AstNode const*>>&,
      arangodb::aql::Variable const*, bool);

  ~MMFilesSkiplistLookupBuilder() {}

  /// @brief Compute the next lookup values
  ///        If returns false there is no further lookup
  bool next() override;
};

class MMFilesSkiplistInLookupBuilder final
    : public MMFilesBaseSkiplistLookupBuilder {
 private:
  struct PosStruct {
    size_t field;
    size_t current;
    size_t _max;  // thanks, windows.h!

    PosStruct(size_t f, size_t c, size_t m) : field(f), current(c), _max(m) {}
  };

  transaction::BuilderLeaser _dataBuilder;
  /// @brief keeps track of the positions in the in-lookup
  /// values. (field, inPosition, maxPosition)
  std::list<PosStruct> _inPositions;

  bool _done;

 public:
  MMFilesSkiplistInLookupBuilder(
      transaction::Methods* trx,
      std::vector<std::vector<arangodb::aql::AstNode const*>>&,
      arangodb::aql::Variable const*, bool);

  ~MMFilesSkiplistInLookupBuilder() {}

  /// @brief Compute the next lookup values
  /// If returns false there is no further lookup
  bool next() override;

 private:
  bool forwardInPosition();

  void buildSearchValues();
};

/// @brief Iterator structure for skip list. We require a start and stop node
///
/// Intervals are open in the sense that both end points are not members
/// of the interval. This means that one has to use MMFilesSkiplist::nextNode
/// on the start node to get the first element and that the stop node
/// can be NULL. Note that it is ensured that all intervals in an iterator
/// are non-empty.
class MMFilesSkiplistIterator final : public IndexIterator {
 private:
  // Shorthand for the skiplist node
  typedef MMFilesSkiplistNode<VPackSlice, MMFilesSkiplistIndexElement> Node;

  typedef MMFilesSkiplist<VPackSlice, MMFilesSkiplistIndexElement> TRI_Skiplist;

 private:
  TRI_Skiplist const* _skiplistIndex;
  size_t _numPaths;
  bool _reverse;
  Node* _cursor;

  // The pair.first is the left border
  // The pair.second is the right border
  // Both borders are inclusive
  std::vector<std::pair<Node*, Node*>> _intervals;
  size_t _currentInterval;

  MMFilesBaseSkiplistLookupBuilder* _builder;

  std::function<int(void*, MMFilesSkiplistIndexElement const*,
                    MMFilesSkiplistIndexElement const*, MMFilesSkiplistCmpType)>
      _CmpElmElm;

 public:
  MMFilesSkiplistIterator(
      LogicalCollection* collection, transaction::Methods* trx,
      ManagedDocumentResult* mmdr, arangodb::MMFilesSkiplistIndex const* index,
      TRI_Skiplist const* skiplist, size_t numPaths,
      std::function<int(void*, MMFilesSkiplistIndexElement const*,
                        MMFilesSkiplistIndexElement const*,
                        MMFilesSkiplistCmpType)> const& CmpElmElm,
      bool reverse, MMFilesBaseSkiplistLookupBuilder* builder);

  ~MMFilesSkiplistIterator() { delete _builder; }

  // always holds the last node returned, initially equal to
  // the _leftEndPoint (or the
  // _rightEndPoint in the reverse case),
  // can be nullptr if the iterator is exhausted.

 public:
  char const* typeName() const override { return "skiplist-index-iterator"; }

  /// @brief Get the next elements in the skiplist
  bool next(TokenCallback const& cb, size_t limit) override;

  /// @brief Reset the cursor
  void reset() override;

  size_t numPaths() const { return _numPaths; }

 private:
  /// @brief Initialize left and right endpoints with current lookup
  ///        value. Also points the _cursor to the border of this interval.
  void initNextInterval();

  /// @brief Forward the cursor to the next interval. If there was no
  ///        interval the next one is computed. If the _cursor has
  ///        nullptr after this call the iterator is exhausted.
  void forwardCursor();

  /// @brief Checks if the interval is valid. It is declared invalid if
  ///        one border is nullptr or the right is lower than left.
  bool intervalValid(void*, Node*, Node*) const;
};

class MMFilesSkiplistIndex final : public MMFilesPathBasedIndex {
  struct KeyElementComparator {
    int operator()(void* userData, VPackSlice const* leftKey,
                   MMFilesSkiplistIndexElement const* rightElement) const;

    explicit KeyElementComparator(MMFilesSkiplistIndex* idx) { _idx = idx; }

   private:
    MMFilesSkiplistIndex* _idx;
  };

  struct ElementElementComparator {
    int operator()(void* userData,
                   MMFilesSkiplistIndexElement const* leftElement,
                   MMFilesSkiplistIndexElement const* rightElement,
                   MMFilesSkiplistCmpType cmptype) const;

    explicit ElementElementComparator(MMFilesSkiplistIndex* idx) { _idx = idx; }

   private:
    MMFilesSkiplistIndex* _idx;
  };

  friend struct KeyElementComparator;
  friend struct ElementElementComparator;

  typedef MMFilesSkiplist<VPackSlice, MMFilesSkiplistIndexElement> TRI_Skiplist;

 public:
  MMFilesSkiplistIndex() = delete;

  MMFilesSkiplistIndex(TRI_idx_iid_t, LogicalCollection*,
                       arangodb::velocypack::Slice const&);

  ~MMFilesSkiplistIndex();

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_SKIPLIST_INDEX; }

  char const* typeName() const override { return "skiplist"; }

  bool allowExpansion() const override { return true; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return true; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override;

  void toVelocyPackFigures(VPackBuilder&) const override;

  Result insert(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  Result remove(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  void unload() override;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t, double&,
                             size_t&) const override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

 private:
  bool isDuplicateOperator(arangodb::aql::AstNode const*,
                           std::unordered_set<int> const&) const;

  bool accessFitsIndex(
      arangodb::aql::AstNode const*, arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      std::unordered_set<std::string>& nonNullAttributes, bool) const;

  bool accessFitsIndex(
      arangodb::aql::AstNode const*, arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::vector<std::vector<arangodb::aql::AstNode const*>>&,
      std::unordered_set<std::string>& nonNullAttributes) const;

  void matchAttributes(
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      size_t& values, std::unordered_set<std::string>& nonNullAttributes,
      bool) const;

  bool findMatchingConditions(
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::vector<std::vector<arangodb::aql::AstNode const*>>&, bool&) const;

  /// @brief Checks if the interval is valid. It is declared invalid if
  ///        one border is nullptr or the right is lower than left.
  // Shorthand for the skiplist node
  typedef MMFilesSkiplistNode<VPackSlice, MMFilesSkiplistIndexElement> Node;

  bool intervalValid(void*, Node* left, Node* right) const;

 private:
  ElementElementComparator CmpElmElm;

  KeyElementComparator CmpKeyElm;

  /// @brief the actual skiplist index
  TRI_Skiplist* _skiplistIndex;
};
}

#endif
