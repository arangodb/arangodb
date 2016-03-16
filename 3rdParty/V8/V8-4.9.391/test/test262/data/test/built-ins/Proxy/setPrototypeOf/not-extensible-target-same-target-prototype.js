// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.2
description: >
    Return true if boolean trap result is true, target is not extensible, and
    given parameter is the same as target prototype.
features: [Reflect]
---*/

var target = Object.create(Array.prototype);
var p = new Proxy(target, {
    setPrototypeOf: function(t, v) {
        return true;
    }
});

Object.preventExtensions(target);

assert(Reflect.setPrototypeOf(p, Array.prototype));
