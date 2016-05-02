// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.14
description: >
    Return the result from the trap method.
info: >
    [[Construct]] ( argumentsList, newTarget)

    12. Return newObj
---*/

function Target(a, b) {
    this.sum = a + b;
};
var handler = {
    construct: function(t, c, args) {
        return { sum: 42 };
    }
};
var P = new Proxy(Target, handler);

assert.sameValue((new P(1, 2)).sum, 42);
