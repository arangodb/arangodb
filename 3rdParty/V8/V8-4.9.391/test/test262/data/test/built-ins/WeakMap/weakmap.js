// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.1.1
description: >
  WeakMap ( [ iterable ] )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

verifyNotEnumerable(this, 'WeakMap');
verifyWritable(this, 'WeakMap');
verifyConfigurable(this, 'WeakMap');
