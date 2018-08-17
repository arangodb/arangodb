'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2012-2014 triAGENS GmbH, Cologne, Germany
// / Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
// / @brief generate a random number
// /
// / the value returned might be positive or negative or even 0.
// / if random number generation fails, then undefined is returned
// //////////////////////////////////////////////////////////////////////////////

exports.rand = function () {
  return internal.rand();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply an HMAC
// //////////////////////////////////////////////////////////////////////////////

exports.hmac = function (key, message, algorithm) {
  return internal.hmac(key, message, algorithm);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply a PBKDF2
// //////////////////////////////////////////////////////////////////////////////

exports.pbkdf2 = function (salt, password, iterations, keyLength, algorithm) {
  if (algorithm === undefined || algorithm === null) {
    return internal.pbkdf2hs1(salt, password, iterations, keyLength);
  }
  return internal.pbkdf2(salt, password, iterations, keyLength, algorithm);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply an MD5 hash
// //////////////////////////////////////////////////////////////////////////////

exports.md5 = function (value) {
  return internal.md5(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply an SHA 512 hash
// //////////////////////////////////////////////////////////////////////////////

exports.sha512 = function (value) {
  return internal.sha512(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply an SHA 384 hash
// //////////////////////////////////////////////////////////////////////////////

exports.sha384 = function (value) {
  return internal.sha384(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply an SHA 256 hash
// //////////////////////////////////////////////////////////////////////////////

exports.sha256 = function (value) {
  return internal.sha256(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply an SHA 224 hash
// //////////////////////////////////////////////////////////////////////////////

exports.sha224 = function (value) {
  return internal.sha224(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief apply an SHA 1 hash
// //////////////////////////////////////////////////////////////////////////////

exports.sha1 = function (value) {
  return internal.sha1(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a string of a given length containing numbers.
// //////////////////////////////////////////////////////////////////////////////

exports.genRandomNumbers = function (length) {
  return internal.genRandomNumbers(length);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a string of a given length containing numbers and characters.
// //////////////////////////////////////////////////////////////////////////////

exports.genRandomAlphaNumbers = function (length) {
  return internal.genRandomAlphaNumbers(length);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a string of a given length containing ASCII characters.
// //////////////////////////////////////////////////////////////////////////////

exports.genRandomSalt = function (length) {
  return internal.genRandomSalt(length);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a buffer of a given length containing random bytes.
// //////////////////////////////////////////////////////////////////////////////

exports.genRandomBytes = function (length = 0) {
  const numBytes = Math.ceil(length / 4);
  const buf = new Buffer(numBytes * 4);
  buf.fill(0);
  for (let i = 0; i < numBytes; i++) {
    buf.writeInt32LE(exports.rand(), i * 4);
  }
  return buf.slice(0, length);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a base64 encoded nonce string.
// //////////////////////////////////////////////////////////////////////////////

exports.createNonce = function () {
  return internal.createNonce();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Checks and marks a nonce
// //////////////////////////////////////////////////////////////////////////////

exports.checkAndMarkNonce = function (value) {
  return internal.checkAndMarkNonce(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a UUID v4 string
// //////////////////////////////////////////////////////////////////////////////

exports.uuidv4 = function () {
  const bytes = exports.genRandomBytes(16);
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  return `${
    bytes.hexSlice(0, 4)
    }-${
    bytes.hexSlice(4, 6)
    }-${
    bytes.hexSlice(6, 8)
    }-${
    bytes.hexSlice(8, 10)
    }-${
    bytes.hexSlice(10, bytes.length)
    }`;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Compares two strings in constant time
// //////////////////////////////////////////////////////////////////////////////

exports.constantEquals = function (a, b) {
  const length = a.length > b.length ? a.length : b.length;
  let result = true;
  for (let i = 0; i < length; i++) {
    if (a.charCodeAt(i) !== b.charCodeAt(i)) {
      result = false;
    }
  }
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Encodes JSON Web Token
// //////////////////////////////////////////////////////////////////////////////

function jwtUrlEncode (str) {
  return str.replace(/[+]/g, '-').replace(/[/]/g, '_').replace(/[=]/g, '');
}

function jwtHmacSigner (algorithm) {
  return function (key, segments) {
    return new Buffer(
      exports.hmac(key, segments.join('.'), algorithm),
      'hex'
    ).toString('base64');
  };
}

function jwtHmacVerifier (algorithm) {
  return function (key, segments) {
    return exports.constantEquals(
      exports.hmac(key, segments.slice(0, 2).join('.'), algorithm),
      segments[2]
    );
  };
}

exports.jwtAlgorithms = {
  HS256: {
    sign: jwtHmacSigner('sha256'),
    verify: jwtHmacVerifier('sha256')
  },
  HS384: {
    sign: jwtHmacSigner('sha384'),
    verify: jwtHmacVerifier('sha384')
  },
  HS512: {
    sign: jwtHmacSigner('sha512'),
    verify: jwtHmacVerifier('sha512')
  },
  none: {
    sign: function (key) {
      if (key) {
        throw new Error('Can not sign message with key using algorithm "none"!');
      }
      return '';
    },
    verify: function (key) {
      // see https://auth0.com/blog/2015/03/31/critical-vulnerabilities-in-json-web-token-libraries/
      return !key;
    }
  }
};

exports.jwtCanonicalAlgorithmName = function (algorithm) {
  if (algorithm && typeof algorithm === 'string') {
    if (exports.jwtAlgorithms.hasOwnProperty(algorithm.toLowerCase())) {
      return algorithm.toLowerCase();
    }
    if (exports.jwtAlgorithms.hasOwnProperty(algorithm.toUpperCase())) {
      return algorithm.toUpperCase();
    }
  }
  throw new Error(
    `Unknown algorithm "${
      algorithm
    }". Only the following algorithms are supported at this time: ${
      Object.keys(exports.jwtAlgorithms).join(', ')
    }`
  );
};

exports.jwtEncode = function (key, message, algorithm) {
  algorithm = algorithm ? exports.jwtCanonicalAlgorithmName(algorithm) : 'HS256';
  const header = {typ: 'JWT', alg: algorithm};
  const segments = [];
  segments.push(jwtUrlEncode(new Buffer(JSON.stringify(header)).toString('base64')));
  segments.push(jwtUrlEncode(new Buffer(JSON.stringify(message)).toString('base64')));
  segments.push(jwtUrlEncode(exports.jwtAlgorithms[algorithm].sign(key, segments)));
  return segments.join('.');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Decodes JSON Web Token
// //////////////////////////////////////////////////////////////////////////////

function jwtUrlDecode (str) {
  while ((str.length % 4) !== 0) {
    str += '=';
  }
  return str.replace(/-/g, '+').replace(/_/g, '/');
}

exports.jwtDecode = function (key, token, noVerify) {
  if (!token) {
    return null;
  }

  const segments = token.split('.');
  if (segments.length !== 3) {
    throw new Error('Wrong number of JWT segments!');
  }
  const headerSeg = new Buffer(jwtUrlDecode(segments[0]), 'base64').toString();
  const messageSeg = new Buffer(jwtUrlDecode(segments[1]), 'base64').toString();
  segments[2] = new Buffer(jwtUrlDecode(segments[2]), 'base64').toString('hex');

  if (!noVerify) {
    const header = JSON.parse(headerSeg);
    header.alg = exports.jwtCanonicalAlgorithmName(header.alg);
    if (!exports.jwtAlgorithms[header.alg].verify(key, segments)) {
      throw new Error('Signature verification failed!');
    }
  }

  return JSON.parse(messageSeg);
};

function numToUInt64LE (value) {
  const bytes = Array(8);
  for (let i = 8; i > 0; i--) {
    bytes[i - 1] = value & 255;
    value = value >> 8;
  }
  return new Buffer(bytes);
}

function hotpTruncateHS1 (hmacResult) {
  // See https://tools.ietf.org/html/rfc4226#section-5.4
  const offset = hmacResult[19] & 0xf;
  const binCode = (
    (hmacResult[offset] & 0x7f) << 24 |
    (hmacResult[offset + 1] & 0xff) << 16 |
    (hmacResult[offset + 2] & 0xff) << 8 |
    (hmacResult[offset + 3] & 0xff)
  );
  return binCode % Math.pow(10, 6);
}

exports.hotpGenerate = function (key, counter = 0) {
  const hotpCounter = numToUInt64LE(counter);
  const hash = new Buffer(exports.hmac(key, hotpCounter, 'sha1'), 'hex');
  const truncated = String(hotpTruncateHS1(hash));
  return Array(7 - truncated.length).join('0') + truncated;
};

exports.hotpVerify = function (token, key, counter = 0, window = 50) {
  for (let c = counter - window; c <= counter + window; c++) {
    if (exports.hotpGenerate(key, c) === token) {
      return {delta: c - counter};
    }
  }
  return null;
};

exports.totpGenerate = function (key, step = 30) {
  const seconds = Date.now() / 1000;
  const counter = Math.floor(seconds / step);
  return exports.hotpGenerate(key, counter);
};

exports.totpVerify = function (token, key, step = 30, window = 6) {
  const seconds = Date.now() / 1000;
  const counter = Math.floor(seconds / step);
  return exports.hotpVerify(key, token, counter, window);
};
