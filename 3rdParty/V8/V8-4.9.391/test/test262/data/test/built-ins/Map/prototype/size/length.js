// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.10
description: >
  Map.prototype.size.length value and descriptor.
info: >
  get Map.prototype.size

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

var descriptor = Object.getOwnPropertyDescriptor(Map.prototype, 'size');

assert.sameValue(
  descriptor.get.length, 0,
  'The value of `Map.prototype.size.length` is `0`'
);

verifyNotEnumerable(descriptor.get, 'length');
verifyNotWritable(descriptor.get, 'length');
verifyConfigurable(descriptor.get, 'length');
