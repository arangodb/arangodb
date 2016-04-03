// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.2
description: >
    Return a boolean trap result if target is extensible.
info: >
    [[SetPrototypeOf]] (V)

    ...
    13. If extensibleTarget is true, return booleanTrapResult.
    ...
flags: [onlyStrict]
features: [Reflect]
---*/

var target = {};
var p = new Proxy(target, {
    setPrototypeOf: function(t, v) {
        return v.attr;
    }
});

var result = Reflect.setPrototypeOf(p, { attr: 0 });
assert.sameValue(result, false);

result = Reflect.setPrototypeOf(p, { attr: 1 });
assert.sameValue(result, true);
