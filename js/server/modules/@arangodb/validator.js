/* jshint strict: false, unused: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief validator functions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');

// //////////////////////////////////////////////////////////////////////////////
// / @brief positive number
// //////////////////////////////////////////////////////////////////////////////

exports.positiveNumber = function (value, info, lang) {
  if (value <= 0.0) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_VALIDATION_FAILED;
    error.errorMessage = 'number must be positive';

    throw error;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief negative number
// //////////////////////////////////////////////////////////////////////////////

exports.negativeNumber = function (value, info, lang) {
  if (0.0 <= value) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_VALIDATION_FAILED;
    error.errorMessage = 'number must be negative';

    throw error;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief zero
// //////////////////////////////////////////////////////////////////////////////

exports.zeroNumber = function (value, info, lang) {
  if (value === 0.0) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_VALIDATION_FAILED;
    error.errorMessage = 'number must be zero';

    throw error;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief non-positive number
// //////////////////////////////////////////////////////////////////////////////

exports.nonPositiveNumber = function (value, info, lang) {
  if (0.0 < value) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_VALIDATION_FAILED;
    error.errorMessage = 'number must be non-positive';

    throw error;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief non-negative number
// //////////////////////////////////////////////////////////////////////////////

exports.nonNegativeNumber = function (value, info, lang) {
  if (value < 0.0) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_VALIDATION_FAILED;
    error.errorMessage = 'number must be non-negative';

    throw error;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief zero
// //////////////////////////////////////////////////////////////////////////////

exports.nonZeroNumber = function (value, info, lang) {
  if (value !== 0.0) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_VALIDATION_FAILED;
    error.errorMessage = 'number must be non-zero';

    throw error;
  }
};
