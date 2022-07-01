////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchInvertedIndexMeta.h"
#include "Indexes/Index.h"

namespace arangodb {
namespace iresearch {


class IResearchInvertedIndexMock final : public Index,
                                         public IResearchInvertedIndex {
public:
  IResearchInvertedIndexMock(IndexId iid, LogicalCollection &collection);

  //  [[nodiscard]] static auto
  //  setCallbakForScope(std::function<irs::directory_attributes()> callback) {
  //    InitCallback = callback;
  //    return irs::make_finally([]() noexcept { InitCallback = nullptr; });
  //  }

  bool canBeDropped() const override { return false; }

  Result drop() override { return Result(ErrorCode(0)); }

  bool hasSelectivityEstimate() const override {
    return IResearchDataStore::hasSelectivityEstimate();
  }

  Result insert(transaction::Methods &trx, LocalDocumentId const &documentId,
                velocypack::Slice const doc) {
    IResearchInvertedIndexMeta meta;
    using InvertedIndexFieldIterator = arangodb::iresearch::FieldIterator<
        arangodb::iresearch::IResearchInvertedIndexMeta,
        arangodb::iresearch::InvertedIndexField>;

    return IResearchDataStore::insert<InvertedIndexFieldIterator,
                                      IResearchInvertedIndexMeta>(
        trx, documentId, doc, meta);
  }

  bool isSorted() const override { return IResearchInvertedIndex::isSorted(); }

  bool isHidden() const override { return false; }

  bool needsReversal() const override { return true; }

  void load() override { /*IResearchDataStore::load();*/
  }

  bool matchesDefinition(velocypack::Slice const &slice) const override {
    return false;
  }

  size_t memory() const override {
    // FIXME return in memory size
    return stats().indexSize;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack; // for std::shared_ptr<Builder>
                             // Index::toVelocyPack(bool, Index::Serialize)
                             //  void
                             //  toVelocyPack(velocypack::Builder &builder,
  //               std::underlying_type<Index::Serialize>::type) const override;

  void toVelocyPackFigures(velocypack::Builder &builder) const override {
    IResearchInvertedIndex::toVelocyPackStats(builder);
  }

  IndexType type() const override { return Index::TRI_IDX_TYPE_INVERTED_INDEX; }

  char const *typeName() const override { return "inverted"; }

  void unload() override {}

  //  static std::function<irs::directory_attributes()> InitCallback;
};

} // namespace iresearch
} // namespace arangodb
