// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    If target is extensible, return the non-falsy trap result if it contains all
    of target's non-configurable keys.
info: >
    [[OwnPropertyKeys]] ( )

    ...
    22. If extensibleTarget is true, return trapResult.
---*/

var target = {};

Object.defineProperty(target, "foo", {
    configurable: false,
    enumerable: true,
    value: true
});

var p = new Proxy(target, {
    ownKeys: function() {
        return ["foo", "bar"];
    }
});

var keys = Object.getOwnPropertyNames(p);

assert.sameValue(keys[0], "foo");
assert.sameValue(keys[1], "bar");

assert.sameValue(keys.length, 2);
