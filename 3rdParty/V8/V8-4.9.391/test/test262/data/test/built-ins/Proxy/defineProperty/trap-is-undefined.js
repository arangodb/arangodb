// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.6
description: >
    Return target.[[DefineOwnProperty]](P, Desc) if trap is undefined.
info: >
    [[DefineOwnProperty]] (P, Desc)

    ...
    8. If trap is undefined, then
        a. Return target.[[DefineOwnProperty]](P, Desc).
    ...
includes: [propertyHelper.js]
---*/

var target = {};
var p = new Proxy(target, {});

Object.defineProperty(p, "attr", {
    configurable: true,
    enumerable: true,
    writable: true,
    value: 1
});

verifyEqualTo(target, "attr", 1);
verifyWritable(target, "attr");
verifyEnumerable(target, "attr");
verifyConfigurable(target, "attr");

Object.defineProperty(p, "attr", {
    configurable: false,
    enumerable: false,
    writable: false,
    value: 2
});

verifyEqualTo(target, "attr", 2);
verifyNotWritable(target, "attr");
verifyNotEnumerable(target, "attr");
verifyNotConfigurable(target, "attr");
