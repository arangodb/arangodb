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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_FULLTEXT_INDEX_H
#define ARANGOD_ROCKSDB_ENGINE_FULLTEXT_INDEX_H 1

#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

/// @brief maximum length of an indexed word in characters
/// a character may consist of up to 4 bytes
#define TRI_FULLTEXT_MAX_WORD_LENGTH 40

/// @brief default minimum word length for a fulltext index
#define TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT 2

/// @brief maximum number of search words in a query
#define TRI_FULLTEXT_SEARCH_MAX_WORDS 32

namespace arangodb {
class LocalDocumentId;

struct FulltextQueryToken {
  /// @brief fulltext query match options
  /// substring not implemented, maybe later
  enum MatchType { COMPLETE, PREFIX, SUBSTRING };
  /// @brief fulltext query logical operators
  enum Operation { AND, OR, EXCLUDE };

  FulltextQueryToken(std::string const& v, MatchType t, Operation o)
      : value(v), matchType(t), operation(o) {}

  std::string value;
  MatchType matchType;
  Operation operation;
};
/// A query consists of a list of tokens evaluated left to right:
/// An AND operation causes the entire result set on the left to
/// be intersected with every result containing the token.
/// Similarly an OR triggers union
typedef std::vector<FulltextQueryToken> FulltextQuery;

class RocksDBFulltextIndex final : public RocksDBIndex {
 public:
  RocksDBFulltextIndex() = delete;

  RocksDBFulltextIndex(TRI_idx_iid_t iid, LogicalCollection& collection,
                       arangodb::velocypack::Slice const& info);

  ~RocksDBFulltextIndex() {}

  IndexType type() const override { return Index::TRI_IDX_TYPE_FULLTEXT_INDEX; }

  char const* typeName() const override { return "fulltext"; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return true; }

  bool hasSelectivityEstimate() const override { return false; }

  void toVelocyPack(VPackBuilder&, std::underlying_type<Index::Serialize>::type) const override;

  bool matchesDefinition(VPackSlice const&) const override;

  bool isSame(std::string const& field, int minWordLength) const {
    std::string fieldString;
    TRI_AttributeNamesToString(fields()[0], fieldString);
    return (_minWordLength == minWordLength && fieldString == field);
  }

  IndexIterator* iteratorForCondition(transaction::Methods* trx, ManagedDocumentResult*,
                                      aql::AstNode const* condNode, aql::Variable const* var,
                                      IndexIteratorOptions const&) override;

  arangodb::Result parseQueryString(std::string const&, FulltextQuery&);
  Result executeQuery(transaction::Methods* trx, FulltextQuery const& query,
                      std::set<LocalDocumentId>& resultSet);

 protected:
  /// insert index elements into the specified write batch.
  Result insertInternal(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& documentId,
                        velocypack::Slice const& doc, Index::OperationMode mode) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& documentId,
                        velocypack::Slice const& doc, Index::OperationMode mode) override;

 private:
  std::set<std::string> wordlist(arangodb::velocypack::Slice const&);

  /// @brief the indexed attribute (path)
  std::vector<std::string> _attr;

  /// @brief minimum word length
  int _minWordLength;

  arangodb::Result applyQueryToken(transaction::Methods* trx, FulltextQueryToken const&,
                                   std::set<LocalDocumentId>& resultSet);
};

/// El Cheapo index iterator
class RocksDBFulltextIndexIterator : public IndexIterator {
 public:
  RocksDBFulltextIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                               std::set<LocalDocumentId>&& docs)
      : IndexIterator(collection, trx), _docs(std::move(docs)), _pos(_docs.begin()) {}

  ~RocksDBFulltextIndexIterator() {}

  char const* typeName() const override { return "fulltext-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    TRI_ASSERT(limit > 0);
    while (_pos != _docs.end() && limit > 0) {
      cb(*_pos);
      ++_pos;
      limit--;
    }
    return _pos != _docs.end();
  }

  void reset() override { _pos = _docs.begin(); }

  void skip(uint64_t count, uint64_t& skipped) override {
    while (_pos != _docs.end() && skipped < count) {
      ++_pos;
      skipped++;
    }
  }

 private:
  std::set<LocalDocumentId> const _docs;
  std::set<LocalDocumentId>::iterator _pos;
};

}  // namespace arangodb

#endif
