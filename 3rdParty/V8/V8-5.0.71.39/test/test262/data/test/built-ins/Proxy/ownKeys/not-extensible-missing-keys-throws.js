// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.12
description: >
    If target is not extensible, the result must contain all the keys of the own
    properties of the target object.
info: >
    [[OwnPropertyKeys]] ( )

    ...
    23. Repeat, for each key that is an element of targetConfigurableKeys,
        a. If key is not an element of uncheckedResultKeys, throw a TypeError
        exception.
---*/

var target = {
    foo: 1,
    bar: 2
};

var p = new Proxy(target, {
    ownKeys: function() {
        return ["foo"];
    }
});

Object.preventExtensions(target);

assert.throws(TypeError, function() {
    Object.keys(p);
});
