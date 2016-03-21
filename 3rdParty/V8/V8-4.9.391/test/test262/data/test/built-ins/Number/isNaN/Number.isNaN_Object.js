// Copyright (c) 2014 Ryan Lewis. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.1.2.4
author: Ryan Lewis
description: >
    Number.IsNaN should return false if called with a non-number
    Object.
---*/

assert.sameValue(Number.isNaN(new Object()), false, 'Number.isNaN(new Object())');
