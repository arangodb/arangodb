// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Test Object.Assign(target,...sources), string have own enumerable properties, so it can be wrapped to objects.
es6id:  19.1.2.1.5.c
---*/

var target = new Object();
var result = Object.assign(target,"123");

assert.sameValue(result[0], "1", "The value should be {\"0\":\"1\"}.");
assert.sameValue(result[1], "2", "The value should be {\"1\":\"2\"}.");
assert.sameValue(result[2], "3", "The value should be {\"2\":\"3\"}.");
