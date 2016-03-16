// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.10
description: >
  Reflect.isExtensible.name value and property descriptor
info: >
  26.1.10 Reflect.isExtensible (target)

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Reflect.isExtensible.name, 'isExtensible',
  'The value of `Reflect.isExtensible.name` is `"isExtensible"`'
);

verifyNotEnumerable(Reflect.isExtensible, 'name');
verifyNotWritable(Reflect.isExtensible, 'name');
verifyConfigurable(Reflect.isExtensible, 'name');
