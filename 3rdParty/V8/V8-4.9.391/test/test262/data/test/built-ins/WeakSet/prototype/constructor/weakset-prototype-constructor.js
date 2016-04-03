// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.2
description: >
  WeakSet.prototype.constructor property descriptor
info: >
  WeakSet ( [ iterable ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyNotEnumerable(WeakSet.prototype, 'constructor');
verifyWritable(WeakSet.prototype, 'constructor');
verifyConfigurable(WeakSet.prototype, 'constructor');
