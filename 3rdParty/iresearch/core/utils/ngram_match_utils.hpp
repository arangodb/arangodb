////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_NGRAM_MATCH_UTILS_H
#define IRESEARCH_NGRAM_MATCH_UTILS_H

#include "shared.hpp"
#include "utf8_utils.hpp"
#include <vector>

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// Evaluates ngram similarity between the specified strings based on
/// "N-gram similarity and distance" by Grzegorz Kondrak
/// http://www.cs.ualberta.ca/~kondrak/papers/spire05.pdf
/// Could operate in two forms depending on search_semantics.
/// If search_semantics is false then positional ngram similarity is used,
/// binary ngram similarity is used otherwise. Also setting search_semantics to
/// true disables normalizing resulting similarity by length of longest string and
/// result is evaluated based strictly on target length (search-like semantics).
/// If search_semantics is false there is no difference which string pass
/// as target. First characters affixing currently is not implemented
/// @param target string to seek similarity for
/// @param target_size length of target string
/// @param src string to seek similarity in
/// @param src_size length of source string
/// @param ngram_size ngram size
/// @returns similarity value
////////////////////////////////////////////////////////////////////////////////
template<typename T, bool search_semantics>
float_t ngram_similarity(const T* target, size_t target_size,
                         const T* src, size_t src_size,
                         size_t ngram_size) {
  if (ngram_size == 0) {
    return 0.f;
  }

  if /*consexpr*/ (!search_semantics) {
    if (target_size > src_size) {
      std::swap(target_size, src_size);
      std::swap(target, src);
    }
  }

  if (target_size < ngram_size || src_size < ngram_size) {
    if constexpr (!search_semantics) {
      if (target_size == 0 && src_size == 0) {
        return 1; // consider two empty strings as matched
      }
      const T* r = src;
      size_t matched = 0;
      for (const T* it = target; it != target + target_size; ) {
        matched += size_t(*it == *r);
        ++r;
        ++it;
      }
      return float_t(matched) / float_t(src_size);
    } else {
      if (target_size == src_size) {
        return memcmp(target, src, target_size * sizeof(T)) == 0 ? 1 : 0;
      }
      return 0;
    }
  }

  const size_t t_ngram_count = target_size - ngram_size + 1;
  const size_t s_ngram_count = src_size - ngram_size + 1;
  const T* t_ngram_start = target;
  const T* t_ngram_start_end  = target + target_size - ngram_size + 1; // end() analog for target ngram start

  float_t d = 0; // will store upper-left cell value for current cache row
  std::vector<float_t> cache(s_ngram_count + 1, 0);

  // here could be constructed source string with start characters affixing

  for (; t_ngram_start != t_ngram_start_end; ++t_ngram_start) {
    const T* t_ngram_end = t_ngram_start + ngram_size;
    const T* s_ngram_start = src;
    size_t s_ngram_idx = 1;
    const T* s_ngram_start_end  = src + src_size - ngram_size + 1; // end() analog for src ngram start

    for (; s_ngram_start != s_ngram_start_end; ++s_ngram_start, ++s_ngram_idx) {
      const T* rhs_ngram_end = s_ngram_start + ngram_size;
      float_t similarity = !search_semantics ? 0 : 1;
      for (const T* l = t_ngram_start, *r = s_ngram_start; l != t_ngram_end && r != rhs_ngram_end; ++l, ++r) {
        if constexpr (search_semantics) {
          if (*l != *r) {
            similarity = 0;
            break;
          }
        } else {
          if (*l == *r) {
            ++similarity;
          }
        }
      }
      if constexpr (!search_semantics) {
        similarity = similarity / float_t(ngram_size);
      }

      auto tmp = cache[s_ngram_idx];
      cache[s_ngram_idx] =
          std::max(
            std::max(cache[s_ngram_idx - 1],
                     cache[s_ngram_idx]),
            d + similarity);
      d = tmp;
    }
  }
  return cache[s_ngram_count] /
         float_t((!search_semantics) ? s_ngram_count : t_ngram_count);
}

}


#endif // IRESEARCH_NGRAM_MATCH_UTILS_H
