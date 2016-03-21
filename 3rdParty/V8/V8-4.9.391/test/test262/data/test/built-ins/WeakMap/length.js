// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.2
description: >
  The length property of the WeakMap constructor is 0.
includes: [propertyHelper.js]
---*/

assert.sameValue(WeakMap.length, 0, 'The value of `WeakMap.length` is `0`');

verifyNotEnumerable(WeakMap, 'length');
verifyNotWritable(WeakMap, 'length');
verifyConfigurable(WeakMap, 'length');
