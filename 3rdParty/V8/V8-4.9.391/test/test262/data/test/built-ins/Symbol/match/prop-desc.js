// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.4.2.6
description: >
    `Symbol.match` property descriptor
info: >
    This property has the attributes { [[Writable]]: false, [[Enumerable]]:
    false, [[Configurable]]: false }.
includes: [propertyHelper.js]
features: [Symbol.match]
---*/

assert.sameValue(typeof Symbol.match, 'symbol');
verifyNotEnumerable(Symbol, 'match');
verifyNotWritable(Symbol, 'match');
verifyNotConfigurable(Symbol, 'match');
