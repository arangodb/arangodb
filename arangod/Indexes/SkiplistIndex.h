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
#include "IndexOperators/index-operator.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

typedef struct {
  TRI_shaped_json_t* _fields;  // list of shaped json objects which the
                               // collection should know about
  size_t _numFields;           // Note that the number of fields coming from
                               // a query can be smaller than the number of
                               // fields indexed
} TRI_skiplist_index_key_t;

namespace arangodb {
namespace aql {
class SortCondition;
struct Variable;
}

class SkiplistIndex;
class Transaction;

////////////////////////////////////////////////////////////////////////////////
/// @brief Iterator structure for skip list. We require a start and stop node
///
/// Intervals are open in the sense that both end points are not members
/// of the interval. This means that one has to use SkipList::nextNode
/// on the start node to get the first element and that the stop node
/// can be NULL. Note that it is ensured that all intervals in an iterator
/// are non-empty.
////////////////////////////////////////////////////////////////////////////////

class SkiplistIterator {
 private:
  friend class SkiplistIndex;

 private:
  // Shorthand for the skiplist node
  typedef arangodb::basics::SkipListNode<TRI_skiplist_index_key_t,
                                         TRI_index_element_t> Node;

  struct SkiplistIteratorInterval {
    Node* _leftEndPoint;
    Node* _rightEndPoint;

    SkiplistIteratorInterval()
        : _leftEndPoint(nullptr), _rightEndPoint(nullptr) {}
  };

 private:
  SkiplistIndex const* _index;
  size_t _currentInterval;  // starts with 0, current interval used
  bool _reverse;
  Node* _cursor;
  std::vector<SkiplistIteratorInterval> _intervals;

 public:
  SkiplistIterator(SkiplistIndex const* idx, bool reverse)
      : _index(idx), _currentInterval(0), _reverse(reverse), _cursor(nullptr) {}

  ~SkiplistIterator() {}

  // always holds the last node returned, initially equal to
  // the _leftEndPoint of the first interval (or the
  // _rightEndPoint of the last interval in the reverse
  // case), can be nullptr if there are no intervals
  // (yet), or, in the reverse case, if the cursor is
  // at the end of the last interval. Additionally
  // in the non-reverse case _cursor is set to nullptr
  // if the cursor is exhausted.
  // See SkiplistNextIterationCallback and
  // SkiplistPrevIterationCallback for the exact
  // condition for the iterator to be exhausted.

 public:
  size_t size() const;

  bool hasNext() const;

  TRI_index_element_t* next();

  void initCursor();

  void findHelper(TRI_index_operator_t const* indexOperator,
                  std::vector<SkiplistIteratorInterval>& interval);

 private:
  bool hasPrevIteration() const;
  TRI_index_element_t* prevIteration();

  bool hasNextIteration() const;
  TRI_index_element_t* nextIteration();

  bool findHelperIntervalIntersectionValid(
      SkiplistIteratorInterval const& lInterval,
      SkiplistIteratorInterval const& rInterval,
      SkiplistIteratorInterval& interval);

  bool findHelperIntervalValid(SkiplistIteratorInterval const& interval);
};

class SkiplistIndexIterator final : public IndexIterator {
 public:
  SkiplistIndexIterator(arangodb::Transaction* trx, SkiplistIndex const* index,
                        std::vector<TRI_index_operator_t*> op, bool reverse)
      : _trx(trx),
        _index(index),
        _operators(op),
        _reverse(reverse),
        _currentOperator(0),
        _iterator(nullptr) {}

  ~SkiplistIndexIterator() {
    for (auto& op : _operators) {
      delete op;
    }
    delete _iterator;
  }

  TRI_doc_mptr_t* next() override;

  void reset() override;

 private:
  arangodb::Transaction* _trx;
  SkiplistIndex const* _index;
  std::vector<TRI_index_operator_t*> _operators;
  bool _reverse;
  size_t _currentOperator;
  SkiplistIterator* _iterator;
};

class SkiplistIndex final : public PathBasedIndex {
  struct KeyElementComparator {
    int operator()(TRI_skiplist_index_key_t const* leftKey,
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

  typedef arangodb::basics::SkipList<TRI_skiplist_index_key_t,
                                     TRI_index_element_t> TRI_Skiplist;

 public:
  SkiplistIndex() = delete;

  SkiplistIndex(
      TRI_idx_iid_t, struct TRI_document_collection_t*,
      std::vector<std::vector<arangodb::basics::AttributeName>> const&, bool,
      bool);

  explicit SkiplistIndex(VPackSlice const&);

  ~SkiplistIndex();

 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_SKIPLIST_INDEX;
  }

  bool isSorted() const override final { return true; }

  bool hasSelectivityEstimate() const override final { return false; }

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;
  void toVelocyPackFigures(VPackBuilder&) const override final;

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief attempts to locate an entry in the skip list index
  ///
  /// Note: this function will not destroy the passed slOperator before it
  /// returns
  /// Warning: who ever calls this function is responsible for destroying
  /// the TRI_index_operator_t* and the TRI_skiplist_iterator_t* results
  //////////////////////////////////////////////////////////////////////////////

  SkiplistIterator* lookup(arangodb::Transaction*, TRI_index_operator_t*,
                           bool) const;

  bool supportsFilterCondition(arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&) const override;

  bool supportsSortCondition(arangodb::aql::SortCondition const*,
                             arangodb::aql::Variable const*, size_t,
                             double&) const override;

  IndexIterator* iteratorForCondition(arangodb::Transaction*,
                                      IndexIteratorContext*,
                                      arangodb::aql::Ast*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) const override;

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode*, arangodb::aql::Variable const*) const override;

 private:
  bool isDuplicateOperator(arangodb::aql::AstNode const*,
                           std::unordered_set<int> const&) const;

  int _CmpElmElm(TRI_index_element_t const* leftElement,
                 TRI_index_element_t const* rightElement,
                 arangodb::basics::SkipListCmpType cmptype);

  int _CmpKeyElm(TRI_skiplist_index_key_t const* leftKey,
                 TRI_index_element_t const* rightElement);

  bool accessFitsIndex(
      arangodb::aql::AstNode const*, arangodb::aql::AstNode const*,
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      bool) const;

  void matchAttributes(
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      std::unordered_map<size_t, std::vector<arangodb::aql::AstNode const*>>&,
      size_t&, bool) const;

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
