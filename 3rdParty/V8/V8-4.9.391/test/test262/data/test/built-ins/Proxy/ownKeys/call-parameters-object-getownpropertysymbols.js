// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    [[OwnPropertyKeys]] ( )

    8. Let trapResultArray be Call(trap, handler, «target»).
features: [Symbol]
---*/

var _target, _handler;
var target = {};
var a = Symbol('a');
var b = Symbol('b');

target[a] = 1;
target[b] = 2;

var handler = {
    ownKeys: function(t) {
        _handler = this;
        _target = t;
        return Object.getOwnPropertySymbols(t);
    }
}
var p = new Proxy(target, handler);

var symbols = Object.getOwnPropertySymbols(p);

assert.sameValue(symbols[0], a);
assert.sameValue(symbols[1], b);
assert.sameValue(symbols.length, 2);
assert.sameValue(_handler, handler);
assert.sameValue(_target, target);
