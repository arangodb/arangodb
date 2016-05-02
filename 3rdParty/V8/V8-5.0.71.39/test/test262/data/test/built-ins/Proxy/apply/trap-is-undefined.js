// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.13
description: >
    If trap is undefined, propagate the call to the target object.
info: >
    [[Call]] (thisArgument, argumentsList)

    7. If trap is undefined, then Return Call(target, thisArgument,
    argumentsList).
---*/

var target = function(a, b) {
    return a + b;
};
var p = new Proxy(target, {
    apply: undefined
});

assert.sameValue(p(1, 2), 3);
