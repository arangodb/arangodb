// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.5
description: >
  Reflect.enumerate is configurable, writable and not enumerable.
info: >
  26.1.5 Reflect.enumerate ( target )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Reflect, 'enumerate');
verifyWritable(Reflect, 'enumerate');
verifyConfigurable(Reflect, 'enumerate');
