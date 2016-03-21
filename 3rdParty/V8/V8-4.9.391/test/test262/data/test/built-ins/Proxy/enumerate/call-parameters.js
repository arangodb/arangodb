// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.11
description: >
    Trap called as trap.call(handler, target)
info: >
    [[Enumerate]] ()

    8. Let trapResult be Call(trap, handler, «target»).
---*/

var x, _target, _handler;
var target = {
    attr: 1
};
var handler = {
    enumerate: function(t) {
        _target = t;
        _handler = this;
    }
};
var p = new Proxy(target, handler);

try {
    for (x in p) {}
} catch(e) {}

assert.sameValue(_handler, handler);
assert.sameValue(_target, target);
