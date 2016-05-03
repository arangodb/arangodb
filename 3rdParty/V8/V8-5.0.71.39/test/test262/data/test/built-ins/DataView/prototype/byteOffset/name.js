// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 24.2.4.2
description: >
  get DataView.prototype.byteOffset

  17 ECMAScript Standard Built-in Objects

  Functions that are specified as get or set accessor functions of built-in
  properties have "get " or "set " prepended to the property name string.

includes: [propertyHelper.js]
---*/

var descriptor = Object.getOwnPropertyDescriptor(
  DataView.prototype, 'byteOffset'
);

assert.sameValue(
  descriptor.get.name, 'get byteOffset',
  'The value of `descriptor.get.name` is `"get byteOffset"`'
);

verifyNotEnumerable(descriptor.get, 'name');
verifyNotWritable(descriptor.get, 'name');
verifyConfigurable(descriptor.get, 'name');
