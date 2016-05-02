// Copyright (c) 2014 Ryan Lewis. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.2.2.18
author: Ryan Lewis
description: Math.hypot should return 4 if called with 3 and 2.6457513110645907.
---*/

assert.sameValue(Math.hypot(3,2.6457513110645907), 4, 'Math.hypot(3,2.6457513110645907)');
