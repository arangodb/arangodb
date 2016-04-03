// Copyright 2014 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Array.prototype.splice sets `length` on `this`
es5id: 15.4.4.12_A6.1_T1
description: Array.prototype.splice sets `length` on Array
---*/

var a = [0, 1, 2];

a.splice(1, 2, 4);

if (a.length !== 2) {
    $ERROR("Expected a.length === 2, actually " + a.length);
}

if (a[0] !== 0) {
    $ERROR("Expected a[0] === 0, actually " + a[0]);
}

if (a[1] !== 4) {
    $ERROR("Expected a[1] === 4, actually " + a[1]);
}
