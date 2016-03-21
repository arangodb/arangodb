// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    If target is extensible, return the non-falsy trap result if target doesn't
    contain any non-configurable keys.
info: >
    [[OwnPropertyKeys]] ( )

    ...
    19. If extensibleTarget is true and targetNonconfigurableKeys is empty, then
        a. Return trapResult.
---*/

var p = new Proxy({attr: 42}, {
    ownKeys: function() {
        return ["foo", "bar"];
    }
});

var keys = Object.getOwnPropertyNames(p);

assert.sameValue(keys[0], "foo");
assert.sameValue(keys[1], "bar");

assert.sameValue(keys.length, 2);
