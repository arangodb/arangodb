// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.15
description: >
    A Proxy exotic object only accepts a constructor call if target is
    constructor.
info: >
    Proxy ( target, handler )

    7. If IsCallable(target) is true, then
        b. If target has a [[Construct]] internal method, then
            i. Set the [[Construct]] internal method of P as specified in
            9.5.14.
    ...

    12.3.3.1.1 Runtime Semantics: EvaluateNew(constructProduction, arguments)

    8. If IsConstructor (constructor) is false, throw a TypeError exception.
---*/

var p = new Proxy(eval, {});

p(); // the Proxy object is callable

assert.throws(TypeError, function() {
    new p();
});
