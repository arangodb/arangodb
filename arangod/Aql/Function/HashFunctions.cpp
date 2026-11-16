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

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/StringUtils.h"
#include "Basics/ThreadLocalLeaser.h"
#include "Basics/conversions.h"
#include "Basics/hashes.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <absl/crc/crc32c.h>

#include <boost/hash2/md5.hpp>
#include <boost/hash2/sha1.hpp>
#include <boost/hash2/sha2.hpp>

#include <string_view>

using namespace arangodb;

namespace arangodb::aql {

namespace {
template<typename HashType>
AqlValue AqlHash(std::string const& buffer) {
  HashType hash;
  hash.update(buffer.c_str(), buffer.size());
  return AqlValue{boost::hash2::to_string(hash.result())};
}
}  // anonymous namespace

/// @brief function SOUNDEX
AqlValue functions::Soundex(ExpressionContext* expr, AstNode const&,
                            VPackFunctionParametersView parameters) {
  auto& trx = expr->trx();
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);

  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  appendAsString(trx.vpackOptions(), adapter, value);

  std::string encoded = basics::StringUtils::soundex(
      basics::StringUtils::trim(basics::StringUtils::tolower(
          std::string(buffer->data(), buffer->length()))));

  return AqlValue(encoded);
}

/// @brief function MD5
AqlValue functions::Md5(ExpressionContext* exprCtx, AstNode const&,
                        VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &exprCtx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);
  return AqlHash<boost::hash2::md5_128>(*buffer);
}

/// @brief function SHA1
AqlValue functions::Sha1(ExpressionContext* exprCtx, AstNode const&,
                         VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &exprCtx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);
  return AqlHash<boost::hash2::sha1_160>(*buffer);
}

/// @brief function SHA256
AqlValue functions::Sha256(ExpressionContext* exprCtx, AstNode const&,
                           VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &exprCtx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);
  return AqlHash<boost::hash2::sha2_256>(*buffer);
}

/// @brief function SHA512
AqlValue functions::Sha512(ExpressionContext* exprCtx, AstNode const&,
                           VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &exprCtx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);
  return AqlHash<boost::hash2::sha2_512>(*buffer);
}

/// @brief function Crc32
AqlValue functions::Crc32(ExpressionContext* exprCtx, AstNode const&,
                          VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &exprCtx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);
  auto const crc = static_cast<uint32_t>(
      absl::ComputeCrc32c(std::string_view{buffer->data(), buffer->length()}));
  char out[9];
  size_t length = TRI_StringUInt32HexInPlace(crc, &out[0]);
  return AqlValue(std::string_view{&out[0], length});
}

/// @brief function Fnv64
AqlValue functions::Fnv64(ExpressionContext* exprCtx, AstNode const&,
                          VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &exprCtx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);

  uint64_t hashval = FnvHashPointer(buffer->data(), buffer->length());
  char out[17];
  size_t length = TRI_StringUInt64HexInPlace(hashval, &out[0]);
  return AqlValue(std::string_view{&out[0], length});
}

/// @brief function HASH
AqlValue functions::Hash(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  // throw away the top bytes so the hash value can safely be used
  // without precision loss when storing in JavaScript etc.
  uint64_t hash = value.hash() & 0x0007ffffffffffffULL;

  return AqlValue(AqlValueHintUInt(hash));
}
}  // namespace arangodb::aql
