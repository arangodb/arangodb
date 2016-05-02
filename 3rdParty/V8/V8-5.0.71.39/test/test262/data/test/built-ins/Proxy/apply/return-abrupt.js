// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.13
description: >
    Return is an abrupt completion
includes: [Test262Error.js]
---*/

var target = function(a, b) { return a + b; };
var p = new Proxy(target, {
    apply: function(t, c, args) {
        throw new Test262Error();
    }
});

assert.throws(Test262Error, function() {
    p.call();
});
