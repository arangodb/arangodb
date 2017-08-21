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

#include "RegexCache.h"
#include "Basics/Utf8Helper.h"

using namespace arangodb::aql;

RegexCache::~RegexCache() {
  clear();
}

void RegexCache::clear() noexcept {
  clear(_regexCache);
  clear(_likeCache);
}
 
icu::RegexMatcher* RegexCache::buildRegexMatcher(char const* ptr, size_t length, bool caseInsensitive) {
  buildRegexPattern(_temp, ptr, length, caseInsensitive);

  return fromCache(_temp, _regexCache);
}

icu::RegexMatcher* RegexCache::buildLikeMatcher(char const* ptr, size_t length, bool caseInsensitive) {
  buildLikePattern(_temp, ptr, length, caseInsensitive);

  return fromCache(_temp, _likeCache);
}
                                         
void RegexCache::clear(std::unordered_map<std::string, icu::RegexMatcher*>& cache) noexcept {
  try {
    for (auto& it : cache) {
      delete it.second;
    }
    cache.clear();
  } catch (...) {
  }
}

/// @brief get matcher from cache, or insert a new matcher for the specified pattern
icu::RegexMatcher* RegexCache::fromCache(std::string const& pattern, 
                                         std::unordered_map<std::string, icu::RegexMatcher*>& cache) {
  auto it = cache.find(pattern);

  if (it != cache.end()) {
    return (*it).second;
  }
    
  icu::RegexMatcher* matcher = arangodb::basics::Utf8Helper::DefaultUtf8Helper.buildMatcher(pattern);

  try {
    // insert into cache, no matter if pattern is valid or not
    cache.emplace(_temp, matcher);
    return matcher;
  } catch (...) {
    delete matcher;
    throw;
  }
}

/// @brief compile a REGEX pattern from a string
void RegexCache::buildRegexPattern(std::string& out,
                                   char const* ptr, size_t length,
                                   bool caseInsensitive) {
  out.clear();
  if (caseInsensitive) {
    out.reserve(length + 4);
    out.append("(?i)");
  }

  out.append(ptr, length);
}

/// @brief compile a LIKE pattern from a string
void RegexCache::buildLikePattern(std::string& out,
                                  char const* ptr, size_t length,
                                  bool caseInsensitive) {
  out.clear();
  out.reserve(length + 8); // reserve some room
  
  // pattern is always anchored
  out.push_back('^');
  if (caseInsensitive) {
    out.append("(?i)");
  }

  bool escaped = false;

  for (size_t i = 0; i < length; ++i) {
    char const c = ptr[i];

    if (c == '\\') {
      if (escaped) {
        // literal backslash
        out.append("\\\\");
      }
      escaped = !escaped;
    } else {
      if (c == '%') {
        if (escaped) {
          // literal %
          out.push_back('%');
        } else {
          // wildcard
          out.append("(.|[\r\n])*");
        }
      } else if (c == '_') {
        if (escaped) {
          // literal underscore
          out.push_back('_');
        } else {
          // wildcard character
          out.append("(.|[\r\n])");
        }
      } else if (c == '?' || c == '+' || c == '[' || c == '(' || c == ')' ||
                 c == '{' || c == '}' || c == '^' || c == '$' || c == '|' ||
                 c == '\\' || c == '.') {
        // character with special meaning in a regex
        out.push_back('\\');
        out.push_back(c);
      } else {
        if (escaped) {
          // found a backslash followed by no special character
          out.append("\\\\");
        }

        // literal character
        out.push_back(c);
      }

      escaped = false;
    }
  }

  // always anchor the pattern
  out.push_back('$');
}
