/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, eqeq: true */
/*global require, exports, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief Mini Stub and Mock Framework
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
/// @author Lucas Dohmen
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany

var stub,
  allow,
  expect,
  FunctionStub,
  FunctionMock,
  mockConstructor,
  _ = require("underscore");

// Sorry for Yak Shaving. But I can't take it anymore.

// x = stub();

stub = function () {
  'use strict';
  return function() {};
};

// allow(x)
//   .toReceive("functionName")
//   .andReturn({ x: 1 })

FunctionStub = function(obj) {
  'use strict';
  this.obj = obj;
};

_.extend(FunctionStub.prototype, {
  toReceive: function (functionName) {
    'use strict';
    this.functionName = functionName;
    this.buildFunctionStub();
    return this;
  },

  andReturn: function (returnValue) {
    'use strict';
    this.returnValue = returnValue;
    this.buildFunctionStub();
    return this;
  },

  buildFunctionStub: function () {
    'use strict';
    var returnValue = this.returnValue;

    this.obj[this.functionName] = function () {
      return returnValue;
    };
  }
});

allow = function(obj) {
  'use strict';
  return (new FunctionStub(obj));
};

// expect(x)
//   .toReceive("functionName")
//   .withArguments(5)
//   .andReturn({ x: 1 })
//
// ...
//
// x.assertIsSatisfied();

FunctionMock = function(obj) {
  'use strict';
  this.obj = obj;
  this.obj.satisfied = false;

  this.obj.assertIsSatisfied = function () {
    assertTrue(this.satisfied, "Mock expectation was not satisfied");
  };
};

_.extend(FunctionMock.prototype, {
  toReceive: function (functionName) {
    'use strict';
    this.functionName = functionName;
    this.buildFunctionMock();
    return this;
  },

  andReturn: function (returnValue) {
    'use strict';
    this.returnValue = returnValue;
    this.buildFunctionMock();
    return this;
  },

  withArguments: function () {
    'use strict';
    this.expectedArguments = arguments;
    this.buildFunctionMock();
    return this;
  },

  buildFunctionMock: function () {
    'use strict';
    var returnValue = this.returnValue,
      expectedArguments = this.expectedArguments,
      obj = this.obj;

    this.obj[this.functionName] = function () {
      if ((expectedArguments === undefined) || (_.isEqual(arguments, expectedArguments))) {
        obj.satisfied = true;
      }

      return returnValue;
    };
  }
});

expect = function(obj) {
  'use strict';
  return (new FunctionMock(obj));
};

/* Create a Mock Constructor
 *
 * Give the arguments you expect to mockConstructor:
 * It checks, if it was called with new and the
 * correct arguments.
 *
 * MyProto = mockConstructor(test);
 *
 * a = new MyProto(test);
 *
 * MyProto.assertIsSatisfied();
 */
mockConstructor = function () {
  'use strict';
  var expectedArguments = arguments,
    satisfied = false,
    MockConstructor = function () {
    if (this.constructor === MockConstructor) {
      // Was called as a constructor
      satisfied = _.isEqual(arguments, expectedArguments);
    }
  };

  MockConstructor.assertIsSatisfied = function () {
    assertTrue(satisfied);
  };

  return MockConstructor;
};

exports.stub = stub;
exports.allow = allow;
exports.expect = expect;
exports.mockConstructor = mockConstructor;
