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

#include "FulltextIndex.h"
#include "Basics/StringRef.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "FulltextIndex/fulltext-index.h"
#include "Logger/Logger.h"
#include "VocBase/transaction.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief walk over the attribute. Also Extract sub-attributes and elements in
///        list.
////////////////////////////////////////////////////////////////////////////////

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

FulltextIndex::FulltextIndex(TRI_idx_iid_t iid,
                             arangodb::LogicalCollection* collection,
                             std::string const& attribute, int minWordLength)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>{
                {arangodb::basics::AttributeName(attribute, false)}},
            false, true),
      _fulltextIndex(nullptr),
      _minWordLength(minWordLength > 0 ? minWordLength : 1) {
  TRI_ASSERT(iid != 0);

  _attr = arangodb::basics::StringUtils::split(attribute, ".");

  _fulltextIndex = TRI_CreateFtsIndex(2048, 1, 1);

  if (_fulltextIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

FulltextIndex::FulltextIndex(TRI_idx_iid_t iid,
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
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
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



FulltextIndex::~FulltextIndex() {
  if (_fulltextIndex != nullptr) {
    LOG(TRACE) << "destroying fulltext index";
    TRI_FreeFtsIndex(_fulltextIndex);
  }
}

size_t FulltextIndex::memory() const {
  return TRI_MemoryFulltextIndex(_fulltextIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void FulltextIndex::toVelocyPack(VPackBuilder& builder,
                                 bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(true));
  builder.add("minLength", VPackValue(_minWordLength));
}

/// @brief Test if this index matches the definition
bool FulltextIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == typeName());
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


int FulltextIndex::insert(arangodb::Transaction*, TRI_doc_mptr_t const* doc,
                          bool isRollback) {
  int res = TRI_ERROR_NO_ERROR;

  std::vector<std::string> words = wordlist(doc);
   
  if (words.empty()) {
    // TODO: distinguish the cases "empty wordlist" and "out of memory"
    // LOG(WARN) << "could not build wordlist";
    return res;
  }

  // TODO: use status codes
  if (!TRI_InsertWordsFulltextIndex(
          _fulltextIndex, (TRI_fulltext_doc_t)((uintptr_t)doc), words)) {
    LOG(ERR) << "adding document to fulltext index failed";
    res = TRI_ERROR_INTERNAL;
  }
  return res;
}

int FulltextIndex::remove(arangodb::Transaction*, TRI_doc_mptr_t const* doc,
                          bool) {
  TRI_DeleteDocumentFulltextIndex(_fulltextIndex,
                                  (TRI_fulltext_doc_t)((uintptr_t)doc));

  return TRI_ERROR_NO_ERROR;
}

int FulltextIndex::unload() {
  TRI_TruncateFulltextIndex(_fulltextIndex);
  return TRI_ERROR_NO_ERROR;
}

int FulltextIndex::cleanup() {
  LOG(TRACE) << "fulltext cleanup called";

  int res = TRI_ERROR_NO_ERROR;

  // check whether we should do a cleanup at all
  if (!TRI_CompactFulltextIndex(_fulltextIndex)) {
    res = TRI_ERROR_INTERNAL;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function called by the fulltext index to determine the
/// words to index for a specific document
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> FulltextIndex::wordlist(
    TRI_doc_mptr_t const* document) {
  std::vector<std::string> words;
  try {
    VPackSlice const slice(document->vpack());
    VPackSlice const value = slice.get(_attr);

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
