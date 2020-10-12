////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/helper.h>
#include <fuerte/jwt.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>

#ifdef OPENSSL_NO_SSL2  // OpenSSL > 1.1.0 deprecates RAND_pseudo_bytes
#define RAND_BYTES RAND_bytes
#else
#define RAND_BYTES RAND_pseudo_bytes
#endif

namespace arangodb { namespace fuerte { inline namespace v1 {

/// generate a JWT token for internal cluster communication
std::string jwt::generateInternalToken(std::string const& secret,
                                       std::string const& id) {
  std::chrono::seconds iss = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());
  /*std::chrono::seconds exp =
   std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
   + _validFor;*/

  VPackBuilder bodyBuilder;
  bodyBuilder.openObject();
  bodyBuilder.add("server_id", VPackValue(id));
  bodyBuilder.add("iss", VPackValue("arangodb"));
  bodyBuilder.add("iat", VPackValue(iss.count()));
  // bodyBuilder.add("exp", VPackValue(exp.count()));
  bodyBuilder.close();
  return generateRawJwt(secret, bodyBuilder.slice());
}

/// Generate JWT token as used for 'users' in arangodb
std::string jwt::generateUserToken(std::string const& secret,
                                   std::string const& username,
                                   std::chrono::seconds validFor) {
  assert(!secret.empty());

  std::chrono::seconds iss = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());

  VPackBuilder bodyBuilder;
  bodyBuilder.openObject(/*unindexed*/ true);
  bodyBuilder.add("preferred_username", VPackValue(username));
  bodyBuilder.add("iss", VPackValue("arangodb"));
  bodyBuilder.add("iat", VPackValue(iss.count()));
  if (validFor.count() > 0) {
    bodyBuilder.add("exp", VPackValue((iss + validFor).count()));
  }
  bodyBuilder.close();
  return generateRawJwt(secret, bodyBuilder.slice());
}

std::string jwt::generateRawJwt(std::string const& secret,
                                VPackSlice const& body) {
  VPackBuilder headerBuilder;
  {
    VPackObjectBuilder h(&headerBuilder);
    headerBuilder.add("alg", VPackValue("HS256"));
    headerBuilder.add("typ", VPackValue("JWT"));
  }

  // https://tools.ietf.org/html/rfc7515#section-2 requires
  // JWT to use base64-encoding without trailing padding `=` chars
  bool const pad = false;

  std::string fullMessage(encodeBase64(headerBuilder.toJson(), pad) + "." +
                          encodeBase64(body.toJson(), pad));

  std::string signature =
      sslHMAC(secret.c_str(), secret.length(), fullMessage.c_str(),
              fullMessage.length(), Algorithm::ALGORITHM_SHA256);

  return fullMessage + "." + encodeBase64U(signature, pad);
}

// code from ArangoDBs SslInterface.cpp

std::string jwt::sslHMAC(char const* key, size_t keyLength, char const* message,
                         size_t messageLen, Algorithm algorithm) {
  EVP_MD* evp_md = nullptr;

  if (algorithm == Algorithm::ALGORITHM_SHA1) {
    evp_md = const_cast<EVP_MD*>(EVP_sha1());
  } else if (algorithm == jwt::Algorithm::ALGORITHM_SHA224) {
    evp_md = const_cast<EVP_MD*>(EVP_sha224());
  } else if (algorithm == jwt::Algorithm::ALGORITHM_MD5) {
    evp_md = const_cast<EVP_MD*>(EVP_md5());
  } else if (algorithm == jwt::Algorithm::ALGORITHM_SHA384) {
    evp_md = const_cast<EVP_MD*>(EVP_sha384());
  } else if (algorithm == jwt::Algorithm::ALGORITHM_SHA512) {
    evp_md = const_cast<EVP_MD*>(EVP_sha512());
  } else {
    // default
    evp_md = const_cast<EVP_MD*>(EVP_sha256());
  }

  unsigned char* md = nullptr;
  try {
    md = static_cast<unsigned char*>(malloc(EVP_MAX_MD_SIZE + 1));
    if (!md) {
      return "";
    }
    unsigned int md_len;
    HMAC(evp_md, key, (int)keyLength, (const unsigned char*)message, messageLen,
         md, &md_len);
    std::string copy((char*)md, md_len);
    free(md);
    return copy;
  } catch (...) {
    free(md);
  }
  return "";
}

bool jwt::verifyHMAC(char const* challenge, size_t challengeLength,
                     char const* secret, size_t secretLen, char const* response,
                     size_t responseLen, jwt::Algorithm algorithm) {
  // challenge = key
  // secret, secretLen = message
  // result must == BASE64(response, responseLen)

  std::string s =
      sslHMAC(challenge, challengeLength, secret, secretLen, algorithm);

  if (s.length() == responseLen &&
      s.compare(std::string(response, responseLen)) == 0) {
    return true;
  }

  return false;
}
}}}  // namespace arangodb::fuerte::v1
