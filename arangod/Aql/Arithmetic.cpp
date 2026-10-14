////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Arithmetic.h"

#include "Aql/AstNode.h"
#include "Basics/Arithmetic.h"
#include "Basics/Exceptions.h"

#include <cctype>
#include <charconv>
#include <cmath>
#include <system_error>

namespace {

/// @brief returns whether or not the string is empty
bool isEmptyString(char const* p, size_t length) noexcept {
  char const* e = p + length;

  while (p < e) {
    if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '\f' &&
        *p != '\b') {
      return false;
    }
    ++p;
  }

  return true;
}

}  // namespace

namespace arangodb::aql {

double stringToNumber(std::string const& value, bool& failed) noexcept {
  failed = false;
  char const* p = value.data();
  char const* end = p + value.size();
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ||
                     *p == '\f' || *p == '\v')) {
    ++p;
  }
  bool negative = false;
  if (p < end && (*p == '+' || *p == '-')) {
    negative = *p == '-';
    ++p;
  }
  auto format = std::chars_format::general;
  if (end - p > 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') &&
      (std::isxdigit(static_cast<unsigned char>(p[2])) || p[2] == '.')) {
    format = std::chars_format::hex;
    p += 2;
  }
  if (p < end && (*p == '+' || *p == '-')) {
    failed = true;
    return 0.0;
  }
  double v = 0.0;
  auto const [ptr, ec] = std::from_chars(p, end, v, format);
  if (ec == std::errc::invalid_argument) {
    failed = !isEmptyString(value.data(), value.size());
    return 0.0;
  }
  if (ec == std::errc::result_out_of_range) {
    failed = true;
    return 0.0;
  }
  for (char const* q = ptr; q < end; ++q) {
    char c = *q;
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
      failed = true;
      return 0.0;
    }
  }
  return negative ? -v : v;
}

std::optional<int64_t> integerArithmetic(AstNodeType type, int64_t l,
                                         int64_t r) noexcept {
  switch (type) {
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
      if (isUnsafeAddition(l, r)) {
        return std::nullopt;
      }
      return l + r;
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
      if (isUnsafeSubtraction(l, r)) {
        return std::nullopt;
      }
      return l - r;
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
      if (isUnsafeMultiplication(l, r)) {
        return std::nullopt;
      }
      return l * r;
    case NODE_TYPE_OPERATOR_BINARY_DIV:
      // other divisors may produce a fractional result
      if ((r != 1 && r != -1) || isUnsafeDivision(l, r)) {
        return std::nullopt;
      }
      return l / r;
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      if (r == 0 || isUnsafeDivision(l, r)) {
        return std::nullopt;
      }
      return l % r;
    default:
      return std::nullopt;
  }
}

double doubleArithmetic(AstNodeType type, double l, double r) {
  switch (type) {
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
      return l + r;
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
      return l - r;
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
      return l * r;
    case NODE_TYPE_OPERATOR_BINARY_DIV:
      return l / r;
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      return std::fmod(l, r);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid operator");
  }
}

}  // namespace arangodb::aql
