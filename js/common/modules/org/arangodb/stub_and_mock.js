/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, eqeq: true */
/*global require, exports */

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
  _ = require("underscore");

// Sorry for Yak Shaving. But I can't take it anymore.

// x = stub();

stub = function () {
  return function() {};
};

// allow(x).to({
//   receive: "functionName",
//   and_return: { x: 1 }
// });

FunctionStub = function(obj) {
  this.obj = obj;
};

FunctionStub.prototype.to = function(config) {
  this.obj[config.receive] = function () {
    return config.and_return;
  };
};

allow = function(obj) {
  return (new FunctionStub(obj));
};

// expect(x).to({
//   receive: "functionName",
//   withArguments: [ 5 ],
//   and_return { x: 1 }
// });
//
// ...
//
// x.assertIsSatisfied();

FunctionMock = function(obj) {
  this.obj = obj;
  this.obj.satisfied = true;

  this.obj.assertIsSatisfied = function () {
    assertTrue(this.satisfied, "Mock Expectation was not satisfied");
  };
};

FunctionMock.prototype.to = function(config) {
  var obj = this.obj;
  obj.satisfied = false;

  this.obj[config.receive] = function () {
    var args = Array.prototype.slice.call(arguments, 0);

    if ((config.withArguments === undefined) || (_.isEqual(args, config.withArguments))) {
      obj.satisfied = true;
    }

    return config.and_return;
  };
};

expect = function(obj) {
  return (new FunctionMock(obj));
};

exports.stub = stub;
exports.allow = allow;
exports.expect = expect;
