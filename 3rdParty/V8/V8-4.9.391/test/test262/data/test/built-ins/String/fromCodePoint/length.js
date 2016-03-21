// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.2
description: >
  The length property of the String.fromCodePoint constructor is 1.
includes: [propertyHelper.js]
---*/

assert.sameValue(
  String.fromCodePoint.length, 1,
  'The value of `String.fromCodePoint.length` is `1`'
);

verifyNotEnumerable(String.fromCodePoint, 'length');
verifyNotWritable(String.fromCodePoint, 'length');
verifyConfigurable(String.fromCodePoint, 'length');
