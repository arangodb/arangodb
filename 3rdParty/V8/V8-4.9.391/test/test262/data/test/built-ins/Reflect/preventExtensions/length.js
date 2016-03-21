// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.12
description: >
  Reflect.preventExtensions.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.preventExtensions.length, 1,
  'The value of `Reflect.preventExtensions.length` is `1`'
);

verifyNotEnumerable(Reflect.preventExtensions, 'length');
verifyNotWritable(Reflect.preventExtensions, 'length');
verifyConfigurable(Reflect.preventExtensions, 'length');
