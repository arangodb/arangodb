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

#ifndef ARANGOD_INDEXES_FULLTEXT_INDEX_H
#define ARANGOD_INDEXES_FULLTEXT_INDEX_H 1

#include "Basics/Common.h"
#include "FulltextIndex/fulltext-common.h"
#include "Indexes/Index.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class FulltextIndex final : public Index {
 public:
  FulltextIndex() = delete;

  FulltextIndex(TRI_idx_iid_t, arangodb::LogicalCollection*, std::string const&,
                int);

  FulltextIndex(TRI_idx_iid_t, LogicalCollection*,
                arangodb::velocypack::Slice const&);

  ~FulltextIndex();

 public:
  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_FULLTEXT_INDEX;
  }
  
  bool allowExpansion() const override final { return false; }
  
  bool canBeDropped() const override final { return true; }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return false; }

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(VPackSlice const&) const override final;

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int unload() override final;

  int cleanup() override final;

  bool isSame(std::string const& field, int minWordLength) const {
    std::string fieldString;
    TRI_AttributeNamesToString(fields()[0], fieldString);
    return (_minWordLength == minWordLength && fieldString == field);
  }

  TRI_fts_index_t* internals() { return _fulltextIndex; }

 private:
  std::vector<std::string> wordlist(struct TRI_doc_mptr_t const*);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the indexed attribute (path)
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::string> _attr;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the fulltext index
  //////////////////////////////////////////////////////////////////////////////

  TRI_fts_index_t* _fulltextIndex;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief minimum word length
  //////////////////////////////////////////////////////////////////////////////

  int _minWordLength;
};
}

#endif
