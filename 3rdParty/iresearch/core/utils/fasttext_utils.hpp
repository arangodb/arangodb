////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FASTTEXT_UTILS_H
#define IRESEARCH_FASTTEXT_UTILS_H

#include "fasttext.h"

namespace fasttext {

class ImmutableFastText : public FastText {
 public:
  void loadModel(const std::string& filename) {
    FastText::loadModel(filename);
    FastText::lazyComputeWordVectors(); // ensure word vectors are computed
  }

  std::vector<std::pair<real, std::string>> getNN(
      const std::string& word,
      int32_t k) const {
    Vector query(args_->dim);

    getWordVector(query, word);

    assert(wordVectors_);
    return getNN(*wordVectors_, query, k, {word});
  }

  std::vector<std::pair<real, std::string>> getNN(
      const DenseMatrix& wordVectors,
      const Vector& queryVec,
      int32_t k,
      const std::set<std::string>& banSet) const {
    return FastText::getNN(wordVectors, queryVec, k, banSet);
  }
}; // ImmutableFastText

}

#endif // IRESEARCH_FASTTEXT_UTILS_H

