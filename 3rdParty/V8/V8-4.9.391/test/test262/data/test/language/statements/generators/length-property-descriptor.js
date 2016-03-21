// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Generator functions should define a `length` property.
includes: [propertyHelper.js]
es6id: 25.2.4
---*/

function* g() {}

assert.sameValue(g.length, 0);
verifyNotEnumerable(g, 'length');
verifyNotWritable(g, 'length');
verifyConfigurable(g, 'length');
