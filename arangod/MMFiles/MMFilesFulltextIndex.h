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
#include "Indexes/Index.h"
#include "MMFiles/fulltext-common.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

// TRI_fulltext_doc_t must be capable of storing revision ids
static_assert(sizeof(TRI_fulltext_doc_t) >= sizeof(TRI_voc_rid_t), "invalid size of TRI_fulltext_doc_t");

namespace arangodb {

class MMFilesFulltextIndex final : public Index {
 public:
  MMFilesFulltextIndex() = delete;

  MMFilesFulltextIndex(TRI_idx_iid_t, LogicalCollection*,
                arangodb::velocypack::Slice const&);

  ~MMFilesFulltextIndex();

 public:
  IndexType type() const override {
    return Index::TRI_IDX_TYPE_FULLTEXT_INDEX;
  }
  
  bool allowExpansion() const override { return false; }
  
  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool) const override;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(VPackSlice const&) const override;

  int insert(arangodb::Transaction*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback) override;

  int remove(arangodb::Transaction*, TRI_voc_rid_t, arangodb::velocypack::Slice const&, bool isRollback) override;

  int unload() override;

  int cleanup() override;

  bool isSame(std::string const& field, int minWordLength) const {
    std::string fieldString;
    TRI_AttributeNamesToString(fields()[0], fieldString);
    return (_minWordLength == minWordLength && fieldString == field);
  }

  TRI_fts_index_t* internals() { return _fulltextIndex; }

  static TRI_fulltext_doc_t fromRevision(TRI_voc_rid_t revisionId) {
    return static_cast<TRI_fulltext_doc_t>(revisionId);
  }

  static TRI_voc_rid_t toRevision(TRI_fulltext_doc_t internal) {
    return static_cast<TRI_voc_rid_t>(internal);
  }

 private:
  std::vector<std::string> wordlist(arangodb::velocypack::Slice const&);

 private:
  /// @brief the indexed attribute (path)
  std::vector<std::string> _attr;

  /// @brief the fulltext index
  TRI_fts_index_t* _fulltextIndex;

  /// @brief minimum word length
  int _minWordLength;
};
}

#endif
