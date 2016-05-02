// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.15
description: >
    Proxy ( target, handler )
    ...
    4.  If handler is a Proxy exotic object and the value of the
    [[ProxyHandler]] internal slot of handler is null, throw a
    TypeError exception.
    ...
---*/

var revocable = Proxy.revocable({}, {});

revocable.revoke();

assert.throws(TypeError, function() {
    new Proxy({}, revocable.proxy);
});
