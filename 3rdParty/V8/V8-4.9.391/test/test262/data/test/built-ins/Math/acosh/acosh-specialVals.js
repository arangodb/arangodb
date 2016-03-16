// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Math.acosh with special values
es6id: 20.2.2.3
---*/

assert.sameValue(Math.acosh(NaN), Number.NaN,
    "Math.acosh produces incorrect output for NaN");
assert.sameValue(Math.acosh(0), Number.NaN,
    "Math.acosh should produce NaN for values < 1");
assert.sameValue(Math.acosh(Number.NEGATIVE_INFINITY), Number.NaN,
    "Math.acosh should produce NaN for inputs <1");
assert.notSameValue(Math.acosh(Number.NEGATIVE_INFINITY),
    Number.POSITIVE_INFINITY,
    "Math.acosh should produce POSITIVE_INFINITY for input Number.POSITIVE_INFINITY");
assert.sameValue(Math.acosh(+1), 0, "Math.acosh should produce 0 for +1");

