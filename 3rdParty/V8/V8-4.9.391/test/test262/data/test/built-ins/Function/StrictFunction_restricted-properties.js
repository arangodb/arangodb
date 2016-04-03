// Copyright (C) 2015 Caitlin Potter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*--- 
description: >
    ECMAScript Function objects defined using syntactic constructors
    in strict mode code do not have own properties "caller" or
    "arguments", but inherit them from %FunctionPrototype%.
flags: [onlyStrict]
es6id: 16.1
---*/

function func() {}

assert.sameValue(func.hasOwnProperty('caller'), false, 'Functions defined using syntactic constructors in strict mode code do not have own property "caller"');
assert.sameValue(func.hasOwnProperty('arguments'), false, 'Functions defined using syntactic constructors in strict mode code do not have own property "arguments"');

assert.throws(TypeError, function() {
  return func.caller;
});

assert.throws(TypeError, function() {
  func.caller = {};
});

assert.throws(TypeError, function() {
  return func.arguments;
});

assert.throws(TypeError, function() {
  func.arguments = {};
});

var newfunc = new Function('"use strict"');

assert.sameValue(newfunc.hasOwnProperty('caller'), false, 'strict Functions created using Function constructor do not have own property "caller"');
assert.sameValue(newfunc.hasOwnProperty('arguments'), false, 'strict Functions created using Function constructor do not have own property "arguments"');

assert.throws(TypeError, function() {
  return newfunc.caller;
});

assert.throws(TypeError, function() {
  newfunc.caller = {};
});

assert.throws(TypeError, function() {
  return newfunc.arguments;
});

assert.throws(TypeError, function() {
  newfunc.arguments = {};
});
