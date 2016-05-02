// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    Trap is not callable.
info: >
    [[OwnPropertyKeys]] ( )

    5. Let trap be GetMethod(handler, "ownKeys").
    ...

    7.3.9 GetMethod (O, P)

    5. If IsCallable(func) is false, throw a TypeError exception.
---*/

var p = new Proxy({attr:1}, {
    ownKeys: {}
});

assert.throws(TypeError, function() {
    Object.keys(p);
});
