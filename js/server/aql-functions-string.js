////////////////////////////////////////////////////////////////////////////////
/// @brief AQL query language string functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the length of the string in number of characters
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_LENGTH (value) {
  if (!AQL_IS_STRING(value)) {
    return undefined;
  }

  return value.length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a substring from the original string
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_SUBSTRING (value, start, length) {
  if (!AQL_IS_STRING(value)) {
    return undefined;
  }
  if (!AQL_IS_NUMBER(start)) {
    return undefined;
  }

  if (length === undefined) {
    return value.substr(start);
  }

  if (!AQL_IS_NUMBER(length) || length < 0) {
    return undefined;
  }

  return value.substr(start, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the string in upper case
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_UPPER (value) {
  if (!AQL_IS_STRING(value)) {
    return undefined;
  }

  return value.toUpperCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the string in lower case
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_LOWER (value) {
  if (!AQL_IS_STRING(value)) {
    return undefined;
  }

  return value.toLowerCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the position of substring search in value
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_POSITION (value, search, insensitive) {
  if (!AQL_IS_STRING(value)) {
    return undefined;
  }

  var args = "g";
  if (insensitive) {
    args += "i";
  }

  var result = value.search(new RegExp(search), args);
  if (result === -1) {
    return undefined;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether value contains the substring search
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_CONTAINS (value, search, insensitive) {
  if (!AQL_IS_STRING(value)) {
    return undefined;
  }

  var args = "g";
  if (insensitive) {
    args += "i";
  }

  return (value.search(new RegExp(search), args) !== -1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces pattern in value with replace
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_REPLACE (value, pattern, replace, insensitive) {
  if (!AQL_IS_STRING(value)) {
    return undefined;
  }
  if (!AQL_IS_STRING(pattern) || pattern.length == 0) {
    return undefined;
  }
  if (!AQL_IS_STRING(replace)) {
    return undefined;
  }

  var args = "g";
  if (insensitive) {
    args += "i";
  }

  return value.replace(new RegExp(pattern, args), replace);
}

