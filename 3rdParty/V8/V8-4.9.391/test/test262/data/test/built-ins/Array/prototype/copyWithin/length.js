// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: Array.prototype.copyWithin.length value and descriptor.
info: >
  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  The length property of the copyWithin method is 2.
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.copyWithin.length, 2,
  'The value of `Array.prototype.copyWithin.length` is `2`'
);

verifyNotEnumerable(Array.prototype.copyWithin, 'length');
verifyNotWritable(Array.prototype.copyWithin, 'length');
verifyConfigurable(Array.prototype.copyWithin, 'length');
