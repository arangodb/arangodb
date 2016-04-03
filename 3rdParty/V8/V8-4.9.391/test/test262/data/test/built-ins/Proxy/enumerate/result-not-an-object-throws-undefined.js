// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.11
description: >
    [[Enumerate]] ()

    The result must be an Object
---*/

var x;
var target = {
    attr: 1
};
var p = new Proxy(target, {
    enumerate: function() {
        return undefined;
    }
});

assert.throws(TypeError, function() {
    for (x in p) {}
});
