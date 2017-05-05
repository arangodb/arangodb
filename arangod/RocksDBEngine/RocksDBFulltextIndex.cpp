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

#include "RocksDBFulltextIndex.h"

#include "Basics/StringRef.h"
#include "Basics/StaticStrings.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/DocumentIdentifierToken.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief maximum length of an indexed word in characters
/// a character may consist of up to 4 bytes
#define TRI_FULLTEXT_MAX_WORD_LENGTH 40

/// @brief default minimum word length for a fulltext index
#define TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT 2

TRI_voc_rid_t RocksDBFulltextIndex::fromDocumentIdentifierToken(
    DocumentIdentifierToken const& token) {
  auto tkn = static_cast<RocksDBToken const*>(&token);
  return tkn->revisionId();
}

DocumentIdentifierToken RocksDBFulltextIndex::toDocumentIdentifierToken(
    TRI_voc_rid_t revisionId) {
  return RocksDBToken{revisionId};
}

RocksDBFulltextIndex::RocksDBFulltextIndex(TRI_idx_iid_t iid,
                             arangodb::LogicalCollection* collection,
                             VPackSlice const& info)
    : RocksDBIndex(iid, collection, info),
      _minWordLength(TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT) {
  TRI_ASSERT(iid != 0);

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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "fulltext index definition should have exactly one attribute");
  }
  auto& attribute = _fields[0];
  _attr.reserve(attribute.size());
  for (auto& a : attribute) {
    _attr.emplace_back(a.name);
  }
}



RocksDBFulltextIndex::~RocksDBFulltextIndex() {
}

size_t RocksDBFulltextIndex::memory() const {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::FulltextIndexEntries(_objectId, StringRef());
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out;
  db->GetApproximateSizes(&r, 1, &out, true);
  return (size_t)out;
}

/// @brief return a VelocyPack representation of the index
void RocksDBFulltextIndex::toVelocyPack(VPackBuilder& builder,
                                 bool withFigures, bool forPersistence) const {
  builder.openObject();
  Index::toVelocyPack(builder, withFigures, forPersistence);
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
    if(!value.isString()) {
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

int RocksDBFulltextIndex::insert(transaction::Methods* trx, TRI_voc_rid_t revisionId,
                          VPackSlice const& doc, bool isRollback) {

  std::vector<std::string> words = wordlist(doc);
  if (words.empty()) {
    // TODO: distinguish the cases "empty wordlist" and "out of memory"
    // LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not build wordlist";
    return TRI_ERROR_INTERNAL;
  }
  
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();
  
  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  StringRef docKey(doc.get(StaticStrings::KeyString));
  RocksDBValue value = RocksDBValue::IndexValue();
  
  int res = TRI_ERROR_NO_ERROR;
  size_t const count = words.size();
  size_t i = 0;
  for (; i < count; ++i) {
    std::string const& word = words[i];
    RocksDBKey key = RocksDBKey::FulltextIndexValue(_objectId,
                                                    StringRef(word), docKey);

    rocksdb::Status s = rtrx->Put(key.string(), value.string());
    if (!s.ok()) {
      auto status =
      rocksutils::convertStatus(s, rocksutils::StatusHint::index);
      res = status.errorNumber();
      break;
    }
  }
  if (res != TRI_ERROR_NO_ERROR) {
    for (size_t j = 0; j < i; ++j) {
      std::string const& word = words[j];
      RocksDBKey key = RocksDBKey::FulltextIndexValue(_objectId,
                                                      StringRef(word), docKey);
      rtrx->Delete(key.string());
    }
  }
  return res;
}

int RocksDBFulltextIndex::insertRaw(rocksdb::WriteBatchWithIndex* batch, TRI_voc_rid_t,
                                    arangodb::velocypack::Slice const& doc) {
  std::vector<std::string> words = wordlist(doc);
  if (words.empty()) {
    // TODO: distinguish the cases "empty wordlist" and "out of memory"
    // LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not build wordlist";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  StringRef docKey(doc.get(StaticStrings::KeyString));
  RocksDBValue value = RocksDBValue::IndexValue();
  
  size_t const count = words.size();
  for (size_t i = 0; i < count; ++i) {
    std::string const& word = words[i];
    RocksDBKey key = RocksDBKey::FulltextIndexValue(_objectId,
                                                    StringRef(word), docKey);
    batch->Put(key.string(), value.string());
  }

  return TRI_ERROR_NO_ERROR;
}

int RocksDBFulltextIndex::remove(transaction::Methods* trx, TRI_voc_rid_t revisionId,
                          VPackSlice const& doc, bool isRollback) {
  std::vector<std::string> words = wordlist(doc);
  if (words.empty()) {
    // TODO: distinguish the cases "empty wordlist" and "out of memory"
    // LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not build wordlist";
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  RocksDBTransactionState* state = rocksutils::toRocksTransactionState(trx);
  rocksdb::Transaction* rtrx = state->rocksTransaction();
  
  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  StringRef docKey(doc.get(StaticStrings::KeyString));
  int res = TRI_ERROR_NO_ERROR;
  size_t const count = words.size();
  for (size_t i = 0; i < count; ++i) {
    std::string const& word = words[i];
    RocksDBKey key = RocksDBKey::FulltextIndexValue(_objectId,
                                                    StringRef(word), docKey);
    
    rocksdb::Status s = rtrx->Delete(key.string());
    if (!s.ok()) {
      auto status =
      rocksutils::convertStatus(s, rocksutils::StatusHint::index);
      res = status.errorNumber();
    }
  }
  return res;
}

int RocksDBFulltextIndex::removeRaw(rocksdb::WriteBatch* batch, TRI_voc_rid_t,
                                    arangodb::velocypack::Slice const& doc) {
  std::vector<std::string> words = wordlist(doc);
  // now we are going to construct the value to insert into rocksdb
  // unique indexes have a different key structure
  StringRef docKey(doc.get(StaticStrings::KeyString));
  size_t const count = words.size();
  for (size_t i = 0; i < count; ++i) {
    std::string const& word = words[i];
    RocksDBKey key = RocksDBKey::FulltextIndexValue(_objectId,
                                                    StringRef(word), docKey);
    batch->Delete(key.string());
  }
  return TRI_ERROR_NO_ERROR;
}

int RocksDBFulltextIndex::cleanup() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = RocksDBKeyBounds::FulltextIndexEntries(_objectId,
                                                                   StringRef());
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, &b, &e);
  return TRI_ERROR_NO_ERROR;
}

/// @brief walk over the attribute. Also Extract sub-attributes and elements in
///        list.
static void ExtractWords(std::vector<std::string>& words,
                         VPackSlice const value,
                         size_t minWordLength,
                         int level) {
  if (value.isString()) {
    // extract the string value for the indexed attribute
    std::string text = value.copyString();
    
    // parse the document text
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.getWords(
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
std::vector<std::string> RocksDBFulltextIndex::wordlist(VPackSlice const& doc) {
  std::vector<std::string> words;
  try {
    VPackSlice const value = doc.get(_attr);

    if (!value.isString() && !value.isArray() && !value.isObject()) {
      // Invalid Input
      return words;
    }

    ExtractWords(words, value, _minWordLength, 0);
  } catch (...) {
    // Backwards compatibility
    // The pre-vpack impl. did just ignore all errors and returned nulltpr
    return words;
  }
  return words;
}
