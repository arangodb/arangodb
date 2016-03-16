// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.6
description: >
    Set.prototype.forEach ( callbackfn [ , thisArg ] )

    ...
    8. Return undefined.

---*/

var s = new Set([1]);

assert.sameValue(
    s.forEach(function() {}),
    undefined,
    "`s.forEach(function() {})` returns `undefined`"
);
