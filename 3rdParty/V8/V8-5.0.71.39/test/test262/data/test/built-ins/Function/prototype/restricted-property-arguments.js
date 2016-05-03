// Copyright (C) 2015 Caitlin Potter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*--- 
description: Intrinsic %FunctionPrototype% has poisoned own property "arguments"
includes:
  - propertyHelper.js
es6id: 8.2.2 S12, 9.2.7
---*/

var FunctionPrototype = Function.prototype;

assert.sameValue(FunctionPrototype.hasOwnProperty('arguments'), true, 'The result of %FunctionPrototype%.hasOwnProperty("arguments") is true');
var descriptor = Object.getOwnPropertyDescriptor(FunctionPrototype, 'arguments');
assert.sameValue(typeof descriptor.get, 'function', '%FunctionPrototype%.arguments is an accessor property');
assert.sameValue(typeof descriptor.set, 'function', '%FunctionPrototype%.arguments is an accessor property');
assert.sameValue(descriptor.get, descriptor.set, '%FunctionPrototype%.arguments getter/setter are both %ThrowTypeError%');

assert.throws(TypeError, function() {
  return FunctionPrototype.arguments;	
});

assert.throws(TypeError, function() {
  FunctionPrototype.arguments = {};
});

verifyNotEnumerable(FunctionPrototype, 'arguments');
verifyConfigurable(FunctionPrototype, 'arguments');
