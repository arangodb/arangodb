/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Some crypto functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                            RANDOM
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Random
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a random number
///
/// the value returned might be positive or negative or even 0.
/// if random number generation fails, then undefined is returned
////////////////////////////////////////////////////////////////////////////////

exports.rand = function (value) {
  return internal.rand();
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            HASHES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Hashes
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an MD5 hash
////////////////////////////////////////////////////////////////////////////////

exports.md5 = function (value) {
  return internal.md5(value);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an SHA 256 hash
////////////////////////////////////////////////////////////////////////////////

exports.sha256 = function (value) {
  return internal.sha256(value);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Generates a string of a given length containing numbers.
////////////////////////////////////////////////////////////////////////////////

exports.genRandomNumbers = function (value) {
  return internal.genRandomNumbers(value);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Generates a string of a given length containing numbers and characters.
////////////////////////////////////////////////////////////////////////////////

exports.genRandomAlphaNumbers = function (value) {
  return internal.genRandomAlphaNumbers(value);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Generates a string containing numbers and characters (length 8).
////////////////////////////////////////////////////////////////////////////////

exports.genRandomSalt = function (value) {
  return internal.genRandomSalt(value);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Generates a base64 encoded nonce string.
////////////////////////////////////////////////////////////////////////////////

exports.createNonce = function () {
  return internal.createNonce();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks and marks a nonce
////////////////////////////////////////////////////////////////////////////////

exports.checkAndMarkNonce = function (value) {
  return internal.checkAndMarkNonce(value);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
