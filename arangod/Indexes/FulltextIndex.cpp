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
#include "Basics/Logger.h"
#include "Basics/Utf8Helper.h"
#include "FulltextIndex/fulltext-index.h"
#include "FulltextIndex/fulltext-wordlist.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/VocShaper.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief extraction context
////////////////////////////////////////////////////////////////////////////////

struct TextExtractorContext {
  std::vector<std::pair<char const*, size_t>>* _positions;
  VocShaper* _shaper;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief walk over an array shape and extract the string values
////////////////////////////////////////////////////////////////////////////////

static bool ArrayTextExtractor(VocShaper* shaper, TRI_shape_t const* shape,
                               char const*, char const* shapedJson,
                               uint64_t length, void* data) {
  char* text;
  size_t textLength;
  bool ok = TRI_StringValueShapedJson(shape, shapedJson, &text, &textLength);

  if (ok) {
    // add string value found
    try {
      static_cast<TextExtractorContext*>(data)
          ->_positions->emplace_back(text, textLength);
    } catch (...) {
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief walk over a list shape and extract the string values
////////////////////////////////////////////////////////////////////////////////

static bool ListTextExtractor(VocShaper* shaper, TRI_shape_t const* shape,
                              char const* shapedJson, uint64_t length,
                              void* data) {
  if (shape->_type == TRI_SHAPE_ARRAY) {
    // a sub-object
    TRI_IterateShapeDataArray(static_cast<TextExtractorContext*>(data)->_shaper,
                              shape, shapedJson, ArrayTextExtractor, data);
  } else if (shape->_type == TRI_SHAPE_SHORT_STRING ||
             shape->_type == TRI_SHAPE_LONG_STRING) {
    char* text;
    size_t textLength;
    bool ok = TRI_StringValueShapedJson(shape, shapedJson, &text, &textLength);

    if (ok) {
      // add string value found
      try {
        static_cast<TextExtractorContext*>(data)
            ->_positions->emplace_back(text, textLength);
      } catch (...) {
      }
    }
  }

  return true;
}

FulltextIndex::FulltextIndex(TRI_idx_iid_t iid,
                             TRI_document_collection_t* collection,
                             std::string const& attribute, int minWordLength)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>{
                {{attribute, false}}},
            false, true),
      _pid(0),
      _fulltextIndex(nullptr),
      _minWordLength(minWordLength > 0 ? minWordLength : 1) {
  TRI_ASSERT(iid != 0);

  // look up the attribute
  auto shaper =
      _collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
  _pid = shaper->findOrCreateAttributePathByName(attribute.c_str());

  if (_pid == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
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

  // hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(true));
  builder.add("minLength", VPackValue(_minWordLength));
}

int FulltextIndex::insert(arangodb::Transaction*, TRI_doc_mptr_t const* doc,
                          bool isRollback) {
  int res = TRI_ERROR_NO_ERROR;

  TRI_fulltext_wordlist_t* words = wordlist(doc);

  if (words == nullptr) {
    // TODO: distinguish the cases "empty wordlist" and "out of memory"
    // LOG(WARN) << "could not build wordlist";
    return res;
  }

  if (words->_numWords > 0) {
    // TODO: use status codes
    if (!TRI_InsertWordsFulltextIndex(
            _fulltextIndex, (TRI_fulltext_doc_t)((uintptr_t)doc), words)) {
      LOG(ERR) << "adding document to fulltext index failed";
      res = TRI_ERROR_INTERNAL;
    }
  }

  TRI_FreeWordlistFulltextIndex(words);

  return res;
}

int FulltextIndex::remove(arangodb::Transaction*, TRI_doc_mptr_t const* doc,
                          bool) {
  TRI_DeleteDocumentFulltextIndex(_fulltextIndex,
                                  (TRI_fulltext_doc_t)((uintptr_t)doc));

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

TRI_fulltext_wordlist_t* FulltextIndex::wordlist(
    TRI_doc_mptr_t const* document) {
  TRI_shaped_json_t shaped;
  TRI_shaped_json_t shapedJson;
  TRI_shape_t const* shape;

  // extract the shape
  auto shaper = _collection->getShaper();

  TRI_EXTRACT_SHAPED_JSON_MARKER(
      shaped, document->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME
  bool ok =
      shaper->extractShapedJson(&shaped, 0, _pid, &shapedJson,
                                &shape);  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (!ok || shape == nullptr) {
    return nullptr;
  }

  TRI_vector_string_t* words;

  // extract the string value for the indexed attribute
  if (shape->_type == TRI_SHAPE_SHORT_STRING ||
      shape->_type == TRI_SHAPE_LONG_STRING) {
    char* text;
    size_t textLength;
    ok = TRI_StringValueShapedJson(shape, shapedJson._data.data, &text,
                                   &textLength);

    if (!ok) {
      return nullptr;
    }

    // parse the document text
    words = TRI_get_words(text, textLength, (size_t)_minWordLength,
                          (size_t)TRI_FULLTEXT_MAX_WORD_LENGTH, true);
  } else if (shape->_type == TRI_SHAPE_ARRAY) {
    std::vector<std::pair<char const*, size_t>> values;
    TextExtractorContext context{&values, shaper};
    TRI_IterateShapeDataArray(shaper, shape, shapedJson._data.data,
                              ArrayTextExtractor, &context);

    words = nullptr;
    for (auto const& it : values) {
      if (!TRI_get_words(words, it.first, it.second, (size_t)_minWordLength,
                         (size_t)TRI_FULLTEXT_MAX_WORD_LENGTH, true)) {
        if (words != nullptr) {
          TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);
        }
        return nullptr;
      }
    }
  } else if (shape->_type == TRI_SHAPE_LIST ||
             shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST ||
             shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    std::vector<std::pair<char const*, size_t>> values;
    TextExtractorContext context{&values, shaper};
    TRI_IterateShapeDataList(shaper, shape, shapedJson._data.data,
                             ListTextExtractor, &context);

    words = nullptr;
    for (auto const& it : values) {
      if (!TRI_get_words(words, it.first, it.second, (size_t)_minWordLength,
                         (size_t)TRI_FULLTEXT_MAX_WORD_LENGTH, true)) {
        if (words != nullptr) {
          TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);
        }
        return nullptr;
      }
    }
  } else {
    words = nullptr;
  }

  if (words == nullptr) {
    return nullptr;
  }

  TRI_fulltext_wordlist_t* wordlist =
      TRI_CreateWordlistFulltextIndex(words->_buffer, words->_length);

  if (wordlist == nullptr) {
    TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);
    return nullptr;
  }

  // this really is a hack, but it works well:
  // make the word list vector think it's empty and free it
  // this does not free the word list, that we have already over the result
  words->_length = 0;
  words->_buffer = nullptr;
  TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);

  return wordlist;
}
