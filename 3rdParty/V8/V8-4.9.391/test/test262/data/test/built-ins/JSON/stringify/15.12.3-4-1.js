// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-4-1
description: >
    JSON.stringify ignores replacer aruguments that are not functions
    or arrays..
---*/

assert.sameValue(JSON.stringify([42],{}), '[42]', 'JSON.stringify([42],{})');
