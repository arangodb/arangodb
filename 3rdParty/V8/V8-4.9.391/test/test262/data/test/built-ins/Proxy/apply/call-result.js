// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.13
description: >
    Return the result from the trap method.
info: >
    [[Call]] (thisArgument, argumentsList)

    9. Return Call(trap, handler, «target, thisArgument, argArray»).
---*/

var target = function(a, b) { return a + b; };
var result = {};
var handler = {
    apply: function(t, c, args) {
        return result;
    }
};
var p = new Proxy(target, handler);

assert.sameValue(p.call(), result);
