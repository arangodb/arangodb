//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "StorageEngineMock.h"
#include "Aql/AstNode.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/asio_ns.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/VelocyPackHelper.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/asio/io_context.hpp>

namespace {

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
std::vector<std::vector<arangodb::basics::AttributeName>> const IndexAttributes{
    {arangodb::basics::AttributeName("_from", false)},
    {arangodb::basics::AttributeName("_to", false)}};

/// @brief add a single value node to the iterator's keys
void handleValNode(VPackBuilder* keys, arangodb::aql::AstNode const* valNode) {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->openObject();
  keys->add(arangodb::StaticStrings::IndexEq,
            VPackValuePair(valNode->getStringValue(),
                           valNode->getStringLength(), VPackValueType::String));
  keys->close();

  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

class EdgeIndexIteratorMock final : public arangodb::IndexIterator {
 public:
  typedef std::unordered_multimap<std::string, arangodb::LocalDocumentId> Map;

  EdgeIndexIteratorMock(arangodb::LogicalCollection* collection,
                        arangodb::transaction::Methods* trx, arangodb::Index const* index,
                        Map const& map, std::unique_ptr<VPackBuilder>&& keys)
      : IndexIterator(collection, trx),
        _map(map),
        _begin(_map.begin()),
        _end(_map.end()),
        _keys(std::move(keys)),
        _keysIt(_keys->slice()) {}

  char const* typeName() const override { return "edge-index-iterator-mock"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    while (limit && _begin != _end && _keysIt.valid()) {
      auto key = _keysIt.value();

      if (key.isObject()) {
        key = key.get(arangodb::StaticStrings::IndexEq);
      }

      std::tie(_begin, _end) = _map.equal_range(key.toString());

      while (limit && _begin != _end) {
        cb(_begin->second);
        ++_begin;
        --limit;
      }

      ++_keysIt;
    }

    return _begin != _end || _keysIt.valid();
  }

  void reset() override {
    _keysIt.reset();
    _begin = _map.begin();
    _end = _map.end();
  }

 private:
  Map const& _map;
  Map::const_iterator _begin;
  Map::const_iterator _end;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _keysIt;
};  // EdgeIndexIteratorMock

class EdgeIndexMock final : public arangodb::Index {
 public:
  static std::shared_ptr<arangodb::Index> make(TRI_idx_iid_t iid,
                                               arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition) {
    auto const typeSlice = definition.get("type");

    if (typeSlice.isNone()) {
      return nullptr;
    }

    auto const type =
        arangodb::basics::VelocyPackHelper::getStringRef(typeSlice,
                                                         arangodb::velocypack::StringRef());

    if (type.compare("edge") != 0) {
      return nullptr;
    }

    return std::make_shared<EdgeIndexMock>(iid, collection);
  }

  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool isPersistent() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool isHidden() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override { return sizeof(EdgeIndexMock); }

  void load() override {}
  void unload() override {}
  void afterTruncate(TRI_voc_tick_t) override {
    _edgesFrom.clear();
    _edgesTo.clear();
  }

  void toVelocyPack(VPackBuilder& builder,
                    std::underlying_type<arangodb::Index::Serialize>::type flags) const override {
    builder.openObject();
    Index::toVelocyPack(builder, flags);
    // hard-coded
    builder.add("unique", VPackValue(false));
    builder.add("sparse", VPackValue(false));
    builder.close();
  }

  void toVelocyPackFigures(VPackBuilder& builder) const override {
    Index::toVelocyPackFigures(builder);

    builder.add("from", VPackValue(VPackValueType::Object));
    //_edgesFrom->appendToVelocyPack(builder);
    builder.close();

    builder.add("to", VPackValue(VPackValueType::Object));
    //_edgesTo->appendToVelocyPack(builder);
    builder.close();
  }

  arangodb::Result insert(arangodb::transaction::Methods& trx,
                          arangodb::LocalDocumentId const& documentId,
                          arangodb::velocypack::Slice const& doc, OperationMode) {
    if (!doc.isObject()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const fromValue(arangodb::transaction::helpers::extractFromFromDocument(doc));

    if (!fromValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const toValue(arangodb::transaction::helpers::extractToFromDocument(doc));

    if (!toValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    _edgesFrom.emplace(fromValue.toString(), documentId);
    _edgesTo.emplace(toValue.toString(), documentId);

    return {};  // ok
  }

  arangodb::Result remove(arangodb::transaction::Methods& trx,
                          arangodb::LocalDocumentId const&,
                          arangodb::velocypack::Slice const& doc, OperationMode) {
    if (!doc.isObject()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const fromValue(arangodb::transaction::helpers::extractFromFromDocument(doc));

    if (!fromValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const toValue(arangodb::transaction::helpers::extractToFromDocument(doc));

    if (!toValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    _edgesFrom.erase(fromValue.toString());
    _edgesTo.erase(toValue.toString());

    return {};  // ok
  }

  Index::FilterCosts supportsFilterCondition(
      std::vector<std::shared_ptr<arangodb::Index>> const& /*allIndexes*/,
      arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
      size_t itemsInIndex) const override {
    arangodb::SimpleAttributeEqualityMatcher matcher(IndexAttributes);
    return matcher.matchOne(this, node, reference, itemsInIndex);
  }

  std::unique_ptr<arangodb::IndexIterator> iteratorForCondition(
      arangodb::transaction::Methods* trx, arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const*, arangodb::IndexIteratorOptions const&) override {
    TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

    TRI_ASSERT(node->numMembers() == 1);

    auto comp = node->getMember(0);

    // assume a.b == value
    auto attrNode = comp->getMember(0);
    auto valNode = comp->getMember(1);

    if (attrNode->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
      // got value == a.b  -> flip sides
      std::swap(attrNode, valNode);
    }
    TRI_ASSERT(attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

    if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // a.b == value
      return createEqIterator(trx, attrNode, valNode);
    }

    if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      // a.b IN values
      if (!valNode->isArray()) {
        // a.b IN non-array
        return std::make_unique<arangodb::EmptyIndexIterator>(&_collection, trx);
      }

      return createInIterator(trx, attrNode, valNode);
    }

    // operator type unsupported
    return std::make_unique<arangodb::EmptyIndexIterator>(&_collection, trx);
  }

  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode* node,
                                              arangodb::aql::Variable const* reference) const override {
    arangodb::SimpleAttributeEqualityMatcher matcher(IndexAttributes);

    return matcher.specializeOne(this, node, reference);
  }

  EdgeIndexMock(TRI_idx_iid_t iid, arangodb::LogicalCollection& collection)
      : arangodb::Index(
            iid, collection, arangodb::StaticStrings::IndexNameEdge,
            {{arangodb::basics::AttributeName(arangodb::StaticStrings::FromString, false)},
             {arangodb::basics::AttributeName(arangodb::StaticStrings::ToString, false)}},
            true, false) {}

  std::unique_ptr<arangodb::IndexIterator> createEqIterator(
      arangodb::transaction::Methods* trx, arangodb::aql::AstNode const* attrNode,
      arangodb::aql::AstNode const* valNode) const {
    // lease builder, but immediately pass it to the unique_ptr so we don't leak
    arangodb::transaction::BuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->openArray();

    handleValNode(keys.get(), valNode);
    TRI_IF_FAILURE("EdgeIndex::noIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    keys->close();

    // _from or _to?
    bool const isFrom = (attrNode->stringEquals(arangodb::StaticStrings::FromString));

    return std::make_unique<EdgeIndexIteratorMock>(&_collection, trx, this,
                                                   isFrom ? _edgesFrom : _edgesTo,
                                                   std::move(keys));
  }

  /// @brief create the iterator
  std::unique_ptr<arangodb::IndexIterator> createInIterator(
      arangodb::transaction::Methods* trx, arangodb::aql::AstNode const* attrNode,
      arangodb::aql::AstNode const* valNode) const {
    // lease builder, but immediately pass it to the unique_ptr so we don't leak
    arangodb::transaction::BuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->openArray();

    size_t const n = valNode->numMembers();
    for (size_t i = 0; i < n; ++i) {
      handleValNode(keys.get(), valNode->getMemberUnchecked(i));
      TRI_IF_FAILURE("EdgeIndex::iteratorValNodes") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    TRI_IF_FAILURE("EdgeIndex::noIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    keys->close();

    // _from or _to?
    bool const isFrom = (attrNode->stringEquals(arangodb::StaticStrings::FromString));

    return std::make_unique<EdgeIndexIteratorMock>(&_collection, trx, this,
                                                   isFrom ? _edgesFrom : _edgesTo,
                                                   std::move(keys));
  }

  /// @brief the hash table for _from
  EdgeIndexIteratorMock::Map _edgesFrom;
  /// @brief the hash table for _to
  EdgeIndexIteratorMock::Map _edgesTo;
};  // EdgeIndexMock

class ReverseAllIteratorMock final : public arangodb::IndexIterator {
 public:
  ReverseAllIteratorMock(uint64_t size, arangodb::LogicalCollection* coll,
                         arangodb::transaction::Methods* trx)
      : arangodb::IndexIterator(coll, trx), _end(size), _size(size) {}

  virtual char const* typeName() const override {
    return "ReverseAllIteratorMock";
  }

  virtual void reset() override { _end = _size; }

  virtual bool next(LocalDocumentIdCallback const& callback, size_t limit) override {
    while (_end && limit) {  // `_end` always > 0
      callback(arangodb::LocalDocumentId(_end--));
      --limit;
    }

    return 0 == limit;
  }

 private:
  uint64_t _end;
  uint64_t _size;  // the original size
};                 // ReverseAllIteratorMock

class AllIteratorMock final : public arangodb::IndexIterator {
 public:
  AllIteratorMock(std::unordered_map<arangodb::velocypack::StringRef, PhysicalCollectionMock::DocElement> const& data,
                  arangodb::LogicalCollection& coll, arangodb::transaction::Methods* trx)
      : arangodb::IndexIterator(&coll, trx), _data(data), _it{_data.begin()} {}

  virtual char const* typeName() const override { return "AllIteratorMock"; }

  virtual void reset() override { _it = _data.begin(); }

  virtual bool next(LocalDocumentIdCallback const& callback, size_t limit) override {
    while (_it != _data.end() && limit != 0) {
      callback(_it->second.docId());
      ++_it;
      --limit;
    }
    return 0 == limit;
  }

 private:
  std::unordered_map<arangodb::velocypack::StringRef, PhysicalCollectionMock::DocElement> const& _data;
  std::unordered_map<arangodb::velocypack::StringRef, PhysicalCollectionMock::DocElement>::const_iterator _it;
};  // AllIteratorMock

struct IndexFactoryMock : arangodb::IndexFactory {
  virtual void fillSystemIndexes(arangodb::LogicalCollection& col,
                                 std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const override {
    // NOOP
  }

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(arangodb::LogicalCollection& col,
                              arangodb::velocypack::Slice const& indexesSlice,
                              std::vector<std::shared_ptr<arangodb::Index>>& indexes) const override {
    // NOOP
  }
};

}  // namespace

PhysicalCollectionMock::DocElement::DocElement(
    std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> data, uint64_t docId)
    : _data(data), _docId(docId) {}

arangodb::velocypack::Slice PhysicalCollectionMock::DocElement::data() const {
  return arangodb::velocypack::Slice(_data->data());
}

std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> PhysicalCollectionMock::DocElement::rawData() const {
  return _data;
}

void PhysicalCollectionMock::DocElement::swapBuffer(
    std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>& newData) {
  _data.swap(newData);
}

arangodb::LocalDocumentId PhysicalCollectionMock::DocElement::docId() const {
  return arangodb::LocalDocumentId::create(_docId);
}

uint8_t const* PhysicalCollectionMock::DocElement::vptr() const {
  return _data->data();
}

std::function<void()> PhysicalCollectionMock::before = []() -> void {};

PhysicalCollectionMock::PhysicalCollectionMock(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& info)
    : PhysicalCollection(collection, info), _lastDocumentId{0} {}

arangodb::PhysicalCollection* PhysicalCollectionMock::clone(arangodb::LogicalCollection& collection) const {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

int PhysicalCollectionMock::close() {
  for (auto& index : _indexes) {
    index->unload();
  }

  return TRI_ERROR_NO_ERROR;  // assume close successful
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::createIndex(
    arangodb::velocypack::Slice const& info, bool restore, bool& created) {
  before();

  std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> docs;
  docs.reserve(_documents.size());
  for (auto const& [key, doc] : _documents) {
    docs.emplace_back(doc.docId(), doc.data());
  }

  struct IndexFactory : public arangodb::IndexFactory {
    using arangodb::IndexFactory::validateSlice;
  };
  auto id = IndexFactory::validateSlice(info, true, false);  // trie+false to ensure id generation if missing

  auto const type =
      arangodb::basics::VelocyPackHelper::getStringRef(info.get("type"),
                                                       arangodb::velocypack::StringRef());

  std::shared_ptr<arangodb::Index> index;

  if (0 == type.compare("edge")) {
    index = EdgeIndexMock::make(id, _logicalCollection, info);
  } else if (0 == type.compare(arangodb::iresearch::DATA_SOURCE_TYPE.name())) {
    try {
      if (arangodb::ServerState::instance()->isCoordinator()) {
        index = arangodb::iresearch::IResearchLinkCoordinator::factory().instantiate(
            _logicalCollection, info, id, false);
      } else {
        index = arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(
            _logicalCollection, info, id, false);
      }
    } catch (std::exception const& ex) {
      // ignore the details of all errors here
      LOG_DEVEL << "caught: " << ex.what();
    }
  }

  if (!index) {
    return nullptr;
  }

  asio_ns::io_context ioContext;
  auto poster = [&ioContext](std::function<void()> fn) -> bool {
    ioContext.post(fn);
    return true;
  };
  arangodb::basics::LocalTaskQueue taskQueue(poster);
  std::shared_ptr<arangodb::basics::LocalTaskQueue> taskQueuePtr(
      &taskQueue, [](arangodb::basics::LocalTaskQueue*) -> void {});

  TRI_vocbase_t& vocbase = _logicalCollection.vocbase();
  arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                            _logicalCollection,
                                            arangodb::AccessMode::Type::WRITE);
  auto res = trx.begin();
  TRI_ASSERT(res.ok());

  if (index->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    auto* l = dynamic_cast<EdgeIndexMock*>(index.get());
    TRI_ASSERT(l != nullptr);
    for (auto const& pair : docs) {
      l->insert(trx, pair.first, pair.second, arangodb::Index::OperationMode::internal);
    }
  } else if (index->type() == arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK) {
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
    TRI_ASSERT(l != nullptr);
    ;
    l->batchInsert(trx, docs, taskQueuePtr);
  } else {
    TRI_ASSERT(false);
  }

  if (TRI_ERROR_NO_ERROR != taskQueue.status()) {
    return nullptr;
  }

  _indexes.emplace(index);
  created = true;

  res = trx.commit();
  TRI_ASSERT(res.ok());

  return index;
}

void PhysicalCollectionMock::deferDropCollection(
    std::function<bool(arangodb::LogicalCollection&)> const& callback) {
  before();

  callback(_logicalCollection);  // assume noone is using this collection (drop immediately)
}

bool PhysicalCollectionMock::dropIndex(TRI_idx_iid_t iid) {
  before();

  for (auto itr = _indexes.begin(), end = _indexes.end(); itr != end; ++itr) {
    if ((*itr)->id() == iid) {
      if ((*itr)->drop().ok()) {
        _indexes.erase(itr);
        return true;
      }
    }
  }

  return false;
}

void PhysicalCollectionMock::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) {
  before();
  TRI_ASSERT(false);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAllIterator(
    arangodb::transaction::Methods* trx) const {
  before();

  return std::make_unique<AllIteratorMock>(_documents, this->_logicalCollection, trx);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAnyIterator(
    arangodb::transaction::Methods* trx) const {
  before();
  return std::make_unique<AllIteratorMock>(_documents, this->_logicalCollection, trx);
}

void PhysicalCollectionMock::getPropertiesVPack(arangodb::velocypack::Builder&) const {
  before();
}

arangodb::Result PhysicalCollectionMock::insert(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options,
    bool lock, arangodb::KeyLockInfo* /*keyLockInfo*/,
    std::function<void()> const& callbackDuringLock) {
  TRI_ASSERT(callbackDuringLock == nullptr);  // not implemented
  before();

  TRI_ASSERT(newSlice.isObject());
  VPackSlice newKey = newSlice.get(arangodb::StaticStrings::KeyString);
  if (newKey.isString()) {
    if (_documents.find(arangodb::velocypack::StringRef{newKey}) != _documents.end()) {
      return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
    }
  }

  arangodb::velocypack::Builder builder;
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  TRI_voc_rid_t revisionId;
  auto res = newObjectForInsert(trx, newSlice, isEdgeCollection, builder,
                                options.isRestore, revisionId);

  if (res.fail()) {
    return res;
  }
  TRI_ASSERT(builder.slice().get(arangodb::StaticStrings::KeyString).isString());

  arangodb::velocypack::StringRef key{builder.slice().get(arangodb::StaticStrings::KeyString)};
  auto const& [ref, didInsert] =
      _documents.emplace(key, DocElement{builder.steal(), ++_lastDocumentId});
  TRI_ASSERT(didInsert);

  result.setUnmanaged(ref->second.vptr());
  TRI_ASSERT(result.revisionId() == revisionId);

  for (auto& index : _indexes) {
    if (index->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      auto* l = static_cast<EdgeIndexMock*>(index.get());
      if (!l->insert(*trx, ref->second.docId(),
                     arangodb::velocypack::Slice(result.vpack()),
                     arangodb::Index::OperationMode::normal)
               .ok()) {
        return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
      }
      continue;
    } else if (index->type() == arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK) {
      if (arangodb::ServerState::instance()->isCoordinator()) {
        auto* l =
            static_cast<arangodb::iresearch::IResearchLinkCoordinator*>(index.get());
        if (!l->insert(*trx, ref->second.docId(),
                       arangodb::velocypack::Slice(result.vpack()),
                       arangodb::Index::OperationMode::normal)
                 .ok()) {
          return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
        }
      } else {
        auto* l = static_cast<arangodb::iresearch::IResearchMMFilesLink*>(index.get());
        if (!l->insert(*trx, ref->second.docId(),
                       arangodb::velocypack::Slice(result.vpack()),
                       arangodb::Index::OperationMode::normal)
                 .ok()) {
          return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
        }
      }
      continue;
    }
    TRI_ASSERT(false);
  }

  return arangodb::Result();
}

arangodb::LocalDocumentId PhysicalCollectionMock::lookupKey(
    arangodb::transaction::Methods*, arangodb::velocypack::Slice const&) const {
  before();
  TRI_ASSERT(false);
  return arangodb::LocalDocumentId();
}

size_t PhysicalCollectionMock::memory() const {
  before();
  TRI_ASSERT(false);
  return 0;
}

uint64_t PhysicalCollectionMock::numberDocuments(arangodb::transaction::Methods*) const {
  before();
  return _documents.size();
}

void PhysicalCollectionMock::open(bool ignoreErrors) {
  before();
  TRI_ASSERT(false);
}

std::string const& PhysicalCollectionMock::path() const {
  before();

  return physicalPath;
}

arangodb::Result PhysicalCollectionMock::persistProperties() {
  before();
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

bool PhysicalCollectionMock::addIndex(std::shared_ptr<arangodb::Index> idx) {
  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return false;
    }
  }

  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

  _indexes.emplace(idx);
  return true;
}

void PhysicalCollectionMock::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  before();

  auto* engine = arangodb::EngineSelectorFeature::ENGINE;
  auto& idxFactory = engine->indexFactory();

  for (VPackSlice v : VPackArrayIterator(indexesSlice)) {
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error", false)) {
      // We have an error here.
      // Do not add index.
      continue;
    }

    try {
      auto idx = idxFactory.prepareIndexFromSlice(v, false, _logicalCollection, true);

      if (!idx) {
        continue;
      }

      if (!addIndex(idx)) {
        return;
      }
    } catch (std::exception const&) {
      // error is just ignored here
    }
  }
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods*,
                                              arangodb::velocypack::StringRef const& key,
                                              arangodb::ManagedDocumentResult& result, bool) {
  before();
  auto it = _documents.find(key);
  if (it != _documents.end()) {
    result.setUnmanaged(it->second.vptr());
    return arangodb::Result(TRI_ERROR_NO_ERROR);
  }
  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods* trx,
                                              arangodb::velocypack::Slice const& key,
                                              arangodb::ManagedDocumentResult& result,
                                              bool unusedFlag) {
  return read(trx, arangodb::velocypack::StringRef(key), result, unusedFlag);
}

bool PhysicalCollectionMock::readDocument(arangodb::transaction::Methods* trx,
                                          arangodb::LocalDocumentId const& token,
                                          arangodb::ManagedDocumentResult& result) const {
  before();
  for (auto const& [key, doc] : _documents) {
    if (doc.docId() == token) {
      result.setUnmanaged(doc.vptr());
      return true;
    }
  }
  return false;
}

bool PhysicalCollectionMock::readDocumentWithCallback(
    arangodb::transaction::Methods* trx, arangodb::LocalDocumentId const& token,
    arangodb::IndexIterator::DocumentCallback const& cb) const {
  before();
  for (auto const& [key, doc] : _documents) {
    if (doc.docId() == token) {
      cb(token, doc.data());
      return true;
    }
  }
  return false;
}

arangodb::Result PhysicalCollectionMock::remove(
    arangodb::transaction::Methods& trx, arangodb::velocypack::Slice slice,
    arangodb::ManagedDocumentResult& previous, arangodb::OperationOptions& options,
    bool lock, arangodb::KeyLockInfo* /*keyLockInfo*/,
    std::function<void()> const& callbackDuringLock) {
  TRI_ASSERT(callbackDuringLock == nullptr);  // not implemented
  before();

  auto key = slice.get(arangodb::StaticStrings::KeyString);
  TRI_ASSERT(key.isString());
  arangodb::velocypack::StringRef keyRef{key};
  auto old = _documents.find(keyRef);
  if (old != _documents.end()) {
    previous.setUnmanaged(old->second.vptr());
    _graveyard.emplace_back(old->second.rawData());
    TRI_ASSERT(previous.revisionId() == TRI_ExtractRevisionId(old->second.data()));
    _documents.erase(keyRef);
    return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume document was removed
  }
  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::update(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options,
    bool lock, arangodb::ManagedDocumentResult& previous) {
  return updateInternal(trx, newSlice, result, options, lock, previous, true);
}

arangodb::Result PhysicalCollectionMock::replace(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options,
    bool lock, arangodb::ManagedDocumentResult& previous) {
  return updateInternal(trx, newSlice, result, options, lock, previous, false);
}

TRI_voc_rid_t PhysicalCollectionMock::revision(arangodb::transaction::Methods*) const {
  before();
  TRI_ASSERT(false);
  return 0;
}

void PhysicalCollectionMock::setPath(std::string const& value) {
  before();
  physicalPath = value;
}

arangodb::Result PhysicalCollectionMock::truncate(arangodb::transaction::Methods& trx,
                                                  arangodb::OperationOptions& options) {
  before();
  _documents.clear();
  return arangodb::Result();
}

arangodb::Result PhysicalCollectionMock::compact() {
  return arangodb::Result();
}

arangodb::Result PhysicalCollectionMock::updateInternal(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options,
    bool lock, arangodb::ManagedDocumentResult& previous, bool isUpdate) {
  auto key = newSlice.get(arangodb::StaticStrings::KeyString);
  if (!key.isString()) {
    return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  before();
  arangodb::velocypack::StringRef keyRef{key};
  auto it = _documents.find(keyRef);
  if (it != _documents.end()) {
    auto doc = it->second.data();
    if (!options.ignoreRevs) {
      TRI_voc_rid_t expectedRev = 0;
      if (newSlice.isObject()) {
        expectedRev = TRI_ExtractRevisionId(newSlice);
      }
      TRI_ASSERT(doc.isObject());
      TRI_voc_rid_t oldRev = TRI_ExtractRevisionId(doc);
      int res = checkRevision(trx, expectedRev, oldRev);
      if (res != TRI_ERROR_NO_ERROR) {
        return arangodb::Result(res);
      }
    }
    arangodb::velocypack::Builder builder;
    TRI_voc_rid_t revisionId = 0;  // unused
    auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());
    if (isUpdate) {
      arangodb::Result res =
          mergeObjectsForUpdate(trx, doc, newSlice, isEdgeCollection,
                                options.mergeObjects, options.keepNull, builder,
                                options.isRestore, revisionId);
      if (res.fail()) {
        return res;
      }
    } else {
      arangodb::Result res = newObjectForReplace(trx, doc, newSlice, isEdgeCollection,
                                                 builder, options.isRestore, revisionId);
      if (res.fail()) {
        return res;
      }
    }
    auto nextBuffer = builder.steal();
    // Set previous
    previous.setUnmanaged(it->second.vptr());
    TRI_ASSERT(previous.revisionId() == TRI_ExtractRevisionId(doc));

    // swap with new data
    // Replace the existing Buffer and nextBuffer
    it->second.swapBuffer(nextBuffer);
    // Put the now old buffer into the graveyour for previous to stay valid
    _graveyard.emplace_back(nextBuffer);

    result.setUnmanaged(it->second.vptr());
    TRI_ASSERT(result.revisionId() != previous.revisionId());
    return TRI_ERROR_NO_ERROR;
  }
  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::updateProperties(arangodb::velocypack::Slice const& slice,
                                                          bool doSync) {
  before();

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock collection updated OK
}

std::function<void()> StorageEngineMock::before = []() -> void {};
arangodb::RecoveryState StorageEngineMock::recoveryStateResult =
    arangodb::RecoveryState::DONE;
TRI_voc_tick_t StorageEngineMock::recoveryTickResult = 0;
std::function<void()> StorageEngineMock::recoveryTickCallback = []() -> void {};

/*static*/ std::string StorageEngineMock::versionFilenameResult;

StorageEngineMock::StorageEngineMock(arangodb::application_features::ApplicationServer& server)
    : StorageEngine(server, "Mock", "",
                    std::unique_ptr<arangodb::IndexFactory>(new IndexFactoryMock())),
      vocbaseCount(1),
      _releasedTick(0) {}

arangodb::WalAccess const* StorageEngineMock::walAccess() const {
  TRI_ASSERT(false);
  return nullptr;
}

void StorageEngineMock::addOptimizerRules(arangodb::aql::OptimizerRulesFeature& /*feature*/) {
  before();
  // NOOP
}

void StorageEngineMock::addRestHandlers(arangodb::rest::RestHandlerFactory& handlerFactory) {
  TRI_ASSERT(false);
}

void StorageEngineMock::addV8Functions() { TRI_ASSERT(false); }

void StorageEngineMock::changeCollection(TRI_vocbase_t& vocbase,
                                         arangodb::LogicalCollection const& collection,
                                         bool doSync) {
  // NOOP, assume physical collection changed OK
}

arangodb::Result StorageEngineMock::changeView(TRI_vocbase_t& vocbase,
                                               arangodb::LogicalView const& view,
                                               bool doSync) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) != views.end());
  arangodb::velocypack::Builder builder;

  builder.openObject();
  view.properties(builder, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed,
                               arangodb::LogicalDataSource::Serialize::ForPersistence));
  builder.close();
  views[std::make_pair(vocbase.id(), view.id())] = std::move(builder);
  return {};
}

std::string StorageEngineMock::collectionPath(TRI_vocbase_t const& vocbase,
                                              TRI_voc_cid_t id) const {
  TRI_ASSERT(false);
  return "<invalid>";
}

std::string StorageEngineMock::createCollection(TRI_vocbase_t& vocbase,
                                                arangodb::LogicalCollection const& collection) {
  return "<invalid>";  // physical path of the new collection
}

std::unique_ptr<TRI_vocbase_t> StorageEngineMock::createDatabase(arangodb::CreateDatabaseInfo&& info,
                                                                 int& status) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR,
                                           std::move(info));
  }
  return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                         std::move(info));
}

arangodb::Result StorageEngineMock::createLoggerState(TRI_vocbase_t*, VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::PhysicalCollection> StorageEngineMock::createPhysicalCollection(
    arangodb::LogicalCollection& collection, arangodb::velocypack::Slice const& info) {
  before();
  return std::unique_ptr<arangodb::PhysicalCollection>(
      new PhysicalCollectionMock(collection, info));
}

arangodb::Result StorageEngineMock::createTickRanges(VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::TransactionCollection> StorageEngineMock::createTransactionCollection(
    arangodb::TransactionState& state, TRI_voc_cid_t cid,
    arangodb::AccessMode::Type accessType, int nestingLevel) {
  return std::unique_ptr<arangodb::TransactionCollection>(
      new TransactionCollectionMock(&state, cid, accessType));
}

std::unique_ptr<arangodb::transaction::ContextData> StorageEngineMock::createTransactionContextData() {
  return std::unique_ptr<arangodb::transaction::ContextData>();
}

std::unique_ptr<arangodb::transaction::Manager> StorageEngineMock::createTransactionManager(
    arangodb::transaction::ManagerFeature& feature) {
  return std::make_unique<arangodb::transaction::Manager>(feature, /*keepData*/ false);
}

std::unique_ptr<arangodb::TransactionState> StorageEngineMock::createTransactionState(
    TRI_vocbase_t& vocbase, TRI_voc_tid_t tid, arangodb::transaction::Options const& options) {
  return std::unique_ptr<arangodb::TransactionState>(
      new TransactionStateMock(vocbase, tid, options));
}

arangodb::Result StorageEngineMock::createView(TRI_vocbase_t& vocbase, TRI_voc_cid_t id,
                                               arangodb::LogicalView const& view) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) == views.end());  // called after createView()
  arangodb::velocypack::Builder builder;

  builder.openObject();
  view.properties(builder, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed,
                               arangodb::LogicalDataSource::Serialize::ForPersistence));
  builder.close();
  views[std::make_pair(vocbase.id(), view.id())] = std::move(builder);

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock view persisted OK
}

void StorageEngineMock::getViewProperties(TRI_vocbase_t& vocbase,
                                          arangodb::LogicalView const& view,
                                          VPackBuilder& builder) {
  before();
  // NOOP
}

TRI_voc_tick_t StorageEngineMock::currentTick() const {
  return TRI_CurrentTickServer();
}

std::string StorageEngineMock::dataPath() const {
  before();
  return "";  // no valid path filesystem persisted, return empty string
}

std::string StorageEngineMock::databasePath(TRI_vocbase_t const* vocbase) const {
  before();
  return "";  // no valid path filesystem persisted, return empty string
}

void StorageEngineMock::destroyCollection(TRI_vocbase_t& vocbase,
                                          arangodb::LogicalCollection& collection) {
  // NOOP, assume physical collection destroyed OK
}

void StorageEngineMock::destroyView(TRI_vocbase_t const& vocbase,
                                    arangodb::LogicalView const& view) noexcept {
  before();
  // NOOP, assume physical view destroyed OK
}

arangodb::Result StorageEngineMock::dropCollection(TRI_vocbase_t& vocbase,
                                                   arangodb::LogicalCollection& collection) {
  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume physical collection dropped OK
}

arangodb::Result StorageEngineMock::dropDatabase(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::Result StorageEngineMock::dropView(TRI_vocbase_t const& vocbase,
                                             arangodb::LogicalView const& view) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) != views.end());
  views.erase(std::make_pair(vocbase.id(), view.id()));

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock view dropped OK
}

arangodb::Result StorageEngineMock::firstTick(uint64_t&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

void StorageEngineMock::getCollectionInfo(TRI_vocbase_t& vocbase, TRI_voc_cid_t cid,
                                          arangodb::velocypack::Builder& result,
                                          bool includeIndexes, TRI_voc_tick_t maxTick) {
  arangodb::velocypack::Builder parameters;

  parameters.openObject();
  parameters.close();

  result.openObject();
  result.add("parameters", parameters.slice());  // required entry of type object
  result.close();

  // nothing more required, assume info used for PhysicalCollectionMock
}

int StorageEngineMock::getCollectionsAndIndexes(TRI_vocbase_t& vocbase,
                                                arangodb::velocypack::Builder& result,
                                                bool wasCleanShutdown, bool isUpgrade) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void StorageEngineMock::getDatabases(arangodb::velocypack::Builder& result) {
  before();
  arangodb::velocypack::Builder system;

  system.openObject();
  system.add("name", arangodb::velocypack::Value(TRI_VOC_SYSTEM_DATABASE));
  system.close();

  // array expected
  result.openArray();
  result.add(system.slice());
  result.close();
}

void StorageEngineMock::cleanupReplicationContexts() {
  // nothing to do here
}

arangodb::velocypack::Builder StorageEngineMock::getReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase, int& result) {
  before();
  result = TRI_ERROR_FILE_NOT_FOUND;  // assume no ReplicationApplierConfiguration for vocbase

  return arangodb::velocypack::Builder();
}

arangodb::velocypack::Builder StorageEngineMock::getReplicationApplierConfiguration(int& result) {
  before();
  result = TRI_ERROR_FILE_NOT_FOUND;

  return arangodb::velocypack::Builder();
}

int StorageEngineMock::getViews(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result) {
  result.openArray();

  for (auto& entry : views) {
    result.add(entry.second.slice());
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::handleSyncKeys(arangodb::DatabaseInitialSyncer& syncer,
                                                   arangodb::LogicalCollection& col,
                                                   std::string const& keysId) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::RecoveryState StorageEngineMock::recoveryState() {
  return recoveryStateResult;
}
TRI_voc_tick_t StorageEngineMock::recoveryTick() {
  if (recoveryTickCallback) {
    recoveryTickCallback();
  }
  return recoveryTickResult;
}

arangodb::Result StorageEngineMock::lastLogger(
    TRI_vocbase_t& vocbase, std::shared_ptr<arangodb::transaction::Context> transactionContext,
    uint64_t tickStart, uint64_t tickEnd, std::shared_ptr<VPackBuilder>& builderSPtr) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<TRI_vocbase_t> StorageEngineMock::openDatabase(arangodb::CreateDatabaseInfo&& info,
                                                               bool isUpgrade) {
  before();

  auto new_info = info;
  new_info.setId(++vocbaseCount);

  return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                         std::move(new_info));
}

arangodb::Result StorageEngineMock::persistCollection(TRI_vocbase_t& vocbase,
                                                      arangodb::LogicalCollection const& collection) {
  before();
  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock collection persisted OK
}

void StorageEngineMock::prepareDropDatabase(TRI_vocbase_t& vocbase,
                                            bool useWriteMarker, int& status) {
  // NOOP
}

TRI_voc_tick_t StorageEngineMock::releasedTick() const {
  before();
  return _releasedTick;
}

void StorageEngineMock::releaseTick(TRI_voc_tick_t tick) {
  before();
  _releasedTick = tick;
}

int StorageEngineMock::removeReplicationApplierConfiguration(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::removeReplicationApplierConfiguration() {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::renameCollection(TRI_vocbase_t& vocbase,
                                                     arangodb::LogicalCollection const& collection,
                                                     std::string const& oldName) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

int StorageEngineMock::saveReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                           arangodb::velocypack::Slice slice,
                                                           bool doSync) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::saveReplicationApplierConfiguration(arangodb::velocypack::Slice, bool) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::shutdownDatabase(TRI_vocbase_t& vocbase) {
  before();
  return TRI_ERROR_NO_ERROR;  // assume shutdown successful
}

void StorageEngineMock::signalCleanup(TRI_vocbase_t& vocbase) {
  before();
  // NOOP, assume cleanup thread signaled OK
}

bool StorageEngineMock::supportsDfdb() const {
  TRI_ASSERT(false);
  return false;
}

void StorageEngineMock::unloadCollection(TRI_vocbase_t& vocbase,
                                         arangodb::LogicalCollection& collection) {
  before();
  // NOOP assume collection unloaded OK
}

std::string StorageEngineMock::versionFilename(TRI_voc_tick_t) const {
  return versionFilenameResult;
}

void StorageEngineMock::waitForEstimatorSync(std::chrono::milliseconds) {
  TRI_ASSERT(false);
}

void StorageEngineMock::waitForSyncTick(TRI_voc_tick_t tick) {
  // NOOP
}

std::vector<std::string> StorageEngineMock::currentWalFiles() const {
  return std::vector<std::string>();
}

arangodb::Result StorageEngineMock::flushWal(bool waitForSync, bool waitForCollector,
                                             bool writeShutdownFile) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

void StorageEngineMock::waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) {
  // NOOP
}

int StorageEngineMock::writeCreateDatabaseMarker(TRI_voc_tick_t id, VPackSlice const& slice) {
  return TRI_ERROR_NO_ERROR;
}

TransactionCollectionMock::TransactionCollectionMock(arangodb::TransactionState* state,
                                                     TRI_voc_cid_t cid,
                                                     arangodb::AccessMode::Type accessType)
    : TransactionCollection(state, cid, accessType, 0) {}

bool TransactionCollectionMock::canAccess(arangodb::AccessMode::Type accessType) const {
  return nullptr != _collection;  // collection must have be opened previously
}

void TransactionCollectionMock::freeOperations(arangodb::transaction::Methods* activeTrx,
                                               bool mustRollback) {
  TRI_ASSERT(false);
}

bool TransactionCollectionMock::hasOperations() const {
  TRI_ASSERT(false);
  return false;
}

void TransactionCollectionMock::release() {
  if (_collection) {
    if (!arangodb::ServerState::instance()->isCoordinator()) {
      _transaction->vocbase().releaseCollection(_collection.get());
    }
    _collection = nullptr;
  }
}

void TransactionCollectionMock::unuse(int nestingLevel) {
  // NOOP, assume success
}

int TransactionCollectionMock::use(int nestingLevel) {
  TRI_vocbase_col_status_e status;

  bool shouldLock = !arangodb::AccessMode::isNone(_accessType);

  if (shouldLock && !isLocked()) {
    // r/w lock the collection
    int res = doLock(_accessType, nestingLevel);

    if (res == TRI_ERROR_LOCKED) {
      // TRI_ERROR_LOCKED is not an error, but it indicates that the lock
      // operation has actually acquired the lock (and that the lock has not
      // been held before)
      res = TRI_ERROR_NO_ERROR;
    } else if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  if (!_collection) {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      auto& ci = _transaction->vocbase()
                     .server()
                     .getFeature<arangodb::ClusterFeature>()
                     .clusterInfo();
      _collection =
          ci.getCollectionNT(_transaction->vocbase().name(), std::to_string(_cid));
    } else {
      _collection = _transaction->vocbase().useCollection(_cid, status);
    }
  }

  return _collection ? TRI_ERROR_NO_ERROR : TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
}

int TransactionCollectionMock::doLock(arangodb::AccessMode::Type type, int nestingLevel) {
  if (_lockType > _accessType) {
    return TRI_ERROR_INTERNAL;
  }

  _lockType = type;

  return TRI_ERROR_NO_ERROR;
}

int TransactionCollectionMock::doUnlock(arangodb::AccessMode::Type type, int nestingLevel) {
  if (_lockType != type) {
    return TRI_ERROR_INTERNAL;
  }

  _lockType = arangodb::AccessMode::Type::NONE;

  return TRI_ERROR_NO_ERROR;
}

size_t TransactionStateMock::abortTransactionCount;
size_t TransactionStateMock::beginTransactionCount;
size_t TransactionStateMock::commitTransactionCount;

// ensure each transaction state has a unique ID
TransactionStateMock::TransactionStateMock(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                                           arangodb::transaction::Options const& options)
    : TransactionState(vocbase, tid, options) {}

arangodb::Result TransactionStateMock::abortTransaction(arangodb::transaction::Methods* trx) {
  ++abortTransactionCount;
  updateStatus(arangodb::transaction::Status::ABORTED);
  unuseCollections(nestingLevel());
  // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)
  const_cast<TRI_voc_tid_t&>(_id) = 0;

  return arangodb::Result();
}

arangodb::Result TransactionStateMock::beginTransaction(arangodb::transaction::Hints hints) {
  ++beginTransactionCount;
  _hints = hints;

  auto res = useCollections(nestingLevel());

  if (!res.ok()) {
    updateStatus(arangodb::transaction::Status::ABORTED);
    // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)
    const_cast<TRI_voc_tid_t&>(_id) = 0;
    return res;
  }

  if (nestingLevel() == 0) {
    updateStatus(arangodb::transaction::Status::RUNNING);
  }

  return arangodb::Result();
}

arangodb::Result TransactionStateMock::commitTransaction(arangodb::transaction::Methods* trx) {
  ++commitTransactionCount;
  if (nestingLevel() == 0) {
    updateStatus(arangodb::transaction::Status::COMMITTED);
    // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)
    const_cast<TRI_voc_tid_t&>(_id) = 0;
  }
  unuseCollections(nestingLevel());

  return arangodb::Result();
}

bool TransactionStateMock::hasFailedOperations() const {
  return false;  // assume no failed operations
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
