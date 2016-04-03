// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.3
description: >
  Reflect.defineProperty.name value and property descriptor
info: >
  26.1.3 Reflect.defineProperty ( target, propertyKey, attributes )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.defineProperty.name, 'defineProperty',
  'The value of `Reflect.defineProperty.name` is `"defineProperty"`'
);

verifyNotEnumerable(Reflect.defineProperty, 'name');
verifyNotWritable(Reflect.defineProperty, 'name');
verifyConfigurable(Reflect.defineProperty, 'name');
