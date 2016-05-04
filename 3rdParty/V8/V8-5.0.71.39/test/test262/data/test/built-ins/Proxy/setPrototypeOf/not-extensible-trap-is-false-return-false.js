// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.2
description: >
    Return false if boolean trap result is false.
features: [Reflect]
---*/

var target = [];
var p = new Proxy(target, {
    setPrototypeOf: function(t, v) {
        return false;
    }
});

Object.preventExtensions(target);

assert.sameValue(Reflect.setPrototypeOf(p, {}), false);
