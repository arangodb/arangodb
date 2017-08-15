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

#include "MMFilesFulltextIndex.h"
#include "Basics/StringRef.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesToken.h"
#include "MMFiles/mmfiles-fulltext-index.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "StorageEngine/TransactionState.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief walk over the attribute. Also Extract sub-attributes and elements in
///        list.
void MMFilesFulltextIndex::extractWords(std::set<std::string>& words,
                                        VPackSlice value,
                                        int level) const {
  if (value.isString()) {
    // extract the string value for the indexed attribute
    std::string text = value.copyString();

    // parse the document text
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(
        words, text, _minWordLength, TRI_FULLTEXT_MAX_WORD_LENGTH, true);
    // We don't care for the result. If the result is false, words stays
    // unchanged and is not indexed
  } else if (value.isArray() && level == 0) {
    for (auto const& v : VPackArrayIterator(value)) {
      extractWords(words, v, level + 1);
    }
  } else if (value.isObject() && level == 0) {
    for (auto const& v : VPackObjectIterator(value)) {
      extractWords(words, v.value, level + 1);
    }
  }
}

TRI_voc_rid_t MMFilesFulltextIndex::fromDocumentIdentifierToken(
    DocumentIdentifierToken const& token) {
  auto tkn = static_cast<MMFilesToken const*>(&token);
  return tkn->revisionId();
}

DocumentIdentifierToken MMFilesFulltextIndex::toDocumentIdentifierToken(
    TRI_voc_rid_t revisionId) {
  return MMFilesToken{revisionId};
}

MMFilesFulltextIndex::MMFilesFulltextIndex(TRI_idx_iid_t iid,
                             arangodb::LogicalCollection* collection,
                             VPackSlice const& info)
    : Index(iid, collection, info),
      _fulltextIndex(nullptr),
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

  _fulltextIndex = TRI_CreateFtsIndex(2048, 1, 1);

  if (_fulltextIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

MMFilesFulltextIndex::~MMFilesFulltextIndex() {
  if (_fulltextIndex != nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "destroying fulltext index";
    TRI_FreeFtsIndex(_fulltextIndex);
  }
}

size_t MMFilesFulltextIndex::memory() const {
  return TRI_MemoryMMFilesFulltextIndex(_fulltextIndex);
}

/// @brief return a VelocyPack representation of the index
void MMFilesFulltextIndex::toVelocyPack(VPackBuilder& builder,
                                 bool withFigures, bool forPersistence) const {
  builder.openObject();
  Index::toVelocyPack(builder, withFigures, forPersistence);
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(true));
  builder.add("minLength", VPackValue(_minWordLength));
  builder.close();
}

/// @brief Test if this index matches the definition
bool MMFilesFulltextIndex::matchesDefinition(VPackSlice const& info) const {
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

Result MMFilesFulltextIndex::insert(transaction::Methods*,
                                    TRI_voc_rid_t revisionId,
                                    VPackSlice const& doc, bool isRollback) {
  int res = TRI_ERROR_NO_ERROR;
  std::set<std::string> words = wordlist(doc);

  if (!words.empty()) {
    res = TRI_InsertWordsMMFilesFulltextIndex(_fulltextIndex, revisionId, words);
  }
  return IndexResult(res, this);
}

Result MMFilesFulltextIndex::remove(transaction::Methods*,
                                    TRI_voc_rid_t revisionId,
                                    VPackSlice const& doc, bool isRollback) {
  int res = TRI_ERROR_NO_ERROR;
  std::set<std::string> words = wordlist(doc);

  if (!words.empty()) {
    res = TRI_RemoveWordsMMFilesFulltextIndex(_fulltextIndex, revisionId, words);
  }
  return IndexResult(res, this);
}

void MMFilesFulltextIndex::unload() {
  TRI_TruncateMMFilesFulltextIndex(_fulltextIndex);
}

/// @brief callback function called by the fulltext index to determine the
/// words to index for a specific document
std::set<std::string> MMFilesFulltextIndex::wordlist(VPackSlice const& doc) {
  std::set<std::string> words;
  VPackSlice const value = doc.get(_attr);

  if (!value.isString() && !value.isArray() && !value.isObject()) {
    // Invalid Input
    return words;
  }

  extractWords(words, value, 0);
  return words;
}
