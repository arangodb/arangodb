// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.3
description: >
  Array.of property descriptor
info: >
  Array.of ( ...items )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyNotEnumerable(Array, 'of');
verifyWritable(Array, 'of');
verifyConfigurable(Array, 'of');
