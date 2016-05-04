// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.11
description: >
    [[Enumerate]] ()

    7. If trap is undefined, then Return target.[[Enumerate]]().
---*/

var x;
var target = {
    attr: 1
};
var p = new Proxy(target, {});

var count = 0;
for (x in p) {
    count++;
}

assert.sameValue(count, 1);
