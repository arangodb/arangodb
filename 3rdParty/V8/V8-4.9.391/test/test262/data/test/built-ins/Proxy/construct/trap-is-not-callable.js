// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.14
description: >
    Throws if trap is not callable.
---*/

function Target() {}
var p = new Proxy(Target, {
    construct: {}
});

assert.throws(TypeError, function() {
    new p();
});
