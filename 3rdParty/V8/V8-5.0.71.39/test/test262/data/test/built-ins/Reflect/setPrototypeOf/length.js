// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.14
description: >
  Reflect.setPrototypeOf.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.setPrototypeOf.length, 2,
  'The value of `Reflect.setPrototypeOf.length` is `2`'
);

verifyNotEnumerable(Reflect.setPrototypeOf, 'length');
verifyNotWritable(Reflect.setPrototypeOf, 'length');
verifyConfigurable(Reflect.setPrototypeOf, 'length');
