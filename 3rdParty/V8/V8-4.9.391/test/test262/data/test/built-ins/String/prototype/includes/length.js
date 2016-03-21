// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.3.7
description: >
  String.prototype.includes.length value and descriptor.
info: >
  21.1.3.7 String.prototype.includes ( searchString [ , position ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  String.prototype.includes.length, 1,
  'The value of `String.prototype.includes.length` is `1`'
);

verifyNotEnumerable(String.prototype.includes, 'length');
verifyNotWritable(String.prototype.includes, 'length');
verifyConfigurable(String.prototype.includes, 'length');
