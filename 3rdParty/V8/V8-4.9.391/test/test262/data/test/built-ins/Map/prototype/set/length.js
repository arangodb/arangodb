// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.9
description: >
  Map.prototype.set.length value and descriptor.
info: >
  Map.prototype.set ( key , value )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.set.length, 2,
  'The value of `Map.prototype.set.length` is `2`'
);

verifyNotEnumerable(Map.prototype.set, 'length');
verifyNotWritable(Map.prototype.set, 'length');
verifyConfigurable(Map.prototype.set, 'length');
