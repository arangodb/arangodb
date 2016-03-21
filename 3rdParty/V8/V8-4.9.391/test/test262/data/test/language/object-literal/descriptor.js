// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 12.2.5
description: >
    object literal property shorthand desciptor defaults
---*/
var x = 1;
var object = {x};
var desc = Object.getOwnPropertyDescriptor(object, 'x');
assert.sameValue(desc.value, 1, "The value of `desc.value` is `1`, where `var desc = Object.getOwnPropertyDescriptor(object, 'x');`");
assert.sameValue(desc.enumerable, true, "The value of `desc.enumerable` is `true`, where `var desc = Object.getOwnPropertyDescriptor(object, 'x');`");
assert.sameValue(desc.writable, true, "The value of `desc.writable` is `true`, where `var desc = Object.getOwnPropertyDescriptor(object, 'x');`");
assert.sameValue(desc.configurable, true, "The value of `desc.configurable` is `true`, where `var desc = Object.getOwnPropertyDescriptor(object, 'x');`");
