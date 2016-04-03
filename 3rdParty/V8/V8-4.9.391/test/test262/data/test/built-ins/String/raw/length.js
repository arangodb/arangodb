// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.4
description: >
  String.raw.length value and property descriptor
info: >
  String.raw ( template , ...substitutions )

  The length property of the raw function is 1.
includes: [propertyHelper.js]
---*/

assert.sameValue(
  String.raw.length, 1,
  'The value of `String.raw.length` is `1`'
);

verifyNotEnumerable(String.raw, 'length');
verifyNotWritable(String.raw, 'length');
verifyConfigurable(String.raw, 'length');
