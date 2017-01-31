/* jshint strict: false */
/* global Buffer */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Some crypto functions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');

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

exports.pbkdf2 = function (salt, password, iterations, keyLength) {
  return internal.pbkdf2(salt, password, iterations, keyLength);
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

exports.genRandomNumbers = function (value) {
  return internal.genRandomNumbers(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a string of a given length containing numbers and characters.
// //////////////////////////////////////////////////////////////////////////////

exports.genRandomAlphaNumbers = function (value) {
  return internal.genRandomAlphaNumbers(value);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a string of a given length containing ASCII characters.
// //////////////////////////////////////////////////////////////////////////////

exports.genRandomSalt = function (value) {
  return internal.genRandomSalt(value);
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
// / @brief Compares two strings in constant time
// //////////////////////////////////////////////////////////////////////////////

exports.constantEquals = function (a, b) {
  'use strict';
  var length, result, i;
  length = a.length > b.length ? a.length : b.length;
  result = true;
  for (i = 0; i < length; i++) {
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
  'use strict';
  return str.replace(/[+]/g, '-').replace(/[\/]/g, '_').replace(/[=]/g, '');
}

function jwtHmacSigner (algorithm) {
  'use strict';
  return function (key, segments) {
    return new Buffer(exports.hmac(key, segments.join('.'), algorithm), 'hex').toString('base64');
  };
}

function jwtHmacVerifier (algorithm) {
  'use strict';
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
      'use strict';
      if (key) {
        throw new Error('Can not sign message with key using algorithm "none"!');
      }
      return '';
    },
    verify: function (key) {
      'use strict';
      // see https://auth0.com/blog/2015/03/31/critical-vulnerabilities-in-json-web-token-libraries/
      return !key;
    }
  }
};

exports.jwtCanonicalAlgorithmName = function (algorithm) {
  'use strict';
  if (algorithm && typeof algorithm === 'string') {
    if (exports.jwtAlgorithms.hasOwnProperty(algorithm.toLowerCase())) {
      return algorithm.toLowerCase();
    }
    if (exports.jwtAlgorithms.hasOwnProperty(algorithm.toUpperCase())) {
      return algorithm.toUpperCase();
    }
  }
  throw new Error(
    'Unknown algorithm "'
    + algorithm
    + '". Only the following algorithms are supported at this time: '
    + Object.keys(exports.jwtAlgorithms).join(', ')
  );
};

exports.jwtEncode = function (key, message, algorithm) {
  'use strict';
  algorithm = algorithm ? exports.jwtCanonicalAlgorithmName(algorithm) : 'HS256';
  var header = {typ: 'JWT', alg: algorithm}, segments = [];
  segments.push(jwtUrlEncode(new Buffer(JSON.stringify(header)).toString('base64')));
  segments.push(jwtUrlEncode(new Buffer(JSON.stringify(message)).toString('base64')));
  segments.push(jwtUrlEncode(exports.jwtAlgorithms[algorithm].sign(key, segments)));
  return segments.join('.');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Decodes JSON Web Token
// //////////////////////////////////////////////////////////////////////////////

function jwtUrlDecode (str) {
  'use strict';
  while ((str.length % 4) !== 0) {
    str += '=';
  }
  return str.replace(/\-/g, '+').replace(/_/g, '/');
}

exports.jwtDecode = function (key, token, noVerify) {
  'use strict';
  if (!token) {
    return null;
  }

  var segments = token.split('.');
  if (segments.length !== 3) {
    throw new Error('Wrong number of JWT segments!');
  }
  var headerSeg = new Buffer(jwtUrlDecode(segments[0]), 'base64').toString();
  var messageSeg = new Buffer(jwtUrlDecode(segments[1]), 'base64').toString();
  segments[2] = new Buffer(jwtUrlDecode(segments[2]), 'base64').toString('hex');

  if (!noVerify) {
    var header = JSON.parse(headerSeg);
    header.alg = exports.jwtCanonicalAlgorithmName(header.alg);
    if (!exports.jwtAlgorithms[header.alg].verify(key, segments)) {
      throw new Error('Signature verification failed!');
    }
  }

  return JSON.parse(messageSeg);
};
