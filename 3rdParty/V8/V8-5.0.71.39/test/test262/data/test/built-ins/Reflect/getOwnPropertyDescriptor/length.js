// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.7
description: >
  Reflect.getOwnPropertyDescriptor.length value and property descriptor
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.getOwnPropertyDescriptor.length, 2,
  'The value of `Reflect.getOwnPropertyDescriptor.length` is `2`'
);

verifyNotEnumerable(Reflect.getOwnPropertyDescriptor, 'length');
verifyNotWritable(Reflect.getOwnPropertyDescriptor, 'length');
verifyConfigurable(Reflect.getOwnPropertyDescriptor, 'length');
