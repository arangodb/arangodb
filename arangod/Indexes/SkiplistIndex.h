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

#ifndef ARANGOD_INDEXES_SKIPLIST_INDEX_H
#define ARANGOD_INDEXES_SKIPLIST_INDEX_H 1

#include "Basics/Common.h"
#include "Aql/AstNode.h"
#include "Basics/SkipList.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/PathBasedIndex.h"
#include "Utils/Transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <list>

namespace arangodb {
namespace aql {
class SortCondition;
struct Variable;
}

class SkiplistIndex;
class Transaction;


/// @brief Abstract Builder for lookup values in skiplist index

class BaseSkiplistLookupBuilder {
 protected:
  bool _isEquality;
  bool _includeLower;
  bool _includeUpper;

  TransactionBuilderLeaser _lowerBuilder;
  arangodb::velocypack::Slice _lowerSlice;

  TransactionBuilderLeaser _upperBuilder;
  arangodb::velocypack::Slice _upperSlice;

 public:
  explicit BaseSkiplistLookupBuilder(Transaction* trx) :
    _lowerBuilder(trx), _upperBuilder(trx)
  {
    _isEquality = true;
    _includeUpper = true;
    _includeLower = true;

    _lowerBuilder->clear();
    _upperBuilder->clear();
  }

  virtual ~BaseSkiplistLookupBuilder() {};

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

class SkiplistLookupBuilder : public BaseSkiplistLookupBuilder {

  public:
   SkiplistLookupBuilder(
       Transaction* trx,
       std::vector<std::vector<arangodb::aql::AstNode const*>>&,
       arangodb::aql::Variable const*, bool);

    ~SkiplistLookupBuilder() {}

/// @brief Compute the next lookup values
///        If returns false there is no further lookup
    bool next() override;

};

class SkiplistInLookupBuilder : public BaseSkiplistLookupBuilder {

  private:

    struct PosStruct {
      size_t field;
      size_t current;
      size_t _max; // thanks, windows.h!

      PosStruct(size_t f, size_t c, size_t m) : field(f), current(c), _max(m) {}
    };

    TransactionBuilderLeaser _dataBuilder;
    /// @brief keeps track of the positions in the in-lookup
    /// values. (field, inPosition, maxPosition)
    std::list<PosStruct> _inPositions;

    bool _done;

  public:
   SkiplistInLookupBuilder(
       Transaction* trx,
       std::vector<std::vector<arangodb::aql::AstNode const*>>&,
       arangodb::aql::Variable const*, bool);

    ~SkiplistInLookupBuilder() {}

/// @brief Compute the next lookup values
///        If returns false there is no further lookup
    bool next() override;

  private:

    bool forwardInPosition();

    void buildSearchValues();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
///
/// Intervals are open in the sense that both end points are not members
/// of the interval. This means that one has to use SkipList::nextNode
/// on the start node to get the first element and that the stop node
/// can be NULL. Note that it is ensured that all intervals in an iterator
/// are non-empty.
////////////////////////////////////////////////////////////////////////////////

class SkiplistIterator final : public IndexIterator {
 private:
  friend class SkiplistIndex;

 private:
  // Shorthand for the skiplist node
  typedef arangodb::basics::SkipListNode<VPackSlice,
                                         TRI_index_element_t> Node;

 private:
  bool _reverse;
  Node* _cursor;

  Node* _leftEndPoint;   // Interval left border, first excluded element
  Node* _rightEndPoint;  // Interval right border, first excluded element

 public:
  SkiplistIterator(bool reverse, Node* left,
                   Node* right)
      : _reverse(reverse),
        _leftEndPoint(left),
        _rightEndPoint(right) {
    reset(); // Initializes the cursor
  }

  // always holds the last node returned, initially equal to
  // the _leftEndPoint (or the
  // _rightEndPoint in the reverse case),
  // can be nullptr if the iterator is exhausted.

 public:

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next element in the skiplist
  ////////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* next() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the cursor
  ////////////////////////////////////////////////////////////////////////////////

  void reset() override;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
///
/// Intervals are open in the sense that both end points are not members
/// of the interval. This means that one has to use SkipList::nextNode
/// on the start node to get the first element and that the stop node
/// can be NULL. Note that it is ensured that all intervals in an iterator
/// are non-empty.
////////////////////////////////////////////////////////////////////////////////

class SkiplistIterator2 final : public IndexIterator {
 private:
  // Shorthand for the skiplist node
  typedef arangodb::basics::SkipListNode<VPackSlice,
                                         TRI_index_element_t> Node;

  typedef arangodb::basics::SkipList<VPackSlice,
                                     TRI_index_element_t> TRI_Skiplist;

 private:

  TRI_Skiplist const* _skiplistIndex;
  bool _reverse;
  Node* _cursor;

  // The pair.first is the left border
  // The pair.second is the right border
  // Both borders are inclusive
  std::vector<std::pair<Node*, Node*>> _intervals;
  size_t _currentInterval;

  BaseSkiplistLookupBuilder* _builder;

  std::function<int(TRI_index_element_t const*, TRI_index_element_t const*,
                      arangodb::basics::SkipListCmpType)> _CmpElmElm;

 public:
  SkiplistIterator2(
      TRI_Skiplist const* skiplist,
      std::function<int(TRI_index_element_t const*, TRI_index_element_t const*,
                        arangodb::basics::SkipListCmpType)> const& CmpElmElm,
      bool reverse, BaseSkiplistLookupBuilder* builder)
      : _skiplistIndex(skiplist),
        _reverse(reverse),
        _cursor(nullptr),
        _currentInterval(0),
        _builder(builder),
        _CmpElmElm(CmpElmElm) {
    TRI_ASSERT(_builder != nullptr);
    initNextInterval(); // Initializes the cursor
    TRI_ASSERT((_intervals.empty() && _cursor == nullptr) ||
               (!_intervals.empty() && _cursor != nullptr));
  }

  ~SkiplistIterator2() {
    delete _builder;
  }

  // always holds the last node returned, initially equal to
  // the _leftEndPoint (or the
  // _rightEndPoint in the reverse case),
  // can be nullptr if the iterator is exhausted.

 public:

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next element in the skiplist
  ////////////////////////////////////////////////////////////////////////////////

  TRI_doc_mptr_t* next() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the cursor
  ////////////////////////////////////////////////////////////////////////////////

  void reset() override;

 private:

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize left and right endpoints with current lookup
  ///        value. Also points the _cursor to the border of this interval.
  ////////////////////////////////////////////////////////////////////////////////

  void initNextInterval();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Forward the cursor to the next interval. If there was no
  ///        interval the next one is computed. If the _cursor has
  ///        nullptr after this call the iterator is exhausted.
  ////////////////////////////////////////////////////////////////////////////////

  void forwardCursor();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Checks if the interval is valid. It is declared invalid if
  ///        one border is nullptr or the right is lower than left.
  ////////////////////////////////////////////////////////////////////////////////

  bool intervalValid(Node*, Node*) const;
};



class SkiplistIndex final : public PathBasedIndex {
  struct KeyElementComparator {
    int operator()(VPackSlice const* leftKey,
                   TRI_index_element_t const* rightElement) const;

    explicit KeyElementComparator(SkiplistIndex* idx) { _idx = idx; }

   private:
    SkiplistIndex* _idx;
  };

  struct ElementElementComparator {
    int operator()(TRI_index_element_t const* leftElement,
                   TRI_index_element_t const* rightElement,
                   arangodb::basics::SkipListCmpType cmptype) const;

    explicit ElementElementComparator(SkiplistIndex* idx) { _idx = idx; }

   private:
    SkiplistIndex* _idx;
  };

  friend class SkiplistIterator;
  friend struct KeyElementComparator;
  friend struct ElementElementComparator;

  typedef arangodb::basics::SkipList<VPackSlice,
                                     TRI_index_element_t> TRI_Skiplist;

 public:
  SkiplistIndex() = delete;

  SkiplistIndex(TRI_idx_iid_t, LogicalCollection*,
                arangodb::velocypack::Slice const&);

  SkiplistIndex(
      TRI_idx_iid_t, arangodb::LogicalCollection*,
      std::vector<std::vector<arangodb::basics::AttributeName>> const&, bool,
      bool);

  explicit SkiplistIndex(VPackSlice const&);

  ~SkiplistIndex();

 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_SKIPLIST_INDEX;
  }
  
  bool allowExpansion() const override final { return true; }

  bool canBeDropped() const override final { return true; }

  bool isSorted() const override final { return true; }

  bool hasSelectivityEstimate() const override final { return false; }

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;
  void toVelocyPackFigures(VPackBuilder&) const override final;

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;
  
  int unload() override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief attempts to locate an entry in the skip list index
  ///
  /// Warning: who ever calls this function is responsible for destroying
  /// the velocypack::Slice and the SkiplistIterator* results
  //////////////////////////////////////////////////////////////////////////////

  SkiplistIterator* lookup(arangodb::Transaction*, arangodb::velocypack::Slice const,
                           bool) const;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t,
                             double&, size_t&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      IndexIteratorContext*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

 private:
  bool isDuplicateOperator(arangodb::aql::AstNode const*,
                           std::unordered_set<int> const&) const;

  bool accessFitsIndex(
      arangodb::aql::AstNode const*, arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      bool) const;

  bool accessFitsIndex(
      arangodb::aql::AstNode const*, arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::vector<std::vector<arangodb::aql::AstNode const*>>&) const;


  void matchAttributes(
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      size_t&, bool) const;

  bool findMatchingConditions(
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::vector<std::vector<arangodb::aql::AstNode const*>>&, bool&) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Checks if the interval is valid. It is declared invalid if
  ///        one border is nullptr or the right is lower than left.
  ////////////////////////////////////////////////////////////////////////////////

  // Shorthand for the skiplist node
  typedef arangodb::basics::SkipListNode<VPackSlice,
                                         TRI_index_element_t> Node;

  bool intervalValid(Node* left, Node* right) const;

 private:

  ElementElementComparator CmpElmElm;

  KeyElementComparator CmpKeyElm;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual skiplist index
  //////////////////////////////////////////////////////////////////////////////

  TRI_Skiplist* _skiplistIndex;
};
}

#endif
