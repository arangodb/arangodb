// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.2
description: >
  The length property of the WeakSet constructor is 0.
includes: [propertyHelper.js]
---*/

assert.sameValue(WeakSet.length, 0, 'The value of `WeakSet.length` is `0`');

verifyNotEnumerable(WeakSet, 'length');
verifyNotWritable(WeakSet, 'length');
verifyConfigurable(WeakSet, 'length');
