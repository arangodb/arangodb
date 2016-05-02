// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.3
description: >
    Map.prototype.delete ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
    typeof Map.prototype.delete,
    'function',
    'typeof Map.prototype.delete is "function"'
);

verifyNotEnumerable(Map.prototype, 'delete');
verifyWritable(Map.prototype, 'delete');
verifyConfigurable(Map.prototype, 'delete');
