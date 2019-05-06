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

namespace {

class RocksDBCipherStream final : public irs::encryption::stream {
 public:
  typedef std::unique_ptr<rocksdb::BlockAccessCipherStream> StreamPtr;

  explicit RocksDBCipherStream(StreamPtr&& stream) noexcept
    : _stream(std::move(stream)) {
    TRI_ASSERT(_stream);
  }

  virtual size_t block_size() const override {
    return _stream->BlockSize();
  }

  virtual bool decrypt(uint64_t offset, irs::byte_type* data, size_t size) override {
    return _stream->Decrypt(offset, reinterpret_cast<char*>(data), size).ok();
  }

  virtual bool encrypt(uint64_t offset, irs::byte_type* data, size_t size) override {
    return _stream->Encrypt(offset, reinterpret_cast<char*>(data), size).ok();
  }

 private:
  StreamPtr _stream;
}; // RocksDBCipherStream

class RocksDBEncryptionProvider final : public irs::encryption {
 public:
  static std::shared_ptr<RocksDBEncryptionProvider> make(
      rocksdb::EncryptionProvider& encryption,
      rocksdb::Options const& options) {
    return std::make_shared<RocksDBEncryptionProvider>(encryption, options);
  }

  explicit RocksDBEncryptionProvider(
      rocksdb::EncryptionProvider& encryption,
      rocksdb::Options const& options)
    : _encryption(&encryption),
      _options(options) {
  }

  virtual size_t header_length() override {
    return _encryption->GetPrefixLength();
  }

  virtual bool create_header(
      std::string const& filename,
      irs::byte_type* header) override {
    return _encryption->CreateNewPrefix(
      filename, reinterpret_cast<char*>(header), header_length()
    ).ok();
  }

  virtual encryption::stream::ptr create_stream(
      std::string const& filename,
      irs::byte_type* header) override {
    rocksdb::Slice headerSlice(
      reinterpret_cast<char const*>(header),
      header_length()
    );

    std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
    if (!_encryption->CreateCipherStream(filename, _options, headerSlice, &stream).ok()) {
      return nullptr;
    }

    return std::make_unique<RocksDBCipherStream>(std::move(stream));
  }

 private:
  rocksdb::EncryptionProvider* _encryption;
  rocksdb::EnvOptions _options;
}; // RocksDBEncryptionProvider

std::function<void(irs::directory&)> const RocksDBLinkInitCallback = [](irs::directory& dir) {
  TRI_ASSERT(arangodb::EngineSelectorFeature::isRocksDB());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* engine = dynamic_cast<arangodb::RocksDBEngine*>(arangodb::EngineSelectorFeature::ENGINE);
#else
  auto* engine = static_cast<arangodb::RocksDBEngine*>(arangodb::EngineSelectorFeature::ENGINE);
#endif

  auto* encryption = engine ? engine->encryptionProvider() : nullptr;

  if (encryption) {
    dir.attributes().emplace<RocksDBEncryptionProvider>(
      *encryption, engine->rocksDBOptions()
    );
  }
};

}

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchRocksDBLink-specific implementation of an IndexTypeFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchRocksDBLink::IndexFactory : public arangodb::IndexTypeFactory {
  virtual bool equal(arangodb::velocypack::Slice const& lhs,
                     arangodb::velocypack::Slice const& rhs) const override {
    return arangodb::iresearch::IResearchLinkHelper::equal(lhs, rhs);
  }

  virtual arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                                       arangodb::LogicalCollection& collection,
                                       arangodb::velocypack::Slice const& definition,
                                       TRI_idx_iid_t id,
                                       bool /*isClusterConstructor*/) const override {
    try {
      auto link =
          std::shared_ptr<IResearchRocksDBLink>(new IResearchRocksDBLink(id, collection));
      auto res = link->init(definition, RocksDBLinkInitCallback);

      if (!res.ok()) {
        return res;
      }

      index = link;
    } catch (arangodb::basics::Exception const& e) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(e.code(),
                              std::string("caught exception while creating "
                                          "arangosearch view RocksDB link '") +
                                  std::to_string(id) + "': " + e.what());
    } catch (std::exception const& e) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string("caught exception while creating "
                                          "arangosearch view RocksDB link '") +
                                  std::to_string(id) + "': " + e.what());
    } catch (...) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string("caught exception while creating "
                                          "arangosearch view RocksDB link '") +
                                  std::to_string(id) + "'");
    }

    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    return IResearchLinkHelper::normalize( // normalize
      normalized, definition, isCreation, vocbase // args
    );
  }
};

IResearchRocksDBLink::IResearchRocksDBLink(TRI_idx_iid_t iid,
                                           arangodb::LogicalCollection& collection)
    : RocksDBIndex(iid, collection, IResearchLinkHelper::emptyIndexSlice(),
                   RocksDBColumnFamily::invalid(), false),
      IResearchLink(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

/*static*/ arangodb::IndexTypeFactory const& IResearchRocksDBLink::factory() {
  static const IndexFactory factory;

  return factory;
}

void IResearchRocksDBLink::toVelocyPack( // generate definition
    arangodb::velocypack::Builder& builder, // destination buffer
    std::underlying_type<arangodb::Index::Serialize>::type flags // definition flags
) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("failed to generate link definition for arangosearch view RocksDB link '") + std::to_string(arangodb::Index::id()) + "'"
    ));
  }

  auto forPersistence = // definition for persistence
    arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Internals);

  builder.openObject();

  if (!properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failed to generate link definition for arangosearch view RocksDB link '") + std::to_string(arangodb::Index::id()) + "'"
    ));
  }

  if (arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Internals)) {
    TRI_ASSERT(_objectId != 0);  // If we store it, it cannot be 0
    builder.add("objectId", VPackValue(std::to_string(_objectId)));
  }

  if (arangodb::Index::hasFlag(flags, arangodb::Index::Serialize::Figures)) {
    VPackBuilder figuresBuilder;

    figuresBuilder.openObject();
    toVelocyPackFigures(figuresBuilder);
    figuresBuilder.close();
    builder.add("figures", figuresBuilder.slice());
  }

  builder.close();
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------