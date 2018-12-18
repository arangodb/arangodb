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
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "MMFiles/mmfiles-fulltext-index.h"
#include "MMFiles/mmfiles-fulltext-query.h"
#include "StorageEngine/TransactionState.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief walk over the attribute. Also Extract sub-attributes and elements in
///        list.
void MMFilesFulltextIndex::extractWords(std::set<std::string>& words,
                                        VPackSlice value, int level) const {
  if (value.isString()) {
    // extract the string value for the indexed attribute
    // parse the document text
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.tokenize(
        words, value.stringRef(), _minWordLength, TRI_FULLTEXT_MAX_WORD_LENGTH, true);
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

MMFilesFulltextIndex::MMFilesFulltextIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
)
    : MMFilesIndex(iid, collection, info),
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
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "fulltext index definition should have exactly one attribute");
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
    LOG_TOPIC(TRACE, arangodb::Logger::ENGINES)
        << "destroying fulltext index";
    TRI_FreeFtsIndex(_fulltextIndex);
  }
}

size_t MMFilesFulltextIndex::memory() const {
  return TRI_MemoryMMFilesFulltextIndex(_fulltextIndex);
}

/// @brief return a VelocyPack representation of the index
void MMFilesFulltextIndex::toVelocyPack(VPackBuilder& builder,
       std::underlying_type<Index::Serialize>::type flags) const {
  builder.openObject();
  Index::toVelocyPack(builder, flags);
  builder.add(
    arangodb::StaticStrings::IndexUnique,
    arangodb::velocypack::Value(false)
  );
  builder.add(
    arangodb::StaticStrings::IndexSparse,
    arangodb::velocypack::Value(true)
  );
  builder.add("minLength", VPackValue(_minWordLength));
  builder.close();
}

/// @brief Test if this index matches the definition
bool MMFilesFulltextIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = info.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
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

  value = info.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                   info, arangodb::StaticStrings::IndexUnique, false
                 )
     ) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                   info, arangodb::StaticStrings::IndexSparse, true
                 )
     ) {
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

Result MMFilesFulltextIndex::insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  Result res;
  int r = TRI_ERROR_NO_ERROR;
  std::set<std::string> words = wordlist(doc);
  if (!words.empty()) {
    r = TRI_InsertWordsMMFilesFulltextIndex(_fulltextIndex, documentId, words);
  }
  if (r != TRI_ERROR_NO_ERROR) {
    addErrorMsg(res, r);
  }
  return res;
}

Result MMFilesFulltextIndex::remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
) {
  Result res;
  int r = TRI_ERROR_NO_ERROR;
  std::set<std::string> words = wordlist(doc);

  if (!words.empty()) {
    r = TRI_RemoveWordsMMFilesFulltextIndex(_fulltextIndex, documentId, words);
  }
  if (r != TRI_ERROR_NO_ERROR) {
    addErrorMsg(res, r);
  }
  return res;
}

void MMFilesFulltextIndex::unload() {
  TRI_TruncateMMFilesFulltextIndex(_fulltextIndex);
}

IndexIterator* MMFilesFulltextIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult*,
    aql::AstNode const* condNode, aql::Variable const* var,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  TRI_ASSERT(condNode != nullptr);
  TRI_ASSERT(condNode->numMembers() == 1);  // should only be an FCALL
  aql::AstNode const* fcall = condNode->getMember(0);
  TRI_ASSERT(fcall->type == arangodb::aql::NODE_TYPE_FCALL);
  TRI_ASSERT(fcall->numMembers() == 1);
  aql::AstNode const* args = fcall->getMember(0);

  size_t numMembers = args->numMembers();
  TRI_ASSERT(numMembers == 3 || numMembers == 4);

  std::string query = args->getMember(2)->getString();

  size_t limit = 0;
  if (numMembers == 4) {
    limit = args->getMember(3)->getIntValue();
  }
  TRI_fulltext_query_t* ft =
      TRI_CreateQueryMMFilesFulltextIndex(TRI_FULLTEXT_SEARCH_MAX_WORDS, limit);
  if (ft == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  bool isSubQuery = false;
  int res = TRI_ParseQueryMMFilesFulltextIndex(ft, query.c_str(), &isSubQuery);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeQueryMMFilesFulltextIndex(ft);
    THROW_ARANGO_EXCEPTION(res);
  }

  // note: the following call will free "ft"!
  std::set<TRI_voc_rid_t> results =
      TRI_QueryMMFilesFulltextIndex(_fulltextIndex, ft);

  return new MMFilesFulltextIndexIterator(
    &_collection, trx, std::move(results)
  );
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
