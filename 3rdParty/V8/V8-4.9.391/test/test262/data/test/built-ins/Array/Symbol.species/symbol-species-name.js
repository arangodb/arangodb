// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.5
description: >
  Array[Symbol.species] accessor property get name
info: >
  22.1.2.5 get Array [ @@species ]

  ...
  The value of the name property of this function is "get [Symbol.species]".
features: [Symbol.species]
includes: [propertyHelper.js]
---*/

var descriptor = Object.getOwnPropertyDescriptor(Array, Symbol.species);

assert.sameValue(
  descriptor.get.name,
  'get [Symbol.species]'
);

verifyNotEnumerable(descriptor.get, 'name');
verifyNotWritable(descriptor.get, 'name');
verifyConfigurable(descriptor.get, 'name');
