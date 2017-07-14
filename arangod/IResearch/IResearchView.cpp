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

#include "formats/formats.hpp"
#include "search/boolean_filter.hpp"
#include "search/scorers.hpp"
#include "store/memory_directory.hpp"
#include "store/fs_directory.hpp"
#include "utils/directory_utils.hpp"
#include "utils/memory.hpp"
#include "utils/misc.hpp"
#include "utils/utf8_path.hpp"

#include "IResearchDocument.h"
#include "IResearchFeature.h"
#include "IResearchLink.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Result.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Transaction/UserTransaction.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "Views/ViewIterator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/PhysicalView.h"

#include "IResearchView.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief the storage format used with iResearch writers
////////////////////////////////////////////////////////////////////////////////
const irs::string_ref IRESEARCH_STORE_FORMAT("1_0");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the iResearch View definition denoting the
///        corresponding link definitions
////////////////////////////////////////////////////////////////////////////////
const std::string LINKS_FIELD("links");

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief generates user-friendly description of the specified view
////////////////////////////////////////////////////////////////////////////////
std::string toString(arangodb::iresearch::IResearchView const& view) {
  std::string str(arangodb::iresearch::IResearchView::type());
  str += ":";
  str += view.id();
  return str;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns 'Flush' feature from AppicationServer
////////////////////////////////////////////////////////////////////////////////
arangodb::FlushFeature& getFlushFeature() {
  auto* flush = arangodb::application_features::ApplicationServer::getFeature<arangodb::FlushFeature>("Flush");
  TRI_ASSERT(flush); // getFeature throws in case of fail

  return *flush;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple directory readers
////////////////////////////////////////////////////////////////////////////////
class CompoundReader: public irs::index_reader {
 public:
  irs::sub_reader const& operator[](size_t subReaderId) const;
  void add(irs::directory_reader const& reader);
  virtual reader_iterator begin() const override;
  virtual uint64_t docs_count(const irs::string_ref& field) const override;
  virtual uint64_t docs_count() const override;
  virtual reader_iterator end() const override;
  virtual uint64_t live_docs_count() const override;
  irs::field_id pkColumnId(size_t subReaderId) const;
  virtual size_t size() const override;

 private:
  typedef std::vector<std::pair<irs::sub_reader*, irs::field_id>> SubReadersType;
  class IteratorImpl: public irs::index_reader::reader_iterator_impl {
   public:
    IteratorImpl(SubReadersType::const_iterator const& itr);
    virtual void operator++() override;
    virtual reference operator*() override;
    virtual const_reference operator*() const override;
    virtual bool operator==(const reader_iterator_impl& other) override;

   private:
    SubReadersType::const_iterator _itr;
  };

  std::vector<irs::directory_reader> _readers;
  SubReadersType _subReaders;
};

CompoundReader::IteratorImpl::IteratorImpl(
    SubReadersType::const_iterator const& itr
): _itr(itr) {
}

void CompoundReader::IteratorImpl::operator++() {
  ++_itr;
}

irs::index_reader::reader_iterator_impl::reference CompoundReader::IteratorImpl::operator*() {
  return *(_itr->first);
}

irs::index_reader::reader_iterator_impl::const_reference CompoundReader::IteratorImpl::operator*() const {
  return *(_itr->first);
}

bool CompoundReader::IteratorImpl::operator==(
    reader_iterator_impl const& other
) {
  return static_cast<IteratorImpl const&>(other)._itr == _itr;
}

irs::sub_reader const& CompoundReader::operator[](size_t subReaderId) const {
  return *(_subReaders[subReaderId].first);
}

void CompoundReader::add(irs::directory_reader const& reader) {
  _readers.emplace_back(reader);

  for(auto& entry: _readers.back()) {
    auto* pkColMeta = entry.column(arangodb::iresearch::DocumentPrimaryKey::PK());

    if (!pkColMeta) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "encountered a sub-reader without a primary key column while creating a reader for iResearch view, ignoring";

      continue;
    }

    _subReaders.emplace_back(&entry, pkColMeta->id);
  }
}

irs::index_reader::reader_iterator CompoundReader::begin() const {
  return reader_iterator(new IteratorImpl(_subReaders.begin()));
}

uint64_t CompoundReader::docs_count(const irs::string_ref& field) const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->docs_count(field);
  }

  return count;
}

uint64_t CompoundReader::docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->docs_count();
  }

  return count;
}

irs::index_reader::reader_iterator CompoundReader::end() const {
  return reader_iterator(new IteratorImpl(_subReaders.end()));
}

uint64_t CompoundReader::live_docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->live_docs_count();
  }

  return count;
}

irs::field_id CompoundReader::pkColumnId(size_t subReaderId) const {
  return _subReaders[subReaderId].second;
}

size_t CompoundReader::size() const {
  return _subReaders.size();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for iterators of query results from iResearch View
////////////////////////////////////////////////////////////////////////////////
class ViewIteratorBase: public arangodb::ViewIterator {
 public:
  DECLARE_PTR(arangodb::ViewIterator);
  ViewIteratorBase(
    const char* typeName,
    arangodb::ViewImplementation& view,
    arangodb::transaction::Methods& trx,
    CompoundReader&& reader
  );
  virtual bool hasExtra() const override;
  virtual bool hasMore() const override; // FIXME remove
  virtual bool nextExtra(ExtraCallback const& callback, size_t limit) override;
  virtual bool readDocument(
    arangodb::DocumentIdentifierToken const& token,
    arangodb::ManagedDocumentResult& result
  ) const override;
  virtual char const* typeName() const override;

 protected:
  CompoundReader _reader;

  bool loadToken(
    arangodb::DocumentIdentifierToken& buf,
    size_t subReaderId,
    irs::doc_id_t subDocId
  ) const noexcept;

 private:
  size_t _subDocIdBits; // bits reserved for doc_id of sub-readers (depends on size of sub_readers)
  size_t _subDocIdMask; // bit mask for the sub-reader doc_id portion of doc_id
  char const* _typeName;

  irs::doc_id_t subDocId(
    arangodb::DocumentIdentifierToken const& token
  ) const noexcept;
  size_t subReaderId(
    arangodb::DocumentIdentifierToken const& token
  ) const noexcept;
};

ViewIteratorBase::ViewIteratorBase(
    char const* typeName,
    arangodb::ViewImplementation& view,
    arangodb::transaction::Methods& trx,
    CompoundReader&& reader
): ViewIterator(&view, &trx),
   _reader(std::move(reader)),
   _typeName(typeName) {
  typedef decltype(arangodb::DocumentIdentifierToken::_data) doc_id_t;
  _subDocIdBits = _reader.size()
                ? irs::math::math_traits<doc_id_t>::clz(_reader.size())
                : irs::bits_required<doc_id_t>(); // not really a usable scenario (no data)
  _subDocIdMask = (size_t(1) <<_subDocIdBits) - 1;
}

bool ViewIteratorBase::hasExtra() const {
  // shut up compiler warning...
  // FIXME TODO: implementation
  return false;
}

bool ViewIteratorBase::hasMore() const { // FIXME remove
  // shut up compiler warning...
  // FIXME TODO: implementation
  return false;
}

bool ViewIteratorBase::loadToken(
  arangodb::DocumentIdentifierToken& buf,
  size_t subReaderId,
  irs::doc_id_t subDocId
) const noexcept {
  if (subReaderId >= _reader.size() || subDocId > _subDocIdMask) {
    return false; // no valid token can be specified for these args
  }

  buf._data = (subReaderId << _subDocIdBits) | subDocId;

  return true;
}

bool ViewIteratorBase::nextExtra(ExtraCallback const& callback, size_t limit) {
  // shut up compiler warning...
  // FIXME TODO: implementation
  TRI_ASSERT(!hasExtra());
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "Requested extra values from an index that "
                                 "does not support it. This seems to be a bug "
                                 "in ArangoDB. Please report the query you are "
                                 "using + the indexes you have defined on the "
                                 "relevant collections to arangodb.com");
  return false;
}

bool ViewIteratorBase::readDocument(
    arangodb::DocumentIdentifierToken const& token,
    arangodb::ManagedDocumentResult& result
) const {
  auto docId = subDocId(token);
  auto readerId = subReaderId(token);
  auto& reader = _reader[readerId];
  auto pkColId = _reader.pkColumnId(readerId);
  arangodb::iresearch::DocumentPrimaryKey docPk;
  irs::bytes_ref tmpRef;

  if (!reader.values(pkColId)(docId, tmpRef) || !docPk.read(tmpRef)) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to read document primary key while reading document from iResearch view, doc_id '" << docId << "'";

    return false; // not a valid document reference
  }

  static const std::string unknown("<unknown>");

  _trx->addCollectionAtRuntime(docPk.cid(), unknown);

  auto* collection = _trx->documentCollection(docPk.cid());

  if (!collection) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to find collection while reading document from iResearch view, cid '" << docPk.cid() << "', rid '" << docPk.rid() << "'";

    return false; // not a valid collection reference
  }

  arangodb::DocumentIdentifierToken colToken;

  colToken._data = docPk.rid();

  return collection->readDocument(_trx, colToken, result);
}

irs::doc_id_t ViewIteratorBase::subDocId(
  arangodb::DocumentIdentifierToken const& token
) const noexcept {
  return irs::doc_id_t(token._data & _subDocIdMask);
}

size_t ViewIteratorBase::subReaderId(
  arangodb::DocumentIdentifierToken const& token
) const noexcept {
  return token._data >> _subDocIdBits;
}

char const* ViewIteratorBase::typeName() const {
  return _typeName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for ordered set of query results from iResearch View
////////////////////////////////////////////////////////////////////////////////
class OrderedViewIterator: public ViewIteratorBase {
 public:
  OrderedViewIterator(
    arangodb::ViewImplementation& view,
    arangodb::transaction::Methods& trx,
    CompoundReader&& reader,
    irs::filter const& filter,
    irs::order const& order
  );
  virtual bool next(TokenCallback const& callback, size_t limit) override;
  virtual void reset() override;
  virtual void skip(uint64_t count, uint64_t& skipped) override;
  virtual bool hasMore() const override { // FIXME remove
    return _hasMore;
  }

 private:
  struct State {
    size_t _skip;
  };

  irs::filter::prepared::ptr _filter;
  irs::order::prepared _order;
  State _state; // previous iteration state
  bool _hasMore{ true };

  void next(TokenCallback const& callback, size_t limit, bool sort);
};

OrderedViewIterator::OrderedViewIterator(
  arangodb::ViewImplementation& view,
    arangodb::transaction::Methods& trx,
    CompoundReader&& reader,
    irs::filter const& filter,
    irs::order const& order
): ViewIteratorBase("iresearch-ordered-iterator", view, trx, std::move(reader)),
   _order(order.prepare()) {
  _filter = filter.prepare(_reader, _order);
  reset();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief expects a callback taking DocumentIdentifierTokens that are created
///        from RevisionIds. In addition it expects a limit.
///        The iterator has to walk through the Index and call the callback with
///        at most limit many elements. On the next iteration it has to continue
///        after the last returned Token.
/// @return has more
////////////////////////////////////////////////////////////////////////////////
bool OrderedViewIterator::next(TokenCallback const& callback, size_t limit) {
  auto& order = _order;
  auto scoreLess = [&order](
      irs::bstring const& lhs,
      irs::bstring const& rhs
  )->bool {
    return order.less(lhs.c_str(), rhs.c_str());
  };

  // FIXME use irs::memory_pool
  std::multimap<irs::bstring, arangodb::DocumentIdentifierToken, decltype(scoreLess)> orderedDocTokens(scoreLess);
  auto maxDocCount = _state._skip + limit;
  arangodb::DocumentIdentifierToken tmpToken;

  for (size_t i = 0, count = _reader.size(); i < count; ++i) {
    auto& segmentReader = _reader[i];
    auto itr = _filter->execute(segmentReader, _order);
    auto& score = itr->attributes().get<irs::score>();

    while (itr->next()) {
      if (!loadToken(tmpToken, i, itr->value())) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to generate document identifier token while iterating iResearch view, ignoring: reader_id '" << i << "', doc_id '" << itr->value() << "'";

        continue; // if here then there is probably a bug in IResearchView while indexing
      }

      itr->score(); // compute a score for the current document

      if (!score) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to generate document score while iterating iResearch view, ignoring: reader_id '" << i << "', doc_id '" << itr->value() << "'";

        continue; // if here then there is probably a bug in IResearchView or in the iResearch library
      }

      orderedDocTokens.emplace(score->value(), tmpToken);

      if (orderedDocTokens.size() > maxDocCount) {
        orderedDocTokens.erase(--(orderedDocTokens.end())); // remove element with the least score
      }
    }
  }

  auto tokenItr = orderedDocTokens.begin();
  auto tokenEnd = orderedDocTokens.end();

  // skip documents previously returned
  for (size_t i = _state._skip; i; --i, ++tokenItr) {
    if (tokenItr == tokenEnd) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "document count less than the document count during the previous iteration on the same query while iterating iResearch view'";

      break; // if here then there is probably a bug in the iResearch library
    }
  }

  // iterate through documents
  while (limit && tokenItr != tokenEnd) {
    callback(tokenItr->second);
    ++tokenItr;
    ++_state._skip;
    --limit;
  }

  _hasMore = (limit == 0);
  return _hasMore; // exceeded limit
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resets the iterator
////////////////////////////////////////////////////////////////////////////////
void OrderedViewIterator::reset() {
  _state._skip = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skip 'count' records from the next call to next
////////////////////////////////////////////////////////////////////////////////
void OrderedViewIterator::skip(uint64_t count, uint64_t& skipped) {
  auto skip = _state._skip;
  arangodb::DocumentIdentifierToken tmpToken;

  skipped = 0;

  for (size_t i = 0, readerCount = _reader.size(); i < readerCount; ++i) {
    auto& segmentReader = _reader[i];
    auto itr = _filter->execute(segmentReader);

    while (count > skipped && itr->next()) {
      if (!loadToken(tmpToken, i, itr->value())) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to generate document identifier token while iterating iResearch view, ignoring: reader_id '" << i << "', doc_id '" << itr->value() << "'";

        continue; // if here then there is probably a bug in IResearchView while indexing
      }

      if (skip) {
        --skip;
      } else {
        ++skipped;
      }
    }
  }

  _state._skip += skipped;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for unordered set of query results from iResearch View
////////////////////////////////////////////////////////////////////////////////
class UnorderedViewIterator: public ViewIteratorBase {
 public:
  UnorderedViewIterator(
    arangodb::ViewImplementation& view,
    arangodb::transaction::Methods& trx,
    CompoundReader&& reader,
    irs::filter const& filter
  );
  virtual bool next(TokenCallback const& callback, size_t limit) override;
  virtual void reset() override;
  virtual void skip(uint64_t count, uint64_t& skipped) override;

  virtual bool hasMore() const override { // FIXME remove
    return _hasMore;
  }

 private:
  struct State {
    irs::doc_iterator::ptr _itr;
    size_t _readerOffset;
  };

  irs::filter::prepared::ptr _filter;
  State _state; // previous iteration state
  mutable bool _hasMore{true};
};

UnorderedViewIterator::UnorderedViewIterator(
  arangodb::ViewImplementation& view,
    arangodb::transaction::Methods& trx,
    CompoundReader&& reader,
    irs::filter const& filter
): ViewIteratorBase("iresearch-unordered-iterator", view, trx, std::move(reader)),
   _filter(filter.prepare(_reader)) {
  reset();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief expects a callback taking DocumentIdentifierTokens that are created
///        from RevisionIds. In addition it expects a limit.
///        The iterator has to walk through the Index and call the callback with
///        at most limit many elements. On the next iteration it has to continue
///        after the last returned Token.
/// @return has more
////////////////////////////////////////////////////////////////////////////////
bool UnorderedViewIterator::next(TokenCallback const& callback, size_t limit) {
  arangodb::DocumentIdentifierToken tmpToken;

  for (size_t count = _reader.size();
       _state._readerOffset < count;
       ++_state._readerOffset, _state._itr.reset()
  ) {
    auto& segmentReader = _reader[_state._readerOffset];

    if (!_state._itr) {
      _state._itr = _filter->execute(segmentReader);
    }

    while (limit && _state._itr->next()) {
      if (!loadToken(tmpToken, _state._readerOffset, _state._itr->value())) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to generate document identifier token while iterating iResearch view, ignoring: reader_id '" << _state._readerOffset << "', doc_id '" << _state._itr->value() << "'";

        continue;
      }

      callback(tmpToken);
      --limit;
    }
  }

  _hasMore = (limit == 0); // exceeded limit
  return _hasMore;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resets the iterator
////////////////////////////////////////////////////////////////////////////////
void UnorderedViewIterator::reset() {
  _state._itr.reset();
  _state._readerOffset = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skip 'count' records from the next call to next
////////////////////////////////////////////////////////////////////////////////
void UnorderedViewIterator::skip(uint64_t count, uint64_t& skipped) {
  skipped = 0;
  next(
    [&skipped](arangodb::DocumentIdentifierToken const&)->void { ++skipped; },
    count
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an absolute path from the IResearchViewMeta configuration
////////////////////////////////////////////////////////////////////////////////
bool appendAbsolutePersistedDataPath(
    std::string& buf,
    arangodb::iresearch::IResearchView const& view,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  auto& path = meta._dataPath;

  if (TRI_PathIsAbsolute(path)) {
    buf = path;

    return true;
  }

  auto* feature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");

  if (!feature) {
    return false;
  }

  // get base path from DatabaseServerFeature (similar to MMFilesEngine)
  irs::utf8_path absPath(feature->directory());
  static std::string subPath("databases");

  absPath/subPath;
  absPath/(path.empty()
           ? (arangodb::iresearch::IResearchView::type() + "-" + std::to_string(view.id()))
           : path
          );
  buf = absPath.utf8();

  return true;
}

arangodb::Result createPersistedDataDirectory(
    irs::directory::ptr& dstDirectory, // out param
    irs::index_writer::ptr& dstWriter, // out param
    std::string const& dstDataPath,
    irs::directory_reader srcReader, // !srcReader ==  do not import
    TRI_voc_cid_t viewId
) noexcept {
  try {
    auto directory = irs::directory::make<irs::fs_directory>(dstDataPath);

    if (!directory) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error creating persistent directry for iResearch view '") + std::to_string(viewId) + "' at path '" + dstDataPath + "'"
      );
    }

    auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);
    auto writer = irs::index_writer::make(*directory, format, irs::OM_CREATE_APPEND);

    if (!writer) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error creating persistent writer for iResearch view '") + std::to_string(viewId) + "' at path '" + dstDataPath + "'"
      );
    }

    irs::all filter;

    writer->remove(filter);
    writer->commit(); // clear destination directory

    if (srcReader) {
      srcReader = srcReader.reopen();
      writer->import(srcReader);
      writer->commit(); // initialize 'store' as a copy of the existing store
    }

    dstDirectory = std::move(directory);
    dstWriter = std::move(writer);

    return arangodb::Result(/*TRI_ERROR_NO_ERROR*/);
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught exception while creating iResearch view '" << viewId << "' data path '" + dstDataPath + "': " << e.what();
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error creating iResearch view '") + std::to_string(viewId) + "' data path '" + dstDataPath + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught exception while creating iResearch view '" << viewId << "' data path '" + dstDataPath + "'";
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error creating iResearch view '") + std::to_string(viewId) + "' data path '" + dstDataPath + "'"
    );
  }
}

// @brief approximate iResearch directory instance size
size_t directoryMemory(irs::directory const& directory, TRI_voc_cid_t viewId) noexcept {
  size_t size = 0;

  try {
    directory.visit([&directory, &size](std::string& file)->bool {
      uint64_t length;

      size += directory.length(length, file) ? length : 0;

      return true;
    });
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught error while calculating size of iResearch view '" << viewId << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught error while calculating size of iResearch view '" << viewId << "'";
    IR_EXCEPTION();
  }

  return size;
}

arangodb::iresearch::IResearchLink* findFirstMatchingLink(
    arangodb::LogicalCollection const& collection,
    arangodb::iresearch::IResearchView const& view
) {
  for (auto& index: collection.getIndexes()) {
    if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue; // not an iresearch Link
    }

    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
    #else
      auto* link = static_cast<arangodb::iresearch::IResearchLink*>(index.get());
    #endif

    if (link && *link == view) {
      return link; // found required link
    }
  }

  return nullptr;
}

arangodb::Result updateLinks(
    TRI_vocbase_t& vocbase,
    arangodb::iresearch::IResearchView& view,
    arangodb::velocypack::Slice const& links
) noexcept {
  try {
    if (!links.isObject()) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error parsing link parameters from json for iResearch view '") + std::to_string(view.id()) + "'"
      );
    }

    struct State {
      arangodb::LogicalCollection* _collection = nullptr;
      size_t _collectionsToLockOffset; // std::numeric_limits<size_t>::max() == removal only
      arangodb::iresearch::IResearchLink const* _link = nullptr;
      size_t _linkDefinitionsOffset;
      bool _valid = true;
      State(size_t collectionsToLockOffset)
        : State(collectionsToLockOffset, std::numeric_limits<size_t>::max()) {}
      State(size_t collectionsToLockOffset, size_t linkDefinitionsOffset)
        : _collectionsToLockOffset(collectionsToLockOffset), _linkDefinitionsOffset(linkDefinitionsOffset) {}
    };
    std::vector<std::string> collectionsToLock;
    std::vector<std::pair<arangodb::velocypack::Builder, arangodb::iresearch::IResearchLinkMeta>> linkDefinitions;
    std::vector<State> linkModifications;

    for (arangodb::velocypack::ObjectIterator linksItr(links); linksItr.valid(); ++linksItr) {
      auto collection = linksItr.key();

      if (!collection.isString()) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error parsing link parameters from json for iResearch view '") + std::to_string(view.id()) + "' offset '" + arangodb::basics::StringUtils::itoa(linksItr.index()) + '"'
        );
      }

      auto link = linksItr.value();
      auto collectionName = collection.copyString();

      if (link.isNull()) {
        linkModifications.emplace_back(collectionsToLock.size());
        collectionsToLock.emplace_back(collectionName);

        continue; // only removal requested
      }

      arangodb::velocypack::Builder namedJson;

      namedJson.openObject();

      if (!arangodb::iresearch::mergeSlice(namedJson, link)
          || !arangodb::iresearch::IResearchLink::setSkipViewRegistration(namedJson)
          || !arangodb::iresearch::IResearchLink::setType(namedJson)
          || !arangodb::iresearch::IResearchLink::setView(namedJson, view.id())) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failed to update link definition with the view name while updating iResearch view '") + std::to_string(view.id()) + "' collection '" + collectionName + "'"
        );
      }

      namedJson.close();

      std::string error;
      arangodb::iresearch::IResearchLinkMeta linkMeta;

      if (!linkMeta.init(namedJson.slice(), error)) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error parsing link parameters from json for iResearch view '") + std::to_string(view.id()) + "' collection '" + collectionName + "' error '" + error + "'"
        );
      }

      linkModifications.emplace_back(collectionsToLock.size(), linkDefinitions.size());
      collectionsToLock.emplace_back(collectionName);
      linkDefinitions.emplace_back(std::move(namedJson), std::move(linkMeta));
    }

    if (collectionsToLock.empty()) {
      return arangodb::Result(/*TRI_ERROR_NO_ERROR*/); // nothing to update
    }

    static std::vector<std::string> const EMPTY;

    // use default lock timeout
    arangodb::transaction::Options options;
    options.allowImplicitCollections = false;
    options.waitForSync = false;

    arangodb::transaction::UserTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      EMPTY, // readCollections
      EMPTY, // writeCollections
      collectionsToLock, // exclusiveCollections
      options
    );
    auto res = trx.begin();

    if (res.fail()) {
      return res;
//      return arangodb::Result(
//        res.error,
//        std::string("failed to start collection updating transaction for iResearch view '") + view.name() + "'"
//      );
    }

    auto* resolver = trx.resolver();

    if (!resolver) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to get resolver from transaction while updating while iResearch view '") + std::to_string(view.id()) + "'"
      );
    }

    // resolve corresponding collection and link
    for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
      auto& state = *itr;
      auto& collectionName = collectionsToLock[state._collectionsToLockOffset];

      state._collection = const_cast<arangodb::LogicalCollection*>(resolver->getCollectionStruct(collectionName));

      if (!state._collection) {
        return arangodb::Result(
          TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
          std::string("failed to get collection while updating iResearch view '") + std::to_string(view.id()) + "' collection '" + collectionName + "'"
        );
      }

      state._link = findFirstMatchingLink(*(state._collection), view);

      // remove modification state if no change on existing link or removal of non-existant link
      if ((state._link && state._linkDefinitionsOffset < linkDefinitions.size() && *(state._link) == linkDefinitions[state._linkDefinitionsOffset].second)
        || (!state._link && state._linkDefinitionsOffset >= linkDefinitions.size())) {
        itr = linkModifications.erase(itr);
        continue;
      }

      ++itr;
    }

    // execute removals
    for (auto& state: linkModifications) {
      if (state._link) {
        state._valid = state._collection->dropIndex(state._link->id());
      }
    }

    // execute additions
    for (auto& state: linkModifications) {
      if (state._valid && state._linkDefinitionsOffset < linkDefinitions.size()) {
        bool isNew = false;
        auto linkPtr = state._collection->createIndex(&trx, linkDefinitions[state._linkDefinitionsOffset].first.slice(), isNew);

        // TODO FIXME find a better way to retrieve an iResearch Link
        #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto* ptr = dynamic_cast<arangodb::iresearch::IResearchLink*>(linkPtr.get());
        #else
          auto* ptr = static_cast<arangodb::iresearch::IResearchLink*>(linkPtr.get());
        #endif

        // convert to pointer type registerable with IResearchView, and hold a reference to the original
        auto irsLinkPtr = arangodb::iresearch::IResearchView::LinkPtr(ptr, [linkPtr](arangodb::iresearch::IResearchLink*)->void{});

        state._valid = linkPtr && isNew && view.linkRegister(irsLinkPtr);
      }
    }

    std::string error;

    // validate success
    for (auto& state: linkModifications) {
      if (!state._valid) {
        error.append(error.empty() ? "" : ", ").append(collectionsToLock[state._collectionsToLockOffset]);
      }
    }

    if (error.empty()) {
      return arangodb::Result(trx.commit());
    }

    return arangodb::Result(
      TRI_ERROR_ARANGO_ILLEGAL_STATE,
      std::string("failed to update links while updating iResearch view '") + std::to_string(view.id()) + "', retry same request or examine errors for collections: " + error
    );
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught exception while updating links for iResearch view '" << view.id() << "': " << e.what();
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for iResearch view '") + std::to_string(view.id()) + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught exception while updating links for iResearch view '" << view.id() << "'";
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for iResearch view '") + std::to_string(view.id()) + "'"
    );
  }
}

// inserts ArangoDB document into IResearch index
inline void insertDocument(
    irs::index_writer::document& doc,
    arangodb::iresearch::FieldIterator& body,
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid) {
  using namespace arangodb::iresearch;

  // reuse the 'Field' instance stored
  // inside the 'FieldIterator' after
  auto& field = const_cast<Field&>(*body);

  // User fields
  while (body.valid()) {
    doc.insert<irs::Action::INDEX | irs::Action::STORE>(field);
    ++body;
  }

  // System fields
  // Indexed: CID
  Field::setCidValue(field, cid, Field::init_stream_t());
  doc.insert<irs::Action::INDEX>(field);

  // Indexed: RID
  Field::setRidValue(field, rid);
  doc.insert<irs::Action::INDEX>(field);

  // Stored: CID + RID
  DocumentPrimaryKey const primaryKey(cid, rid);
  doc.insert<irs::Action::STORE>(primaryKey);
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchView::DataStore::DataStore(DataStore&& other) noexcept {
  *this = std::move(other);
}

IResearchView::DataStore& IResearchView::DataStore::operator=(
    IResearchView::DataStore&& other
) noexcept {
  if (this != &other) {
    _directory = std::move(other._directory);
    _reader = std::move(other._reader);
    _writer = std::move(other._writer);
  }

  return *this;
}

IResearchView::DataStore::operator bool() const noexcept {
  return _directory && _writer;
}

IResearchView::MemoryStore::MemoryStore() {
  auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

  _directory = irs::directory::make<irs::memory_directory>();

  // create writer before reader to ensure data directory is present
  _writer = irs::index_writer::make(*_directory, format, irs::OM_CREATE_APPEND);
  _writer->commit(); // initialize 'store'
  _reader = irs::directory_reader::open(*_directory); // open after 'commit' for valid 'store'
}

IResearchView::SyncState::PolicyState::PolicyState(
    size_t intervalStep,
    std::shared_ptr<irs::index_writer::consolidation_policy_t> policy
): _intervalCount(0), _intervalStep(intervalStep), _policy(policy) {
}

IResearchView::SyncState::SyncState() noexcept
  : _cleanupIntervalCount(0),
    _cleanupIntervalStep(0) {
}

IResearchView::SyncState::SyncState(
    IResearchViewMeta::CommitBaseMeta const& meta
): SyncState() {
  _cleanupIntervalStep = meta._cleanupIntervalStep;

  for (auto& entry: meta._consolidationPolicies) {
    if (entry.policy()) {
      _consolidationPolicies.emplace_back(
        entry.intervalStep(),
        irs::memory::make_unique<irs::index_writer::consolidation_policy_t>(entry.policy())
      );
    }
  }
}

IResearchView::TidStore::TidStore(
    transaction::Methods& trx,
    std::function<void(transaction::Methods*)> const& trxCallback
) {
  // FIXME trx copies the provided callback inside,
  // probably it's better to store just references instead

  // register callback for newly created transactions
  trx.registerCallback(trxCallback);
}

IResearchView::IResearchView(
  arangodb::LogicalView* view,
  arangodb::velocypack::Slice const& info
) : ViewImplementation(view, info),
    FlushTransaction(toString(*this)),
   _asyncMetaRevision(1),
   _asyncTerminate(false),
   _memoryNode(_memoryNodes), // set current memory node
   _toFlush(_memoryNodes),
   _threadPool(0, 0) { // 0 == create pool with no threads, i.e. not running anything

  // initialize round-robin memory store chain
  auto it = _memoryNodes;
  for (auto next = it + 1, end = std::end(_memoryNodes); next < end; ++it, ++next) {
    it->_next = next;
  }
  it->_next = _memoryNodes;

  // initialize transaction callback
  _transactionCallback = [this](transaction::Methods* trx)->void {
    if (!trx || !trx->state()) {
      LOG_TOPIC(WARN, Logger::FIXME) << "failed to find transaction id while processing transaction callback for iResearch view '" << id() << "'";
      return; // 'trx' and transaction state required
    }

    switch (trx->status()) {
     case transaction::Status::ABORTED:
      finish(trx->state()->id(), false);
      return;
     case transaction::Status::COMMITTED:
      finish(trx->state()->id(), true);
      return;
     default:
      {} // NOOP
    }
  };

  // add asynchronous commit job
  _threadPool.run(
    [this]()->void {
    struct State: public SyncState {
      size_t _asyncMetaRevision;
      size_t _commitIntervalMsecRemainder;
      size_t _commitTimeoutMsec;

      State():
        SyncState(),
        _asyncMetaRevision(0), // '0' differs from IResearchView constructor above
        _commitIntervalMsecRemainder(std::numeric_limits<size_t>::max()),
        _commitTimeoutMsec(0) {
      }
      State(IResearchViewMeta::CommitItemMeta const& meta)
        : SyncState(meta),
          _asyncMetaRevision(0), // '0' differs from IResearchView constructor above
          _commitIntervalMsecRemainder(std::numeric_limits<size_t>::max()),
          _commitTimeoutMsec(meta._commitTimeoutMsec) {
      }
    };

    State state;
    ReadMutex mutex(_mutex); // '_meta' can be asynchronously modified

    for(;;) {
      // sleep until timeout
      {
        SCOPED_LOCK_NAMED(mutex, lock); // for '_meta._commitItem._commitIntervalMsec'
        SCOPED_LOCK_NAMED(_asyncMutex, asyncLock); // aquire before '_asyncTerminate' check

        if (_asyncTerminate.load()) {
          break;
        }

        if (!_meta._commitItem._commitIntervalMsec) {
          lock.unlock(); // do not hold read lock while waiting on condition
          _asyncCondition.wait(asyncLock); // wait forever
          continue;
        }

        auto startTime = std::chrono::system_clock::now();
        auto endTime = startTime
          + std::chrono::milliseconds(std::min(state._commitIntervalMsecRemainder, _meta._commitItem._commitIntervalMsec))
          ;

        lock.unlock(); // do not hold read lock while waiting on condition
        state._commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time assuming an uninterrupted sleep

        if (std::cv_status::timeout != _asyncCondition.wait_until(asyncLock, endTime)) {
          auto nowTime = std::chrono::system_clock::now();

          // if still need to sleep more then must relock '_meta' and sleep for min (remainder, interval)
          if (nowTime < endTime) {
            state._commitIntervalMsecRemainder = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - nowTime).count();

            continue; // need to reaquire lock to chech for change in '_meta'
          }
        }

        if (_asyncTerminate.load()) {
          break;
        }
      }

      // reload state if required
      if (_asyncMetaRevision.load() != state._asyncMetaRevision) {
        SCOPED_LOCK(mutex);
        state = State(_meta._commitItem);
        state._asyncMetaRevision = _asyncMetaRevision.load();
      }

      // perform sync
      sync(state, state._commitTimeoutMsec);
    }
  });
}

IResearchView::~IResearchView() {
  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  {
    SCOPED_LOCK(_asyncMutex);
    _asyncCondition.notify_all(); // trigger reload of settings for async jobs
  }

  _threadPool.max_threads_delta(int(std::max(size_t(std::numeric_limits<int>::max()), _threadPool.tasks_pending()))); // finish ASAP
  _threadPool.stop();

  _flushCallback.reset(); // unregister flush callback from flush thread

  WriteMutex mutex(_mutex); // '_meta' can be asynchronously read
  SCOPED_LOCK(mutex);

  if (_storePersisted) {
    _storePersisted._writer->commit();
    _storePersisted._writer->close();
    _storePersisted._writer.reset();
    _storePersisted._directory->close();
    _storePersisted._directory.reset();
  }
}

bool IResearchView::cleanup(size_t maxMsec /*= 0*/) {
  ReadMutex mutex(_mutex);
  auto thresholdSec = TRI_microtime() + maxMsec/1000.0;

  try {
    SCOPED_LOCK(mutex);

    LOG_TOPIC(DEBUG, Logger::FIXME) << "starting memory-store cleanup for iResearch view '" << id() << "'";
    auto& memoryStore = activeMemoryStore();
    memoryStore._writer->commit();
    LOG_TOPIC(DEBUG, Logger::FIXME) << "finished memory-store cleanup for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    if (_storePersisted) {
      LOG_TOPIC(DEBUG, Logger::FIXME) << "starting persisted-store cleanup for iResearch view '" << id() << "'";
      _storePersisted._writer->commit();
      LOG_TOPIC(DEBUG, Logger::FIXME) << "finished persisted-store cleanup for iResearch view '" << id() << "'";
    }

    return true;
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during cleanup of iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during cleanup of iResearch view '" << id() << "'";
    IR_EXCEPTION();
  }

  return false;
}

void IResearchView::drop() {
  // drop all known links
  if (_logicalView && _logicalView->vocbase()) {
    arangodb::velocypack::Builder builder;
    ReadMutex mutex(_mutex); // '_meta' can be asynchronously updated

    {
      arangodb::velocypack::ObjectBuilder builderWrapper(&builder);
      SCOPED_LOCK(mutex);

      for (auto& entry: _meta._collections) {
        builderWrapper->add(
          arangodb::basics::StringUtils::itoa(entry),
          arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
        );
      }
    }

    if (!updateLinks(*(_logicalView->vocbase()), *this, builder.slice()).ok()) {
      throw std::runtime_error(std::string("failed to remove links while removing iResearch view '") + std::to_string(id()) + "'");
    }
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  {
    SCOPED_LOCK(_asyncMutex);
    _asyncCondition.notify_all(); // trigger reload of settings for async jobs
  }

  _threadPool.stop();

  WriteMutex mutex(_mutex); // members can be asynchronously updated
  SCOPED_LOCK(mutex);

  // ArangoDB global consistency check, no known dangling links
  if (!_meta._collections.empty() || !_links.empty()) {
    throw std::runtime_error(std::string("links still present while removing iResearch view '") + std::to_string(id()) + "'");
  }

  // ...........................................................................
  // if an exception occurs below than a drop retry would most likely happen
  // ...........................................................................
  try {
    _storeByTid.clear();

    auto& memoryStore = activeMemoryStore();
    memoryStore._writer->close();
    memoryStore._writer.reset();
    memoryStore._directory->close();
    memoryStore._directory.reset();

    if (_storePersisted) {
      _storePersisted._writer->close();
      _storePersisted._writer.reset();
      _storePersisted._directory->close();
      _storePersisted._directory.reset();
    }

    if (!TRI_IsDirectory(_meta._dataPath.c_str()) || TRI_ERROR_NO_ERROR == TRI_RemoveDirectory(_meta._dataPath.c_str())) {
      return; // success
    }
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing iResearch view '" << id() << "'";
    IR_EXCEPTION();
    throw;
  }

  throw std::runtime_error(std::string("failed to remove iResearch view '") + arangodb::basics::StringUtils::itoa(id()) + "'");
}

int IResearchView::drop(TRI_voc_cid_t cid) {
  if (!_logicalView || !_logicalView->getPhysical()) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failed to find meta-store while dropping collection from iResearch view '" << id() <<"' cid '" << cid << "'";

    return TRI_ERROR_INTERNAL;
  }

  auto& metaStore = *(_logicalView->getPhysical());
  std::shared_ptr<irs::filter> shared_filter(iresearch::FilterFactory::filter(cid));
  WriteMutex mutex(_mutex); // '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);

  _meta._collections.erase(cid); // will no longer be fully indexed
  mutex.unlock(true); // downgrade to a read-lock

  auto res = metaStore.persistProperties(); // persist '_meta' definition

  if (!res.ok()) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failed to persist view definition while dropping collection from iResearch view '" << id() << "' cid '" << cid << "'";

    return res.errorNumber();
  }

  // ...........................................................................
  // if an exception occurs below than a drop retry would most likely happen
  // ...........................................................................
  try {
    for (auto& tidStore: _storeByTid) {
      tidStore.second._store._writer->remove(shared_filter);
    }

    auto& memoryStore = activeMemoryStore();
    memoryStore._writer->remove(shared_filter);

    if (_storePersisted) {
      _storePersisted._writer->remove(shared_filter);
    }

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing from iResearch view '" << id() << "', collection '" << cid << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing from iResearch view '" << id() << "', collection '" << cid << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

int IResearchView::finish(TRI_voc_tid_t tid, bool commit) {
  WriteMutex mutex(_mutex); // '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);
  auto tidStoreItr = _storeByTid.find(tid);

  if (tidStoreItr == _storeByTid.end()) {
    return TRI_ERROR_NO_ERROR; // nothing to finish
  }

  if (!commit) {
    _storeByTid.erase(tidStoreItr);

    return TRI_ERROR_NO_ERROR; // nothing more to do
  }

  // no need to lock TidStore::_mutex since have write-lock on IResearchView::_mutex
  auto removals = std::move(tidStoreItr->second._removals);
  auto trxStore = std::move(tidStoreItr->second._store);

  _storeByTid.erase(tidStoreItr);
  mutex.unlock(true); // downgrade to a read-lock

  try {
    // transfer filters first since they only apply to pre-merge data
    auto& memoryStore = activeMemoryStore();
    for (auto& filter: removals) {
      memoryStore._writer->remove(filter);
    }

    // transfer filters to persisted store as well otherwise query resuts will be incorrect
    // on recovery the same removals will be replayed from the WAL
    if (_storePersisted) {
      for (auto& filter: removals) {
        _storePersisted._writer->remove(filter);
      }
    }

    trxStore._writer->commit(); // ensure have latest view in reader
    memoryStore._writer->import(trxStore._reader.reopen());

    // FIXME remove it from here, once
    // view will be initialized properly in runtime
    // (currently _thread_pool doesn't start)
    memoryStore._writer->commit();

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception& e) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught exception while committing transaction for iResearch view '" << id() << "', tid '" << tid << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught exception while committing transaction for iResearch view '" << id() << "', tid '" << tid << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

int IResearchView::finish() {
  MemoryStore tmp; // new clear store

  ReadMutex mutex(_mutex); // '_storePersisted' can be asynchronously updated
  SCOPED_LOCK(mutex);

  if (!_storePersisted) {
    return TRI_ERROR_NO_ERROR; // nothing more to do
  }

  auto& memoryStore = _toFlush->_store;

  try {
    memoryStore._writer->commit(); // ensure have latest view in reader

    // merge memory store into persisted
    if (!_storePersisted._writer->import(memoryStore._reader.reopen())) {
      return TRI_ERROR_INTERNAL;
    }

    _storePersisted._writer->commit(); // finishing flush transaction

    // noexcept
    memoryStore = std::move(tmp); // FIXME find a better way to clear the store

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception& e) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught exception while committing memory store for iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(ERR, Logger::FIXME) << "caught exception while committing memory store for iResearch view '" << id();
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

void IResearchView::getPropertiesVPack(
  arangodb::velocypack::Builder& builder
) const {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_meta'/'_links' can be asynchronously updated

  _meta.json(builder);

  if (!_logicalView) {
    return; // nothing more to output
  }

  std::vector<std::string> collections;

  // add CIDs of known collections to list
  for (auto& entry: _meta._collections) {
    collections.emplace_back(arangodb::basics::StringUtils::itoa(entry));
  }

  // add CIDs of registered collections to list
  for (auto& entry: _links) {
    if (entry) {
      auto* collection = entry->collection();

      if (collection) {
        collections.emplace_back(arangodb::basics::StringUtils::itoa(collection->cid()));
      }
    }
  }

  arangodb::velocypack::Builder linksBuilder;

  static std::vector<std::string> const EMPTY;

  // use default lock timeout
  arangodb::transaction::Options options;
  options.waitForSync = false;
  options.allowImplicitCollections = false;

  try {
    arangodb::transaction::UserTransaction trx(
      transaction::StandaloneContext::Create(_logicalView->vocbase()),
      collections, // readCollections
      EMPTY, // writeCollections
      EMPTY, // exclusiveCollections
      options
    );

    if (trx.begin().fail()) {
      return; // nothing more to output
    }

    auto* state = trx.state();

    if (!state) {
      return; // nothing more to output
    }

    arangodb::velocypack::ObjectBuilder linksBuilderWrapper(&linksBuilder);

    for (auto& collectionName: state->collectionNames()) {
      for (auto& index: trx.indexesForCollection(collectionName)) {
        if (index && arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()) {
          // TODO FIXME find a better way to retrieve an iResearch Link
          #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
            auto* ptr = dynamic_cast<IResearchLink*>(index.get());
          #else
            auto* ptr = static_cast<IResearchLink*>(index.get());
          #endif

          if (!ptr || *ptr != *this) {
            continue; // the index is not a link for the current view
          }

          arangodb::velocypack::Builder linkBuilder;

          ptr->toVelocyPack(linkBuilder, false, false); // FIXE check last parameter
          linksBuilderWrapper->add(collectionName, linkBuilder.slice());
        }
      }
    }

    trx.commit();
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught error while generating json for iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
    return; // do not add 'links' section
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "caught error while generating json for iResearch view '" << id() << "'";
    IR_EXCEPTION();
    return; // do not add 'links' section
  }

  builder.add(LINKS_FIELD, linksBuilder.slice());
}

TRI_voc_cid_t IResearchView::id() const noexcept {
  return _logicalView ? _logicalView->id() : 0; // 0 is an invalid view id
}

int IResearchView::insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid,
    arangodb::velocypack::Slice const& doc,
    IResearchLinkMeta const& meta
) {
  if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction id required
  }

  FieldIterator body(doc, meta);

  if (!body.valid()) {
    // nothing to index
    return TRI_ERROR_NO_ERROR;
  }

  WriteMutex mutex(_mutex); // '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);

  auto storeItr = irs::map_utils::try_emplace(
    _storeByTid, trx.state()->id(), trx, _transactionCallback
  );

  mutex.unlock(true); // downgrade to a read-lock

  auto& store = storeItr.first->second._store;

  auto insert = [&body, cid, rid] (irs::index_writer::document& doc) {
    insertDocument(doc, body, cid, rid);

    return false; // break the loop
  };

  try {
    if (store._writer->insert(insert)) {
      return TRI_ERROR_NO_ERROR;
    }

    LOG_TOPIC(WARN, Logger::FIXME) << "failed inserting into iResearch view '" << id() << "', collection '" << cid << "', revision '" << rid << "'";
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while inserting into iResearch view '" << id() << "', collection '" << cid << "', revision '" << rid << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while inserting into iResearch view '" << id() << "', collection '" << cid << "', revision '" << rid << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

int IResearchView::insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const& batch,
    IResearchLinkMeta const& meta
) {
  if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction id required
  }

  WriteMutex mutex(_mutex); // '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex); // '_meta' can be asynchronously updated

  auto storeItr = irs::map_utils::try_emplace(
    _storeByTid, trx.state()->id(), trx, _transactionCallback
  );

  const size_t commitBatch = _meta._commitBulk._commitIntervalBatchSize;
  SyncState state = SyncState(_meta._commitBulk);

  mutex.unlock(true); // downgrade to a read-lock

  auto& store = storeItr.first->second._store;

  size_t batchCount = 0;
  auto begin = batch.begin();
  auto const end = batch.end();
  FieldIterator body;

  auto batchInsert = [&meta, &batchCount, &body, &begin, end, cid, commitBatch] (irs::index_writer::document& doc) mutable {
    body.reset(begin->second, meta);
    // FIXME: what if the body is empty?

    insertDocument(doc, body, cid, begin->first);

    return ++begin != end && ++batchCount < commitBatch;
  };

  while (begin != end) {
    if (commitBatch && batchCount >= commitBatch) {
      if (!sync(state)) {
        return TRI_ERROR_INTERNAL;
      }

      batchCount = 0;
    }

    try {
      if (!store._writer->insert(batchInsert)) {
        LOG_TOPIC(WARN, Logger::FIXME) << "failed inserting batch into iResearch view '" << id() << "', collection '" << cid;
        return TRI_ERROR_INTERNAL;
      }
    } catch (std::exception& e) {
      LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while inserting batch into iResearch view '" << id() << "', collection '" << cid << e.what();
      IR_EXCEPTION();
    } catch (...) {
      LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while inserting batch into iResearch view '" << id() << "', collection '" << cid;
      IR_EXCEPTION();
    }
  }

  if (commitBatch) {
    if (!sync(state)) {
      return TRI_ERROR_INTERNAL;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

arangodb::ViewIterator* IResearchView::iteratorForCondition(
    transaction::Methods* trx,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition
) {
  if (!trx) {
    LOG_TOPIC(WARN, Logger::FIXME) << "no transaction supplied while querying iResearch view '" << id() << "'";

    return nullptr;
  }

  if (!node) {
    LOG_TOPIC(WARN, Logger::FIXME) << "no query supplied while querying iResearch view '" << id() << "'";

    return nullptr;
  }

  irs::Or filter;

  if (!FilterFactory::filter(&filter, *node)) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failed to build filter while querying iResearch view '" << id() << "', query" << node->toVelocyPack(true)->toJson();

    return nullptr;
  }

  irs::order order;
  OrderFactory::OrderContext orderCtx{ order, *trx };
  ReadMutex mutex(_mutex); // members can be asynchronously updated
  SCOPED_LOCK(mutex);

  if (sortCondition && !OrderFactory::order(&orderCtx, *sortCondition, _meta)) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failed to build order while querying iResearch view '" << id() << "', query" << node->toVelocyPack(true)->toJson();

    return nullptr;
  }

  CompoundReader compoundReader; // FIXME remove add method

  try {
    auto& memoryStore = activeMemoryStore();
    auto reader = memoryStore._reader.reopen();

    compoundReader.add(reader); // refresh to latest version
    memoryStore._reader = reader; // cache reopened reader for reuse by other queries

    if (_storePersisted) {
      auto reader = _storePersisted._reader.reopen(); // refresh to latest version

      compoundReader.add(reader);
      _storePersisted._reader = reader; // cache reopened reader for reuse by other queries
    }
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while collecting readers for querying iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
    return nullptr;
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while collecting readers for querying iResearch view '" << id() << "'";
    IR_EXCEPTION();
    return nullptr;
  }

  // add CIDs of known collections to transaction
  for (auto& entry: _meta._collections) {
    trx->addCollectionAtRuntime(arangodb::basics::StringUtils::itoa(entry));
  }

  // add CIDs of registered collections to transaction
  for (auto& entry: _links) {
    if (entry) {
      auto* collection = entry->collection();

      if (collection) {
        trx->addCollectionAtRuntime(arangodb::basics::StringUtils::itoa(collection->cid()));
      }
    }
  }

  if (order.empty()) {
    PTR_NAMED(UnorderedViewIterator, iterator, *this, *trx, std::move(compoundReader), filter);

    return iterator.release();
  }

  PTR_NAMED(OrderedViewIterator, iterator, *this, *trx, std::move(compoundReader), filter, order);

  return iterator.release();
}

size_t IResearchView::linkCount() const noexcept {
  ReadMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  return _meta._collections.size();
}

bool IResearchView::linkRegister(LinkPtr& ptr) {
  if (!ptr || !ptr->collection()) {
    return false; // do not register empty pointers
  }

  if (!_logicalView || !_logicalView->getPhysical()) {
    return false; // cannot persist view
  }

  WriteMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  auto itr = _links.emplace(ptr);
  auto registered = _meta._collections.emplace(ptr->collection()->cid()).second;

  // do not allow duplicate registration
  if (registered) {
    auto* feature = arangodb::application_features::ApplicationServer::getFeature<arangodb::iresearch::IResearchFeature>("IResearch");

    if (feature && feature->running()) {
      auto res = _logicalView->getPhysical()->persistProperties(); // persist '_meta' definition

      if (res.ok()) {
        ptr->_view = this;

        return true;
      }
    }

    LOG_TOPIC(WARN, Logger::FIXME) << "failed to persist iResearch view definition during new iResearch link registration for iResearch view '" << id() <<"' cid '" << ptr->collection()->cid() << "' iid '" << ptr->id() << "'";

    _meta._collections.erase(ptr->collection()->cid()); // revert state
  } else if (itr.second) {
    // first time an IResearchLink object was seen for the specified cid
    // e.g. this happends during startup when links initially register with the view
    ptr->_view = this;

    return true;
  } else {
    LOG_TOPIC(WARN, Logger::FIXME) << "duplicate iResearch link registration detected for iResearch view '" << id() <<"' cid '" << ptr->collection()->cid() << "' iid '" << ptr->id() << "'";
  }

  if (itr.second) {
    _links.erase(itr.first); // revert state
  }

  return false;
}

bool IResearchView::linkUnregister(TRI_voc_cid_t cid) {
  if (!_logicalView || !_logicalView->getPhysical()) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failed to find meta-store while unregistering collection from iResearch view '" << id() <<"' cid '" << cid << "'";

    return false;
  }

  WriteMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);
  LinkPtr ptr;

  for (auto itr = _links.begin(); itr != _links.end();) {
    if (!*itr || !(*itr)->collection()) {
      itr = _links.erase(itr); // remove stale links

      continue;
    }

    if ((*itr)->collection()->cid() == cid) {
      ptr = *itr;
      itr = _links.erase(itr);

      continue;
    }

    ++itr;
  }

  auto unregistered = _meta._collections.erase(cid) > 0;

  if (!unregistered) {
    return false;
  }

  auto res = _logicalView->getPhysical()->persistProperties(); // persist '_meta' definition

  if (res.ok()) {
    return true;
  }

  if (ptr) {
    _links.emplace(ptr); // revert state
  }

  if (unregistered) {
    _meta._collections.emplace(cid); // revert state
  }

  return false;
}

/*static*/ IResearchView::ptr IResearchView::make(
  arangodb::LogicalView* view,
  arangodb::velocypack::Slice const& info,
  bool isNew
) {
  PTR_NAMED(IResearchView, ptr, view, info);
  auto& impl = reinterpret_cast<IResearchView&>(*ptr);
  auto& json = info.isNone() ? emptyObjectSlice() : info; // if no 'info' then assume defaults
  std::string error;

  if (!impl._meta.init(json, error)) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failed to initialize iResearch view from definition, error: " << error;

    return nullptr;
  }

  // skip link creation for previously created views or if no links were specified in the definition
  if (!isNew || !json.hasKey(LINKS_FIELD)) {
    return std::move(ptr);
  }

  if (!impl._logicalView || !impl._logicalView->vocbase()) {
    LOG_TOPIC(WARN, Logger::FIXME) << "failed to find vocbase while updating links for iResearch view '" << impl.id() << "'";

    return nullptr;
  }

  // create links for a new iresearch View instance
  auto res = updateLinks(*(impl._logicalView->vocbase()), impl, json.get(LINKS_FIELD));

  return res.ok() ? std::move(ptr) : nullptr;
}

size_t IResearchView::memory() const {
  ReadMutex mutex(_mutex); // view members can be asynchronously updated
  SCOPED_LOCK(mutex);
  size_t size = sizeof(IResearchView);

  for (auto& entry: _links) {
    size += sizeof(entry.get()) + sizeof(*(entry.get())); // link contents are not part of size calculation
  }

  size += _meta.memory();

  for (auto& tidEntry: _storeByTid) {
    size += sizeof(tidEntry.first) + sizeof(tidEntry.second);
    size += directoryMemory(*tidEntry.second._store._directory, id());

    // no way to determine size of actual filter
    SCOPED_LOCK(tidEntry.second._mutex);
    size += tidEntry.second._removals.size() * (sizeof(decltype(tidEntry.second._removals)::pointer) + sizeof(decltype(tidEntry.second._removals)::value_type));
  }

  size += sizeof(_memoryNode) + sizeof(_memoryNodes);
  size += directoryMemory(*activeMemoryStore()._directory, id());

  if (_storePersisted) {
    size += directoryMemory(*(_storePersisted._directory), id());
  }

  return size;
}

void IResearchView::open() {
  WriteMutex mutex(_mutex); // '_meta' can be asynchronously updated
  SCOPED_LOCK(mutex);

  if (_storePersisted) {
    return; // view already open
  }

  std::string absoluteDataPath;

  try {
    auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

    if (format && appendAbsolutePersistedDataPath(absoluteDataPath, *this, _meta)) {
      _storePersisted._directory =
        irs::directory::make<irs::fs_directory>(absoluteDataPath);

      if (_storePersisted._directory) {
        // create writer before reader to ensure data directory is present
        _storePersisted._writer =
          irs::index_writer::make(*(_storePersisted._directory), format, irs::OM_CREATE_APPEND);

        if (_storePersisted._writer) {
          _storePersisted._writer->commit(); // initialize 'store'
          _threadPool.max_idle(_meta._threadsMaxIdle);
          _threadPool.max_threads(_meta._threadsMaxTotal); // start pool

          _storePersisted._reader
            = irs::directory_reader::open(*(_storePersisted._directory));

          if (_storePersisted._reader) {
            registerFlushCallback();
            return; // success
          }

          _storePersisted._writer.reset(); // unlock the directory
        }
      }
    }
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while opening iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while opening iResearch view '" << id() << "'";
    IR_EXCEPTION();
    throw;
  }

  LOG_TOPIC(WARN, Logger::FIXME) << "failed to open iResearch view '" << id() << "' at: " << absoluteDataPath;
  throw std::runtime_error(std::string("failed to open iResearch view '") + std::to_string(id()) + "' at: " + absoluteDataPath);
}

int IResearchView::remove(
  transaction::Methods& trx,
  TRI_voc_cid_t cid,
  TRI_voc_rid_t rid
) {
  if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction id required
  }

  std::shared_ptr<irs::filter> shared_filter(FilterFactory::filter(cid, rid));
  WriteMutex mutex(_mutex); // '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);

  auto storeItr = irs::map_utils::try_emplace(
    _storeByTid, trx.state()->id(), trx, _transactionCallback
  );

  mutex.unlock(true); // downgrade to a read-lock

  auto& store = storeItr.first->second;

  // ...........................................................................
  // if an exception occurs below than the transaction is droped including all
  // all of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    store._store._writer->remove(shared_filter);

    SCOPED_LOCK(store._mutex); // '_removals' can be asynchronously updated
    store._removals.emplace_back(shared_filter);

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing from iResearch view '" << id() << "', collection '" << cid << "', revision '" << rid << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while removing from iResearch view '" << id() << "', collection '" << cid << "', revision '" << rid << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

arangodb::aql::AstNode* IResearchView::specializeCondition(
  arangodb::aql::Ast* ast,
  arangodb::aql::AstNode const* node,
  arangodb::aql::Variable const* reference
) {
  // FIXME TODO implement
  return nullptr;
}

bool IResearchView::supportsFilterCondition(
  arangodb::aql::AstNode const* node,
  arangodb::aql::Variable const* reference,
  size_t& estimatedItems,
  double& estimatedCost
) const {
  // no way to estimate items/cost before preparing the query
  return node && FilterFactory::filter(nullptr, *node);
}

bool IResearchView::supportsSortCondition(
  arangodb::aql::SortCondition const* sortCondition,
  arangodb::aql::Variable const* reference,
  double& estimatedCost,
  size_t& coveredAttributes
) const {
  irs::order order;
  ReadMutex mutex(_mutex); // '_meta' can be asynchronously updated
  SCOPED_LOCK(mutex);

  return sortCondition && OrderFactory::order(nullptr, *sortCondition, _meta);
}

IResearchView::MemoryStore& IResearchView::activeMemoryStore() const {
  return _memoryNode->_store;
}

bool IResearchView::sync(size_t maxMsec /*= 0*/) {
  ReadMutex mutex(_mutex);
  auto thresholdSec = TRI_microtime() + maxMsec/1000.0;

  try {
    SCOPED_LOCK(mutex);

    LOG_TOPIC(DEBUG, Logger::FIXME) << "starting memory-store sync for iResearch view '" << id() << "'";
    auto& memoryStore = activeMemoryStore();
    memoryStore._writer->commit();
    LOG_TOPIC(DEBUG, Logger::FIXME) << "finished memory-store sync for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    if (_storePersisted) {
      LOG_TOPIC(DEBUG, Logger::FIXME) << "starting persisted-sync cleanup for iResearch view '" << id() << "'";
      _storePersisted._writer->commit();
      LOG_TOPIC(DEBUG, Logger::FIXME) << "finished persisted-sync cleanup for iResearch view '" << id() << "'";
    }

    return true;
  } catch (std::exception& e) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during sync of iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::FIXME) << "caught exception during sync of iResearch view '" << id() << "'";
    IR_EXCEPTION();
  }

  return false;
}

bool IResearchView::sync(SyncState& state, size_t maxMsec /*= 0*/) {
  char runId = 0; // value not used
  auto thresholdMsec = TRI_microtime() * 1000 + maxMsec;
  ReadMutex mutex(_mutex); // '_memoryStore'/'_storePersisted' can be asynchronously modified

  LOG_TOPIC(DEBUG, Logger::FIXME) << "starting flush for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

  // .............................................................................
  // apply consolidation policies
  // .............................................................................
  for (auto& entry: state._consolidationPolicies) {
    if (!entry._intervalStep || ++entry._intervalCount < entry._intervalStep) {
      continue; // skip if interval not reached or no valid policy to execute
    }

    entry._intervalCount = 0;
    LOG_TOPIC(DEBUG, Logger::FIXME) << "starting consolidation for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

    try {
      SCOPED_LOCK(mutex);

      auto& memoryStore = activeMemoryStore();
      memoryStore._writer->consolidate(entry._policy, false);

      if (_storePersisted) {
        _storePersisted._writer->consolidate(entry._policy, false);
      }
    } catch (std::exception& e) {
      LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while consolidating iResearch view '" << id() << "': " << e.what();
      IR_EXCEPTION();
    } catch (...) {
      LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while consolidating iResearch view '" << id() << "'";
      IR_EXCEPTION();
    }

    LOG_TOPIC(DEBUG, Logger::FIXME) << "finished consolidation for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";
  }

  // .............................................................................
  // apply data store commit
  // .............................................................................

  LOG_TOPIC(DEBUG, Logger::FIXME) << "starting commit for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";
  auto res = sync(std::max(size_t(1), size_t(thresholdMsec - TRI_microtime() * 1000))); // set min 1 msec to enable early termination
  LOG_TOPIC(DEBUG, Logger::FIXME) << "finished commit for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

  // .............................................................................
  // apply cleanup
  // .............................................................................
  if (state._cleanupIntervalStep && ++state._cleanupIntervalCount >= state._cleanupIntervalStep) {
    state._cleanupIntervalCount = 0;
    LOG_TOPIC(DEBUG, Logger::FIXME) << "starting cleanup for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";
    cleanup(std::max(size_t(1), size_t(thresholdMsec - TRI_microtime() * 1000))); // set min 1 msec to enable early termination
    LOG_TOPIC(DEBUG, Logger::FIXME) << "finished cleanup for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";
  }

  LOG_TOPIC(DEBUG, Logger::FIXME) << "finished flush for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

  return res;
}

/*static*/ std::string const& IResearchView::type() noexcept {
  static const std::string  type = "iresearch";

  return type;
}

arangodb::Result IResearchView::updateProperties(
  arangodb::velocypack::Slice const& slice,
  bool partialUpdate,
  bool doSync
) {
  if (!_logicalView || !_logicalView->getPhysical()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find meta-store while updating iResearch view '") + std::to_string(id()) + "'"
    );
  }

  auto* vocbase = _logicalView->vocbase();

  if (!vocbase) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find vocbase while updating links for iResearch view '") + std::to_string(id()) + "'"
    );
  }

  auto& metaStore = *(_logicalView->getPhysical());
  std::string error;
  IResearchViewMeta meta;
  IResearchViewMeta::Mask mask;
  WriteMutex mutex(_mutex); // '_meta' can be asynchronously read
  arangodb::Result res = arangodb::Result(/*TRI_ERROR_NO_ERROR*/);

  {
    SCOPED_LOCK(mutex);

    arangodb::velocypack::Builder originalMetaJson; // required for reverting links on failure

    if (!_meta.json(arangodb::velocypack::ObjectBuilder(&originalMetaJson))) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to generate json definition while updating iResearch view '") + std::to_string(id()) + "'"
      );
    }

    if (!meta.init(slice, error, partialUpdate ? _meta : IResearchViewMeta::DEFAULT(), &mask)) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER, std::move(error));
    }

    // reset non-updatable values to match current meta
    meta._collections = _meta._collections;

    DataStore storePersisted; // renamed persisted data store
    std::string srcDataPath = _meta._dataPath;
    char const* dropDataPath = nullptr;

    // copy directory to new location
    if (mask._dataPath) {
      std::string absoluteDataPath;

      if (!appendAbsolutePersistedDataPath(absoluteDataPath, *this, meta)) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error generating absolute path for iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      }

      auto res = createPersistedDataDirectory(
        storePersisted._directory,
        storePersisted._writer,
        absoluteDataPath,
        _storePersisted._reader, // reader from the original persisted data store
        id()
      );

      if (!res.ok()) {
        return res;
      }

      try {
        storePersisted._reader = irs::directory_reader::open(*(storePersisted._directory));
        dropDataPath = _storePersisted ? srcDataPath.c_str() : nullptr;
      } catch (std::exception& e) {
        LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while opening iResearch view '" << id() << "' data path '" + meta._dataPath + "': " << e.what();
        IR_EXCEPTION();
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error opening iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      } catch (...) {
        LOG_TOPIC(WARN, Logger::FIXME) << "caught exception while opening iResearch view '" << id() << "' data path '" + meta._dataPath + "'";
        IR_EXCEPTION();
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error opening iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      }

      if (!storePersisted._reader) {
        LOG_TOPIC(WARN, Logger::FIXME) << "error while opening reader for iResearch view '" << id() << "' data path '" + meta._dataPath + "'";
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error opening reader for iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      }
    }

    IResearchViewMeta metaBackup;

    metaBackup = std::move(_meta);
    _meta = std::move(meta);

    res = metaStore.persistProperties(); // persist '_meta' definition (so that on failure can revert meta)

    if (!res.ok()) {
      _meta = std::move(metaBackup); // revert to original meta
      LOG_TOPIC(WARN, Logger::FIXME) << "failed to persist view definition while updating iResearch view '" << id() << "'";

      return res;
    }

    if (mask._dataPath) {
      _storePersisted = std::move(storePersisted);
    }

    if (mask._threadsMaxIdle) {
      _threadPool.max_idle(meta._threadsMaxIdle);
    }

    if (mask._threadsMaxTotal) {
      _threadPool.max_threads(meta._threadsMaxTotal);
    }

    {
      SCOPED_LOCK(_asyncMutex);
      _asyncCondition.notify_all(); // trigger reload of timeout settings for async jobs
    }

    if (dropDataPath) {
      res = TRI_RemoveDirectory(dropDataPath); // ignore error (only done to tidy-up filesystem)
    }
  }

  // update links if requested (on a best-effort basis)
  // indexing of collections is done in different threads so no locks can be held and rollback is not possible
  // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
  if (slice.hasKey(LINKS_FIELD)) {
    res = updateLinks(*vocbase, *this, slice.get(LINKS_FIELD));
  }

  return res;
}

void IResearchView::registerFlushCallback() {
  getFlushFeature().registerCallback(this, [this]() noexcept {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief opens a flush transaction and returns a control object to be used
    ///        by FlushThread spawned by FlushFeature
    /// @returns empty object if something's gone wrong
    ////////////////////////////////////////////////////////////////////////////////
    WriteMutex mutex(_mutex); // ensure that _memoryNode->_store is not in use
    SCOPED_LOCK(mutex);

    _toFlush = _memoryNode; // memory store to be flushed into the persisted store
    _memoryNode = _memoryNode->_next; // switch to the next node

    mutex.unlock(true); // downgrade to a read-lock

    return IResearchView::FlushTransactionPtr(
      this,
      [](arangodb::FlushTransaction*){} // empty deleter
    );
  });

  // noexcept
  _flushCallback.reset(this); // mark for future unregistration
}

void IResearchView::FlushCallbackUnregisterer::operator()(IResearchView* view) const noexcept {
  if (!view) {
    return;
  }

  try {
    getFlushFeature().unregisterCallback(view);
  } catch (...) {
    // suppress all errors
  }
}

arangodb::Result IResearchView::commit() {
  // FIMXE handle fails
  return arangodb::Result(finish());
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
