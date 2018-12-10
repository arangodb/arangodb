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

#ifndef ARANGOD_MMFILES_FULLTEXT_INDEX_H
#define ARANGOD_MMFILES_FULLTEXT_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesIndex.h"
#include "MMFiles/mmfiles-fulltext-common.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
class LocalDocumentId;

class MMFilesFulltextIndex final : public MMFilesIndex {
 public:
  MMFilesFulltextIndex() = delete;

  MMFilesFulltextIndex(
    TRI_idx_iid_t iid,
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
  );

  ~MMFilesFulltextIndex();

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_FULLTEXT_INDEX; }

  char const* typeName() const override { return "fulltext"; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, std::underlying_type<Serialize>::type) const override;

  bool matchesDefinition(VPackSlice const&) const override;

  Result insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

  Result remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  ) override;

  void load() override {}
  void unload() override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override final;

  bool isSame(std::string const& field, int minWordLength) const {
    std::string fieldString;
    TRI_AttributeNamesToString(fields()[0], fieldString);
    return (_minWordLength == minWordLength && fieldString == field);
  }

  TRI_fts_index_t* internals() { return _fulltextIndex; }

 private:
  std::set<std::string> wordlist(arangodb::velocypack::Slice const&);
  void extractWords(std::set<std::string>& words,
                    arangodb::velocypack::Slice value, int level) const;

 private:
  /// @brief the indexed attribute (path)
  std::vector<std::string> _attr;

  /// @brief the fulltext index
  TRI_fts_index_t* _fulltextIndex;

  /// @brief minimum word length
  int _minWordLength;
};

/// El Cheapo index iterator
class MMFilesFulltextIndexIterator : public IndexIterator {
 public:
  MMFilesFulltextIndexIterator(LogicalCollection* collection,
                               transaction::Methods* trx,
                               std::set<TRI_voc_rid_t>&& docs)
      : IndexIterator(collection, trx),
        _docs(std::move(docs)),
        _pos(_docs.begin()) {}

  ~MMFilesFulltextIndexIterator() {}

  char const* typeName() const override { return "fulltext-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    TRI_ASSERT(limit > 0);
    while (_pos != _docs.end() && limit > 0) {
      cb(LocalDocumentId(*_pos));
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
  std::set<TRI_voc_rid_t> const _docs;
  std::set<TRI_voc_rid_t>::iterator _pos;
};
}

#endif