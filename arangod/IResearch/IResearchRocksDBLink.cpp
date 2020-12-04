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

#include "Basics/Common.h"  // required for RocksDBColumnFamily.h
#include "IResearchLinkHelper.h"
#include "IResearchView.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include <rocksdb/env_encryption.h>

#include "IResearchRocksDBLink.h"
#include "utils/encryption.hpp"

namespace arangodb {
namespace iresearch {

IResearchRocksDBLink::IResearchRocksDBLink(IndexId iid, LogicalCollection& collection)
    : RocksDBIndex(iid, collection, IResearchLinkHelper::emptyIndexSlice(),
                   RocksDBColumnFamily::invalid(), false),
      IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

void IResearchRocksDBLink::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(Result(  // result
        TRI_ERROR_BAD_PARAMETER,              // code
        std::string("failed to generate link definition for arangosearch view "
                    "RocksDB link '") +
            std::to_string(Index::id().id()) + "'"));
  }

  auto forPersistence = Index::hasFlag(flags, Index::Serialize::Internals);

  builder.openObject();

  if (!IResearchLink::properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to generate link definition for arangosearch view "
                    "RocksDB link '") +
            std::to_string(Index::id().id()) + "'"));
  }

  if (Index::hasFlag(flags, Index::Serialize::Internals)) {
    TRI_ASSERT(objectId() != 0);  // If we store it, it cannot be 0
    builder.add("objectId", VPackValue(std::to_string(objectId())));
  }

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

class RocksDBCipherStream final : public irs::encryption::stream {
 public:
  typedef std::unique_ptr<rocksdb::BlockAccessCipherStream> StreamPtr;

  explicit RocksDBCipherStream(StreamPtr&& stream) noexcept
      : _stream(std::move(stream)) {
    TRI_ASSERT(_stream);
  }

  virtual size_t block_size() const override { return _stream->BlockSize(); }

  virtual bool decrypt(uint64_t offset, irs::byte_type* data, size_t size) override {
    return _stream->Decrypt(offset, reinterpret_cast<char*>(data), size).ok();
  }

  virtual bool encrypt(uint64_t offset, irs::byte_type* data, size_t size) override {
    return _stream->Encrypt(offset, reinterpret_cast<char*>(data), size).ok();
  }

 private:
  StreamPtr _stream;
};  // RocksDBCipherStream

class RocksDBEncryptionProvider final : public irs::encryption {
 public:
  static std::shared_ptr<RocksDBEncryptionProvider> make(rocksdb::EncryptionProvider& encryption,
                                                         rocksdb::Options const& options) {
    return std::make_shared<RocksDBEncryptionProvider>(encryption, options);
  }

  explicit RocksDBEncryptionProvider(rocksdb::EncryptionProvider& encryption,
                                     rocksdb::Options const& options)
      : _encryption(&encryption), _options(options) {}

  virtual size_t header_length() override {
    return _encryption->GetPrefixLength();
  }

  virtual bool create_header(std::string const& filename, irs::byte_type* header) override {
    return _encryption
        ->CreateNewPrefix(filename, reinterpret_cast<char*>(header), header_length())
        .ok();
  }

  virtual encryption::stream::ptr create_stream(std::string const& filename,
                                                irs::byte_type* header) override {
    rocksdb::Slice headerSlice(reinterpret_cast<char const*>(header), header_length());

    std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
    if (!_encryption
             ->CreateCipherStream(filename, _options, headerSlice, &stream)
             .ok()) {
      return nullptr;
    }

    return std::make_unique<RocksDBCipherStream>(std::move(stream));
  }

 private:
  rocksdb::EncryptionProvider* _encryption;
  rocksdb::EnvOptions _options;
};  // RocksDBEncryptionProvider

IResearchRocksDBLink::IndexFactory::IndexFactory(application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {}

bool IResearchRocksDBLink::IndexFactory::equal(
    VPackSlice const& lhs,
    VPackSlice const& rhs,
    std::string const& dbname) const {
  return IResearchLinkHelper::equal(_server, lhs, rhs, dbname);
}

std::shared_ptr<Index> IResearchRocksDBLink::IndexFactory::instantiate(
    LogicalCollection& collection, VPackSlice const& definition,
    IndexId id, bool /*isClusterConstructor*/) const {
  auto link = std::make_shared<IResearchRocksDBLink>(id, collection);

  auto const res = link->init(definition, [](irs::directory& dir) {
    TRI_ASSERT(arangodb::EngineSelectorFeature::isRocksDB());

  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* engine =
        dynamic_cast<arangodb::RocksDBEngine*>(arangodb::EngineSelectorFeature::ENGINE);
  #else
    auto* engine =
        static_cast<arangodb::RocksDBEngine*>(arangodb::EngineSelectorFeature::ENGINE);
  #endif

    auto* encryption = engine ? engine->encryptionProvider() : nullptr;

    if (encryption) {
      dir.attributes().emplace<RocksDBEncryptionProvider>(*encryption,
                                                          engine->rocksDBOptions());
    }
  });

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return link;
}

Result IResearchRocksDBLink::IndexFactory::normalize(
    VPackBuilder& normalized,
    VPackSlice definition,
    bool isCreation,
    TRI_vocbase_t const& vocbase) const {
  return IResearchLinkHelper::normalize(
      normalized, definition, isCreation, vocbase);
}

std::shared_ptr<IResearchRocksDBLink::IndexFactory> IResearchRocksDBLink::createFactory(
    application_features::ApplicationServer& server) {
  return std::shared_ptr<IResearchRocksDBLink::IndexFactory>(
      new IResearchRocksDBLink::IndexFactory(server));
}

}  // namespace iresearch
}  // namespace arangodb
