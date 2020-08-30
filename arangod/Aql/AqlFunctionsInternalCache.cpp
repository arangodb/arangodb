////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "AqlFunctionsInternalCache.h"

#include "Aql/AqlValueMaterializer.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/tryEmplaceHelper.h"

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

AqlFunctionsInternalCache::~AqlFunctionsInternalCache() { clear(); }

void AqlFunctionsInternalCache::clear() noexcept {
  _regexCache.clear();
  _likeCache.clear();
  _validatorCache.clear();
}

icu::RegexMatcher* AqlFunctionsInternalCache::buildRegexMatcher(char const* ptr, size_t length,
                                                 bool caseInsensitive) {
  buildRegexPattern(_temp, ptr, length, caseInsensitive);

  return fromCache(_temp, _regexCache);
}

icu::RegexMatcher* AqlFunctionsInternalCache::buildLikeMatcher(char const* ptr, size_t length,
                                                bool caseInsensitive) {
  buildLikePattern(_temp, ptr, length, caseInsensitive);

  return fromCache(_temp, _likeCache);
}

icu::RegexMatcher* AqlFunctionsInternalCache::buildSplitMatcher(AqlValue const& splitExpression,
                                                 arangodb::velocypack::Options const* opts,
                                                 bool& isEmptyExpression) {
  std::string rx;

  AqlValueMaterializer materializer(opts);
  VPackSlice slice = materializer.slice(splitExpression, false);
  if (splitExpression.isArray()) {
    for (VPackSlice it : VPackArrayIterator(slice)) {
      if (!it.isString() || it.getStringLength() == 0) {
        // one empty string rules them all
        isEmptyExpression = true;
        rx = "";
        break;
      }
      if (rx.size() != 0) {
        rx += '|';
      }

      arangodb::velocypack::ValueLength length;
      char const* str = it.getString(length);
      basics::StringUtils::escapeRegexParams(rx, str, length);
    }
  } else if (splitExpression.isString()) {
    arangodb::velocypack::ValueLength length;
    char const* str = slice.getString(length);
    basics::StringUtils::escapeRegexParams(rx, str, length);
    if (rx.empty()) {
      isEmptyExpression = true;
    }
  } else {
    rx.clear();
  }
  return fromCache(rx, _likeCache);
}

arangodb::ValidatorBase* AqlFunctionsInternalCache::buildValidator(VPackSlice const& validatorParams) {
  auto matcherIter = _validatorCache.try_emplace(
        validatorParams.hash(),
        arangodb::lazyConstruct([&]{
          return std::unique_ptr<arangodb::ValidatorBase>(new arangodb::ValidatorJsonSchema(validatorParams));
        })
      ).first;

  return matcherIter->second.get();
};

/// @brief get matcher from cache, or insert a new matcher for the specified
/// pattern
icu::RegexMatcher* AqlFunctionsInternalCache::fromCache(
    std::string const& pattern,
    std::unordered_map<std::string, std::unique_ptr<icu::RegexMatcher>>& cache) {

  // insert into cache, no matter if pattern is valid or not
  auto matcherIter = cache.try_emplace(
      pattern,
      arangodb::lazyConstruct([&]{
        return std::unique_ptr<icu::RegexMatcher>(arangodb::basics::Utf8Helper::DefaultUtf8Helper.buildMatcher(pattern));
      })).first;

  return matcherIter->second.get();
}

/// @brief compile a REGEX pattern from a string
void AqlFunctionsInternalCache::buildRegexPattern(std::string& out, char const* ptr,
                                   size_t length, bool caseInsensitive) {
  out.clear();
  if (caseInsensitive) {
    out.reserve(length + 4);
    out.append("(?i)");
  }

  out.append(ptr, length);
}

/// @brief compile a LIKE pattern from a string
void AqlFunctionsInternalCache::buildLikePattern(std::string& out, char const* ptr,
                                  size_t length, bool caseInsensitive) {
  out.clear();
  out.reserve(length + 8);  // reserve some room

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
                 c == '\\' || c == '.' || c == '*') {
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

/// @brief inspect a LIKE pattern from a string, and remove all
/// of its escape characters. will stop at the first wildcards found.
/// returns a pair with the following meaning:
/// - first: true if the inspection aborted prematurely because a
///   wildcard was found, and false if the inspection analyzed the
///   complete string
/// - second: true if the found wildcard is the last byte in the pattern,
///   false otherwise. can only be true if first is also true
std::pair<bool, bool> AqlFunctionsInternalCache::inspectLikePattern(std::string& out,
                                                     char const* ptr, size_t length) {
  out.reserve(length);
  bool escaped = false;

  for (size_t i = 0; i < length; ++i) {
    char const c = ptr[i];

    if (c == '\\') {
      if (escaped) {
        // literal backslash
        out.push_back('\\');
      }
      escaped = !escaped;
    } else {
      if (c == '%' || c == '_') {
        if (escaped) {
          // literal %
          out.push_back(c);
        } else {
          // wildcard found
          bool wildcardIsLastChar = (i == length - 1);
          return std::make_pair(true, wildcardIsLastChar);
        }
      } else if (c == '?' || c == '+' || c == '[' || c == '(' || c == ')' ||
                 c == '{' || c == '}' || c == '^' || c == '$' || c == '|' ||
                 c == '\\' || c == '.' || c == '*') {
        // character with special meaning in a regex
        out.push_back(c);
      } else {
        if (escaped) {
          // found a backslash followed by no special character
          out.push_back('\\');
        }

        // literal character
        out.push_back(c);
      }

      escaped = false;
    }
  }

  return std::make_pair(false, false);
}
