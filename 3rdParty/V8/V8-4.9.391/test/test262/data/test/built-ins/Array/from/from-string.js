// Copyright (c) 2014 Hank Yates. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.1.2.1_T1
description: Testing Array.from when passed a String
author: Hank Yates (hankyates@gmail.com)
---*/

var arrLikeSource = 'Test';
var result = Array.from(arrLikeSource);

assert.sameValue(result.length, 4, 'result.length');
assert.sameValue(result[0], 'T', 'result[0]');
assert.sameValue(result[1], 'e', 'result[1]');
assert.sameValue(result[2], 's', 'result[2]');
assert.sameValue(result[3], 't', 'result[3]');
