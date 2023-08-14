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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchLink.h"

#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "VocBase/Identifiers/IndexId.h"

namespace arangodb {

struct IndexTypeFactory;

namespace iresearch {

class IResearchLinkMock final : public Index, public IResearchLink {
  Index& index() noexcept final { return *this; }
  Index const& index() const noexcept final { return *this; }

 public:
  IResearchLinkMock(IndexId iid, LogicalCollection& collection);

  ~IResearchLinkMock() final { IResearchLink::unload(); }

  [[nodiscard]] static auto setCallbackForScope(
      std::function<irs::directory_attributes()> const& callback) {
    InitCallback = callback;
    return irs::Finally{[]() noexcept { InitCallback = nullptr; }};
  }

  bool canBeDropped() const final { return IResearchLink::canBeDropped(); }

  Result drop() final { return IResearchLink::drop(); }

  bool hasSelectivityEstimate() const final {
    return IResearchDataStore::hasSelectivityEstimate();
  }

  Result insert(transaction::Methods& trx, LocalDocumentId documentId,
                velocypack::Slice doc) {
    return IResearchDataStore::insert<FieldIterator<FieldMeta>,
                                      IResearchLinkMeta>(trx, documentId, doc,
                                                         meta());
  }

  void recoveryInsert(uint64_t tick, LocalDocumentId documentId,
                      velocypack::Slice doc) {
    IResearchDataStore::recoveryInsert<FieldIterator<FieldMeta>,
                                       IResearchLinkMeta>(tick, documentId, doc,
                                                          meta());
  }

  Result remove(transaction::Methods& trx, LocalDocumentId documentId);

  bool isSorted() const final { return IResearchLink::isSorted(); }

  bool isHidden() const final { return IResearchLink::isHidden(); }

  bool needsReversal() const final { return true; }

  void load() final {}

  bool matchesDefinition(velocypack::Slice const& slice) const final {
    return IResearchLink::matchesDefinition(slice);
  }

  size_t memory() const final {
    // FIXME return in memory size
    return stats().indexSize;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack;  // for std::shared_ptr<Builder>
                              // Index::toVelocyPack(bool, Index::Serialize)
  void toVelocyPack(velocypack::Builder& builder,
                    std::underlying_type<Index::Serialize>::type) const final;

  void toVelocyPackFigures(velocypack::Builder& builder) const final {
    IResearchDataStore::toVelocyPackStats(builder);
  }

  IndexType type() const final { return Index::TRI_IDX_TYPE_IRESEARCH_LINK; }

  char const* typeName() const final { return oldtypeName(); }

  void unload() final {
    auto res = IResearchLink::unload();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  static std::function<irs::directory_attributes()> InitCallback;
};

}  // namespace iresearch
}  // namespace arangodb
