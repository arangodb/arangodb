// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.2
description: >
  Reflect.construct.name value and property descriptor
info: >
  26.1.2 Reflect.construct ( target, argumentsList [, newTarget] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.construct.name, 'construct',
  'The value of `Reflect.construct.name` is `"construct"`'
);

verifyNotEnumerable(Reflect.construct, 'name');
verifyNotWritable(Reflect.construct, 'name');
verifyConfigurable(Reflect.construct, 'name');
