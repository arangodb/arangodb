// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.14
description: >
    If trap is undefined, propagate the construct to the target object.
info: >
    [[Construct]] ( argumentsList, newTarget)

    7. If trap is undefined, then
        b. Return Construct(target, argumentsList, newTarget).
---*/

function Target(arg) {
    this.attr = arg;
}
var P = new Proxy(Target, {
    construct: undefined
});

assert.sameValue((new P("foo")).attr, "foo");
