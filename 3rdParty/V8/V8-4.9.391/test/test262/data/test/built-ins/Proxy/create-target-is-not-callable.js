// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.15
description: >
    A Proxy exotic object is only callable if the given target is callable.
info: >
    Proxy ( target, handler )

    7. If IsCallable(target) is true, then
        a. Set the [[Call]] internal method of P as specified in 9.5.13.
    ...


    12.3.4.3 Runtime Semantics: EvaluateDirectCall( func, thisValue, arguments,
    tailPosition )

    4. If IsCallable(func) is false, throw a TypeError exception.
---*/

var p = new Proxy({}, {});

assert.throws(TypeError, function() {
    p.call();
});
