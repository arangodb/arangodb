// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    If target is not extensible, the result can't contain keys names not
    contained in the target object.
info: >
    [[OwnPropertyKeys]] ( )

    ...
    24. If uncheckedResultKeys is not empty, throw a TypeError exception.
---*/

var target = {
    foo: 1
};

var p = new Proxy(target, {
    ownKeys: function() {
        return ["foo", "bar"];
    }
});

Object.preventExtensions(target);

assert.throws(TypeError, function() {
    Object.keys(p);
});
