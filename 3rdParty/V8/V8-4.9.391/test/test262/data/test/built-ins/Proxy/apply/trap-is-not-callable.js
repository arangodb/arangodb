// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.13
description: >
    Throws if trap is not callable.
---*/

var p = new Proxy(function() {}, {
    apply: {}
});

assert.throws(TypeError, function() {
    p();
});
