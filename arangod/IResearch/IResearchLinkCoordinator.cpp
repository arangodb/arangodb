////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchLinkCoordinator.h"
#include "IResearchLinkHelper.h"
#include "IResearchLinkMeta.h"
#include "IResearchFeature.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBIndex.h"

#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

namespace {

using namespace arangodb::iresearch;

class IResearchLinkCoordinator {
 public:
  virtual ~IResearchLinkCoordinator() = default;

  bool init(VPackSlice definition) {
    std::string error;
    IResearchLinkMeta meta;

    if (!meta.init(definition, error)) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "error parsing view link parameters from json: " << error;
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);

      return false; // failed to parse metadata
    }

    return true;
  }

  bool matchesDefinition(VPackSlice const& slice) const {
    IResearchLinkMeta rhs;
    std::string errorField;

    return rhs.init(slice, errorField) && _meta == rhs;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept {
    return _meta.memory();
  }

 protected:
  IResearchLinkMeta _meta;
}; // IResearchLinkCoordinator

class IResearchLinkMMFilesCoordinator final
    : public arangodb::Index,
      public IResearchLinkCoordinator {
 public:
  IResearchLinkMMFilesCoordinator(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection
  ) : Index(iid, collection, IResearchLinkHelper::emptyIndexSlice()) {
    _unique = false; // cannot be unique since multiple fields are indexed
    _sparse = true;  // always sparse
  }

  bool allowExpansion() const noexcept {
    return true;
  }

  void batchInsert(
      arangodb::transaction::Methods*,
      std::vector<std::pair<arangodb::LocalDocumentId, VPackSlice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue>
  ) override {
    TRI_ASSERT(false); // should not be called
  }

  bool canBeDropped() const noexcept override {
    return true;
  }

  int drop() {
    // FIXME
    return TRI_ERROR_NO_ERROR;
  }

  bool hasBatchInsert() const noexcept override {
    return true;
  }

  bool hasSelectivityEstimate() const noexcept override {
    // selectivity can only be determined per query since multiple fields are indexed
    return false;
  }

  arangodb::Result insert(
      arangodb::transaction::Methods* trx,
      arangodb::LocalDocumentId const& documentId,
      VPackSlice const& doc,
      Index::OperationMode mode
  ) override {
    TRI_ASSERT(false); // should not be called
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  bool isPersistent() const noexcept override {
    return true;
  }

  bool isSorted() const noexcept override {
    // IResearch does not provide a fixed default sort order
    return false;
  }

  void load() noexcept {
    // NOOP
  }

  bool matchesDefinition(VPackSlice const& slice) const override {
    return IResearchLinkCoordinator::matchesDefinition(slice);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept override {
    return IResearchLinkCoordinator::memory();
  }

  arangodb::Result remove(
      arangodb::transaction::Methods*,
      arangodb::LocalDocumentId const&,
      VPackSlice const&,
      Index::OperationMode
  ) override {
    TRI_ASSERT(false); // should not be called
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type enum value
  ////////////////////////////////////////////////////////////////////////////////
  Index::IndexType type() const noexcept override {
    return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const noexcept override {
    return IResearchLinkHelper::type().c_str();
  }

  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      bool withFigures,
      bool /*forPersistence*/
  ) const {
    TRI_ASSERT(!builder.isOpenObject());
    builder.openObject();
    {
      bool const success = _meta.json(builder);
      TRI_ASSERT(success);
    }
    builder.add("id", VPackValue(std::to_string(id())));
    IResearchLinkHelper::setType(builder);

    if (withFigures) {
      VPackBuilder figuresBuilder;

      figuresBuilder.openObject();
      toVelocyPackFigures(figuresBuilder);
      figuresBuilder.close();
      builder.add("figures", figuresBuilder.slice());
    }

    builder.close();
  }

  void unload() noexcept override {
    // NOOP
  }
}; // IResearchLinkMMFilesCoordinator

class IResearchLinkRocksDBCoordinator final
    : public arangodb::RocksDBIndex,
      public IResearchLinkCoordinator {
 public:
  IResearchLinkRocksDBCoordinator(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection
  ) : arangodb::RocksDBIndex(
        iid,
        collection,
        IResearchLinkHelper::emptyIndexSlice(),
        arangodb::RocksDBColumnFamily::invalid(),
        false
      ) {
    _unique = false; // cannot be unique since multiple fields are indexed
    _sparse = true;  // always sparse
  }

  bool allowExpansion() const noexcept {
    // maps to multivalued
    return true;
  }

  void batchInsert(
      arangodb::transaction::Methods*,
      std::vector<std::pair<arangodb::LocalDocumentId, VPackSlice>> const&,
      std::shared_ptr<arangodb::basics::LocalTaskQueue>
  ) override {
    TRI_ASSERT(false); // should not be called
  }

  bool canBeDropped() const noexcept override {
    return true;
  }

  int drop() {
    // FIXME
    return TRI_ERROR_NO_ERROR;
  }

  bool hasBatchInsert() const noexcept override {
    return true;
  }

  bool hasSelectivityEstimate() const noexcept override {
    // selectivity can only be determined per query since multiple fields are indexed
    return false;
  }

  arangodb::Result insertInternal(
      arangodb::transaction::Methods*,
      arangodb::RocksDBMethods*,
      arangodb::LocalDocumentId const&,
      const VPackSlice&,
      OperationMode
  ) override {
    TRI_ASSERT(false); // should not be called
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  bool isSorted() const noexcept override {
    // IResearch does not provide a fixed default sort order
    return false;
  }

  void load() noexcept {
    // NOOP
  }

  bool matchesDefinition(VPackSlice const& slice) const override {
    IResearchLinkMeta rhs;
    std::string errorField;

    return rhs.init(slice, errorField) && _meta == rhs;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept override {
    return IResearchLinkCoordinator::memory();
  }

  arangodb::Result removeInternal(
      arangodb::transaction::Methods*,
      arangodb::RocksDBMethods*,
      arangodb::LocalDocumentId const&,
      const VPackSlice&,
      OperationMode
  ) override {
    TRI_ASSERT(false); // should not be called
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type enum value
  ////////////////////////////////////////////////////////////////////////////////
  Index::IndexType type() const noexcept override {
    return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const noexcept override {
    return IResearchLinkHelper::type().c_str();
  }

  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      bool withFigures,
      bool /*forPersistence*/
  ) const {
    TRI_ASSERT(!builder.isOpenObject());
    builder.openObject();
    {
      bool const success = _meta.json(builder);
      TRI_ASSERT(success);
    }
    builder.add("id", VPackValue(std::to_string(id())));
    IResearchLinkHelper::setType(builder);

    if (withFigures) {
      VPackBuilder figuresBuilder;

      figuresBuilder.openObject();
      toVelocyPackFigures(figuresBuilder);
      figuresBuilder.close();
      builder.add("figures", figuresBuilder.slice());
    }

    builder.close();
  }

  void unload() noexcept override {
    // NOOP
  }

 private:
  IResearchLinkMeta _meta;
}; // IResearchLinkRocksDBCoordinator

}

namespace arangodb {
namespace iresearch {

std::shared_ptr<Index> createIResearchMMFilesLinkCoordinator(
    arangodb::LogicalCollection* collection,
    arangodb::velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool /*isClusterConstructor*/
) noexcept {
  try {
    auto link = std::make_shared<IResearchLinkMMFilesCoordinator>(id, collection);

    return link->init(definition) ? link : nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'" << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'";
  }

  return nullptr;
}

std::shared_ptr<Index> createIResearchRocksDBLinkCoordinator(
    arangodb::LogicalCollection* collection,
    arangodb::velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool /*isClusterConstructor*/
) noexcept {
  try {
    auto link = std::make_shared<IResearchLinkRocksDBCoordinator>(id, collection);

    return link->init(definition) ? link : nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'" << e.what();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::DEVEL)
      << "caught exception while creating IResearch view MMFiles link '" << id << "'";
  }

  return nullptr;
}

} // iresearch
} // arangodb
