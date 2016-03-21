// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.1
description: >
    Map.prototype.clear ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
    typeof Map.prototype.clear,
    'function',
    'typeof Map.prototype.clear is "function"'
);

verifyNotEnumerable(Map.prototype, 'clear');
verifyWritable(Map.prototype, 'clear');
verifyConfigurable(Map.prototype, 'clear');
