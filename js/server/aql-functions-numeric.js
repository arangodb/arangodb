////////////////////////////////////////////////////////////////////////////////
/// @brief AQL query language numeric functions
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
/// @brief integer closest to value, not greater than value
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_FLOOR (value) {
  if (!AQL_IS_NUMBER(value)) {
    return undefined;
  }
  
  return Math.floor(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value and not less than value
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_CEIL (value) {
  if (!AQL_IS_NUMBER(value)) {
    return undefined;
  }
  
  return Math.ceil(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value 
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_ROUND (value) {
  if (!AQL_IS_NUMBER(value)) {
    return undefined;
  }
  
  return Math.round(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief absolute value
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_ABS (value) {
  if (!AQL_IS_NUMBER(value)) {
    return undefined;
  }
  
  return Math.abs(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum of all values, ignores undefined
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_MIN_WU () {
  var result = undefined;

  for (var i in arguments) {
    var value = arguments[i];
    if (!AQL_IS_NUMBER(value)) {
      continue;
    }
    if (result === undefined || value < result) {
      result = value;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum of all values, ignores undefined
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_MAX_WU () {
  var result = undefined;

  for (var i in arguments) {
    var value = arguments[i];
    if (!AQL_IS_NUMBER(value)) {
      continue;
    }
    if (result === undefined || value > result) {
      result = value;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_MIN () {
  var result = undefined;

  for (var i in arguments) {
    var value = arguments[i];
    if (!AQL_IS_NUMBER(value)) {
      return undefined;
    }
    if (result === undefined || value < result) {
      result = value;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum of all values
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_MAX () {
  var result = undefined;

  for (var i in arguments) {
    var value = arguments[i];
    if (!AQL_IS_NUMBER(value)) {
      return undefined;
    }
    if (result === undefined || value > result) {
      result = value;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a random value between 0 and 1
////////////////////////////////////////////////////////////////////////////////

function AQL_NUMBER_RAND () {
  return Math.random();
}

