// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.11
description: >
    Trap is not callable.
info: >
    [[Enumerate]] ()

    5. Let trap be GetMethod(handler, "enumerate").
    ...

    7.3.9 GetMethod (O, P)

    5. If IsCallable(func) is false, throw a TypeError exception.
---*/

var x;
var p = new Proxy({attr:1}, {
    enumerate: {}
});

assert.throws(TypeError, function() {
    for (x in p) {}
});
