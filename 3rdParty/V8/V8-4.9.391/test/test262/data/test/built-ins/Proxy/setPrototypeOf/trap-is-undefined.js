// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.2
description: >
    Return target.[[SetPrototypeOf]] (V) if trap is undefined.
---*/

var target = {};
var p = new Proxy(target, {});

Object.setPrototypeOf(p, {attr: 1});

assert.sameValue(target.attr, 1);
