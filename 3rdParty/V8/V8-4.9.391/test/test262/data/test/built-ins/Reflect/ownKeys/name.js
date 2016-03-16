// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.11
description: >
  Reflect.ownKeys.name value and property descriptor
info: >
  26.1.11 Reflect.ownKeys ( target )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.ownKeys.name, 'ownKeys',
  'The value of `Reflect.ownKeys.name` is `"ownKeys"`'
);

verifyNotEnumerable(Reflect.ownKeys, 'name');
verifyNotWritable(Reflect.ownKeys, 'name');
verifyConfigurable(Reflect.ownKeys, 'name');
