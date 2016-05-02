// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.11
description: >
  Reflect.ownKeys.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.ownKeys.length, 1,
  'The value of `Reflect.ownKeys.length` is `1`'
);

verifyNotEnumerable(Reflect.ownKeys, 'length');
verifyNotWritable(Reflect.ownKeys, 'length');
verifyConfigurable(Reflect.ownKeys, 'length');
