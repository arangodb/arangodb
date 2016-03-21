// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    [[OwnPropertyKeys]] ( )

    2. If handler is null, throw a TypeError exception.
---*/

var p = Proxy.revocable({}, {});

p.revoke();

assert.throws(TypeError, function() {
    Object.keys(p.proxy);
});
