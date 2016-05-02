// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.8
description: >
  Reflect.getPrototypeOf.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.getPrototypeOf.length, 1,
  'The value of `Reflect.getPrototypeOf.length` is `1`'
);

verifyNotEnumerable(Reflect.getPrototypeOf, 'length');
verifyNotWritable(Reflect.getPrototypeOf, 'length');
verifyConfigurable(Reflect.getPrototypeOf, 'length');
