// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.11
description: >
    Trap returns a iterable result.
info: >
    [[Enumerate]] ()

    11. Return trapResult
includes: [compareArray.js]
---*/

var x;
var iter = [
    {done: false, value: 1},
    {done: false, value: 2},
    {done: false, value: 3},
    {done: true, value: 42}
];
var target = {
    attr: 1
};
var foo = { bar: 1 };
var p = new Proxy(target, {
    enumerate: function() {
        return { next: function() { return iter.shift(); } };
    }
});

var results = [];
for (x in p) {
    results.push(x);
}

assert(compareArray(results, [1,2,3]));
