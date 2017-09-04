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

#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tri-strings.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/DocumentIdentifierToken.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>

using namespace arangodb;

TRI_voc_rid_t RocksDBFulltextIndex::fromDocumentIdentifierToken(
    DocumentIdentifierToken const& token) {
  auto tkn = static_cast<RocksDBToken const*>(&token);
  return tkn->revisionId();
}

DocumentIdentifierToken RocksDBFulltextIndex::toDocumentIdentifierToken(
    TRI_voc_rid_t revisionId) {
  return RocksDBToken{revisionId};
}

RocksDBFulltextIndex::RocksDBFulltextIndex(
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    VPackSlice const& info)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::fulltext(), false),
      _minWordLength(TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT) {
  TRI_ASSERT(iid != 0);
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

RocksDBFulltextIndex::~RocksDBFulltextIndex() {}

/// @brief return a VelocyPack representation of the index
void RocksDBFulltextIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                        bool forPersistence) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(true));
  builder.add("minLength", VPackValue(_minWordLength));
  builder.close();
}

/// @brief Test if this index matches the definition
bool RocksDBFulltextIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get("id");
  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
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

  value = info.get("fields");
  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "sparse", true)) {
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
    arangodb::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                      false)) {
      return false;
    }
  }
  return true;
}

Result RocksDBFulltextIndex::insertInternal(transaction::Methods* trx,
                                            RocksDBMethods* mthd,
                                            TRI_voc_rid_t revisionId,
                                            VPackSlice const& doc) {
  std::set<std::string> words = wordlist(doc);
  if (words.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  RocksDBValue value = RocksDBValue::VPackIndexValue();

  int res = TRI_ERROR_NO_ERROR;
  // size_t const count = words.size();
  for (std::string const& word : words) {
    RocksDBKeyLeaser key(trx);
    key->constructFulltextIndexValue(_objectId, StringRef(word), revisionId);

    Result r = mthd->Put(_cf, key.ref(), value.string(), rocksutils::index);
    if (!r.ok()) {
      res = r.errorNumber();
      break;
    }
  }
  return IndexResult(res, this);
}

Result RocksDBFulltextIndex::removeInternal(transaction::Methods* trx,
                                            RocksDBMethods* mthd,
                                            TRI_voc_rid_t revisionId,
                                            VPackSlice const& doc) {
  std::set<std::string> words = wordlist(doc);
  if (words.empty()) {
    return IndexResult(TRI_ERROR_NO_ERROR);
  }

  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  int res = TRI_ERROR_NO_ERROR;
  for (std::string const& word : words) {
    RocksDBKeyLeaser key(trx);
    key->constructFulltextIndexValue(_objectId, StringRef(word), revisionId);

    Result r = mthd->Delete(_cf, key.ref());
    if (!r.ok()) {
      res = r.errorNumber();
      break;
    }
  }
  return IndexResult(res, this);
}

/// @brief walk over the attribute. Also Extract sub-attributes and elements in
///        list.
static void ExtractWords(std::set<std::string>& words, VPackSlice const value,
                         size_t minWordLength, int level) {
  if (value.isString()) {
    // extract the string value for the indexed attribute
    std::string text = value.copyString();

    // parse the document text
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(
        words, text, minWordLength, TRI_FULLTEXT_MAX_WORD_LENGTH, true);
    // We don't care for the result. If the result is false, words stays
    // unchanged and is not indexed
  } else if (value.isArray() && level == 0) {
    for (auto const& v : VPackArrayIterator(value)) {
      ExtractWords(words, v, minWordLength, level + 1);
    }
  } else if (value.isObject() && level == 0) {
    for (auto const& v : VPackObjectIterator(value)) {
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

Result RocksDBFulltextIndex::parseQueryString(std::string const& qstr,
                                              FulltextQuery& query) {
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
    char* normalized = TRI_normalize_utf8_to_NFC(TRI_UNKNOWN_MEM_ZONE, word,
                                                 wordLength, &outLength);
    if (normalized == nullptr) {
      return Result(TRI_ERROR_OUT_OF_MEMORY);
    }

    // lower case string
    int32_t outLength2;
    char* lowered = TRI_tolower_utf8(TRI_UNKNOWN_MEM_ZONE, normalized,
                                     (int32_t)outLength, &outLength2);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, normalized);
    if (lowered == nullptr) {
      return Result(TRI_ERROR_OUT_OF_MEMORY);
    }
    // emplace_back below may throw
    TRI_DEFER(TRI_Free(TRI_UNKNOWN_MEM_ZONE, lowered));

    // calculate the proper prefix
    char* prefixEnd =
        TRI_PrefixUtf8String(lowered, TRI_FULLTEXT_MAX_WORD_LENGTH);
    ptrdiff_t prefixLength = prefixEnd - lowered;

    query.emplace_back(std::string(lowered, (size_t)prefixLength), matchType,
                       operation);

    ++i;
    if (i >= TRI_FULLTEXT_SEARCH_MAX_WORDS) {
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
                                          size_t maxResults,
                                          VPackBuilder& builder) {
  std::set<TRI_voc_rid_t> resultSet;
  for (FulltextQueryToken const& token : query) {
    applyQueryToken(trx, token, resultSet);
  }

  auto physical = static_cast<RocksDBCollection*>(_collection->getPhysical());
  ManagedDocumentResult mmdr;
  if (maxResults == 0) {  // 0 appearantly means "all results"
    maxResults = SIZE_MAX;
  }

  builder.openArray();
  // get the first N results
  std::set<TRI_voc_rid_t>::iterator it = resultSet.cbegin();
  while (maxResults > 0 && it != resultSet.cend()) {
    RocksDBToken token(*it);
    if (token.revisionId() && physical->readDocument(trx, token, mmdr)) {
      mmdr.addToBuilder(builder, false);
      maxResults--;
    }
    ++it;
  }
  builder.close();

  return Result();
}

static RocksDBKeyBounds MakeBounds(uint64_t oid,
                                   FulltextQueryToken const& token) {
  if (token.matchType == FulltextQueryToken::COMPLETE) {
    return RocksDBKeyBounds::FulltextIndexComplete(oid, StringRef(token.value));
  } else if (token.matchType == FulltextQueryToken::PREFIX) {
    return RocksDBKeyBounds::FulltextIndexPrefix(oid, StringRef(token.value));
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Result RocksDBFulltextIndex::applyQueryToken(
    transaction::Methods* trx, FulltextQueryToken const& token,
    std::set<TRI_voc_rid_t>& resultSet) {
  auto mthds = RocksDBTransactionState::toMethods(trx);
  // why can't I have an assignment operator when I want one
  RocksDBKeyBounds bounds = MakeBounds(_objectId, token);
  rocksdb::Slice end = bounds.end();
  rocksdb::Comparator const* cmp = this->comparator();

  rocksdb::ReadOptions ro = mthds->readOptions();
  ro.iterate_upper_bound = &end;
  std::unique_ptr<rocksdb::Iterator> iter = mthds->NewIterator(ro, _cf);
  iter->Seek(bounds.start());

  // set is used to perform an intersection with the result set
  std::set<TRI_voc_rid_t> intersect;
  // apply left to right logic, merging all current results with ALL previous
  while (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    TRI_ASSERT(_objectId == RocksDBKey::objectId(iter->key()));

    rocksdb::Status s = iter->status();
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    TRI_voc_rid_t revId = RocksDBKey::revisionId(
        RocksDBEntryType::FulltextIndexValue, iter->key());
    if (token.operation == FulltextQueryToken::AND) {
      intersect.insert(revId);
    } else if (token.operation == FulltextQueryToken::OR) {
      resultSet.insert(revId);
    } else if (token.operation == FulltextQueryToken::EXCLUDE) {
      resultSet.erase(revId);
    }
    iter->Next();
  }
  if (token.operation == FulltextQueryToken::AND) {
    if (resultSet.empty() || intersect.empty()) {
      resultSet.clear();
    } else {
      std::set<TRI_voc_rid_t> output;
      std::set_intersection(resultSet.begin(), resultSet.end(),
                            intersect.begin(), intersect.end(),
                            std::inserter(output, output.begin()));
      resultSet = std::move(output);
    }
  }
  return Result();
}
