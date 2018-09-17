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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_REGEX_CACHE_H
#define ARANGOD_AQL_REGEX_CACHE_H 1

#include "Basics/Common.h"
#include "Aql/AqlValue.h"

#include <unicode/regex.h>

namespace arangodb {

namespace transaction {
class Methods;
}

namespace aql {

class RegexCache {
 public:
  RegexCache(RegexCache const&) = delete;
  RegexCache& operator=(RegexCache const&) = delete;

  RegexCache() = default;
  ~RegexCache();
  
  void clear() noexcept;

  icu::RegexMatcher* buildRegexMatcher(char const* ptr, size_t length, bool caseInsensitive);
  icu::RegexMatcher* buildLikeMatcher(char const* ptr, size_t length, bool caseInsensitive);
  icu::RegexMatcher* buildSplitMatcher(AqlValue const& splitExpression, arangodb::transaction::Methods* trx, bool& isEmptyExpression);
  
  /// @brief inspect a LIKE pattern from a string, and remove all
  /// of its escape characters. will stop at the first wildcards found.
  /// returns a pair with the following meaning:
  /// - first: true if the inspection aborted prematurely because a
  ///   wildcard was found, and false if the inspection analyzed at the
  ///   complete string
  /// - second: true if the found wildcard is the last byte in the pattern,
  ///   false otherwise. can only be true if first is also true
  static std::pair<bool, bool> inspectLikePattern(std::string& out, char const* ptr, size_t length);
 
 private: 
  /// @brief get matcher from cache, or insert a new matcher for the specified pattern
  icu::RegexMatcher* fromCache(std::string const& pattern, 
                               std::unordered_map<std::string, std::unique_ptr<icu::RegexMatcher>>& cache);

  static void buildRegexPattern(std::string& out, char const* ptr, size_t length, bool caseInsensitive);
  static void buildLikePattern(std::string& out, char const* ptr, size_t length, bool caseInsensitive);

 private:
  /// @brief cache for compiled regexes (REGEX function)
  std::unordered_map<std::string, std::unique_ptr<icu::RegexMatcher>> _regexCache;
  /// @brief cache for compiled regexes (LIKE function)
  std::unordered_map<std::string, std::unique_ptr<icu::RegexMatcher>> _likeCache;
  /// @brief a reusable string object for pattern generation
  std::string _temp;
};

}
}

#endif
