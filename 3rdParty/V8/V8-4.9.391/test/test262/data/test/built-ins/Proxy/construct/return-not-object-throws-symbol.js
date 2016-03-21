// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.14
description: >
    Throws a TypeError if trap result is not an Object: Symbol
info: >
    [[Construct]] ( argumentsList, newTarget)

    11. If Type(newObj) is not Object, throw a TypeError exception.
features: [Symbol]
---*/

function Target() {
    this.attr = "done";
};
var P = new Proxy(Target, {
    construct: function() {
        return Symbol();
    }
});

assert.throws(TypeError, function() {
    new P();
});
