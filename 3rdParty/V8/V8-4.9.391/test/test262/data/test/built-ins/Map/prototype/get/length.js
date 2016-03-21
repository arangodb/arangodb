// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.6
description: >
  Map.prototype.get.length value and descriptor.
info: >
  Map.prototype.get ( key )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.get.length, 1,
  'The value of `Map.prototype.get.length` is `1`'
);

verifyNotEnumerable(Map.prototype.get, 'length');
verifyNotWritable(Map.prototype.get, 'length');
verifyConfigurable(Map.prototype.get, 'length');
