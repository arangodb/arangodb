// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.8
description: >
  Reflect.getPrototypeOf.name value and property descriptor
info: >
  26.1.8 Reflect.getPrototypeOf ( target )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.getPrototypeOf.name, 'getPrototypeOf',
  'The value of `Reflect.getPrototypeOf.name` is `"getPrototypeOf"`'
);

verifyNotEnumerable(Reflect.getPrototypeOf, 'name');
verifyNotWritable(Reflect.getPrototypeOf, 'name');
verifyConfigurable(Reflect.getPrototypeOf, 'name');
