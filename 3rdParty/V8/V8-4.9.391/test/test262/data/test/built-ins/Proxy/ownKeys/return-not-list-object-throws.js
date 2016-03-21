// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    If return is not a list object, throw a TypeError exception
info: >
    [[OwnPropertyKeys]] ( )

    8. Let trapResultArray be Call(trap, handler, «target»).
    9. Let trapResult be CreateListFromArrayLike(trapResultArray, «‍String,
    Symbol»).
    ...
        7.3.17 CreateListFromArrayLike (obj [, elementTypes] )
            3. If Type(obj) is not Object, throw a TypeError exception.
features: [Symbol]
---*/

var target = {};
var p = new Proxy(target, {
    ownKeys: function() {
        return undefined;
    }
});

assert.throws(TypeError, function() {
    Object.keys(p);
});
