// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 20.3.4.45
description: Date.prototype[Symbol.toPrimitive] `name` property
info: >
    The value of the name property of this function is "[Symbol.toPrimitive]".

    ES6 Section 17:

    [...]

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
features: [Symbol.toPrimitive]
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Date.prototype[Symbol.toPrimitive].name, '[Symbol.toPrimitive]'
);

verifyNotEnumerable(Date.prototype[Symbol.toPrimitive], 'name');
verifyNotWritable(Date.prototype[Symbol.toPrimitive], 'name');
verifyConfigurable(Date.prototype[Symbol.toPrimitive], 'name');
