// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.6
description: >
  Reflect.get.name value and property descriptor
info: >
  26.1.6 Reflect.get ( target, propertyKey [ , receiver ])

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.get.name, 'get',
  'The value of `Reflect.get.name` is `"get"`'
);

verifyNotEnumerable(Reflect.get, 'name');
verifyNotWritable(Reflect.get, 'name');
verifyConfigurable(Reflect.get, 'name');
