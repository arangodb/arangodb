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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBFulltextIndex.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Basics/StaticStrings.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/memory.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>

using namespace arangodb;

namespace arangodb {
/// El Cheapo index iterator
class RocksDBFulltextIndexIterator final : public IndexIterator {
 public:
  RocksDBFulltextIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                               std::set<LocalDocumentId>&& docs)
      : IndexIterator(collection, trx), _docs(std::move(docs)), _pos(_docs.begin()) {}

  char const* typeName() const override { return "fulltext-index-iterator"; }

  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    TRI_ASSERT(limit > 0);
    while (_pos != _docs.end() && limit > 0) {
      cb(*_pos);
      ++_pos;
      limit--;
    }
    return _pos != _docs.end();
  }

  void resetImpl() override { _pos = _docs.begin(); }

  void skipImpl(uint64_t count, uint64_t& skipped) override {
    while (_pos != _docs.end() && skipped < count) {
      ++_pos;
      skipped++;
    }
  }

 private:
  std::set<LocalDocumentId> const _docs;
  std::set<LocalDocumentId>::iterator _pos;
};

} // namespace

RocksDBFulltextIndex::RocksDBFulltextIndex(IndexId iid, arangodb::LogicalCollection& collection,
                                           arangodb::velocypack::Slice const& info)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::fulltext(), false),
      _minWordLength(FulltextIndexLimits::minWordLengthDefault) {
  TRI_ASSERT(iid.isSet());
  TRI_ASSERT(_cf == RocksDBColumnFamily::fulltext());

  VPackSlice const value = info.get("minLength");

  if (value.isNumber()) {
    _minWordLength = value.getNumericValue<int>();
    if (_minWordLength <= 0) {
      // The min length cannot be negative.
      _minWordLength = 1;
    }
  } else if (!value.isNone()) {
    // minLength defined but no number
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "<minLength> must be a number");
  }
  _unique = false;
  _sparse = true;
  if (_fields.size() != 1) {
    // We need exactly 1 attribute
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "fulltext index definition should have exactly one attribute");
  }
  auto& attribute = _fields[0];
  _attr.reserve(attribute.size());
  for (auto& a : attribute) {
    _attr.emplace_back(a.name);
  }
}

/// @brief return a VelocyPack representation of the index
void RocksDBFulltextIndex::toVelocyPack(VPackBuilder& builder,
                                        std::underlying_type<Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add("minLength", VPackValue(_minWordLength));
  builder.close();
}

/// @brief Test if this index matches the definition
bool RocksDBFulltextIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = info.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  arangodb::velocypack::StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }

    // Short circuit. If id is correct the index is identical.
    arangodb::velocypack::StringRef idRef(value);
    return idRef == std::to_string(_iid.id());
  }

  value = info.get("minLength");
  if (value.isNumber()) {
    int cmp = value.getNumericValue<int>();
    if (cmp <= 0) {
      if (_minWordLength != 1) {
        return false;
      }
    } else {
      if (_minWordLength != cmp) {
        return false;
      }
    }
  } else if (!value.isNone()) {
    // Illegal minLength
    return false;
  }

  value = info.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());

  if (n != _fields.size()) {
    return false;
  }

  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, arangodb::StaticStrings::IndexUnique, false)) {
    return false;
  }

  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, arangodb::StaticStrings::IndexSparse, true)) {
    return false;
  }

  // This check takes ordering of attributes into account.
  std::vector<arangodb::basics::AttributeName> translate;
  for (size_t i = 0; i < n; ++i) {
    translate.clear();
    VPackSlice f = value.at(i);
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    arangodb::velocypack::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate, false)) {
      return false;
    }
  }
  return true;
}

Result RocksDBFulltextIndex::insert(transaction::Methods& trx, RocksDBMethods* mthd,
                                    LocalDocumentId const& documentId,
                                    velocypack::Slice const& doc,
                                    OperationOptions& options) {
  Result res;
  std::set<std::string> words = wordlist(doc);

  if (words.empty()) {
    return res;
  }

  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  RocksDBValue value = RocksDBValue::VPackIndexValue();

  // size_t const count = words.size();
  for (std::string const& word : words) {
    RocksDBKeyLeaser key(&trx);
    key->constructFulltextIndexValue(objectId(), arangodb::velocypack::StringRef(word),
                                     documentId);
    TRI_ASSERT(key->containsLocalDocumentId(documentId));

    rocksdb::Status s = mthd->PutUntracked(_cf, key.ref(), value.string());

    if (!s.ok()) {
      res.reset(rocksutils::convertStatus(s, rocksutils::index));
      addErrorMsg(res);
      break;
    }
  }

  return res;
}

Result RocksDBFulltextIndex::remove(transaction::Methods& trx, RocksDBMethods* mthd,
                                    LocalDocumentId const& documentId,
                                    velocypack::Slice const& doc,
                                    Index::OperationMode mode) {
  Result res;
  std::set<std::string> words = wordlist(doc);

  if (words.empty()) {
    return res;
  }

  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  for (std::string const& word : words) {
    RocksDBKeyLeaser key(&trx);

    key->constructFulltextIndexValue(objectId(), arangodb::velocypack::StringRef(word),
                                     documentId);

    rocksdb::Status s = mthd->Delete(_cf, key.ref());

    if (!s.ok()) {
      res.reset(rocksutils::convertStatus(s, rocksutils::index));
      addErrorMsg(res);
      break;
    }
  }

  return res;
}

/// @brief walk over the attribute. Also Extract sub-attributes and elements in
///        list.
static void ExtractWords(std::set<std::string>& words, VPackSlice const value,
                         size_t minWordLength, int level) {
  if (value.isString()) {
    // extract the string value for the indexed attribute
    // parse the document text
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(words, value.stringRef(),
                                                             minWordLength, FulltextIndexLimits::maxWordLength,
                                                             true);
    // We don't care for the result. If the result is false, words stays
    // unchanged and is not indexed
  } else if (value.isArray() && level == 0) {
    for (VPackSlice v : VPackArrayIterator(value)) {
      ExtractWords(words, v, minWordLength, level + 1);
    }
  } else if (value.isObject() && level == 0) {
    for (VPackObjectIterator::ObjectPair v : VPackObjectIterator(value)) {
      ExtractWords(words, v.value, minWordLength, level + 1);
    }
  }
}

/// @brief callback function called by the fulltext index to determine the
/// words to index for a specific document
std::set<std::string> RocksDBFulltextIndex::wordlist(VPackSlice const& doc) {
  std::set<std::string> words;
  VPackSlice const value = doc.get(_attr);

  if (!value.isString() && !value.isArray() && !value.isObject()) {
    // Invalid Input
    return words;
  }

  ExtractWords(words, value, _minWordLength, 0);
  return words;
}

Result RocksDBFulltextIndex::parseQueryString(std::string const& qstr, FulltextQuery& query) {
  if (qstr.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER);
  }

  const char* ptr = qstr.data();
  int i = 0;

  while (*ptr) {
    char c = *ptr;
    // ignore whitespace
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
        c == '\b' || c == ',') {
      ++ptr;
      continue;
    }

    // defaults
    FulltextQueryToken::Operation operation = FulltextQueryToken::AND;
    FulltextQueryToken::MatchType matchType = FulltextQueryToken::COMPLETE;

    // word begin
    // get operation
    if (c == '+') {
      operation = FulltextQueryToken::AND;
      ++ptr;
    } else if (c == '|') {
      operation = FulltextQueryToken::OR;
      ++ptr;
    } else if (c == '-') {
      operation = FulltextQueryToken::EXCLUDE;
      ++ptr;
    }

    // find a word with ':' at the end, i.e. prefix: or complete:
    // set ptr to the end of the word
    char const* split = nullptr;
    char const* start = ptr;
    while (*ptr) {
      c = *ptr;
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
          c == '\b' || c == ',') {
        // end of word
        break;
      } else if (split == nullptr && c == ':') {
        split = ptr + 1;
      }
      ++ptr;
    }
    char const* end = ptr;

    if ((end - start == 0) || (split != nullptr && split - start == 0) ||
        (split != nullptr && end - split == 0)) {
      // invalid string
      return Result(TRI_ERROR_BAD_PARAMETER);
    }

    // get command
    if (split != nullptr) {
      if (TRI_CaseEqualString(start, "prefix:", strlen("prefix:"))) {
        matchType = FulltextQueryToken::PREFIX;
      } else if (TRI_CaseEqualString(start, "complete:", strlen("complete:"))) {
        matchType = FulltextQueryToken::COMPLETE;
      }
      start = split;
    }

    // normalize a word for a fulltext search query this will create a copy of
    // the word
    char const* word = start;
    size_t wordLength = (size_t)(end - start);

    TRI_ASSERT(end >= start);
    size_t outLength;
    char* normalized = TRI_normalize_utf8_to_NFC(word, wordLength, &outLength);
    if (normalized == nullptr) {
      return Result(TRI_ERROR_OUT_OF_MEMORY);
    }

    // lower case string
    int32_t outLength2;
    char* lowered = TRI_tolower_utf8(normalized, (int32_t)outLength, &outLength2);
    TRI_Free(normalized);
    if (lowered == nullptr) {
      return Result(TRI_ERROR_OUT_OF_MEMORY);
    }
    // emplace_back below may throw
    TRI_DEFER(TRI_Free(lowered));

    // calculate the proper prefix
    char* prefixEnd = TRI_PrefixUtf8String(lowered, FulltextIndexLimits::maxWordLength);
    ptrdiff_t prefixLength = prefixEnd - lowered;

    query.emplace_back(std::string(lowered, (size_t)prefixLength), matchType, operation);

    ++i;
    if (i >= FulltextIndexLimits::maxSearchWords) {
      break;
    }
  }

  if (!query.empty()) {
    query[0].operation = FulltextQueryToken::OR;
  }

  return Result(i == 0 ? TRI_ERROR_BAD_PARAMETER : TRI_ERROR_NO_ERROR);
}

Result RocksDBFulltextIndex::executeQuery(transaction::Methods* trx,
                                          FulltextQuery const& query,
                                          std::set<LocalDocumentId>& resultSet) {
  for (size_t i = 0; i < query.size(); i++) {
    FulltextQueryToken const& token = query[i];
    if (i > 0 && token.operation != FulltextQueryToken::OR && resultSet.empty()) {
      // skip tokens which won't do anything
      continue;
    }
    Result res = applyQueryToken(trx, token, resultSet);
    if (res.fail()) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

static RocksDBKeyBounds MakeBounds(uint64_t oid, FulltextQueryToken const& token) {
  if (token.matchType == FulltextQueryToken::COMPLETE) {
    return RocksDBKeyBounds::FulltextIndexComplete(oid, arangodb::velocypack::StringRef(token.value));
  } else if (token.matchType == FulltextQueryToken::PREFIX) {
    return RocksDBKeyBounds::FulltextIndexPrefix(oid, arangodb::velocypack::StringRef(token.value));
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result RocksDBFulltextIndex::applyQueryToken(transaction::Methods* trx,
                                             FulltextQueryToken const& token,
                                             std::set<LocalDocumentId>& resultSet) {
  auto mthds = RocksDBTransactionState::toMethods(trx);
  // why can't I have an assignment operator when I want one
  RocksDBKeyBounds bounds = MakeBounds(objectId(), token);
  rocksdb::Slice end = bounds.end();
  rocksdb::Comparator const* cmp = this->comparator();

  rocksdb::ReadOptions ro = mthds->iteratorReadOptions();
  ro.iterate_upper_bound = &end;
  std::unique_ptr<rocksdb::Iterator> iter = mthds->NewIterator(ro, _cf);

  // set is used to perform an intersection with the result set
  std::set<LocalDocumentId> intersect;
  // apply left to right logic, merging all current results with ALL previous
  for (iter->Seek(bounds.start());
       iter->Valid() && cmp->Compare(iter->key(), end) < 0;
       iter->Next()) {
    TRI_ASSERT(objectId() == RocksDBKey::objectId(iter->key()));

    rocksdb::Status s = iter->status();
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    LocalDocumentId documentId = RocksDBKey::indexDocumentId(iter->key());
    if (token.operation == FulltextQueryToken::AND) {
      intersect.insert(documentId);
    } else if (token.operation == FulltextQueryToken::OR) {
      resultSet.insert(documentId);
    } else if (token.operation == FulltextQueryToken::EXCLUDE) {
      resultSet.erase(documentId);
    }
  }
  if (token.operation == FulltextQueryToken::AND) {
    if (resultSet.empty() || intersect.empty()) {
      resultSet.clear();
    } else {
      std::set<LocalDocumentId> output;
      std::set_intersection(resultSet.begin(), resultSet.end(), intersect.begin(),
                            intersect.end(), std::inserter(output, output.begin()));
      resultSet = std::move(output);
    }
  }
  return Result();
}

std::unique_ptr<IndexIterator> RocksDBFulltextIndex::iteratorForCondition(
    transaction::Methods* trx, aql::AstNode const* condNode,
    aql::Variable const* var, IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  TRI_ASSERT(condNode != nullptr);
  TRI_ASSERT(condNode->numMembers() == 1);  // should only be an FCALL

  aql::AstNode const* fcall = condNode->getMember(0);
  TRI_ASSERT(fcall->type == arangodb::aql::NODE_TYPE_FCALL);
  TRI_ASSERT(fcall->numMembers() == 1);
  aql::AstNode const* args = fcall->getMember(0);

  size_t numMembers = args->numMembers();
  TRI_ASSERT(numMembers == 3 || numMembers == 4);

  aql::AstNode const* queryNode = args->getMember(2);
  if (queryNode->type != aql::NODE_TYPE_VALUE || queryNode->value.type != aql::VALUE_TYPE_STRING) {
    std::string message = basics::Exception::FillExceptionString(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FULLTEXT");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, message);
  }

  FulltextQuery parsedQuery;
  Result res = parseQueryString(queryNode->getString(), parsedQuery);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  std::set<LocalDocumentId> results;
  res = executeQuery(trx, parsedQuery, results);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return std::make_unique<RocksDBFulltextIndexIterator>(&_collection, trx, std::move(results));
}
