// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.4.2.8
description: >
    `Symbol.replace` property descriptor
info: >
    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: false }.
includes: [propertyHelper.js]
features: [Symbol.replace]
---*/

assert.sameValue(typeof Symbol.replace, 'symbol');
verifyNotEnumerable(Symbol, 'replace');
verifyNotWritable(Symbol, 'replace');
verifyNotConfigurable(Symbol, 'replace');
